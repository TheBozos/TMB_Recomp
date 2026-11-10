#!/usr/bin/env python3
"""The launcher's "Enhance textures" step: an AI-upscaled texture pack made on the player's PC from their own disc, into
their user folder's texture_pack, which the game then draws instead of the original textures (docs/TEXTURE_UPSCALING.md).
Nothing of the game is downloaded or shipped: the textures come from the player's game, the tools from the release.

  tools/tmb_texture_enhance.py --game <game folder> --kit <kit folder> --user <user folder> [--jobs 3]
  tools/tmb_texture_enhance.py --user <user folder> --remove

- dump: tools/tmb_texture_dump_all.py's headless runs with the staged tmb-headless.exe and disc.iso (every arena and
  car; resumed when a finished dump is already there);
- upscale: the staged tmb-texture-pack.exe with the kit's Real-ESRGAN (kit/realesrgan) into texture_pack.new, which then
  replaces texture_pack (the game never sees half a pack; files already made are kept, so a stopped run goes on);
- the dump and the work folders are removed at the end (texture_work: the game's own textures, on the player's PC).

Output for the launcher, one per line: "step <dump|prepare|upscale|finish>", "progress <done> <total>", "error <text>",
"done <replacements>". Standard library only (the kit's Python).
"""
import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmb_texture_dump_all import dump_all  # noqa: E402

EXE = '.exe' if os.name == 'nt' else ''


def say(line):
    print(line, flush=True)


def fail(text):
    say('error ' + text)
    sys.exit(1)


def remove(path):
    if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)
    return not os.path.exists(path)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--game')
    p.add_argument('--kit')
    p.add_argument('--user', required=True)
    p.add_argument('--jobs', type=int, default=max(1, min(3, (os.cpu_count() or 4) // 4)))
    p.add_argument('--remove', action='store_true', help='delete the pack (and anything a stopped run left)')
    args = p.parse_args()
    pack = os.path.join(args.user, 'texture_pack')
    fresh, work = pack + '.new', os.path.join(args.user, 'texture_work')
    if args.remove:
        ok = all(remove(d) for d in (pack, fresh, work))
        if not ok:
            fail('the pack could not be deleted (is the game running?)')
        say('done 0')
        return
    if not args.game or not args.kit:
        fail('--game and --kit are needed')

    headless = os.path.join(args.game, 'tmb-headless' + EXE)
    packer = os.path.join(args.game, 'tmb-texture-pack' + EXE)
    iso = os.path.join(args.game, 'disc.iso')
    esrgan = os.path.join(args.kit, 'realesrgan', 'realesrgan-ncnn-vulkan' + EXE)
    if not (os.path.isfile(headless) and os.path.isfile(packer)):
        fail('the game was built before this step existed: Rebuild it first (Maintenance)')
    if not os.path.isfile(iso):
        fail('the game has no disc image: prepare the game first')
    if not os.path.isfile(esrgan):
        fail('this build kit has no Real-ESRGAN: update TMB_Recomp to a release with it')

    # ---- the dump ----
    say('step dump')
    say('progress 0 1')
    elf = os.path.join(args.game, 'SCUS_971.01')
    ok = dump_all(headless, iso, os.path.join(work, 'dump'), os.path.join(work, 'runs'), args.jobs,
                  lambda done, total, name, code: say('progress %d %d' % (done, total)), elf if os.path.isfile(elf) else None)
    if not ok:
        fail('a texture dump run stopped (its log is in %s)' % os.path.join(work, 'runs'))

    # ---- the pack ----
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    proc = subprocess.Popen([packer, os.path.join(work, 'dump'), fresh, '--esrgan', esrgan, '--work', os.path.join(work, 'pack')],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace',
                            creationflags=flags)
    made, error = None, None
    for line in proc.stdout:
        line = line.rstrip('\r\n')
        if line.startswith(('step ', 'progress ')):
            say(line)
        elif line.startswith('error '):
            error = line[6:]
        elif line.startswith('done '):
            made = int(line[5:] or 0)
    if proc.wait() != 0 or made is None:
        fail(error or 'the texture pack could not be made (exit code %d)' % proc.returncode)

    # ---- in place ----
    old = pack + '.old'
    remove(old)
    if os.path.isdir(pack):
        os.replace(pack, old)
    os.replace(fresh, pack)
    remove(old)
    remove(work)
    count = sum(1 for n in os.listdir(pack) if n.endswith('.png'))
    say('done %d' % count)


if __name__ == '__main__':
    main()
