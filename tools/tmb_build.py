#!/usr/bin/env python3
"""tmb_build: from the player's own Twisted Metal: Black disc (USA, SCUS-97101) to a game folder that runs, in one
command. Nothing derived from the game is distributed: the disc's files, the generated code, the packs and the
executable are all made here, on the player's PC.

  python tools/tmb_build.py --disc "Twisted Metal - Black (USA).iso" --out D:/TMB
  python tools/tmb_build.py --disc D:/TMB_Recomp/game_us --out D:/TMB          (an extracted disc folder)

Steps (each skipped when an earlier run finished it with the same inputs; --redo <step> runs it and the ones after
again):
  verify     the disc image or folder: ISO9660 (.iso, .bin/.cue), SYSTEM.CNF's boot file, SCUS_971.01's SHA-1
  extract    the disc's files into <out>/TMB/disc (the runtime's cdrom0:), no outside tool (PS2Recomp/tools/ps2_disc.py),
             and the image itself as <out>/TMB/disc.iso (the game reads raw sectors too): a hard link to the .iso on
             the same drive, else a copy (a .bin / .cue converted)
  tools      ps2_recomp and ps2_vu_aot: --tools <dir> (prebuilt), or built from PS2Recomp with the toolchain
  recompile  ps2_recomp (recomp/tmb_scus97101.toml) and ps2_vu_aot (VU1 microcode) -> <out>/generated
  packs      the texture and mesh packs (tools/tmb_texture_pack.py, tools/tmb_mesh_pack.py) -> <out>/generated
  iop        the IOP modules loaded offline from the disc at their bases (recomp/tmb_iop_modules.json,
             PS2Recomp/tools/irx_load.py) and recompiled (PS2Recomp/tools/iop_recomp.py) -> <out>/generated/iop
  configure  cmake (Ninja) of the runner with the generated code
  compile    the runner (ps2EntryRunner)
  stage      <out>/TMB: the executable, the DLLs it loads from the toolchain, the packs, "Play TMB.cmd"

The build kit (tools/make_build_kit.py): a folder with kit_manifest.json, the compiler (ucrt64/), a minimal static
FFmpeg (ffmpeg/), the libraries' sources (deps/), the prebuilt recompilers (tools/) and an embeddable Python. Used
when it sits in <TMB_Recomp>/kit or next to the TMB_Recomp folder (<parent>/kit), or with --kit <dir> (--no-kit: not
used). With it nothing else is needed (no MSYS2, git, network or installed Python), and the runner is the player's
build: FFmpeg from the kit (no pkg-config, no FFmpeg DLLs: the game folder gets the exe and the three GCC runtime
DLLs), paraLLEl-GS left out (PS2X_ENABLE_PARALLEL_GS=OFF, a GS-emulation diagnostic the native port never uses).
  kit/python/python.exe -I tools/tmb_build.py --disc "Twisted Metal - Black (USA).iso" --out D:/TMB

Without a kit: --toolchain <dir> (a folder whose bin/ has g++, cmake, ninja, pkg-config: an MSYS2 UCRT64 prefix such
as C:/msys64/ucrt64), else the PATH, else C:/msys64/ucrt64; FFmpeg through pkg-config; FetchContent's sources from the
network (git) unless --fetch-cache <dir> holds <name>-src folders. --toolchain, --tools and --fetch-cache also
override the kit's.

A player's build (the default with the kit, --debug-info no): the runner without debug information, the same code as
RelWithDebInfo (its -O2 flags without -g; GCC's -g never changes the code), stripped again when staged: 45 MB instead
of 613. --game <dir> stages the game somewhere else than <out>/TMB (the launcher: <release>/game). --check only checks
the disc (no work folder): OK <message>, or ERROR verify <message> and exit code 1.

Output for a launcher, one line each on stdout (everything else goes to <out>/logs/<step>.log):
  PROGRESS <step> <i>/<n> <message>     a step starts
  PERCENT <step> <0-100>                within a long step (extract, packs, compile)
  SKIP <step> <i>/<n>                   finished by an earlier run
  DONE <step> <i>/<n> <seconds>
  ERROR <step> <message>                then exit code 1
  RESULT <game folder> <launcher>       all done

Standard library only (Python 3.8+): runs on a bundled Windows embeddable Python.
"""
import argparse
import filecmp
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'PS2Recomp', 'tools'))
import ps2_disc  # noqa: E402
import irx_load  # noqa: E402

ELF_NAME = 'SCUS_971.01'
ELF_SHA1 = '3ff17f4897552e355d0bb8c3abfa5125fe3ff2ef'
OTHER_RELEASES = {
    'SCES_503.60': 'the European release (SCES-50360)',
    'SCUS_971.01': 'the USA release',
}
STEPS = ['verify', 'extract', 'tools', 'recompile', 'packs', 'iop', 'configure', 'compile', 'stage']
FETCH_NAMES = ['raylib', 'imgui', 'rlimgui', 'parallel_gs', 'granite',
               'elfio', 'toml11', 'fmt', 'libdwarf', 'rabbitizer', 'nlohmann_json']
