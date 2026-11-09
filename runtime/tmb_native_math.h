// SPDX-License-Identifier: MIT
// Native calculations over TMB's column-major matrix records. No VU registers, flags, microcode or data memory.
#pragma once

#include "bozo/ops_fpu.h"
#include <array>
#include <cstdint>
#include <immintrin.h>

namespace tmb::math
{
    // Audited XYZ-only consumers: collision queries, vehicle spatial records and hierarchy lighting/attachments.
    inline constexpr std::array<uint32_t, 201> directionReturns{
        0x168b30, 0x1cd618, 0x1cda54, 0x1cdabc, 0x11708c, 0x1880e0, 0x1d4284, 0x1d42e8, 0x1cb19c,
        0x1cfd18, 0x1cfde8, 0x1d0224, 0x1d0250, // height-test collision/BSP results
        0x1d25ec, 0x1d261c, 0x1d2784, 0x1d2804, // line-of-sight BSP/volume results
        0x1cbbc8, 0x1cbc2c, 0x1cc634, 0x1cc6b4, // point/line result publication
        0x1ccec8, 0x1ccef8, 0x1cd884, 0x1cd8b4, // terrain point and BSP auxiliary results
        0x1d41c8, 0x1d9cac, // burning attachment and light-volume colour coordinates
        0x1cdf40, 0x1cdfe4, 0x1ce1cc, 0x1ce37c, 0x1ce3a4, 0x1cdd3c, // BSP points and crossing results
        0x1c6bac, 0x1c6c54, 0x1c7134, 0x1c7160, // older collision-node and terrain setup
        0x1c7818, 0x1c78c4, 0x1c7a90, 0x1c7bc8, 0x1c7bf8, 0x1c7de8, 0x1c7e14, // older BSP setup/results
        0x1c8094, 0x1c80ac, 0x1c8120, 0x1c8150, // point-contact result normal/position
        0x1c85cc, 0x1c85e0, 0x1c8608, 0x1c863c, // ray result normal/position
        0x1c8f10, 0x1c8f3c, 0x1c8fd8, 0x1c9000, 0x1c908c, 0x1c90b4,
        0x1c9140, 0x1c9168, 0x1c91f4, 0x1c921c, 0x1c959c, 0x1c96e8, // older car debug geometry
        0x1d4514, 0x1d4560, // hierarchy particle-node positions
        0x1888e8, 0x18b4c0, // suspension ride height and tire pivot reconstruction
        0x1e42d8, 0x1e4818, // attached emitter offset and emitted particle direction
        0x15e1b8, // vehicle shadow vertices
        0x1b42c8, 0x1b430c, 0x1b43a8, 0x1b4434, 0x1b44c0, // weapon shadow motion and height-sample points
        0x150bc8, 0x150cc0, 0x150d2c, // tow-truck launcher offsets and smoothed attachment position
        0x1abd60, // vehicle peel-out visual offset, consumed as XYZ by vehicleSetCsMat
        0x1137cc, 0x11398c, 0x113a78, 0x113b28, 0x113c2c, 0x113f28, 0x1142d8, 0x114394,
        0x1fdd3c, // camera positioning, collision-probe offsets and rear-view publication
        0x1e8a34, // weather viewport centre, consumed as XYZ by placement/grid/projection setup
        0x1b28cc, 0x1b28e8, // weapon-local / coordinate-system translation offsets
        0x168ca0, 0x1691b0, 0x169378, // skid-mark half-width and stationary/moving stretch offsets
        0x11112c, // final-boss captured vehicle position, consumed as XYZ
        0x1ed478, 0x1ed4c4, 0x1ed4dc, // rigid-body local forces and application points
        0x1ed544, 0x1ed55c, // rigid-body local torque and application point
        0x1ed708, 0x1ed754, 0x1ed76c, // rigid-body local impulses and application points
        0x1edf38, 0x1ee07c, // force integration and computed world position
        0x126618, 0x1267f0, 0x1276a4, // driver path offsets and pursuit lookahead
        0x128704, 0x128744, 0x1287dc, 0x12880c, // driver visibility query endpoints
        0x11ad1c, 0x11ada0, 0x11b26c, // collision velocity reconstruction and force application
        0x11bba8, 0x11bbe8, 0x11c368, // impact offsets and morph hit placement
        0x129e58, 0x137cac, // helicopter movement and cable attachment position
        0x1b2d1c, 0x1b3d30, 0x1b3e00, // gascan, saw-blade and taxi-spike movement
        0x1ada84, 0x1adb8c, 0x1adba0, 0x1adca8, 0x1adcbc, 0x1addc4, 0x1addd8, // Minion shield hit-point distances
        0x1b1700, 0x1b1bbc, 0x1b339c, 0x1b3634, 0x1b371c, 0x1b3ef8, // weapon lock-on/look-ahead targets, satellite and rock movement
        0x1b2fb0, 0x1b3118, // ricochet movement before its height test and after its reflection
        0x150efc, 0x1511ec, 0x1513f4, // Hearse, Minion and Tanker launcher turret aim points
        0x150094, 0x1502cc, // Cop SUV laser aim points (player-state and closest-car branches)
        0x14d62c, 0x14d678, // environment gun fire points passed to CreateStaticWeapon
        0x12aad4, 0x169ed0, // falling Ferris wheel car position and smasher hit camera position
        0x12a180, 0x1392e4, // destroyed environment helicopter / helicopter fall velocity
        0x143468, 0x10eca0, // junk jet and blimp waypoint flight steps
        0x135cf8, 0x136200, 0x1362ec, // rolling gas tank step and its two falling bursts
        0x10eee0, 0x10efa0, // sinking blimp drift and explosion point
        0x143864, 0x143940, 0x1439bc, 0x143a18, 0x143b0c, 0x143b60, // falling junk jet: drift, explosion and crash points
        0x129c48, // environment helicopter gun origin passed to CreateStaticWeapon
        0x10cc7c, // walking attract-mode pedestrian step
        0x1b79f8, 0x1b7a14, // machine-gun weapon muzzle positions (alternating barrels)
        0x18b10c, 0x18b19c, // tire-pair height corrections in tireUpdateCarRotation
        0x1af700, 0x1af790, 0x1af890, 0x1af8fc, 0x1af980, // combo special launch positions
        0x14d294, // Zoomies launcher muzzle position
        0x1b3874, 0x1b39a4, 0x1b3b00, // UpdateHalTrans: flying step and the two attached-target points
        0x103534, 0x106af0, // AI hunt bead heading and bait point
        0x1e7e08, 0x1e77e8, // particle flat-arc and electric-arc points
        0x10febc, 0x110114, 0x1101b0, 0x1103e4, 0x1104e4, 0x110880, // FinalBoss gun shot origins
        0x11091c, 0x110a0c, 0x110adc, 0x110d9c, 0x110e8c, 0x110f74,
        0x111788, 0x111cc4, // FinalBoss movement and death-roll position steps
        0x14dc44, 0x14dccc, 0x14dea8, 0x14df2c, 0x14e020}; // UpdateSpecial launch points (vehicle types 60, 120, c0)

