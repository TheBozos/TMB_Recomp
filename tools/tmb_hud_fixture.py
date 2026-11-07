"""Extract exact HUD, vehicle or terrain/contact instruction fallback bodies from fresh BozoEngine output.

Generated fixtures stay in the build tree. Fail closed if a body is absent or is no longer a straight
memory/vector operation; this is an independent original for diagnostic and native ABI comparisons.
"""
import re
import sys
from pathlib import Path

BLOCKS = {
    'hud': ((0x13ce74, 0x13ce98), (0x13ceb4, 0x13ced8), (0x13d03c, 0x13d060),
            (0x13d210, 0x13d22c), (0x13d308, 0x13d324), (0x13d408, 0x13d424)),
    'contact': ((0x1cb1ac, 0x1cb1b8), (0x1cb1f0, 0x1cb1fc), (0x1cb438, 0x1cb454),
                (0x1cb468, 0x1cb474), (0x1cb4a8, 0x1cb4b4), (0x1cb4d8, 0x1cb4f4),
                (0x1cb508, 0x1cb514), (0x1cb534, 0x1cb540), (0x1cb708, 0x1cb724),
                (0x1cb734, 0x1cb740), (0x1cb77c, 0x1cb788), (0x1cb7d0, 0x1cb7ec),
                (0x1cb7fc, 0x1cb808), (0x1cb80c, 0x1cb818), (0x1cb8dc, 0x1cb8e8),
                (0x1cb8f8, 0x1cb904), (0x1cc814, 0x1cc838), (0x1cca40, 0x1cca64),
                (0x1cca6c, 0x1cca90), (0x1cbd48, 0x1cbd54), (0x1cbeb0, 0x1cbebc),
                (0x1cbec0, 0x1cbecc), (0x1cbef0, 0x1cbefc), (0x1cc038, 0x1cc054),
                (0x1cc060, 0x1cc06c), (0x1cc074, 0x1cc090), (0x1cc09c, 0x1cc0a8),
                (0x1c6714, 0x1c6720), (0x1c69bc, 0x1c69c8), (0x1cadc4, 0x1cadd0)),
    'vehicle': ((0x1aada8, 0x1aadc4), (0x1aadd0, 0x1aaddc), (0x1aae38, 0x1aae44),
                (0x1aaf70, 0x1aaf8c), (0x1aaf94, 0x1aafa0), (0x1aafa8, 0x1aafb4),
                (0x1aafb8, 0x1aafc4), (0x1aafdc, 0x1aaff8), (0x1aaffc, 0x1ab008),
                (0x1ab010, 0x1ab01c), (0x1ab124, 0x1ab140), (0x1ab144, 0x1ab150),
                (0x1ab29c, 0x1ab2a8)),
}


def main():
    folder = Path(sys.argv[1])
    family = sys.argv[2] if len(sys.argv) > 2 else 'hud'
    blocks = BLOCKS[family]
    output = Path(sys.argv[3]) if len(sys.argv) > 3 else folder / 'native_original'
    output.mkdir(parents=True, exist_ok=True)
    bodies = {}
    pattern = re.compile(r'^\s*if \(!bozo_native_instruction\(rdram, ctx, rt, (0x[0-9a-f]+)u\)\) \{ (.+) \}\s*$')
    for source in folder.glob('tmb_[0-9][0-9][0-9].cpp'):
        for line in source.read_text(encoding='utf-8').splitlines():
            match = pattern.fullmatch(line)
            if match:
                pc = int(match[1], 16)
                if any(first <= pc <= last for first, last in blocks):
                    if pc in bodies:
                        raise ValueError(f'duplicate instruction {pc:#x}')
                    body = match[2]
                    if not re.fullmatch(r'bozo::(?:load128|store128|vu0::(?:mul|madd|add|sub))\([^;{}]+\);', body):
                        raise ValueError(f'unsafe original {family} instruction {pc:#x}: {body}')
                    bodies[pc] = body
    source = [f'// Generated original {family} blocks. Do not commit.', '#include "tmb_functions.h"', f'#include "tmb_{family}_original.h"']
    for index, (first, last) in enumerate(blocks):
        source.append(f'static void {family}_original_{index}(uint8_t *rdram, R5900Context *ctx, Runtime *rt) {{')
        for pc in range(first, last + 4, 4):
            source.append('    ' + bodies[pc])
        source.append('}')
    source.append(f'const RecompiledFunction tmb_{family}_originals[{len(blocks)}] = {{' + ','.join(f'&{family}_original_{i}' for i in range(len(blocks))) + '};')
    header = f'#pragma once\n#include "bozo/recomp.h"\nextern const RecompiledFunction tmb_{family}_originals[{len(blocks)}];\n'
    (output / f'tmb_{family}_original.h').write_text(header, encoding='utf-8', newline='\n')
    (output / f'tmb_{family}_original.cpp').write_text('\n'.join(source) + '\n', encoding='utf-8', newline='\n')
    print(f'original {family} fixtures: {len(bodies)} instructions in {len(blocks)} blocks')


if __name__ == '__main__':
    main()