LAUNCH_ENV = [('TMB_NATIVE', '1'), ('PS2X_SPU2_WALLCLOCK', '1'), ('PS2X_IOP_WALLCLOCK', '1'),
              ('PS2X_GS_RENDERER', 'native'), ('PS2X_IOP_NATIVE', '1')]
EXE = '.exe' if os.name == 'nt' else ''
KIT_MANIFEST = 'kit_manifest.json'


class BuildError(Exception):
    pass


def emit(*parts):
    print(' '.join(str(p) for p in parts), flush=True)


def sha1_file(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def fingerprint(*items):
    return hashlib.sha1(json.dumps(items, sort_keys=True, default=str).encode()).hexdigest()[:16]


def file_stamp(path):
    try:
        st = os.stat(path)
        return [os.path.normcase(os.path.abspath(path)), st.st_size, int(st.st_mtime)]
    except OSError:
        return [path, None]


def tree_stamp(folder, exts=None):
    """Sizes and mtimes of a source tree (cheap; changes when the source is updated)."""
    out = []
    for dirpath, dirnames, filenames in os.walk(folder):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith('.') and d not in ('build', '__pycache__'))
        for name in sorted(filenames):
            if exts and not name.lower().endswith(exts):
                continue
            p = os.path.join(dirpath, name)
            st = os.stat(p)
            out.append((os.path.relpath(p, folder), st.st_size, int(st.st_mtime)))
    return fingerprint(out)


def sync_tree(src, dst):
    """Copies src over dst comparing contents (like rsync -c --delete): unchanged files keep their mtime, so ninja
    only rebuilds what changed. Returns the number of files written."""
    changed = 0
    wanted = set()
    for dirpath, _dirs, files in os.walk(src):
        rel = os.path.relpath(dirpath, src)
        out_dir = os.path.normpath(os.path.join(dst, rel))
        os.makedirs(out_dir, exist_ok=True)
        wanted.add(os.path.normcase(out_dir))
        for name in files:
            s, d = os.path.join(dirpath, name), os.path.join(out_dir, name)
            wanted.add(os.path.normcase(d))
            if os.path.exists(d) and filecmp.cmp(s, d, shallow=False):
                continue
            shutil.copyfile(s, d)
            changed += 1
    for dirpath, dirs, files in os.walk(dst, topdown=False):
        for name in files:
            p = os.path.join(dirpath, name)
            if os.path.normcase(p) not in wanted:
                os.remove(p)
        if os.path.normcase(os.path.normpath(dirpath)) not in wanted and not os.listdir(dirpath):
            os.rmdir(dirpath)
    return changed


# ---- PE imports (the DLLs the runner needs, found in the toolchain's bin) ----

def pe_imports(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] != b'MZ':
        return []
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        return []
    nsec, opt_size = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    dd = opt + (112 if magic == 0x20b else 96)
    sections = []
    for i in range(nsec):
        s = pe + 24 + opt_size + 40 * i
        vsize, va, rsize, roff = struct.unpack_from('<IIII', data, s + 8)
        sections.append((va, max(vsize, rsize), roff))

    def off(rva):
        for va, size, roff in sections:
            if va <= rva < va + size:
                return rva - va + roff
        return None

    def cstr(o):
        return data[o:data.index(b'\0', o)].decode('latin-1')

    names = []
    for index, stride, name_at in ((1, 20, 12), (13, 32, 4)):  # import table, delay-load table
        rva = struct.unpack_from('<I', data, dd + 8 * index)[0]
        o = off(rva) if rva else None
        while o is not None and o + stride <= len(data):
            name_rva = struct.unpack_from('<I', data, o + name_at)[0]
            if not name_rva:
                break
            no = off(name_rva)
            if no is not None:
                names.append(cstr(no))
            o += stride
    return names


def runtime_dlls(exe, search_dirs):
    """The DLLs exe loads (recursively) that one of search_dirs has: what must sit next to it."""
    found, queue = {}, [exe]
    while queue:
        for name in pe_imports(queue.pop()):
            key = name.lower()
            if key in found:
                continue
            for d in search_dirs:
                p = os.path.join(d, name)
                if os.path.isfile(p):
                    found[key] = p
                    queue.append(p)
                    break
            else:
                found[key] = None  # a Windows DLL
    return sorted(p for p in found.values() if p)


# ---- the build ----

