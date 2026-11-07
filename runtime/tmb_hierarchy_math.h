// SPDX-License-Identifier: MIT
// Record-level hierarchy collision calculations. No processor context or VU data memory.
#pragma once
#include "tmb_native_math.h"
#include <cmath>
#include <cstring>

namespace tmb::hierarchy
{
    inline constexpr uint32_t inverseCallers[] = {
        0x1cf1ac, 0x1cf1c8, // hdColGrid
        0x1cf63c, 0x1cf658, // hdHatTraverse
        0x1d00b8, 0x1d00e4, // animated height-test hierarchy
        0x1d12f8,           // segment collision grid
        0x1d176c, 0x1d1788, // hdTraverse
        0x1d23f8, 0x1d2424, // animated collision hierarchy
        0x1cd658, 0x1cda84, 0x1cdae8, // BSP point/segment setup
        0x1170fc,           // vehicle collision-point offset
        0x1cb1e4, 0x1cb4a4, 0x1cb530, // collision node point/normal setup
        0x1cb770, 0x1cb834, // point-contact setup and scalar axis tests
        0x1d0de0, 0x1d0dfc, 0x1d12d8, // segment collision-grid local points
        0x1d4b94, // control-node BSP segment setup
        0x1cd400, 0x1cd768, // animated point setup and BSP auxiliary segment setup
        0x1cdf80, 0x1ce028, 0x1ce220, // BSP point setup and normalization
        0x1ce644, 0x1ce674, 0x1ce7b4, 0x1cea40, // segment crossing endpoints
    };

    inline math::Vector inverseDirection(const math::Matrix &basis, const math::Vector &input)
    {
        math::Matrix transposed = basis;
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j)
                if (i != j)
                {
                    const auto bits = basis.column[j].bits[i];
                    transposed.column[i].bits[j] = (bits & 0x7f800000u) ? bits : 0u;
                }
        auto result = math::transformXYZ(transposed, input, nullptr);
        // This helper copies the first basis vector before calculating XYZ. Its fourth component is data.
        result.bits[3] = basis.column[0].bits[3];
        return result;
    }

    inline math::Vector planeDistances(const math::Vector &plane, const math::Vector &first, const math::Vector &second)
    {
        const math::ArithmeticScope arithmetic;
        const auto firstProducts = math::multiply(plane, first);
        const auto secondProducts = math::multiply(plane, second);
        const auto one = math::broadcast(0x3f800000u);
        // The second distance adds Y before X; keep that rounding order rather than a reassociated dot product.
        auto x = math::add(math::broadcast(firstProducts.bits[0]), math::multiply(one, math::broadcast(firstProducts.bits[1])));
        auto y = math::add(math::broadcast(secondProducts.bits[1]), math::multiply(one, math::broadcast(secondProducts.bits[0])));
        x = math::add(x, math::multiply(one, math::broadcast(firstProducts.bits[2])));
        y = math::add(y, math::multiply(one, math::broadcast(secondProducts.bits[2])));
        const auto offset = math::multiply(one, math::broadcast(plane.bits[3]));
        return math::Vector{{math::subtract(x, offset).bits[0], math::subtract(y, offset).bits[0], 0, 0}};
    }

    namespace collision
    {
        struct ArithmeticScope
        {
            unsigned saved = _mm_getcsr();
            ArithmeticScope() { _mm_setcsr(saved | 0x6000u); }
            ~ArithmeticScope() { _mm_setcsr(saved); }
        };
        inline float value(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
        inline uint32_t bits(float f) { uint32_t v; std::memcpy(&v, &f, 4); return v; }
        // The collision asset's arithmetic drops the smaller operand's unavailable mantissa bits before summing.
        inline float sum(float a, float b)
        {
            auto x = bits(a), y = bits(b);
            const auto ex = (x >> 23) & 255u, ey = (y >> 23) & 255u;
            if (ex && ey && ex != ey)
            {
                auto &small = ex > ey ? y : x;
                const auto shift = ex > ey ? ex - ey : ey - ex;
                small &= shift >= 24 ? 0x80000000u : ~((1u << shift) - 1u);
            }
            return value(x) + value(y);
        }
        inline float difference(float a, float b) { return sum(a, -b); }
        inline bool negative(float f) { return std::signbit(f) && f != 0.0f; }
        using Point = std::array<float, 3>;
        inline Point xyz(const math::Vector &v) { return {value(v.bits[0]), value(v.bits[1]), value(v.bits[2])}; }
        inline Point difference(Point a, Point b)
        { return {difference(a[0], b[0]), difference(a[1], b[1]), difference(a[2], b[2])}; }
        inline float dot(Point a, Point b)
        { return sum(sum(a[0]*b[0], a[1]*b[1]), a[2]*b[2]); }
    }

    // The game's height-test broad phase, including its ordered endpoint/projection tests and strict boundaries.
    // Radius is stored in start.W; the wrapper's scalar radius argument only controls its large-object shortcut.
    inline uint32_t heightSphere(const math::Vector &start, const math::Vector &centre, const math::Vector &end)
    {
        const collision::ArithmeticScope arithmetic;
        using namespace collision;
        const Point fromStart = difference(xyz(start), xyz(centre));
        const Point fromEnd = difference(xyz(end), xyz(centre));
        const Point segment = difference(xyz(start), xyz(end));
        const float radius = value(start.bits[3]);
        const float radiusSquared = radius * radius;
        const float startSquared = dot(fromStart, fromStart);
        if (negative(difference(startSquared, radiusSquared))) return 0x80;
        const float inverseLength = 1.0f / std::sqrt(std::fabs(dot(fromEnd, fromEnd)));
        const Point direction{fromEnd[0]*inverseLength, fromEnd[1]*inverseLength, fromEnd[2]*inverseLength};
        const float projection = dot(direction, fromStart);
        if (negative(projection)) return 0;
        if (negative(sum(difference(radiusSquared, startSquared), projection*projection))) return 0;
        return negative(difference(dot(segment, segment), radiusSquared)) ? 0x80 : 0;
    }
}
