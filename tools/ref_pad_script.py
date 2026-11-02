#!/usr/bin/env python3
"""A timed pad script (PS2X_PAD_SCRIPT, seconds) as a frame-keyed one for deterministic mode (PS2X_DETERMINISTIC=1:
"frame:BUTTON/frames"), docs/REFERENCE_HARNESS.md.

  tools/ref_pad_script.py --log calibration.log "<seconds script>"     (or --script-file f)
  tools/ref_pad_script.py --log calibration.log --from-log

Calibration: run the timed script once in deterministic mode with the pad on the wall clock and its changes logged
(PS2X_DETERMINISTIC=1 PS2X_PAD_CLOCK=wall PS2X_PAD_LOG=1, stderr to calibration.log): each "[pad] frame N t=T" line
anchors second T to game frame N. A time between two anchors maps linearly between their frames, outside them at 60
frames a second from the nearest one; holds become frames at 60 a second (at least 1). --from-log: pad 1's script
rebuilt from the logged changes themselves (each press at the frame it began in the calibration run, held as long),
the closest to what the calibration run did. Without --log, seconds map to frames at 60 a second from frame 0
(`--offset` frames added): for scripts already written in game time, such as the driving part of a match counted
from its start.
"""
import argparse
import bisect
import re
import sys

BUTTONS = {0x0080: 'LEFT', 0x0040: 'DOWN', 0x0020: 'RIGHT', 0x0010: 'UP', 0x0008: 'START', 0x0004: 'R3', 0x0002: 'L3',
           0x0001: 'SELECT', 0x8000: 'SQUARE', 0x4000: 'CROSS', 0x2000: 'CIRCLE', 0x1000: 'TRIANGLE', 0x0800: 'R1',
           0x0400: 'L1', 0x0200: 'R2', 0x0100: 'L2'}
LOG_LINE = re.compile(r'\[pad\] frame (\d+) t=([0-9.]+): scripted ([0-9a-f]+)')


def anchors_from(log_path):
    points = []
    with open(log_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = LOG_LINE.search(line)
            if m:
                points.append((float(m.group(2)), int(m.group(1))))
    points.sort()
    return points


def from_log(log_path):
    """Pad 1's presses as the calibration run logged them: frame pressed, button, frames held."""
    presses, down, since = [], 0, {}
    with open(log_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = LOG_LINE.search(line)
            if not m:
                continue
            frame, mask = int(m.group(1)), int(m.group(3), 16)
            for bit, name in BUTTONS.items():
                if mask & bit and not down & bit:
                    since[bit] = frame
                elif down & bit and not mask & bit:
                    presses.append((since[bit], name, max(1, frame - since[bit])))
            down = mask
    for bit, name in BUTTONS.items():
        if down & bit:  # still held when the log ends: held to the end
            presses.append((since[bit], name, 1000000))
    presses.sort()
    return ','.join(f'{f}:{b}/{h}' for f, b, h in presses)


def to_frame(seconds, anchors, offset):
    if not anchors:
        return offset + round(seconds * 60.0)
    times = [t for t, _ in anchors]
    i = bisect.bisect_right(times, seconds)
    if i == 0:
        t, f = anchors[0]
        return max(0, f - round((t - seconds) * 60.0))
    if i == len(anchors):
        t, f = anchors[-1]
        return f + round((seconds - t) * 60.0)
    (t0, f0), (t1, f1) = anchors[i - 1], anchors[i]
    if t1 == t0:
        return f0
    return round(f0 + (f1 - f0) * (seconds - t0) / (t1 - t0))


def convert(script, anchors, offset):
    out = []
    for entry in filter(None, (e.strip() for e in script.replace('\n', ',').split(','))):
        when, _, press = entry.partition(':')
        button, _, held = press.partition('/')
        frames = max(1, round(float(held) * 60.0 / 1000.0)) if held else 30
        out.append((to_frame(float(when), anchors, offset), button, frames))
    out.sort()
    return ','.join(f'{f}:{b}/{h}' for f, b, h in out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('script', nargs='?', default=None)
    parser.add_argument('--script-file')
    parser.add_argument('--log', help="a calibration run's log (PS2X_PAD_LOG=1 lines)")
    parser.add_argument('--from-log', action='store_true', help="pad 1's script from the logged changes")
    parser.add_argument('--offset', type=int, default=0, help='frames added (without --log)')
    args = parser.parse_args()
    if args.from_log:
        if not args.log:
            parser.error('--from-log needs --log')
        print(from_log(args.log))
        return 0
    script = open(args.script_file).read() if args.script_file else args.script
    if not script:
        parser.error('no script')
    anchors = anchors_from(args.log) if args.log else []
    print(convert(script, anchors, args.offset))
    return 0


if __name__ == '__main__':
    sys.exit(main())
