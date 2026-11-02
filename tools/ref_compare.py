#!/usr/bin/env python3
"""Compares two reference records (PS2X_REFERENCE=<file.ref>, docs/REFERENCE_HARNESS.md) frame by frame.

  tools/ref_compare.py a.ref b.ref [--ignore eeCycle,...] [--lines 24]

Reports the frames compared and the first frame where a field differs (every field that differs there, and the first
frame each field differs at). For `ram`, when both runs dumped that frame (PS2X_REFERENCE_DUMP=<frame>: <file>.ram.<frame>),
the first differing 32-byte block (its address and both runs' bytes); for `calls`, when both have the frame's call list
(PS2X_REFERENCE_CALLS / _FROM: <file>.calls), the two lists side by side from their first difference.
Exit status 0: identical over the frames both have (and the same length); 1: they differ; 2: unreadable input.
"""
import argparse
import os
import struct
import sys

HEADER = struct.Struct('<8sIIIIIIQII64s64s64s')
RECORD = struct.Struct('<IiQQQQQQQIIII')
FIELDS = ('frame', 'thread', 'ram', 'scratchpad', 'calls', 'draws', 'sound', 'threads', 'eeCycle', 'callCount',
          'drawBatches', 'drawVertices', 'soundSamples')
# The order fields are reported in (the guest's state first, then what it went through, then the counters).
COMPARED = ('ram', 'scratchpad', 'calls', 'callCount', 'draws', 'drawBatches', 'drawVertices', 'sound', 'soundSamples',
            'threads', 'thread', 'eeCycle')
HEXED = {'ram', 'scratchpad', 'calls', 'draws', 'sound', 'threads'}


class Reference:
    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            data = f.read()
        if len(data) < HEADER.size:
            raise ValueError(f'{path}: too short for a header')
        (magic, version, header_bytes, record_bytes, hash_id, ram_bytes, sp_bytes, elf_hash, flags, mask_count, run,
         build, elf) = HEADER.unpack_from(data, 0)
        if magic != b'PS2XREF1':
            raise ValueError(f'{path}: not a reference record (magic {magic!r})')
        if record_bytes != RECORD.size:
            raise ValueError(f'{path}: records of {record_bytes} bytes, this tool reads {RECORD.size}')
        self.version, self.hash_id, self.elf_hash, self.flags = version, hash_id, elf_hash, flags
        self.ram_bytes, self.sp_bytes = ram_bytes, sp_bytes
        self.run, self.build, self.elf = (s.split(b'\0', 1)[0].decode(errors='replace') for s in (run, build, elf))
        self.masks = [struct.unpack_from('<II', data, HEADER.size + 8 * i) for i in range(mask_count)]
        body = data[header_bytes:]
        count = len(body) // RECORD.size
        self.records = [dict(zip(FIELDS, RECORD.unpack_from(body, i * RECORD.size))) for i in range(count)]
        self.partial = len(body) % RECORD.size

    def describe(self):
        return (f'{self.path}: run "{self.run}", build "{self.build}", ELF {self.elf} {self.elf_hash:016x}, '
                f'{"deterministic" if self.flags & 1 else "NOT deterministic"}, {len(self.records)} frames'
                + (f', {len(self.masks)} masked ranges' if self.masks else '')
                + (f' (+{self.partial} bytes of a cut record)' if self.partial else ''))


def value(field, v):
    return f'{v:016x}' if field in HEXED else str(v)


def ram_block(a, b, frame):
    """The first differing 32-byte block of RAM (then the scratchpad) between the two runs' dumps of `frame`."""
    pa, pb = f'{a.path}.ram.{frame}', f'{b.path}.ram.{frame}'
    if not (os.path.exists(pa) and os.path.exists(pb)):
        return f'  (no RAM dumps of frame {frame}: run both again with PS2X_REFERENCE_DUMP={frame} for the first differing block)'
    with open(pa, 'rb') as fa, open(pb, 'rb') as fb:
        da, db = fa.read(), fb.read()
    lines = []
    for offset in range(0, min(len(da), len(db)), 32):
        if da[offset:offset + 32] != db[offset:offset + 32]:
            where = f'RAM {offset:08x}' if offset < a.ram_bytes else f'scratchpad {offset - a.ram_bytes:04x}'
            lines.append(f'  first differing 32-byte block: {where}')
            lines.append(f'    {a.run:>12}: {da[offset:offset + 32].hex(" ", 4)}')
            lines.append(f'    {b.run:>12}: {db[offset:offset + 32].hex(" ", 4)}')
            differing = sum(1 for o in range(0, min(len(da), len(db)), 32) if da[o:o + 32] != db[o:o + 32])
            lines.append(f'    ({differing} blocks differ in all)')
            return '\n'.join(lines)
    return '  (the dumps are the same: the hash differs elsewhere, a masked range?)'


