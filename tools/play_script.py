#!/usr/bin/env python3
"""A frame-keyed BozoEngine pad script: menu presses into the first match (about 150 s in), then accelerating,
steering both ways, firing (R1, L1, CIRCLE), switching weapons and braking until `end` seconds.

  python tools/play_script.py 600 --out controls.txt
  bash scripts/run_native.sh --pad-script controls.txt

Times are authored in seconds, then converted to the game's native 60 Hz frame clock.
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('end', nargs='?', type=float, default=600)
parser.add_argument('--out', type=Path)
args = parser.parse_args()
end = args.end
entries = ['25:CROSS/300'] + [f'{t}:CROSS/250' for t in range(60, 141, 6)]
t = 152.0
while t < end:
    entries.append(f'{t:.1f}:CROSS/5200')  # accelerate, held
    t += 5.0
t, k = 155.0, 0
while t < end:
    entries.append(f'{t:.1f}:{"LEFT" if k % 2 else "RIGHT"}/{450 + 150 * (k % 3)}')  # steer
    t += 1.3 + 0.4 * (k % 4)
    k += 1
for period, button, ms in ((0.6, 'R1', 150), (1.7, 'L1', 150), (2.9, 'CIRCLE', 150), (7.0, 'TRIANGLE', 120),
                           (13.0, 'SQUARE', 700)):
    t = 158.0
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
