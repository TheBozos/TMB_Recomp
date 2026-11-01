#!/usr/bin/env python3
"""make_build_kit: assembles the build kit tools/tmb_build.py uses on a PC with nothing installed (no MSYS2, git,
network or Python): a compiler, CMake and Ninja, a minimal static FFmpeg, the libraries' sources, the prebuilt
recompilers and an embeddable Python. Run once on the developer's PC (MSYS2 UCRT64 installed, network for the
downloads); the kit is build output, never committed.

  python tools/make_build_kit.py --out D:/tmb_kit/kit [--downloads D:/tmb_kit/downloads] [--zip D:/tmb_kit/kit.zip]

The kit (tmb_build uses it when it sits in <TMB_Recomp>/kit or next to the TMB_Recomp folder, or with --kit <dir>):
  ucrt64/      the toolchain: MSYS2 UCRT64's GCC, binutils, mingw-w64 headers and CRT, winpthreads, CMake and
               Ninja, the files those packages install minus what the build never touches (documentation,
               translations, the GCC plugin headers, lto1, gcov, the target-prefixed duplicates, binutils' own
               libraries, old / Store / API-set import libraries, the CMake GUI and tests), plus the DLLs the kept
               programs load. The folder keeps MSYS2's name: GCC finds its sysroot as <bin>/../../ucrt64.
  ffmpeg/      FFmpeg (official release tarball, pinned, SHA-256 checked) configured LGPL-only with nothing but the
               MPEG-2 video decoder, the MPEG video parser, avcodec, swscale and avutil, built static with the kit's
               compiler (configure in MSYS2's sh; the objects through Ninja, the list read from FFmpeg's Makefiles)
  deps/        <name>-src: the FetchContent sources at the tags the CMake files pin (git, without .git)
  tools/       ps2_recomp.exe and ps2_vu_aot.exe built with the kit, statically linked
  python/      the Windows embeddable Python (python.org, pinned, SHA-256 checked)
  sources/     the FFmpeg tarball the kit's FFmpeg is built from (LGPL: its corresponding source)
  licenses/    each package's license files
  kit_manifest.json   versions, sources, hashes, licenses, sizes

Steps (--only <step> runs one): toolchain, python, deps, ffmpeg, tools, manifest; then --zip.
Standard library only.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from tmb_build import pe_imports  # noqa: E402

KIT_VERSION = 1

# ---- pinned downloads (official upstream only) ----
FFMPEG = {
    'version': '9.0.2',
    'url': 'https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz',
    'sha256': '8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e',
    # ffmpeg-9.0.2.tar.xz.asc checked once with gpg against https://ffmpeg.org/ffmpeg-devel.asc:
    'signing_key': 'FCF986EA15E6E293A5644F10B4322F04D67658D8 (FFmpeg release signing key)',
    'license': 'LGPL-2.1-or-later',
}
PYTHON = {
    'version': '3.14.10',
    'url': 'https://www.python.org/ftp/python/3.14.10/python-3.14.10-embed-amd64.zip',
    'sha256': '90905e64a840557d8c1bc4f5b74db3d7ee85e3ffb217611cac1c7813acea4159',
    'license': 'PSF-2.0',
}
# FetchContent sources: (name as tmb_build's --fetch-cache wants it, repository, tag, the CMake file pinning it)
DEPS = [
    ('raylib', 'https://github.com/raysan5/raylib.git', '5.5', 'PS2Recomp/ps2xRuntime/CMakeLists.txt', 'Zlib'),
    ('imgui', 'https://github.com/ocornut/imgui.git', 'v1.92.7-docking', 'PS2Recomp/ps2xRuntime/CMakeLists.txt', 'MIT'),
    ('rlimgui', 'https://github.com/raylib-extras/rlImGui.git', 'Raylib_5_5', 'PS2Recomp/ps2xRuntime/CMakeLists.txt',
     'Zlib'),
    ('elfio', 'https://github.com/serge1/ELFIO.git', 'Release_3.12', 'PS2Recomp/ps2xRecomp/CMakeLists.txt', 'MIT'),
    ('toml11', 'https://github.com/ToruNiina/toml11.git', 'v4.4.0', 'PS2Recomp/ps2xRecomp/CMakeLists.txt', 'MIT'),
    ('fmt', 'https://github.com/fmtlib/fmt.git', '12.1.0', 'PS2Recomp/ps2xRecomp/CMakeLists.txt', 'MIT'),
    ('libdwarf', 'https://github.com/davea42/libdwarf-code.git', 'v2.2.0', 'PS2Recomp/ps2xRecomp/CMakeLists.txt',
     'LGPL-2.1 (libdwarf)'),
    ('rabbitizer', 'https://github.com/Decompollaborate/rabbitizer.git', '1.14.3',
     'PS2Recomp/ps2xRecomp/CMakeLists.txt', 'MIT'),
]

# What the build never reads in those sources (raylib's examples are only added with BUILD_EXAMPLES; rlImGui and imgui
# are compiled from their top-level files; rlImGui's premake binaries are executables nothing runs; ELFIO's examples and
# tests are only added when it is the top-level project, libdwarf's test/ with DO_TESTING: their sample ELF files would
# also be the only ELF files in a release, which tools/make_release.py refuses).
DEP_PRUNE = {
    'elfio': ['tests', 'examples', 'doc'],
    'libdwarf': ['test'],
    'raylib': ['examples', 'projects'],
    'imgui': ['examples', 'docs'],
    'rlimgui': ['examples', 'resources', 'premake5', 'premake5.exe', 'premake5.osx', 'premake5.lua',
                'raylib_premake5.lua', 'premake-VisualStudio.bat', 'premake-mingw.bat'],
}

# ---- the toolchain ----
MSYS_PACKAGES = ['gcc', 'libgcc', 'binutils', 'crt', 'headers', 'winpthreads', 'libwinpthread',
                 'windows-default-manifest', 'cmake', 'ninja']
KEEP_BIN = {
    'gcc', 'g++', 'c++', 'cc', 'cpp', 'gcc-ar', 'gcc-nm', 'gcc-ranlib',                      # gcc
    'ar', 'as', 'ld', 'ld.bfd', 'nm', 'objcopy', 'objdump', 'ranlib', 'strip', 'windres',  # binutils
    'dlltool', 'addr2line', 'readelf', 'size', 'strings', 'c++filt',
    'cmake', 'ninja',
}
# The DLLs a built program loads, staged next to it by tmb_build (the toolchain's bin is where it finds them).
RUNTIME_DLLS = ['libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll']
# Umbrella import libraries for API sets nothing here links (-lonecore ...): 50 MB of the CRT package.
CRT_DROP = re.compile(r'^lib/lib(onecore|onecoreuap|windowsapp|mincore|nanosrv|wsmsvc|windowscoreheadless|'
                      r'api-ms-win-|ext-ms-win-)[^/]*\.a$')


# Import libraries of the older and the Store / debug C runtimes (msvcr80-120, msvcp60-140, *_app, *d): UCRT64 links
# ucrt (libmsvcrt.a and libucrt*.a stay).
CRT_OLD_RUNTIMES = re.compile(r'^lib/lib(msvcr[0-9]+|msvcp[0-9]+|vcruntime140|concrt140|ucrtbased)[^/]*\.a$|'
                              r'^lib/lib[^/]*_app\.a$')


def prune(pkg, rel):
    """True when the toolchain leaves rel (a path under the UCRT64 prefix) out."""
    parts = rel.split('/')
    top = parts[0]
    if top == 'share':
        return not (len(parts) > 1 and parts[1] == 'cmake' and (len(parts) < 3 or parts[2] != 'Help'))
    if top == 'etc':
        return True
    if top == 'bin':
        name = parts[-1]
        if name.lower().endswith('.exe'):
            return name[:-4] not in KEEP_BIN
        return not name.lower().endswith('.dll')
    if rel.startswith('x86_64-w64-mingw32/bin/'):
        return True  # copies of bin's binutils
    if rel.endswith('-gdb.py') or rel.endswith('.idl'):
        return True
    if pkg == 'binutils' and top in ('lib', 'include'):
        return True  # libbfd, libopcodes, libiberty, libctf and their headers
    if pkg == 'crt' and CRT_DROP.match(rel):
        return True
    if re.match(r'lib/gcc/x86_64-w64-mingw32/[^/]+/(plugin/|install-tools/|lto1\.exe$)', rel):
        return True  # lto1: no build here uses -flto (the runner is RelWithDebInfo; CMake's IPO check turns it off)
    if pkg == 'crt' and CRT_OLD_RUNTIMES.match(rel):
        return True
    if rel.startswith('lib/cmake/') or rel.startswith('lib/pkgconfig/'):
        return True
    return False


class KitError(Exception):
    pass


def release_stamp():
    """<ROOT>/release.ini (tools/make_release.py writes it in a release's source folder, which has no .git): the commits
    the kit's tools and manifest are made from."""
    out = {}
    path = os.path.join(ROOT, 'release.ini')
    if os.path.isfile(path):
        with open(path, encoding='utf-8') as f:
            for line in f:
                if '=' in line and not line.startswith('#'):
                    k, v = line.split('=', 1)
                    out[k.strip()] = v.strip()
    return out


def log(*a):
    print(*a, flush=True)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def rmtree(path):
    """Removes a tree this script made (read-only files included); never follows a junction or symlink out."""
    if not os.path.lexists(path):
        return

    def onerror(func, p, _exc):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    if os.path.islink(path) or (hasattr(os.path, 'isjunction') and os.path.isjunction(path)):
        os.unlink(path)
        return
    shutil.rmtree(path, onerror=onerror)


def dir_size(path):
    total = files = 0
    for dirpath, _dirs, names in os.walk(path):
        for n in names:
            total += os.path.getsize(os.path.join(dirpath, n))
            files += 1
    return total, files


def clean_env(extra_path=()):
    """The Windows system folders only, plus extra_path first: nothing from MSYS2, git or another Python."""
    sysroot = os.environ.get('SystemRoot', r'C:\Windows')
    path = list(extra_path) + [os.path.join(sysroot, 'System32'), sysroot, os.path.join(sysroot, 'System32', 'Wbem')]
    keep = ('SystemRoot', 'SystemDrive', 'windir', 'TEMP', 'TMP', 'USERPROFILE', 'APPDATA', 'LOCALAPPDATA',
            'NUMBER_OF_PROCESSORS', 'PROCESSOR_ARCHITECTURE', 'PROCESSOR_IDENTIFIER', 'ComSpec', 'PATHEXT',
            'HOMEDRIVE', 'HOMEPATH', 'USERNAME', 'ProgramData', 'ProgramFiles', 'ProgramFiles(x86)',
            'CommonProgramFiles', 'OS')
    env = {k: v for k, v in os.environ.items() if k in keep}
    env['PATH'] = os.pathsep.join(path)
    return env


def run(cmd, cwd=None, env=None, logfile=None):
    log('  $ ' + subprocess.list2cmdline(cmd))
    with open(logfile, 'a', encoding='utf-8', errors='replace') if logfile else open(os.devnull, 'w') as lf:
        p = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           universal_newlines=True, encoding='utf-8', errors='replace')
        if logfile:
            lf.write('$ %s\n%s\n' % (subprocess.list2cmdline(cmd), p.stdout))
    if p.returncode != 0:
        sys.stderr.write(p.stdout[-6000:])
        raise KitError('%s failed (exit %d)' % (os.path.basename(cmd[0]), p.returncode))
    return p.stdout


