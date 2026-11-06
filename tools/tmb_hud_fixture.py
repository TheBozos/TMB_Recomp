"""Extract the six exact HUD instruction fallback bodies from fresh BozoEngine output.

Generated fixtures stay in the build tree. Fail closed if a body is absent or is no longer a straight
memory/vector operation; this is an independent original for diagnostic and native ABI comparisons.
"""
import re
import sys
from pathlib import Path

BLOCKS = ((0x13ce74, 0x13ce98), (0x13ceb4, 0x13ced8), (0x13d03c, 0x13d060),
          (0x13d210, 0x13d22c), (0x13d308, 0x13d324), (0x13d408, 0x13d424))


def main():
    folder = Path(sys.argv[1])
    bodies = {}
    pattern = re.compile(r'^\s*if \(!bozo_native_instruction\(rdram, ctx, rt, (0x[0-9a-f]+)u\)\) \{ (.+) \}\s*$')
    for source in folder.glob('tmb_[0-9][0-9][0-9].cpp'):
        for line in source.read_text(encoding='utf-8').splitlines():
            match = pattern.fullmatch(line)
            if match:
                pc = int(match[1], 16)
                if any(first <= pc <= last for first, last in BLOCKS):
                    if pc in bodies:
                        raise ValueError(f'duplicate instruction {pc:#x}')
                    body = match[2]
                    if not re.fullmatch(r'bozo::(?:load128|store128|vu0::(?:mul|madd))\([^;{}]+\);', body):
                        raise ValueError(f'unsafe original HUD instruction {pc:#x}: {body}')
                    bodies[pc] = body
    source = ['// Generated original HUD blocks. Do not commit.', '#include "tmb_functions.h"', '#include "tmb_hud_original.h"']
    for index, (first, last) in enumerate(BLOCKS):
        source.append(f'static void hud_original_{index}(uint8_t *rdram, R5900Context *ctx, Runtime *rt) {{')
        for pc in range(first, last + 4, 4):
            source.append('    ' + bodies[pc])
        source.append('}')
    source.append('const RecompiledFunction tmb_hud_originals[6] = {' + ','.join(f'&hud_original_{i}' for i in range(6)) + '};')
    header = '#pragma once\n#include "bozo/recomp.h"\nextern const RecompiledFunction tmb_hud_originals[6];\n'
    (folder / 'tmb_hud_original.h').write_text(header, encoding='utf-8', newline='\n')
    (folder / 'tmb_hud_original.cpp').write_text('\n'.join(source) + '\n', encoding='utf-8', newline='\n')
    print(f'original HUD fixtures: {len(bodies)} instructions in {len(BLOCKS)} blocks')


if __name__ == '__main__':
    main()
