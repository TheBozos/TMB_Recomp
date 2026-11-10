#!/usr/bin/env python3
"""Dumps the textures of every arena and car from your own disc into one folder, for a full texture pack
(docs/TEXTURE_UPSCALING.md; then tools/tmb_texture_upscale.py). Headless runs of the native build, each with its own
user folder under <work>, TMB_UNLOCK_ALL=1 and TMB_TEXTURE_DUMP=<dump>:

- the 21 story levels (TMB_STORY_LEVEL=0..20 on BozoEngine's reference story script): every arena, the player's tow
  truck, the bosses and the AI cars the story brings;
- the actual four-player route (BozoEngine's reference p1..p4 scripts, player 1's intro press dropped, as
  docs/HANDOFF.md describes): the AI versions four players drive;
- two Challenge matches in the Junkyard with other opponents (AXA, HSA, RLA, TKA, TXA).

Each run's dump covers every object of the NGP files loaded (runtime/tmb_meshes.cpp's dumpLoadedObjects), drawn or not.
Not covered: the player-only versions of the other cars (xxP: about 10% of each car's textures differ from its AI
version), which only a 1P match with that car loads.

  tools/tmb_texture_dump_all.py <tmb-headless> <disc .iso> <dump folder> [--work <folder>] [--jobs 3]
"""
import argparse
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
# The pad scripts (button timings, copied from BozoEngine's reference/tmb/scripts, which a release leaves out).
SCRIPTS = os.path.join(ROOT, 'tools', 'texture_dump')
COMPLETE = 'complete' # in the dump folder once every run finished


def challenge(opponent_rights, end=330.0):
    """tools/arena_script.py's Challenge route in the Junkyard, the opponent chosen by RIGHT presses."""
    e = ['25:CROSS/300', '62:CROSS/250', '72:DOWN/200', '80:CROSS/250', '92:CROSS/250', '112:RIGHT/200', '150:CROSS/250',
         '162:CROSS/250']
    t = 168
    for _ in range(opponent_rights):
        e.append(f'{t}:RIGHT/200')
        t += 3
    e += [f'{t + 2}:CROSS/250', f'{t + 10}:SQUARE/250']
    t += 25
    while t < end:
        e.append(f'{t:.1f}:CROSS/5200')
        t += 5
    out = []
    for entry in e:
        s, press = entry.split(':')
        b, ms = press.split('/')
        out.append(f'{round(float(s) * 60)}:{b}/{max(1, round(float(ms) * .06))}')
    return ','.join(out) + '\n'


def dump_all(headless, iso, dump, work, jobs=3, report=print, elf=None):
    """Every run, `jobs` at a time; report(done, total, name, code) after each. True when they all finished and the
    dump folder is marked complete (a later call then does nothing)."""
    dump, work = os.path.abspath(dump), os.path.abspath(work)
    if os.path.isfile(os.path.join(dump, COMPLETE)):
        return True
    os.makedirs(work, exist_ok=True)
    p1 = os.path.join(work, 'p1_4p.txt')
    with open(os.path.join(SCRIPTS, 'p1_frames.txt'), encoding='utf-8') as f:
        text = f.read()
    with open(p1, 'w', encoding='utf-8') as f:
        f.write(text.replace('1292:CROSS/18,', '', 1))
    runs = [(f'story{n}', {'TMB_STORY_LEVEL': str(n), 'TMB_PLAYER_GOD': '1'}, 7000,
             ['--pad-script', os.path.join(SCRIPTS, 'story_frames.txt')]) for n in range(21)]
    runs.append(('four-player', {}, 8500, ['--pad-script', '1=' + p1] +
                 [a for i in (2, 3, 4) for a in ('--pad-script', f'{i}={os.path.join(SCRIPTS, f"p{i}_frames.txt")}')]))
    for rights in (2, 4):
        script = os.path.join(work, f'challenge{rights}.txt')
        with open(script, 'w', encoding='utf-8') as f:
            f.write(challenge(rights))
        runs.append((f'challenge{rights}', {}, 20500, ['--pad-script', script]))
    done = [0]

    def run(entry):
        name, extra, frames, pads = entry
        env = dict(os.environ, PS2X_USER_DIR=os.path.join(work, name), TMB_TEXTURE_DUMP=dump, TMB_UNLOCK_ALL='1',
                   PS2X_NO_DIALOGS='1', **extra)
        os.makedirs(env['PS2X_USER_DIR'], exist_ok=True)
        flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0) | getattr(subprocess, 'BELOW_NORMAL_PRIORITY_CLASS', 0)
        with open(os.path.join(work, name + '.log'), 'w', encoding='utf-8') as log:
            code = subprocess.call([headless] + (['--elf', elf] if elf else []) + ['--iso', iso, '--frames', str(frames)] + pads, env=env,
                                   stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
        done[0] += 1
        report(done[0], len(runs), name, code)
        return code

    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        codes = list(pool.map(run, runs))
    if any(codes):
        return False
    with open(os.path.join(dump, COMPLETE), 'w', encoding='utf-8') as f:
        f.write('%d runs\n' % len(runs))
    return True


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('headless')
    p.add_argument('iso')
    p.add_argument('dump')
    p.add_argument('--work', help='user folders and logs (default: <dump>/../texture-dump-runs)')
    p.add_argument('--jobs', type=int, default=3)
    args = p.parse_args()
    work = args.work or os.path.join(args.dump, '..', 'texture-dump-runs')
    ok = dump_all(args.headless, args.iso, args.dump, work, args.jobs,
                  lambda n, total, name, code: print(f'{name}: exit {code} ({n}/{total})', flush=True))
    count = sum(1 for n in os.listdir(args.dump) if n.endswith('.png')) if os.path.isdir(args.dump) else 0
    print(f'{count} textures in {args.dump}')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