def download(spec, downloads):
    os.makedirs(downloads, exist_ok=True)
    path = os.path.join(downloads, spec['url'].rsplit('/', 1)[1])
    if not (os.path.isfile(path) and sha256_file(path) == spec['sha256']):
        log('  downloading %s' % spec['url'])
        tmp = path + '.part'
        with urllib.request.urlopen(spec['url']) as r, open(tmp, 'wb') as f:
            shutil.copyfileobj(r, f, 1 << 20)
        os.replace(tmp, path)
    digest = sha256_file(path)
    if digest != spec['sha256']:
        raise KitError('%s: SHA-256 %s, expected %s' % (path, digest, spec['sha256']))
    return path


# ---- MSYS2's package database ----

class Msys:
    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.prefix = os.path.join(self.root, 'ucrt64')
        self.db = os.path.join(self.root, 'var', 'lib', 'pacman', 'local')
        if not os.path.isdir(self.db):
            raise KitError('no MSYS2 package database in %s (--msys)' % self.db)
        self.pkgs = {}
        for d in os.listdir(self.db):
            desc = os.path.join(self.db, d, 'desc')
            if not d.startswith('mingw-w64-ucrt-x86_64-') or not os.path.isfile(desc):
                continue
            fields = {k: v or [''] for k, v in self.read_db(desc).items()}
            name = fields['NAME'][0][len('mingw-w64-ucrt-x86_64-'):]
            files = [l for l in self.read_db(os.path.join(self.db, d, 'files')).get('FILES', [])
                     if l.startswith('ucrt64/') and not l.endswith('/')]
            self.pkgs[name] = {
                'package': fields['NAME'][0], 'version': fields['VERSION'][0],
                'base': fields.get('BASE', [''])[0], 'license': ' '.join(fields.get('LICENSE', [])),
                'url': fields.get('URL', [''])[0], 'files': [f[len('ucrt64/'):] for f in files]}
        self.owner = {}
        for name, p in self.pkgs.items():
            for f in p['files']:
                self.owner[f.lower()] = name

    @staticmethod
    def read_db(path):
        out, key = {}, None
        with open(path, encoding='utf-8') as f:
            for line in f:
                line = line.rstrip('\n')
                if line.startswith('%') and line.endswith('%'):
                    key = line[1:-1]
                    out[key] = []
                elif line and key:
                    out[key].append(line)
        return out


