// SPDX-License-Identifier: MIT
#pragma once
#include "tmb_port.h"
#include <array>

namespace tmb::math
{
    struct HudTransform
    {
        uint32_t first, last;
        unsigned basis, input, translation, output; // GPR addresses; translation=0 for directions
    };
    inline constexpr std::array<HudTransform, 6> hudTransforms{{
        {0x13ce74, 0x13ce98, 18, 29, 16, 16},
        {0x13ceb4, 0x13ced8, 18, 29, 16, 29},
        {0x13d03c, 0x13d060, 17, 29, 16, 16},
        {0x13d210, 0x13d22c, 17, 20,  0, 20},
        {0x13d308, 0x13d324,  2, 20,  0, 20},
        {0x13d408, 0x13d424, 17, 16,  0, 16},
    }};
    bool hudInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t pc);
}