    struct alignas(16) Vector { std::array<uint32_t, 4> bits{}; };
    struct Matrix { std::array<Vector, 4> column{}; };
    enum class Product { TransposedBasis, Full, Basis };

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

    inline Vector subtract(const Vector &a, const Vector &b)
    {
        Vector out = record(_mm_sub_ps(lanes(a), lanes(b)));
        for (unsigned i = 0; i < 4; ++i)
            if (extended(a.bits[i]) || extended(b.bits[i]) || extended(out.bits[i]))
            {
                uint32_t ignored = 0;
                out.bits[i] = bozo::ps2f::subNearest(a.bits[i], b.bits[i], ignored);
            }
        return out;
    }

    inline Vector combineXYZ(Vector a, Vector b, bool difference)
    {
        ArithmeticScope arithmetic;
        a.bits[3] = b.bits[3] = 0;
        return difference ? subtract(a, b) : add(a, b);
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

    inline Vector transformXYZ(Matrix basis, const Vector &input, const Vector *translation)
    {
        ArithmeticScope arithmetic;
        // Fourth components are opaque record payloads here, not numerical inputs or outputs.
        for (unsigned i = 0; i < 3; ++i) basis.column[i].bits[3] = 0;
        Vector out = multiply(basis.column[0], broadcast(input.bits[0]));
        out = add(out, multiply(basis.column[1], broadcast(input.bits[1])));
        out = add(out, multiply(basis.column[2], broadcast(input.bits[2])));
        if (translation)
        {
            Vector position = *translation;
            position.bits[3] = 0;
            out = add(out, multiply(position, broadcast(0x3f800000u)));
        }
        return out;
    }
}