# ---- steps ----

class Kit:
    def __init__(self, args):
        self.args = args
        self.out = os.path.abspath(args.out)
        self.downloads = os.path.abspath(args.downloads or os.path.join(os.path.dirname(self.out), 'downloads'))
        self.work = os.path.abspath(args.work or os.path.join(os.path.dirname(self.out), 'kit-work'))
        self.parts = os.path.join(self.out, '.parts')
        os.makedirs(self.parts, exist_ok=True)
        os.makedirs(self.work, exist_ok=True)
        # GCC finds its sysroot (the mingw-w64 headers and libraries) as <bin>/../../ucrt64, as MSYS2 configured it:
        # the folder keeps that name.
        self.toolchain = os.path.join(self.out, 'ucrt64')
        self.bin = os.path.join(self.toolchain, 'bin')
        self._msys = None

    @property
    def msys(self):
        if self._msys is None:
            self._msys = Msys(self.args.msys)
        return self._msys

    def part(self, name, data=None):
        p = os.path.join(self.parts, name + '.json')
        if data is None:
            with open(p) as f:
                return json.load(f)
        with open(p, 'w') as f:
            json.dump(data, f, indent=1)

    def licenses_dir(self, name):
        d = os.path.join(self.out, 'licenses', name)
        rmtree(d)
        os.makedirs(d)
        return d

    # -- toolchain

    def step_toolchain(self):
        m = self.msys
        rmtree(self.toolchain)
        missing = [p for p in MSYS_PACKAGES if p not in m.pkgs]
        if missing:
            raise KitError('MSYS2 UCRT64 packages not installed: %s (pacman -S %s)'
                           % (', '.join(missing), ' '.join('mingw-w64-ucrt-x86_64-' + p for p in missing)))
        copied, dropped = {}, 0
        for pkg in MSYS_PACKAGES:
            for rel in m.pkgs[pkg]['files']:
                src = os.path.join(m.prefix, rel)
                if not os.path.isfile(src):
                    continue
                if prune(pkg, rel):
                    dropped += os.path.getsize(src)
                    continue
                self.copy_in(src, rel)
                copied[rel.lower()] = pkg
        # The DLLs every kept program and DLL loads, from UCRT64's bin (with the package they belong to).
        extra = set()
        queue = [os.path.join(self.toolchain, r) for r in copied if r.endswith(('.exe', '.dll'))]
        queue += [os.path.join(m.prefix, 'bin', d) for d in RUNTIME_DLLS]
        seen = set()
        while queue:
            path = queue.pop()
            names = pe_imports(path)
            if os.path.basename(path) in RUNTIME_DLLS and not path.startswith(self.toolchain):
                names = names + [os.path.basename(path)]
            for name in names:
                key = name.lower()
                if key in seen:
                    continue
                seen.add(key)
                src = os.path.join(m.prefix, 'bin', name)
                if not os.path.isfile(src):
                    continue  # a Windows DLL
                rel = 'bin/' + name
                if rel.lower() not in copied:
                    self.copy_in(src, rel)
                    copied[rel.lower()] = m.owner.get(rel.lower(), '?')
                    extra.add(copied[rel.lower()])
                queue.append(os.path.join(self.toolchain, rel))
        used = sorted(set(copied.values()))
        lic = self.licenses_dir('toolchain')
        for pkg in used:
            d = os.path.join(m.prefix, 'share', 'licenses', pkg)
            if os.path.isdir(d):
                shutil.copytree(d, os.path.join(lic, pkg))
        size, nfiles = dir_size(self.toolchain)
        info = {
            'msys2_packages': {p: {k: m.pkgs[p][k] for k in ('package', 'version', 'base', 'license', 'url')}
                               for p in used if p in m.pkgs},
            'dropped_bytes': dropped, 'bytes': size, 'files': nfiles,
            'pruned': ['share/ except share/cmake (its Help/ too)', 'etc/', 'bin/*.exe not in KEEP_BIN (gcov, '
                       'x86_64-w64-mingw32-*, gprof, elfedit, ld.gold..., cmake-gui, ccmake, ctest, cpack)',
                       'x86_64-w64-mingw32/bin (duplicates)', '*.idl, *-gdb.py', "binutils' lib/ and include/",
                       'lib/gcc/.../plugin, install-tools, lto1.exe', 'old, Store and debug C runtime import '
                       'libraries (msvcr*, msvcp*, *_app, ucrtbased)', 'API-set umbrella import libraries (onecore, '
                       'windowsapp, mincore, ...)', 'lib/cmake, lib/pkgconfig'],
            'keep_bin': sorted(KEEP_BIN),
        }
        self.part('toolchain', info)
        log('  toolchain: %d files, %.1f MB (%.1f MB of the packages left out)' % (nfiles, size / 1e6, dropped / 1e6))
        self.smoke_test()

    def copy_in(self, src, rel):
        dst = os.path.join(self.toolchain, *rel.split('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)

    def smoke_test(self):
        """A C++ program compiled, linked and run with the kit alone (clean PATH), and GCC's search paths checked."""
        d = os.path.join(self.work, 'smoke')
        rmtree(d)
        os.makedirs(d)
        with open(os.path.join(d, 'hello.cpp'), 'w') as f:
            f.write('#include <cstdio>\n#include <string>\n#include <thread>\n#include <windows.h>\n'
                    'int main() { std::string s = "kit"; std::thread t([&] { s += " ok"; }); t.join();\n'
                    '  std::printf("%s %lu\\n", s.c_str(), GetCurrentProcessId() ? 1ul : 0ul); }\n')
        env = clean_env([self.bin])
        out = run([os.path.join(self.bin, 'g++.exe'), '-v', '-O2', 'hello.cpp', '-o', 'hello.exe'], cwd=d, env=env)
        leaks = [l for l in out.splitlines() if 'msys64' in l.lower() and not l.startswith('Configured with')]
        if leaks:
            raise KitError('GCC still looks in MSYS2: ' + leaks[0])
        out = run([os.path.join(d, 'hello.exe')], cwd=d, env=env)
        if 'kit ok' not in out:
            raise KitError('the test program printed %r' % out)
        log('  smoke test: %s' % out.strip())

    # -- python

    def step_python(self):
        z = download(PYTHON, self.downloads)
        dst = os.path.join(self.out, 'python')
        rmtree(dst)
        with zipfile.ZipFile(z) as zf:
            zf.extractall(dst)
        lic = self.licenses_dir('python')
        shutil.copy2(os.path.join(dst, 'LICENSE.txt'), lic)
        out = run([os.path.join(dst, 'python.exe'), '-I', '-c', 'import sys, hashlib, lzma, zlib, json, subprocess; '
                   'print(sys.version.split()[0])'], env=clean_env())
        self.part('python', dict(PYTHON, file=os.path.basename(z)))
        log('  python %s' % out.strip())

    # -- deps

    def git(self):
        for c in (shutil.which('git'), r'C:\Program Files\Git\cmd\git.exe'):
            if c and os.path.isfile(c):
                return c
        raise KitError('git is needed to fetch the libraries\' sources')

    def step_deps(self):
        git = self.git()
        dst_root = os.path.join(self.out, 'deps')
        os.makedirs(dst_root, exist_ok=True)
        info = {}
        for name, repo, tag, cmake_file, lic in DEPS:
            with open(os.path.join(ROOT, cmake_file), encoding='utf-8') as f:
                text = f.read()
            if not re.search(r'GIT_REPOSITORY\s+"?%s"?\s+GIT_TAG\s+"?%s"?' % (re.escape(repo), re.escape(tag)), text):
                raise KitError('%s no longer pins %s at %s: update DEPS' % (cmake_file, repo, tag))
            dst = os.path.join(dst_root, name + '-src')
            rmtree(dst)
            run([git, '-c', 'advice.detachedHead=false', 'clone', '--quiet', '--depth', '1', '--branch', tag,
                 repo, dst])
            commit = run([git, '-C', dst, 'rev-parse', 'HEAD']).strip()
            rmtree(os.path.join(dst, '.git'))
            for rel in DEP_PRUNE.get(name, []):
                p = os.path.join(dst, rel)
                if os.path.isdir(p):
                    rmtree(p)
                elif os.path.isfile(p):
                    os.remove(p)
            size, nfiles = dir_size(dst)
            info[name] = {'repository': repo, 'tag': tag, 'commit': commit, 'license': lic,
                          'left_out': DEP_PRUNE.get(name, []),
                          'bytes': size, 'files': nfiles}
            log('  %s %s %s (%.1f MB)' % (name, tag, commit[:12], size / 1e6))
        self.part('deps', info)

    # -- ffmpeg

    def step_ffmpeg(self):
        tarball = download(FFMPEG, self.downloads)
        src_root = os.path.join(self.work, 'ffmpeg')
        rmtree(src_root)
        os.makedirs(src_root)
        with tarfile.open(tarball) as tf:
            tf.extractall(src_root)
        src = os.path.join(src_root, 'ffmpeg-' + FFMPEG['version'])
        logf = os.path.join(self.work, 'ffmpeg.log')
        if os.path.exists(logf):
            os.remove(logf)
        sh = os.path.join(os.path.abspath(self.args.msys), 'usr', 'bin', 'sh.exe')
        if not os.path.isfile(sh):
            raise KitError('FFmpeg\'s configure needs MSYS2\'s sh (%s)' % sh)
        env = clean_env([self.bin, os.path.dirname(sh)])
        flags = FFMPEG_CONFIGURE
        out = run([sh, './configure'] + flags, cwd=src, env=env, logfile=logf)
        license_line = re.search(r'^License: (.*)$', out, re.M)
        if not license_line or 'LGPL' not in license_line.group(1):
            raise KitError('FFmpeg configure reports %r' % (license_line.group(0) if license_line else out[-300:]))
        with open(os.path.join(src, 'ffbuild', 'config.mak'), encoding='utf-8') as f:
            config_mak = f.read()
        for bad in ('CONFIG_GPL=yes', 'CONFIG_NONFREE=yes', 'CONFIG_VERSION3=yes'):
            if re.search('^' + bad + '$', config_mak, re.M):
                raise KitError('FFmpeg configured with ' + bad)
        run([sh, 'ffbuild/version.sh', '.', 'libavutil/ffversion.h'], cwd=src, env=env, logfile=logf)
        mk = MakeEval(src)
        libs = {}
        for lib in FFMPEG_LIBS:
            libs[lib] = mk.library_objects(lib)
        ninja = os.path.join(src, 'build.ninja')
        write_ffmpeg_ninja(ninja, mk, libs)
        run([os.path.join(self.bin, 'ninja.exe'), '-C', src, '-j', str(self.args.jobs)], env=env, logfile=logf)
        dst = os.path.join(self.out, 'ffmpeg')
        rmtree(dst)
        os.makedirs(os.path.join(dst, 'lib', 'pkgconfig'))
        version = {}
        for lib in FFMPEG_LIBS:
            shutil.copy2(os.path.join(src, 'lib' + lib, 'lib%s.a' % lib), os.path.join(dst, 'lib'))
            for h in libs[lib]['headers']:
                hd = os.path.join(dst, 'include', 'lib' + lib, h)
                os.makedirs(os.path.dirname(hd), exist_ok=True)
                shutil.copy2(os.path.join(src, 'lib' + lib, h), hd)
            version[lib] = lib_version(src, lib)
            write_pc(os.path.join(dst, 'lib', 'pkgconfig', 'lib%s.pc' % lib), lib, version[lib], mk)
        lic = self.licenses_dir('ffmpeg')
        for n in ('COPYING.LGPLv2.1', 'LICENSE.md', 'CREDITS'):
            shutil.copy2(os.path.join(src, n), lic)
        os.makedirs(os.path.join(self.out, 'sources'), exist_ok=True)
        shutil.copy2(tarball, os.path.join(self.out, 'sources'))
        size, nfiles = dir_size(dst)
        self.part('ffmpeg', dict(FFMPEG, configure=flags, configure_license=license_line.group(1), libraries=version,
                                 bytes=size, files=nfiles, objects={lib: len(libs[lib]['objs']) for lib in FFMPEG_LIBS},
                                 system_libs=mk.extralibs()))
        log('  ffmpeg %s: %s, %.1f MB' % (FFMPEG['version'], ', '.join('%s %d objects' % (lib, len(libs[lib]['objs']))
                                                                     for lib in FFMPEG_LIBS), size / 1e6))

    # -- tools

    def step_tools(self):
        build = os.path.join(self.work, 'tools-build')
        rmtree(build)
        env = clean_env([self.bin])
        b = lambda n: os.path.join(self.bin, n + '.exe').replace('\\', '/')  # noqa: E731
        deps = os.path.join(self.out, 'deps')
        fetch = ['-DFETCHCONTENT_SOURCE_DIR_%s=%s' % (n.upper(), os.path.join(deps, n + '-src').replace('\\', '/'))
                 for n, *_ in DEPS if os.path.isdir(os.path.join(deps, n + '-src'))]
        fetch.append('-DFETCHCONTENT_FULLY_DISCONNECTED=ON')
        logf = os.path.join(self.work, 'tools.log')
        if os.path.exists(logf):
            os.remove(logf)
        run([b('cmake'), '-S', os.path.join(ROOT, 'PS2Recomp'), '-B', build, '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Release', '-DPS2X_BUILD_RUNTIME=OFF', '-DPS2X_BUILD_STUDIO=OFF',
             '-DPS2X_BUILD_TEST=OFF', '-DPS2X_BUILD_ANALYZER=OFF',
             '-DCMAKE_C_COMPILER=' + b('gcc'), '-DCMAKE_CXX_COMPILER=' + b('g++'), '-DCMAKE_MAKE_PROGRAM=' + b('ninja'),
             '-DCMAKE_EXE_LINKER_FLAGS=-static'] + fetch, env=env, logfile=logf)
        run([b('cmake'), '--build', build, '--target', 'ps2_recomp', 'ps2_vu_aot', '-j', str(self.args.jobs)],
            env=env, logfile=logf)
        dst = os.path.join(self.out, 'tools')
        rmtree(dst)
        os.makedirs(dst)
        info = {}
        for t in ('ps2_recomp', 'ps2_vu_aot'):
            exe = os.path.join(build, 'ps2xRecomp', t + '.exe')
            run([b('strip'), '--strip-debug', exe], env=env)
            shutil.copy2(exe, dst)
            imports = pe_imports(exe)
            local = [n for n in imports if os.path.isfile(os.path.join(self.bin, n))]
            if local:
                raise KitError('%s still loads %s' % (t, ', '.join(local)))
            info[t] = {'bytes': os.path.getsize(exe), 'sha256': sha256_file(exe), 'imports': sorted(imports)}
            log('  %s: %.1f MB, imports %s' % (t, info[t]['bytes'] / 1e6, ', '.join(sorted(imports))))
        info['fork_commit'] = release_stamp().get('fork_commit') or \
            run([self.git(), '-C', os.path.join(ROOT, 'PS2Recomp'), 'rev-parse', 'HEAD']).strip()
        self.part('tools', info)

    # -- manifest

    def step_manifest(self):
        parts = {n: self.part(n) for n in ('toolchain', 'python', 'deps', 'ffmpeg', 'tools')}
        sizes = {}
        for d in ('ucrt64', 'ffmpeg', 'deps', 'tools', 'python', 'sources', 'licenses'):
            sizes[d] = dir_size(os.path.join(self.out, d))[0]
        manifest = {
            'kit': 'TMB_Recomp build kit', 'kit_version': KIT_VERSION,
            'made': time.strftime('%Y-%m-%d %H:%M:%S'), 'made_from': self.tmb_commit(),
            'layout': {
                'ucrt64': 'the toolchain: GCC, binutils, mingw-w64, winpthreads, CMake, Ninja (MSYS2 UCRT64 '
                          'packages, pruned)',
                'ffmpeg': 'static FFmpeg: avcodec (mpeg2video decoder, mpegvideo parser), swscale, avutil',
                'deps': '<name>-src FetchContent sources',
                'tools': 'ps2_recomp.exe, ps2_vu_aot.exe (static)',
                'python': 'Windows embeddable Python',
                'sources': 'FFmpeg source tarball',
                'licenses': 'license files per component'},
            'toolchain': parts['toolchain'], 'ffmpeg': parts['ffmpeg'], 'python': parts['python'],
            'deps': parts['deps'], 'tools': parts['tools'], 'bytes': sizes, 'total_bytes': sum(sizes.values()),
            'licenses': {
                'gcc': 'GPL-3.0-or-later; the runtime libraries it links into a program (libgcc, libstdc++, '
                       'libatomic) under the GCC Runtime Library Exception 3.1: the built game carries no GPL '
                       'obligation',
                'binutils': 'GPL-3.0-or-later (tools only, nothing linked into the game)',
                'mingw-w64': 'mingw-w64 runtime and headers: ZPL-2.1 / public domain / MIT-style (see licenses)',
                'winpthreads': 'MIT and BSD-3-Clause',
                'cmake': 'BSD-3-Clause', 'ninja': 'Apache-2.0',
                'ffmpeg': 'LGPL-2.1-or-later, configured without --enable-gpl / --enable-version3 / '
                          '--enable-nonfree; its source in sources/',
                'python': 'PSF-2.0',
                'deps': {n: lic for n, _r, _t, _c, lic in DEPS},
                'gpl_source_note': 'GCC, binutils and the other MSYS2 binaries are redistributed unmodified: their '
                                   'corresponding sources are the MSYS2 source packages of the versions listed in '
                                   'toolchain.msys2_packages (repo.msys2.org/mingw/sources); a public kit must ship or '
                                   'offer them'},
        }
        with open(os.path.join(self.out, 'kit_manifest.json'), 'w') as f:
            json.dump(manifest, f, indent=1)
        log('  kit: %.1f MB (%s)' % (sum(sizes.values()) / 1e6,
                                     ', '.join('%s %.1f' % (k, v / 1e6) for k, v in sizes.items())))

    def tmb_commit(self):
        if release_stamp().get('commit'):
            return release_stamp()['commit']
        try:
            return run([self.git(), '-C', ROOT, 'rev-parse', 'HEAD']).strip()
        except KitError:
            return None

    def make_zip(self, path):
        t = time.time()
        tmp = path + '.part'
        top = os.path.basename(self.out)
        with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for dirpath, dirs, names in os.walk(self.out):
                dirs[:] = sorted(d for d in dirs if d != '.parts')
                for n in sorted(names):
                    p = os.path.join(dirpath, n)
                    zf.write(p, os.path.join(top, os.path.relpath(p, self.out)))
        os.replace(tmp, path)
        log('  %s: %.1f MB (%.0f s)' % (path, os.path.getsize(path) / 1e6, time.time() - t))


# ---- FFmpeg: configure flags, the Makefiles read, the Ninja file ----

FFMPEG_LIBS = ['avcodec', 'swscale', 'avutil']  # link order
FFMPEG_CONFIGURE = [
    '--prefix=/kit/ffmpeg', '--arch=x86_64', '--target-os=mingw32',
    '--cc=gcc', '--cxx=g++', '--ar=ar', '--nm=nm', '--ranlib=ranlib', '--strip=strip', '--windres=windres',
    '--pkg-config=false',
    '--enable-static', '--disable-shared', '--disable-programs', '--disable-doc', '--disable-debug',
    '--disable-everything', '--disable-autodetect', '--disable-network',
    '--disable-avdevice', '--disable-avformat', '--disable-avfilter', '--disable-swresample',
    '--enable-avcodec', '--enable-swscale', '--enable-avutil',
    '--enable-decoder=mpeg2video', '--enable-parser=mpegvideo',
    '--enable-w32threads', '--disable-x86asm',
]


class MakeEval:
    """Just enough of GNU make to read FFmpeg's library Makefiles: assignments (=, :=, +=, ?=, immediate), the
    conditionals, include, $(VAR), $(VAR:a=b) and the few functions they use. Rules and recipes are skipped."""

    def __init__(self, src):
        self.src = src
        self.vars = {'MAIN_MAKEFILE': '1'}  # as FFmpeg's top-level Makefile reads config.mak
        self.read(os.path.join(src, 'ffbuild', 'config.mak'))
        self.config = dict(self.vars)

    def expand(self, s):
        out, i = [], 0
        while i < len(s):
            c = s[i]
            if c == '$' and i + 1 < len(s):
                n = s[i + 1]
                if n in '({':
                    close = ')' if n == '(' else '}'
                    depth, j = 1, i + 2
                    while j < len(s) and depth:
                        if s[j] in '({':
                            depth += 1
                        elif s[j] in ')}':
                            depth -= 1
                        j += 1
                    out.append(self.ref(s[i + 2:j - 1]))
                    i = j
                    continue
                if n == '$':
                    out.append('$')
                    i += 2
                    continue
                out.append(self.vars.get(n, ''))
                i += 2
                continue
            out.append(c)
            i += 1
        return ''.join(out)

    def split_args(self, s):
        args, depth, cur = [], 0, []
        for c in s:
            if c in '({':
                depth += 1
            elif c in ')}':
                depth -= 1
            if c == ',' and depth == 0:
                args.append(''.join(cur))
                cur = []
            else:
                cur.append(c)
        args.append(''.join(cur))
        return args

    def ref(self, body):
        m = re.match(r'(addprefix|addsuffix|if|subst|sort|filter|filter-out|strip|patsubst|wildcard|firstword)\s(.*)$',
                     body, re.S)
        if m:
            fn, rest = m.group(1), m.group(2)
            a = self.split_args(rest)
            if fn == 'if':
                return self.expand(a[1]) if self.expand(a[0]).strip() else (self.expand(a[2]) if len(a) > 2 else '')
            a = [self.expand(x) for x in a]
            if fn == 'addprefix':
                return ' '.join(a[0] + w for w in a[1].split())
            if fn == 'addsuffix':
                return ' '.join(w + a[0] for w in a[1].split())
            if fn == 'subst':
                return a[2].replace(a[0], a[1])
            if fn == 'sort':
                return ' '.join(sorted(set(a[0].split())))
            if fn == 'strip':
                return ' '.join(a[0].split())
            if fn == 'firstword':
                w = a[0].split()
                return w[0] if w else ''
            if fn in ('filter', 'filter-out'):
                pats = a[0].split()
                keep = [w for w in a[1].split() if any(pat_match(p, w) is not None for p in pats)]
                return ' '.join(keep if fn == 'filter' else [w for w in a[1].split() if w not in keep])
            if fn == 'patsubst':
                return ' '.join(pat_subst(a[0], a[1], w) for w in a[2].split())
            if fn == 'wildcard':
                return ''
        m = re.match(r'([^:]+):([^=]*)=(.*)$', body)
        if m:
            words = self.expand('$(' + m.group(1) + ')').split()
            frm, to = m.group(2), m.group(3)
            if '%' not in frm:
                frm, to = '%' + frm, '%' + to
            return ' '.join(pat_subst(frm, to, w) for w in words)
        return self.vars.get(self.expand(body), '')

    def read(self, path, must=True):
        if not os.path.isfile(path):
            if must:
                raise KitError('no ' + path)
            return
        with open(path, encoding='utf-8') as f:
            raw = f.read().replace('\\\n', ' ').splitlines()
        stack = []  # each: [active, taken]
        in_define = False
        for line in raw:
            if in_define:
                if line.strip() == 'endef':
                    in_define = False
                continue
            if line.startswith('\t'):
                continue
            s = line.split('#', 1)[0].strip() if '#' in line and not line.lstrip().startswith('$(') else line.strip()
            if not s:
                continue
            active = all(st[0] for st in stack)
            word = s.split(None, 1)[0]
            if word in ('ifdef', 'ifndef', 'ifeq', 'ifneq'):
                cond = False
                if active:
                    arg = s[len(word):].strip()
                    if word in ('ifdef', 'ifndef'):
                        cond = bool(self.vars.get(self.expand(arg), '').strip())
                        cond = cond if word == 'ifdef' else not cond
                    else:
                        a, b = self.cond_args(arg)
                        cond = (a == b) if word == 'ifeq' else (a != b)
                stack.append([cond, cond])
                continue
            if word == 'else':
                top = stack[-1]
                rest = s[4:].strip()
                if rest:  # else ifeq ...
                    w2 = rest.split(None, 1)[0]
                    cond = False
                    if not top[1] and all(st[0] for st in stack[:-1]):
                        arg = rest[len(w2):].strip()
                        if w2 in ('ifdef', 'ifndef'):
                            cond = bool(self.vars.get(self.expand(arg), '').strip())
                            cond = cond if w2 == 'ifdef' else not cond
                        else:
                            a, b = self.cond_args(arg)
                            cond = (a == b) if w2 == 'ifeq' else (a != b)
                    top[0] = cond
                    top[1] = top[1] or cond
                else:
                    top[0] = not top[1]
                    top[1] = True
                continue
            if word == 'endif':
                stack.pop()
                continue
            if word == 'define':
                in_define = True
                continue
            if not active:
                continue
            if word in ('include', '-include', 'sinclude'):
                for p in self.expand(s[len(word):].strip()).split():
                    p = p.replace('$(SRC_PATH)', self.src)
                    full = p if os.path.isabs(p) else os.path.join(self.src, p)
                    self.read(full, must=(word == 'include'))
                continue
            if word in ('override', 'export', 'unexport', 'vpath', '.SUFFIXES:'):
                continue
            m = re.match(r'^([^:=+?!\s][^=]*?)\s*(\+=|:=|::=|\?=|=)\s*(.*)$', s)
            if m and ':' not in m.group(1):
                name = self.expand(m.group(1)).strip()
                op, val = m.group(2), self.expand(m.group(3))
                if op == '+=':
                    self.vars[name] = (self.vars.get(name, '') + ' ' + val).strip()
                elif op == '?=':
                    self.vars.setdefault(name, val)
                else:
                    self.vars[name] = val
                continue
            # a rule, $(eval ...), $(foreach ...): not needed for the object lists

    def cond_args(self, arg):
        arg = arg.strip()
        if arg.startswith('('):
            a = self.split_args(arg[1:-1])
            return self.expand(a[0]).strip(), self.expand(a[1] if len(a) > 1 else '').strip()
        q = re.findall(r'"([^"]*)"|\'([^\']*)\'', arg)
        vals = [x or y for x, y in q]
        return self.expand(vals[0]).strip(), self.expand(vals[1]).strip()

    SUBDIR_VARS = ['OBJS', 'X86ASM-OBJS', 'HEADERS', 'ARCH_HEADERS', 'BUILT_HEADERS', 'SKIPHEADERS', 'SHLIBOBJS',
                   'STLIBOBJS', 'TESTPROGS', 'HOSTPROGS', 'TOOLS']

    def library_objects(self, lib):
        """FFmpeg's Makefile DOSUBDIR for lib<lib>, then common.mak's OBJS += $(OBJS-yes), sorted, prefixed."""
        saved = dict(self.vars)
        for v in self.SUBDIR_VARS:
            self.vars[v] = ''
            self.vars[v + '-yes'] = ''
        self.vars['SUBDIR'] = 'lib%s/' % lib
        base = os.path.join(self.src, 'lib' + lib)
        self.read(os.path.join(base, 'Makefile'))
        self.read(os.path.join(base, self.vars.get('ARCH', ''), 'Makefile'), must=False)
        if self.vars.get('INTRINSICS'):
            self.read(os.path.join(base, self.vars['INTRINSICS'], 'Makefile'), must=False)
        self.read(os.path.join(self.src, 'ffbuild', 'arch.mak'))
        objs = (self.vars.get('OBJS', '') + ' ' + self.vars.get('OBJS-yes', '')).split()
        objs += self.vars.get('STLIBOBJS', '').split() + self.vars.get('STLIBOBJS-yes', '').split()
        headers = self.vars.get('HEADERS', '').split() + self.vars.get('HEADERS-yes', '').split()
        headers += self.vars.get('BUILT_HEADERS', '').split()
        name = self.vars.get('NAME', lib)
        extralibs = self.vars.get('EXTRALIBS-' + name, '')
        self.vars = saved
        self.vars['EXTRALIBS-' + name] = extralibs
        return {'objs': sorted(set('lib%s/%s' % (lib, o) for o in objs)), 'headers': headers,
                'extralibs': extralibs}

    def extralibs(self):
        return {lib: self.config.get('EXTRALIBS-' + lib, '') for lib in FFMPEG_LIBS}


def pat_match(pat, word):
    if '%' not in pat:
        return '' if pat == word else None
    a, b = pat.split('%', 1)
    if word.startswith(a) and word.endswith(b) and len(word) >= len(a) + len(b):
        return word[len(a):len(word) - len(b)]
    return None


def pat_subst(frm, to, word):
    stem = pat_match(frm, word)
    if stem is None:
        return word
    return to.replace('%', stem, 1) if '%' in to else to


def ninja_escape(s):
    return s.replace('$', '$$').replace(' ', '$ ').replace(':', '$:')


def write_ffmpeg_ninja(path, mk, libs):
    c = mk.config
    cppflags = '-I. -I./ ' + c.get('CPPFLAGS', '')
    cflags = c.get('CFLAGS', '') + ' ' + c.get('ECFLAGS', '')
    lines = ['# made by tools/make_build_kit.py from FFmpeg\'s Makefiles', 'ninja_required_version = 1.10',
             'rule cc', '  command = %s %s %s -DHAVE_AV_CONFIG_H -MMD -MF $out.d -c -o $out $in'
             % (c.get('CC', 'gcc'), cppflags, cflags),
             '  depfile = $out.d', '  deps = gcc', '  description = CC $out',
             'rule ar', '  command = %s rcD $out @$out.rsp' % c.get('AR', 'ar'), '  rspfile = $out.rsp',
             '  rspfile_content = $in', '  description = AR $out']
    for lib, info in libs.items():
        objs = []
        for o in info['objs']:
            base = o[:-2]
            srcs = [base + ext for ext in ('.c', '.S') if os.path.isfile(os.path.join(mk.src, base + ext))]
            if not srcs:
                raise KitError('FFmpeg: no source for %s' % o)
            lines.append('build %s: cc %s' % (ninja_escape(o), ninja_escape(srcs[0])))
            objs.append(o)
        a = 'lib%s/lib%s.a' % (lib, lib)
        lines.append('build %s: ar %s' % (a, ' '.join(ninja_escape(o) for o in objs)))
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def lib_version(src, lib):
    path = os.path.join(src, 'lib' + lib, 'version_major.h')
    vals = {}
    for p in (path, os.path.join(src, 'lib' + lib, 'version.h')):
        if os.path.isfile(p):
            with open(p, encoding='utf-8') as f:
                for m in re.finditer(r'#define LIB%s_VERSION_(MAJOR|MINOR|MICRO)\s+(\d+)' % lib.upper(), f.read()):
                    vals[m.group(1)] = m.group(2)
    return '%s.%s.%s' % (vals.get('MAJOR', '0'), vals.get('MINOR', '0'), vals.get('MICRO', '0'))


def write_pc(path, lib, version, mk):
    private = mk.extralibs().get(lib, '')
    with open(path, 'w', newline='\n') as f:
        f.write('prefix=${pcfiledir}/../..\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\n'
                'Name: lib%s\nDescription: FFmpeg %s library (build kit, static)\nVersion: %s\n'
                'Libs: -L${libdir} -l%s\nLibs.private: %s\nCflags: -I${includedir}\n'
                % (lib, lib, version, lib, private))


STEPS = ['toolchain', 'python', 'deps', 'ffmpeg', 'tools', 'manifest']


def main():
    ap = argparse.ArgumentParser(description='Assembles the TMB_Recomp build kit (see the docstring).')
    ap.add_argument('--out', required=True, help='the kit folder to write (outside the repository, or under build/)')
    ap.add_argument('--msys', default='C:/msys64', help='the MSYS2 installation the toolchain comes from')
    ap.add_argument('--downloads', help='where the FFmpeg and Python downloads are kept (default: next to --out)')
    ap.add_argument('--work', help='scratch folder for the FFmpeg and tools builds (default: next to --out)')
    ap.add_argument('--only', choices=STEPS, action='append', help='run only these steps')
    ap.add_argument('--zip', help='also write the kit as this .zip')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    out = os.path.normcase(os.path.abspath(args.out))
    if out.startswith(os.path.normcase(ROOT) + os.sep) and \
            not out.startswith(os.path.normcase(os.path.join(ROOT, 'build')) + os.sep):
        sys.exit('--out inside the repository must be under build/ (the kit is never committed)')
    kit = Kit(args)
    t0 = time.time()
    try:
        for step in STEPS:
            if args.only and step not in args.only:
                continue
            log('[%s]' % step)
            t = time.time()
            getattr(kit, 'step_' + step)()
            log('  %.1f s' % (time.time() - t))
        if args.zip:
            log('[zip]')
            kit.make_zip(os.path.abspath(args.zip))
    except KitError as e:
        sys.exit('make_build_kit: %s' % e)
    log('done in %.1f s' % (time.time() - t0))


if __name__ == '__main__':
    main()
