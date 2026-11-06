// SPDX-License-Identifier: MIT
#pragma once
#include "tmb_native_math.h"

namespace tmb::math
{
    inline Matrix identityMatrix()
    {
        Matrix out;
        for (unsigned i = 0; i < 4; ++i) out.column[i].bits[i] = 0x3f800000;
        return out;
    }

    // The game's row-major affine projection. Input W is not a weight; each row's fourth value is
    // its translation. The shipped reduction rotates its addition order for each output component.
    inline Vector projectPointRows(const Matrix &matrix, const Vector &point)
    {
        const ArithmeticScope arithmetic;
        const Vector weights{{point.bits[0], point.bits[1], point.bits[2], 0}};
        const auto sum = [](uint32_t a, uint32_t b) { return add(broadcast(a), broadcast(b)).bits[0]; };
        Vector out;
        for (unsigned row = 0; row < 4; ++row)
        {
            Vector values = matrix.column[row];
            const uint32_t translation = values.bits[3];
            values.bits[3] = 0;
            values = multiply(values, weights);
            values.bits[3] = translation;
            out.bits[row] = sum(values.bits[row], sum(values.bits[(row+1)&3],
                                sum(values.bits[(row+2)&3], values.bits[(row+3)&3])));
        }
        return out;
    }
}
