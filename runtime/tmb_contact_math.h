// SPDX-License-Identifier: MIT
// Inline terrain/contact XYZ calculations over game records; no VU transport.
#pragma once
#include "tmb_port.h"
#include <array>
namespace tmb::math
{
    enum class ContactOperation { Transform, Add, Subtract };
    struct ContactVector
    {
        uint32_t first, last;
        unsigned basis, input, translation, output; // GPR addresses; translation is add/sub's right operand
        ContactOperation operation;
    };
    inline constexpr std::array<ContactVector, 27> contactVectors{{
        {0x1cb1ac, 0x1cb1b8,  0,  2,  3,  4, ContactOperation::Add},
        {0x1cb1f0, 0x1cb1fc,  0,  4,  2, 23, ContactOperation::Add},
        {0x1cb438, 0x1cb454,  2, 20,  0,  3, ContactOperation::Transform},
        {0x1cb468, 0x1cb474,  0,  2,  4,  3, ContactOperation::Add},
        {0x1cb4a8, 0x1cb4b4,  0, 17,  4, 23, ContactOperation::Add},
        {0x1cb4d8, 0x1cb4f4,  2, 29,  0,  3, ContactOperation::Transform},
        {0x1cb508, 0x1cb514,  0,  2,  4,  3, ContactOperation::Add},
        {0x1cb534, 0x1cb540,  0, 17,  2, 22, ContactOperation::Add},
        {0x1cb708, 0x1cb724,  3,  2,  0, 29, ContactOperation::Transform},
        {0x1cb734, 0x1cb740,  0,  2, 29, 23, ContactOperation::Add},
        {0x1cb77c, 0x1cb788,  0,  3, 16, 20, ContactOperation::Add},
        {0x1cb7d0, 0x1cb7ec,  3,  4,  0, 29, ContactOperation::Transform},
        {0x1cb7fc, 0x1cb808,  0,  3, 29,  2, ContactOperation::Add},
        {0x1cb80c, 0x1cb818,  0,  2, 23, 29, ContactOperation::Subtract},
        {0x1cb8dc, 0x1cb8e8,  0, 20, 16, 19, ContactOperation::Add},
        {0x1cb8f8, 0x1cb904,  0, 20, 16, 19, ContactOperation::Add},
        {0x1cc814, 0x1cc838,  2,  3,  4, 16, ContactOperation::Transform},
        {0x1cca40, 0x1cca64,  3, 19,  4, 17, ContactOperation::Transform},
        {0x1cca6c, 0x1cca90,  3, 20,  4, 16, ContactOperation::Transform},
        {0x1cbd48, 0x1cbd54,  0, 23, 19, 29, ContactOperation::Subtract},
        {0x1cbeb0, 0x1cbebc,  0, 16, 19, 17, ContactOperation::Subtract},
        {0x1cbec0, 0x1cbecc,  0, 23, 19, 18, ContactOperation::Subtract},
        {0x1cbef0, 0x1cbefc,  0, 16, 23, 18, ContactOperation::Subtract},
        {0x1cc038, 0x1cc054,  4,  2,  0,  3, ContactOperation::Transform},
        {0x1cc060, 0x1cc06c,  0,  4, 23,  2, ContactOperation::Subtract},
        {0x1cc074, 0x1cc090,  3,  2,  0, 18, ContactOperation::Transform},
        {0x1cc09c, 0x1cc0a8,  0,  2, 18,  5, ContactOperation::Add},
    }};
    bool contactInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t pc);
}