class Build:
    def __init__(self, args):
        self.args = args
        self.out = os.path.abspath(args.out)
        self.game = os.path.abspath(args.game) if args.game else os.path.join(self.out, 'TMB')
        self.disc_dir = os.path.join(self.game, 'disc')
        self.gen = os.path.join(self.out, 'generated')
        self.cmake_build = os.path.join(self.out, 'build')
        self.logs = os.path.join(self.out, 'logs')
        self.state_path = os.path.join(self.out, 'tmb_build_state.json')
        os.makedirs(self.logs, exist_ok=True)
        try:
            with open(self.state_path) as f:
                self.state = json.load(f)
        except (OSError, ValueError):
            self.state = {}
        self.toolchain_bin = None
        self.env = dict(os.environ)
        self.step = None
        self.kit = find_kit(args)
        self.ffmpeg_root = None
        if self.kit:
            # The kit's parts unless the command line names others.
            args.toolchain = args.toolchain or os.path.join(self.kit, 'ucrt64')
            args.tools = args.tools or os.path.join(self.kit, 'tools')
            args.fetch_cache = args.fetch_cache or os.path.join(self.kit, 'deps')
            self.ffmpeg_root = os.path.join(self.kit, 'ffmpeg')
        if args.debug_info is None:  # a player's build (the kit): no debug information, a 613 MB runner otherwise
            args.debug_info = 'no' if self.kit else 'yes'

    def save_state(self):
        tmp = self.state_path + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(self.state, f, indent=1)
        os.replace(tmp, self.state_path)

    # -- helpers

    def log_path(self):
        return os.path.join(self.logs, self.step + '.log')

    def run(self, cmd, cwd=None, env=None, percent=None, append=False):
        """Runs cmd with its output in the step's log; percent(line) -> 0-100 or None for PERCENT lines."""
        with open(self.log_path(), 'a' if append else 'w', encoding='utf-8', errors='replace') as log:
            log.write('$ %s\n' % subprocess.list2cmdline(cmd))
            log.flush()
            try:
                p = subprocess.Popen(cmd, cwd=cwd, env=env or self.env, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, bufsize=1, universal_newlines=True,
                                     encoding='utf-8', errors='replace')
            except OSError as e:
                raise BuildError('cannot start %s: %s' % (cmd[0], e.strerror))
            last = -1
            for line in p.stdout:
                log.write(line)
                if percent:
                    v = percent(line)
                    if v is not None and v != last:
                        last = v
                        emit('PERCENT', self.step, v)
            code = p.wait()
        if code != 0:
            with open(self.log_path(), encoding='utf-8', errors='replace') as f:
                tail = f.readlines()[-25:]
            sys.stderr.write(''.join(tail))
            raise BuildError('%s failed (exit %d); the full output is in %s'
                             % (os.path.basename(cmd[0]), code, self.log_path()))

    def py(self, script, *argv, **kw):
        self.run([sys.executable, '-I', script] + list(argv), **kw)

    # -- inputs

    def find_toolchain(self):
        if self.toolchain_bin:
            return self.toolchain_bin
        cands = []
        if self.args.toolchain:
            t = os.path.abspath(self.args.toolchain)
            cands.append(t if os.path.isfile(os.path.join(t, 'g++' + EXE)) else os.path.join(t, 'bin'))
        else:
            gxx = shutil.which('g++')
            if gxx:
                cands.append(os.path.dirname(gxx))
            if os.name == 'nt':
                cands.append('C:/msys64/ucrt64/bin')
        for c in cands:
            missing = [t for t in ('g++', 'gcc', 'cmake', 'ninja') if not os.path.isfile(os.path.join(c, t + EXE))]
            if not missing:
                self.toolchain_bin = os.path.normpath(c)
                break
        else:
            raise BuildError('no compiler toolchain found (looked in %s): pass --toolchain <dir> with g++, gcc, cmake '
                             'and ninja in its bin folder (an MSYS2 UCRT64 prefix: C:/msys64/ucrt64)'
                             % (', '.join(cands) or 'PATH'))
        self.env['PATH'] = self.toolchain_bin + os.pathsep + self.env.get('PATH', '')
        return self.toolchain_bin

    def tool(self, name):
        return os.path.join(self.find_toolchain(), name + EXE)

    def disc_input(self):
        src = os.path.abspath(self.args.disc)
        if not os.path.exists(src):
            raise BuildError('%s does not exist' % src)
        return src

    # -- steps

    def key(self, step):
        """What a step's result depends on (its inputs and the earlier steps' keys)."""
        a = self.args
        if step == 'verify':
            return fingerprint(file_stamp(self.disc_input()))
        if step == 'extract':
            return fingerprint(self.state.get('verify', {}).get('key'))
        if step == 'tools':
            return fingerprint(os.path.abspath(a.tools) if a.tools else tree_stamp(
                os.path.join(ROOT, 'PS2Recomp'), ('.cpp', '.h', '.hpp', '.txt', '.cmake')))
        if step == 'recompile':
            return fingerprint(self.state.get('extract', {}).get('key'), self.state.get('tools', {}).get('key'),
                               file_stamp(os.path.join(ROOT, 'recomp', 'tmb_scus97101.toml')))
        if step == 'packs':
            return fingerprint(self.state.get('extract', {}).get('key'),
                               [file_stamp(os.path.join(ROOT, 'tools', t)) for t in ('tmb_texture_pack.py', 'tmb_mesh_pack.py')],
                               tree_stamp(os.path.join(ROOT, 'PS2Recomp', 'tools', 'native_pack'), ('.py',)))
        if step == 'iop':
            return fingerprint(self.state.get('extract', {}).get('key'),
                               file_stamp(os.path.join(ROOT, 'recomp', 'tmb_iop_modules.json')),
                               [file_stamp(os.path.join(ROOT, 'PS2Recomp', 'tools', t)) for t in ('irx_load.py', 'iop_recomp.py')])
        if step == 'configure':
            return fingerprint(self.find_toolchain(), a.build_type, a.debug_info, a.fetch_cache, self.ffmpeg_root,
                               [self.state.get(s, {}).get('key') for s in ('recompile', 'packs', 'iop')])
        if step == 'compile':
            return fingerprint(self.state.get('configure', {}).get('key'),
                               tree_stamp(os.path.join(ROOT, 'runtime')),
                               tree_stamp(os.path.join(ROOT, 'PS2Recomp'), ('.cpp', '.h', '.hpp', '.inl', '.txt', '.cmake')))
        if step == 'stage':
            return fingerprint(self.state.get('compile', {}).get('key'), self.game)
        raise AssertionError(step)

    def step_verify(self):
        src = self.disc_input()
        if os.path.isdir(src):
            elf = os.path.join(src, ELF_NAME)
            if not os.path.isfile(elf):
                others = [n for n in os.listdir(src) if re.match(r'S[CL][A-Z]S_\d{3}\.\d{2}$', n, re.I)]
                raise BuildError(self.wrong_disc(others[0] if others else None, src))
            digest = sha1_file(elf)
        else:
            try:
                disc = ps2_disc.Disc(src)
            except ps2_disc.DiscError as e:
                raise BuildError('%s: %s' % (os.path.basename(src), e))
            try:
                boot = disc.boot_elf()
                if not boot or boot.upper() != ELF_NAME:
                    raise BuildError(self.wrong_disc(boot, src))
                digest = hashlib.sha1(disc.read(ELF_NAME)).hexdigest()
            finally:
                disc.close()
        if digest != ELF_SHA1:
            raise BuildError('%s on this disc is not the one TMB_Recomp supports (SHA-1 %s, expected %s): a different '
                             'printing or a modified or damaged image. Make a fresh 1:1 copy of the USA disc (SCUS-97101).'
                             % (ELF_NAME, digest, ELF_SHA1))
        emit('INFO', 'verify', 'USA disc (SCUS-97101), %s SHA-1 ok' % ELF_NAME)

    @staticmethod
    def wrong_disc(boot, src):
        if boot and boot.upper() in OTHER_RELEASES and boot.upper() != ELF_NAME:
            what = OTHER_RELEASES[boot.upper()]
            return ('%s is %s of Twisted Metal: Black; TMB_Recomp only supports the USA disc (SCUS-97101).'
                    % (os.path.basename(src), what))
        if boot:
            return ('%s boots %s: not Twisted Metal: Black (USA, SCUS-97101), the only disc TMB_Recomp supports.'
                    % (os.path.basename(src), boot))
        return ('%s: no %s found: not the Twisted Metal: Black USA disc (SCUS-97101), or not a PS2 disc image.'
                % (os.path.basename(src), ELF_NAME))

    def step_extract(self):
        src = self.disc_input()
        image = src
        if os.path.isdir(src):
            image = self.find_folder_image(src)
            if not image:
                raise BuildError('the game reads the disc image itself as well as its files: no .iso of the USA disc '
                                 'in %s or the folders above it. Pass the .iso (--disc <image>) instead.' % src)
        os.makedirs(self.game, exist_ok=True)
        self.extract_files(src)
        if sha1_file(os.path.join(self.disc_dir, ELF_NAME)) != ELF_SHA1:
            raise BuildError("the extracted %s differs from the disc's (disk error?)" % ELF_NAME)
        self.place_image(image)

    def find_folder_image(self, folder):
        """An extracted folder: the .iso it came from, in it or up to three folders above (as the runtime looks)."""
        d = os.path.abspath(folder)
        for _ in range(4):
            for name in sorted(os.listdir(d)):
                if not name.lower().endswith('.iso'):
                    continue
                try:
                    disc = ps2_disc.Disc(os.path.join(d, name))
                except ps2_disc.DiscError:
                    continue
                try:
                    if (disc.boot_elf() or '').upper() == ELF_NAME and \
                            hashlib.sha1(disc.read(ELF_NAME)).hexdigest() == ELF_SHA1:
                        return os.path.join(d, name)
                finally:
                    disc.close()
            parent = os.path.dirname(d)
            if parent == d:
                break
            d = parent
        return None

    def place_image(self, src):
        """The game also reads raw sectors (sceCdRead by LBN: the volume descriptor, the directories, then its data
        files), which the extracted files cannot serve: the runtime reads them from a 2048-byte-sector image of the
        disc, <game>/disc.iso (TMB_ISO; the runtime also finds it walking up from the ELF's folder). A hard link to the
        player's .iso when it is on the same drive, else a copy; a .bin / .cue is converted."""
        dest = os.path.join(self.game, 'disc.iso')
        disc = ps2_disc.Disc(src)
        try:
            img = disc.image
            total = img.size // img.sector_size
            if os.path.exists(dest) and os.path.getsize(dest) == total * 2048:
                return
            if os.path.exists(dest):
                os.remove(dest)
            if img.sector_size == 2048:
                try:
                    os.link(src, dest)
                    return
                except OSError:
                    pass
            free = shutil.disk_usage(self.game).free
            if free < total * 2048 + (6 << 30):
                raise BuildError('not enough free space on %s for the disc image: %.1f GB free, about %.1f GB needed'
                                 % (self.game, free / 2**30, (total * 2048 + (6 << 30)) / 2**30))
            tmp = dest + '.part'
            with open(tmp, 'wb') as f:
                lba = 0
                while lba < total:
                    n = min(4096, total - lba)
                    f.write(img.read_sectors(lba, n))
                    lba += n
                    emit('PERCENT', 'extract', lba * 100 // total)
            os.replace(tmp, dest)
        finally:
            disc.close()

    def extract_files(self, src):
        os.makedirs(self.disc_dir, exist_ok=True)
        if os.path.isdir(src):
            if os.path.normcase(os.path.abspath(src)) == os.path.normcase(self.disc_dir):
                return
            files = []
            for dirpath, _dirs, names in os.walk(src):
                for n in names:
                    if n.lower() in ('input.ini', 'video.ini'):
                        continue
                    files.append(os.path.relpath(os.path.join(dirpath, n), src))
            total = sum(os.path.getsize(os.path.join(src, f)) for f in files) or 1
            done, last = 0, -1
            for rel in files:
                s, d = os.path.join(src, rel), os.path.join(self.disc_dir, rel)
                os.makedirs(os.path.dirname(d), exist_ok=True)
                size = os.path.getsize(s)
                if not (os.path.exists(d) and os.path.getsize(d) == size):
                    if os.path.exists(d):
                        os.remove(d)
                    try:
                        os.link(s, d)  # same volume: no copy
                    except OSError:
                        shutil.copy2(s, d)
                done += size
                pct = done * 100 // total
                if pct != last:
                    last = pct
                    emit('PERCENT', 'extract', pct)
            return
        disc = ps2_disc.Disc(src)
        try:
            files = [e for e in disc.entries() if not e.is_dir]
            total = sum(e.size for e in files) or 1
            free = shutil.disk_usage(self.out).free
            if free < total + (6 << 30):
                raise BuildError('not enough free space on %s: %.1f GB free, about %.1f GB needed (the disc\'s files '
                                 'and the build)' % (self.out, free / 2**30, (total + (6 << 30)) / 2**30))
            done, last = 0, -1
            for e in disc.entries():
                dest = os.path.join(self.disc_dir, e.path)
                if e.is_dir:
                    os.makedirs(dest, exist_ok=True)
                    continue
                if not (os.path.exists(dest) and os.path.getsize(dest) == e.size):  # resumes: finished files stay
                    tmp = dest + '.part'
                    with open(tmp, 'wb') as f:
                        for chunk in disc.image.read_extent(e.lba, e.size):
                            f.write(chunk)
                    os.replace(tmp, dest)
                    if e.mtime is not None:
                        os.utime(dest, (e.mtime, e.mtime))
                done += e.size
                pct = done * 100 // total
                if pct != last:
                    last = pct
                    emit('PERCENT', 'extract', pct)
        finally:
            disc.close()

    def tools_dir(self):
        if self.args.tools:
            return os.path.abspath(self.args.tools)
        return os.path.join(self.out, 'tools-build', 'ps2xRecomp')

    def step_tools(self):
        if self.args.tools:
            for t in ('ps2_recomp', 'ps2_vu_aot'):
                if not os.path.isfile(os.path.join(self.tools_dir(), t + EXE)):
                    raise BuildError('%s%s is not in %s (--tools)' % (t, EXE, self.tools_dir()))
            return
        build = os.path.join(self.out, 'tools-build')
        self.run([self.tool('cmake'), '-S', os.path.join(ROOT, 'PS2Recomp'), '-B', build, '-G', 'Ninja',
                  '-DCMAKE_BUILD_TYPE=Release', '-DPS2X_BUILD_RUNTIME=OFF', '-DPS2X_BUILD_STUDIO=OFF',
                  '-DPS2X_BUILD_TEST=OFF', '-DPS2X_BUILD_ANALYZER=OFF',
                  '-DCMAKE_C_COMPILER=' + self.tool('gcc'), '-DCMAKE_CXX_COMPILER=' + self.tool('g++'),
                  '-DCMAKE_MAKE_PROGRAM=' + self.tool('ninja')] + self.fetch_args())
        self.run([self.tool('cmake'), '--build', build, '--target', 'ps2_recomp', 'ps2_vu_aot', '-j', str(self.args.jobs)],
                 percent=ninja_percent, append=True)

    def step_recompile(self):
        staging = os.path.join(self.out, 'recomp-staging')
        shutil.rmtree(staging, ignore_errors=True)
        os.makedirs(staging)
        with open(os.path.join(ROOT, 'recomp', 'tmb_scus97101.toml'), encoding='utf-8') as f:
            toml = f.read()
        fwd = lambda p: p.replace('\\', '/')  # noqa: E731
        toml = re.sub(r'(?m)^input = .*$', 'input = "%s"' % fwd(os.path.join(self.disc_dir, ELF_NAME)), toml)
        toml = re.sub(r'(?m)^output = .*$', 'output = "%s/"' % fwd(staging), toml)
        cfg = os.path.join(self.out, 'tmb_scus97101.toml')
        with open(cfg, 'w', encoding='utf-8', newline='\n') as f:
            f.write(toml)
        self.run([os.path.join(self.tools_dir(), 'ps2_recomp' + EXE), cfg], cwd=ROOT)
        env = dict(self.env, PS2X_VU_AOT_FAST='1')
        self.run([os.path.join(self.tools_dir(), 'ps2_vu_aot' + EXE), os.path.join(self.disc_dir, ELF_NAME),
                  os.path.join(staging, 'vu1')], env=env, append=True)
        # Keep what the other steps write in generated/ (the packs, iop/), refresh the rest by content.
        for keep in ('tmb_textures.pack', 'tmb_meshes.pack', 'iop'):
            p = os.path.join(self.gen, keep)
            if os.path.exists(p):
                d = os.path.join(staging, keep)
                if os.path.isdir(p):
                    shutil.copytree(p, d)
                else:
                    shutil.copyfile(p, d)
        os.makedirs(self.gen, exist_ok=True)
        changed = sync_tree(staging, self.gen)
        shutil.rmtree(staging, ignore_errors=True)
        emit('INFO', 'recompile', '%d files written' % changed)

    def step_packs(self):
        elf = os.path.join(self.disc_dir, ELF_NAME)
        os.makedirs(self.gen, exist_ok=True)
        for i, (script, argv, name) in enumerate((
                ('tmb_texture_pack.py', [self.disc_dir, elf], 'tmb_textures.pack'),
                ('tmb_mesh_pack.py', [self.disc_dir], 'tmb_meshes.pack'))):
            tmp = os.path.join(self.gen, name + '.tmp')
            self.py(os.path.join(ROOT, 'tools', script), *(argv + [tmp]), append=i > 0)
            os.replace(tmp, os.path.join(self.gen, name))
            emit('PERCENT', 'packs', 50 * (i + 1))

    def step_iop(self):
        with open(os.path.join(ROOT, 'recomp', 'tmb_iop_modules.json')) as f:
            table = json.load(f)['modules']
        dumps = os.path.join(self.out, 'iopmods')
        shutil.rmtree(dumps, ignore_errors=True)
        out = os.path.join(self.gen, 'iop')
        staging = os.path.join(self.out, 'iop-staging')
        shutil.rmtree(staging, ignore_errors=True)
        os.makedirs(staging)
        with open(self.log_path(), 'w') as log:
            for m in table:
                irx = os.path.join(self.disc_dir, *m['file'].split('/'))
                with open(irx, 'rb') as f:
                    data = f.read()
                h = '%016x' % irx_load.fnv1a64(data)
                if h != m['hash']:
                    raise BuildError('%s differs from the USA disc\'s (hash %s, expected %s)' % (m['file'], h, m['hash']))
                j = irx_load.write_dump(irx, int(m['base'], 16), m['name'], dumps)
                with open(j) as f:
                    loaded = json.load(f)
                if loaded['size'] != int(m['size'], 16):
                    raise BuildError('%s loaded as %#x bytes, expected %s' % (m['file'], loaded['size'], m['size']))
                log.write('%s -> %s\n' % (irx, j))
        for i, m in enumerate(table):
            j = os.path.join(dumps, '%s_%06x.json' % (m['name'], int(m['base'], 16)))
            self.py(os.path.join(ROOT, 'PS2Recomp', 'tools', 'iop_recomp.py'), j, '--out', staging, append=True)
            emit('PERCENT', 'iop', (i + 1) * 100 // len(table))
        os.makedirs(out, exist_ok=True)
        sync_tree(staging, out)
        shutil.rmtree(staging, ignore_errors=True)

    def fetch_args(self):
        if not self.args.fetch_cache:
            if not shutil.which('git', path=self.env.get('PATH')):
                raise BuildError('git is needed to download the libraries the build uses (raylib, imgui, ...), or '
                                 'pass --fetch-cache <dir> with their sources')
            return []
        cache = os.path.abspath(self.args.fetch_cache)
        out = ['-DFETCHCONTENT_FULLY_DISCONNECTED=ON'] if self.kit else []  # never the network
        for n in FETCH_NAMES:
            d = os.path.join(cache, n + '-src')
            if os.path.isdir(d):
                out.append('-DFETCHCONTENT_SOURCE_DIR_%s=%s' % (n.upper(), d.replace('\\', '/')))
        return out

    def step_configure(self):
        cmd = [self.tool('cmake'), '-S', ROOT, '-B', self.cmake_build, '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=' + self.args.build_type,
               '-DTMB_GENERATED_DIR=' + self.gen.replace('\\', '/'),
               '-DCMAKE_C_COMPILER=' + self.tool('gcc'), '-DCMAKE_CXX_COMPILER=' + self.tool('g++'),
               '-DCMAKE_MAKE_PROGRAM=' + self.tool('ninja')]
        if self.args.debug_info == 'no':
            # The configuration's own flags without -g: the same code (GCC's -g never changes it), the same -O2 and
            # GUI subsystem as RelWithDebInfo; Release would be -O3, which the generated code is too large for.
            flags = {'Release': '-O3 -DNDEBUG', 'RelWithDebInfo': '-O2 -DNDEBUG', 'Debug': '-O0'}[self.args.build_type]
            cmd += ['-DCMAKE_%s_FLAGS_%s=%s' % (lang, self.args.build_type.upper(), flags) for lang in ('C', 'CXX')]
        if self.ffmpeg_root:
            cmd += ['-DPS2X_FFMPEG_ROOT=' + self.ffmpeg_root.replace('\\', '/'), '-DPS2X_ENABLE_PARALLEL_GS=OFF']
        else:
            pkg = os.path.join(self.find_toolchain(), 'pkg-config' + EXE)
            if os.path.isfile(pkg):
                cmd.append('-DPKG_CONFIG_EXECUTABLE=' + pkg)
        self.run(cmd + self.fetch_args())

    def runner_path(self):
        return os.path.join(self.cmake_build, 'PS2Recomp', 'ps2xRuntime', 'ps2EntryRunner' + EXE)

    def step_compile(self):
        self.run([self.tool('cmake'), '--build', self.cmake_build, '--target', 'ps2EntryRunner',
                  '-j', str(self.args.jobs)], percent=ninja_percent)

    def step_stage(self):
        exe = self.runner_path()
        if not os.path.isfile(exe):
            raise BuildError('no runner at %s' % exe)
        os.makedirs(self.game, exist_ok=True)
        staged = [exe]
        dest = os.path.join(self.game, os.path.basename(exe))
        strip = os.path.join(self.find_toolchain(), 'strip' + EXE)
        if self.args.debug_info == 'no' and os.path.isfile(strip):
            # What debug information is left (the libraries' own -g, if any) out of the player's copy.
            if os.path.exists(dest):
                os.remove(dest)
            self.run([strip, '--strip-debug', '-o', dest, exe])
            shutil.copystat(exe, dest)
        else:
            shutil.copy2(exe, dest)
        if os.name == 'nt':
            for dll in runtime_dlls(exe, [os.path.dirname(exe), self.find_toolchain()]):
                shutil.copy2(dll, os.path.join(self.game, os.path.basename(dll)))
                staged.append(dll)
        for name in ('tmb_textures.pack', 'tmb_meshes.pack'):
            shutil.copy2(os.path.join(self.gen, name), os.path.join(self.game, name))
        if os.name == 'nt':
            lines = ['@echo off', 'rem Made by tools/tmb_build.py: Twisted Metal: Black, native PC port.',
                     'cd /d "%~dp0"']
            lines += ['set %s=%s' % kv for kv in LAUNCH_ENV]
            lines += ['set TMB_TEXTURE_PACK=%~dp0tmb_textures.pack', 'set TMB_MESH_PACK=%~dp0tmb_meshes.pack',
                      'set TMB_ISO=%~dp0disc.iso',
                      'start "" "%%~dp0%s" "%%~dp0disc\\%s" %%*' % (os.path.basename(exe), ELF_NAME), '']
            with open(os.path.join(self.game, 'Play TMB.cmd'), 'w', newline='\r\n') as f:
                f.write('\n'.join(lines))
        else:
            sh = os.path.join(self.game, 'play_tmb.sh')
            with open(sh, 'w', newline='\n') as f:
                f.write('#!/bin/sh\ncd "$(dirname "$0")"\n' + ''.join('export %s=%s\n' % kv for kv in LAUNCH_ENV) +
                        'export TMB_TEXTURE_PACK="$PWD/tmb_textures.pack" TMB_MESH_PACK="$PWD/tmb_meshes.pack" TMB_ISO="$PWD/disc.iso"\n'
                        'exec ./%s disc/%s "$@"\n' % (os.path.basename(exe), ELF_NAME))
            os.chmod(sh, 0o755)
        with open(os.path.join(self.game, 'tmb_build.json'), 'w') as f:
            json.dump({'built': time.strftime('%Y-%m-%d %H:%M:%S'), 'elf_sha1': ELF_SHA1,
                       'build_type': self.args.build_type, 'debug_info': self.args.debug_info, 'toolchain': self.find_toolchain(), 'kit': self.kit,
                       'files': [os.path.basename(p) for p in staged],
                       'env': dict(LAUNCH_ENV)}, f, indent=1)
        emit('INFO', 'stage', '%d DLLs staged' % (len(staged) - 1))

    # -- driver

    def main(self):
        redo_from = STEPS.index(self.args.redo) if self.args.redo else len(STEPS)
        n = len(STEPS)
        dirty = False
        total_start = time.time()
        for i, step in enumerate(STEPS, 1):
            self.step = step
            try:
                key = self.key(step)
                st = self.state.get(step, {})
                if not dirty and i - 1 < redo_from and st.get('done') and st.get('key') == key:
                    emit('SKIP', step, '%d/%d' % (i, n))
                    if step == self.args.stop_after:
                        emit('INFO', 'stopped', 'after ' + step)
                        return 0
                    continue
                dirty = True  # every later step runs again
                emit('PROGRESS', step, '%d/%d' % (i, n), STEP_TITLES[step])
                t = time.time()
                self.state[step] = {'done': False}
                self.save_state()
                getattr(self, 'step_' + step)()
                secs = round(time.time() - t, 1)
                self.state[step] = {'done': True, 'key': self.key(step), 'seconds': secs}
                self.save_state()
                emit('DONE', step, '%d/%d' % (i, n), secs)
                if step == self.args.stop_after:
                    emit('INFO', 'stopped', 'after ' + step)
                    return 0
            except BuildError as e:
                emit('ERROR', step, e)
                return 1
            except KeyboardInterrupt:
                emit('ERROR', step, 'interrupted (run again to resume)')
                return 1
            except Exception as e:  # an unexpected failure: still one clear line, details in the log
                with open(os.path.join(self.logs, step + '.log'), 'a') as log:
                    import traceback
                    traceback.print_exc(file=log)
                emit('ERROR', step, '%s: %s (details in %s)' % (type(e).__name__, e, self.log_path()))
                return 1
        launcher = os.path.join(self.game, 'Play TMB.cmd' if os.name == 'nt' else 'play_tmb.sh')
        emit('INFO', 'total', '%.1f s' % (time.time() - total_start))
        emit('RESULT', self.game, launcher)
        return 0


def find_kit(args):
    """The build kit: --kit, else <TMB_Recomp>/kit or <TMB_Recomp's parent>/kit when it has kit_manifest.json."""
    if args.no_kit:
        return None
    if args.kit:
        kit = os.path.abspath(args.kit)
        if not os.path.isfile(os.path.join(kit, KIT_MANIFEST)):
            raise SystemExit('ERROR kit %s has no %s' % (kit, KIT_MANIFEST))
        return kit
    for kit in (os.path.join(ROOT, 'kit'), os.path.join(os.path.dirname(ROOT), 'kit')):
        if os.path.isfile(os.path.join(kit, KIT_MANIFEST)):
            return kit
    return None


STEP_TITLES = {
    'verify': 'Checking the disc',
    'extract': 'Copying the disc\'s files',
    'tools': 'Preparing the recompiler',
    'recompile': 'Recompiling the game\'s code',
    'packs': 'Converting textures and models',
    'iop': 'Recompiling the sound and controller modules',
    'configure': 'Configuring the build',
    'compile': 'Compiling (this takes a while)',
    'stage': 'Assembling the game folder',
}

NINJA_RE = re.compile(r'^\[(\d+)/(\d+)\]')


def ninja_percent(line):
    m = NINJA_RE.match(line)
    if m and int(m.group(2)):
        return int(m.group(1)) * 100 // int(m.group(2))
    return None


def check_disc(path):
    """--check: the verify step alone, for a launcher checking the image the player picked."""
    b = Build.__new__(Build)
    b.args = argparse.Namespace(disc=path)
    try:
        b.step_verify()
    except BuildError as e:
        emit('ERROR', 'verify', e)
        return 1
    except Exception as e:  # noqa: BLE001 (an unreadable file: still one line)
        emit('ERROR', 'verify', '%s: %s' % (os.path.basename(path), e))
        return 1
    emit('OK', 'USA disc (SCUS-97101)')
    return 0


def main():
    ap = argparse.ArgumentParser(description='Builds Twisted Metal: Black (USA) as a native PC game from your own disc.')
    ap.add_argument('--disc', required=True, help='the disc image (.iso, .bin, .cue) or an extracted disc folder')
    ap.add_argument('--out', default='tmb_build', help='the work folder; the game ends up in <out>/TMB')
    ap.add_argument('--game', help='the game folder to stage (default: <out>/TMB)')
    ap.add_argument('--check', action='store_true', help='only check the disc (no work folder): OK <message> or '
                    'ERROR verify <message>')
    ap.add_argument('--kit', help='the build kit folder (default: <TMB_Recomp>/kit or <its parent>/kit if present)')
    ap.add_argument('--no-kit', action='store_true', help='do not use a build kit even if one is found')
    ap.add_argument('--toolchain', help='compiler folder (with bin/g++, cmake, ninja), default: the kit, PATH, '
                    'C:/msys64/ucrt64')
    ap.add_argument('--tools', help='folder with prebuilt ps2_recomp and ps2_vu_aot (default: built here)')
    ap.add_argument('--fetch-cache', help='folder with the FetchContent sources (<name>-src) instead of git')
    ap.add_argument('--build-type', default='RelWithDebInfo', choices=['Release', 'RelWithDebInfo', 'Debug'])
    ap.add_argument('--debug-info', choices=['yes', 'no'], help='debug information in the runner (default: no with '
                    'the build kit, a player build; yes without it)')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--redo', choices=STEPS, help='run this step and the ones after it again')
    ap.add_argument('--stop-after', choices=STEPS, help='stop once this step is done')
    args = ap.parse_args()
    if args.check:
        sys.exit(check_disc(args.disc))
    os.makedirs(os.path.abspath(args.out), exist_ok=True)
    sys.exit(Build(args).main())


if __name__ == '__main__':
    main()