def call_list(path, frame):
    """The call list of `frame` from <file>.calls, or None."""
    calls = path + '.calls'
    if not os.path.exists(calls):
        return None
    found, lines = False, []
    with open(calls, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('frame '):
                if found:
                    break
                found = int(line.split()[1]) == frame
                continue
            if found:
                lines.append(line.rstrip('\n').strip())
    return lines if found else None


def call_lists(a, b, frame, shown):
    la, lb = call_list(a.path, frame), call_list(b.path, frame)
    if la is None or lb is None:
        return (f'  (no call lists of frame {frame} in both: run both with PS2X_REFERENCE_CALLS_FROM={frame} '
                f'PS2X_REFERENCE_CALLS=1 to see them)')
    first = next((i for i in range(max(len(la), len(lb))) if i >= len(la) or i >= len(lb) or la[i] != lb[i]), None)
    if first is None:
        return '  (the lists read the same: the difference is in a value the text rounds?)'
    width = max([len(x) for x in la[first:first + shown]] + [len(a.run), 10])
    out = [f'  calls from #{first} ({len(la)} / {len(lb)} calls):', f'    {a.run:<{width}} | {b.run}']
    for i in range(first, min(max(len(la), len(lb)), first + shown)):
        left = la[i] if i < len(la) else ''
        right = lb[i] if i < len(lb) else ''
        out.append(f'  {"*" if left != right else " "} {left:<{width}} | {right}')
    return '\n'.join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('a')
    parser.add_argument('b')
    parser.add_argument('--ignore', default='', help='fields not compared, comma-separated (e.g. eeCycle)')
    parser.add_argument('--lines', type=int, default=24, help='call list lines shown from the first difference')
    args = parser.parse_args()
    try:
        a, b = Reference(args.a), Reference(args.b)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2
    print(a.describe())
    print(b.describe())
    if a.elf_hash != b.elf_hash:
        print('warning: not the same ELF')
    if a.masks != b.masks:
        print('warning: not the same masked RAM ranges')
    ignored = {f for f in args.ignore.split(',') if f}
    unknown = ignored - set(COMPARED)
    if unknown:
        print(f'unknown fields: {", ".join(sorted(unknown))} (fields: {", ".join(COMPARED)})', file=sys.stderr)
        return 2
    fields = [f for f in COMPARED if f not in ignored]
    frames = min(len(a.records), len(b.records))
    first_by_field = {}
    first = None
    for i in range(frames):
        ra, rb = a.records[i], b.records[i]
        if ra['frame'] != i or rb['frame'] != i:
            print(f'record {i}: frame numbers {ra["frame"]} / {rb["frame"]} (a record missing?)')
        for f in fields:
            if ra[f] != rb[f] and f not in first_by_field:
                first_by_field[f] = i
        if first is None and any(ra[f] != rb[f] for f in fields):
            first = i
    if first is None:
        same = len(a.records) == len(b.records)
        print(f'identical: {frames} frames compared ({", ".join(fields)})'
              + ('' if same else f'; lengths differ ({len(a.records)} / {len(b.records)} frames)'))
        return 0 if same else 1
    ra, rb = a.records[first], b.records[first]
    print(f'first difference at frame {first} (of {frames} compared; the {first} before are identical):')
    for f in fields:
        if ra[f] != rb[f]:
            print(f'  {f:<13} {value(f, ra[f]):>16}  {value(f, rb[f]):>16}')
    print('first frame each field differs at: ' + ', '.join(f'{f} {n}' for f, n in sorted(first_by_field.items(), key=lambda x: x[1])))
    if 'ram' in first_by_field and first_by_field['ram'] == first:
        print(ram_block(a, b, first))
    if 'calls' in first_by_field:
        print(call_lists(a, b, first_by_field['calls'], args.lines))
    return 1


if __name__ == '__main__':
    sys.exit(main())
