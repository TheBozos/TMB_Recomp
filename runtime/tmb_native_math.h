// SPDX-License-Identifier: MIT
// Native calculations over TMB's column-major matrix records. No VU registers, flags, microcode or data memory.
#pragma once

#include "bozo/ops_fpu.h"
#include <array>
#include <cstdint>
#include <immintrin.h>

namespace tmb::math
{
    struct alignas(16) Vector { std::array<uint32_t, 4> bits{}; };
    struct Matrix { std::array<Vector, 4> column{}; };
    enum class Product { TransposedBasis, Full };

    // Macro-mode products/sums round separately to nearest. Preserve the caller's complete host FP control state.
    class ArithmeticScope
    {
        unsigned saved_ = _mm_getcsr();
    public:
        ArithmeticScope() { _mm_setcsr((saved_ & ~0x6000u) | 0x9fc0u); }
        ~ArithmeticScope() { _mm_setcsr(saved_); }
    };

    inline __m128 lanes(const Vector &v) { return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i *>(v.bits.data()))); }
    inline Vector record(__m128 v)
    {
        Vector out;
        _mm_storeu_si128(reinterpret_cast<__m128i *>(out.bits.data()), _mm_castps_si128(v));
        return out;
    }
    inline bool extended(uint32_t v) { return (v & 0x7f800000u) == 0x7f800000u; }
    inline Vector broadcast(uint32_t bits) { return Vector{{bits, bits, bits, bits}}; }

    inline Vector multiply(const Vector &a, const Vector &b)
    {
        Vector out = record(_mm_mul_ps(lanes(a), lanes(b)));
        // TMB's number format has a finite exponent above IEEE's range. Keep those rare values exact as data,
        // without introducing processor state. Ordinary game values use the host SIMD operation above.
        for (unsigned i = 0; i < 4; ++i)
            if (extended(a.bits[i]) || extended(b.bits[i]) || extended(out.bits[i]))
            {
                uint32_t ignored = 0;
                out.bits[i] = bozo::ps2f::mul(a.bits[i], b.bits[i], ignored);
            }
        return out;
    }
    inline Vector add(const Vector &a, const Vector &b)
    {
        Vector out = record(_mm_add_ps(lanes(a), lanes(b)));
        for (unsigned i = 0; i < 4; ++i)
            if (extended(a.bits[i]) || extended(b.bits[i]) || extended(out.bits[i]))
            {
                uint32_t ignored = 0;
                out.bits[i] = bozo::ps2f::addNearest(a.bits[i], b.bits[i], ignored);
            }
        return out;
    }

    inline Matrix product(const Matrix &first, const Matrix &second, Product kind)
    {
        ArithmeticScope arithmetic;
        const Matrix *weights = &first, *basis = &second;
        Matrix transposed;
        if (kind == Product::TransposedBasis)
        {
            transposed = second;
            for (unsigned i = 0; i < 3; ++i)
                for (unsigned j = 0; j < 3; ++j)
                    if (i != j)
                    {
                        const uint32_t v = second.column[j].bits[i];
                        transposed.column[i].bits[j] = (v & 0x7f800000u) ? v : 0u; // the original's +0 on a moved component
                    }
            weights = &transposed;
            basis = &first;
        }
        Matrix out;
        const unsigned columns = kind == Product::Full ? 4u : 3u;
        for (unsigned i = 0; i < columns; ++i)
        {
            Vector value = multiply(basis->column[0], broadcast(weights->column[i].bits[0]));
            value = add(value, multiply(basis->column[1], broadcast(weights->column[i].bits[1])));
            value = add(value, multiply(basis->column[2], broadcast(weights->column[i].bits[2])));
            out.column[i] = kind == Product::Full
                ? add(value, multiply(basis->column[3], broadcast(weights->column[i].bits[3])))
                : add(value, Vector{}); // the basis helpers finish with +0, including their fourth components
        }
        if (kind != Product::Full)
            out.column[3].bits[3] = 0x3f800000u;
        return out;
    }
}
