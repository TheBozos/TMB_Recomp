// SPDX-License-Identifier: MIT
#pragma once
#include "tmb_port.h"
#include <array>

namespace tmb::math
{
    enum class VehicleOperation { Transform, Add, Subtract };
    struct VehicleVector
    {
        uint32_t first, last;
        unsigned basis, input, translation, output; // GPR addresses; translation is the right operand for add/sub
        VehicleOperation operation;
    };
    inline constexpr std::array<VehicleVector, 13> vehicleVectors{{
        {0x1aada8, 0x1aadc4, 21,  3,  0, 19, VehicleOperation::Transform},
        {0x1aadd0, 0x1aaddc,  0, 22, 19,  2, VehicleOperation::Add},
        {0x1aae38, 0x1aae44,  0,  5,  6,  5, VehicleOperation::Add},
        {0x1aaf70, 0x1aaf8c, 21,  3,  0, 19, VehicleOperation::Transform},
        {0x1aaf94, 0x1aafa0,  0, 22, 19,  4, VehicleOperation::Add},
        {0x1aafa8, 0x1aafb4,  0,  2,  4, 19, VehicleOperation::Subtract},
        {0x1aafb8, 0x1aafc4,  0, 22, 19, 22, VehicleOperation::Add},
        {0x1aafdc, 0x1aaff8, 23, 19,  0, 30, VehicleOperation::Transform},
        {0x1aaffc, 0x1ab008,  0, 22, 30, 22, VehicleOperation::Add},
        {0x1ab010, 0x1ab01c,  0, 22,  2, 30, VehicleOperation::Subtract},
        {0x1ab124, 0x1ab140, 21, 16,  0,  2, VehicleOperation::Transform},
        {0x1ab144, 0x1ab150,  0,  2, 22, 16, VehicleOperation::Add},
        {0x1ab29c, 0x1ab2a8,  0,  6,  3,  4, VehicleOperation::Add},
    }};
    bool vehicleInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t pc);
}
