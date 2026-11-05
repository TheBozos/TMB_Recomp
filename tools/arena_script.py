#!/usr/bin/env python3
"""A frame-keyed BozoEngine pad script for one Challenge match: ONE PLAYER, Challenge, first vehicle, `rights` RIGHT
presses after Suburbs (0 Suburbs, 1 Junkyard, 4 Skyscrapers, 5 Downtown, 6 Highway Loop, 7 Snowy Roads, 8 Drive-In;
2 Freeway is locked and 3 Prison Passage can't be picked in Challenge), two opponents, then driving and firing as
tools/play_script.py from 205 s (the match starts about 200 s in) until `end` seconds.

  python tools/arena_script.py 4 380 --out arena.txt
  bash scripts/run_native.sh --pad-script arena.txt
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('rights', nargs='?', type=int, default=4)
parser.add_argument('end', nargs='?', type=float, default=380)
parser.add_argument('--out', type=Path)
args = parser.parse_args()
rights, end = args.rights, args.end
entries = ['25:CROSS/300', '62:CROSS/250', '72:DOWN/200', '80:CROSS/250', '92:CROSS/250']
t = 112
for _ in range(rights):
    entries.append(f'{t}:RIGHT/200')
    t += 6
entries += ['162:CROSS/250', '174:CROSS/250', '180:RIGHT/200', '186:CROSS/250', '194:SQUARE/250']
t = 205.0
while t < end:
    entries.append(f'{t:.1f}:CROSS/5200')  # accelerate, held
    t += 5.0
t, k = 208.0, 0
while t < end:
    entries.append(f'{t:.1f}:{"LEFT" if k % 2 else "RIGHT"}/{450 + 150 * (k % 3)}')  # steer
    t += 1.3 + 0.4 * (k % 4)
    k += 1
for period, button, ms in ((0.6, 'R1', 150), (1.7, 'L1', 150), (2.9, 'CIRCLE', 150), (7.0, 'TRIANGLE', 120),
                           (13.0, 'SQUARE', 700)):
    t = 211.0
    while t < end:
        entries.append(f'{t:.1f}:{button}/{ms}')
        t += period
frames = []
for entry in entries:
    seconds, press = entry.split(':')
    button, milliseconds = press.split('/')
    frames.append(f'{round(float(seconds) * 60)}:{button}/{max(1, round(float(milliseconds) * .06))}')
text = ','.join(frames) + '\n'
if args.out:
    args.out.write_text(text, encoding='utf-8')
else:
    print(text, end='')
