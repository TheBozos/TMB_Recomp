// SPDX-License-Identifier: MIT
#pragma once
#include "tmb_port.h"

namespace tmb::textureInputs
{
    bool owns(uint32_t pc);
    bool instruction(uint8_t *, R5900Context *, Runtime *, uint32_t pc);
}
