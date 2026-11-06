// SPDX-License-Identifier: MIT
#include "tmb_port.h"
#include "tmb_hierarchy_math.h"
#include "bozo/libs/libraries.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace tmb::hierarchy
{
    namespace
    {
        constexpr uint32_t kHeightSphere = 0x001cf7c0u, kInverseDirection = 0x001da1e8u;
        constexpr uint32_t kPlaneDistances = 0x00201660u, kBspDistanceReturn = 0x0020134cu;
        struct Statistics
        {
            uint64_t spheres{}, shortcuts{}, inverse[std::size(inverseCallers)]{}, planes{}, checked{};
            ~Statistics()
            {
                std::fprintf(stderr, "[native:hierarchy] height spheres %llu, shortcuts %llu, plane pairs %llu, original comparisons %llu\n",
                    (unsigned long long)spheres, (unsigned long long)shortcuts, (unsigned long long)planes, (unsigned long long)checked);
                for (unsigned i = 0; i < std::size(inverseCallers); ++i)
                    if (inverse[i]) std::fprintf(stderr, "[native:hierarchy] inverse caller %06x: %llu\n",
                        inverseCallers[i], (unsigned long long)inverse[i]);
            }
        } statistics;
        bool check()
        {
            static const bool enabled = [] { const char *v = std::getenv("TMB_NATIVE_HIERARCHY_CHECK"); return v && *v == '1'; }();
            return enabled;
        }
        const uint8_t *span(Runtime *rt, uint32_t address, uint32_t bytes)
        {
            address &= ~15u;
            const auto offset = bozo::GuestMemory::flatOffset(address, bytes);
            if (offset < 0 || uint64_t(offset) + bytes > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address, bytes};
            return rt->memory.flat() + offset;
        }
        math::Vector vector(Runtime *rt, uint32_t address)
        { math::Vector v; std::memcpy(v.bits.data(), span(rt, address, 16), 16); return v; }

        void heightSphereOverride(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt))
            { hdHatSphereTestAsm__FP8_fvectorfT0T0_0x1cf7c0(ram, ctx, rt); return; }
            R5900Context expected;
            if (check())
            {
                expected = *ctx;
                auto &vu = bozo::port::libraries(rt).vu0;
                const auto saved = vu;
                vu.image = bozo::libs::Vu0Image::Hierarchy; // diagnostic oracle only; no player image selection
                try { hdHatSphereTestAsm__FP8_fvectorfT0T0_0x1cf7c0(ram, &expected, rt); }
                catch (...) { vu=saved; throw; }
                vu=saved;
            }
            // Preserve the scalar prefix's observable CPU contract, including the shortcut's no-vector-read rule.
            ctx->fu[0] = bozo::load<uint32_t>(ram, ctx, rt, getRegU32(ctx, 28) - 0x4854u);
            bozo::fcompare(ctx, bozo::ps2f::lt(ctx->fu[0], ctx->fu[12]));
            uint32_t result = 1;
            if (bozo::fcond(ctx)) ++statistics.shortcuts;
            else result = heightSphere(vector(rt, getRegU32(ctx, 4)), vector(rt, getRegU32(ctx, 5)), vector(rt, getRegU32(ctx, 6)));
            setReturnS32(ctx, result);
            ctx->pc = getRegU32(ctx, 31);
            if (check())
            {
                if (getRegU32(&expected, 2) != result) throw std::runtime_error("native hierarchy sphere differs from original");
                ++statistics.checked;
            }
            ++statistics.spheres;
        }

        void inverseDirectionOverride(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            const auto caller = std::find(std::begin(inverseCallers), std::end(inverseCallers), getRegU32(ctx, 31));
            if (!bozo::port::nativeGraphics(rt) || caller == std::end(inverseCallers))
            { mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram, ctx, rt); return; }
            math::Matrix basis{};
            for (unsigned i = 0; i < 3; ++i) basis.column[i] = vector(rt, getRegU32(ctx, 4) + 16u*i);
            const auto result = inverseDirection(basis, vector(rt, getRegU32(ctx, 5)));
            auto *out = const_cast<uint8_t *>(span(rt, getRegU32(ctx, 6), 16));
            if (check())
            {
                math::Vector saved, expected;
                std::memcpy(&saved, out, 16);
                R5900Context original = *ctx;
                mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram, &original, rt);
                std::memcpy(&expected, out, 16); std::memcpy(out, &saved, 16);
                if (std::memcmp(&result, &expected, 16)) throw std::runtime_error("native hierarchy inverse differs from original");
                ++statistics.checked;
            }
            std::memcpy(out, &result, 16);
            ++statistics.inverse[caller - std::begin(inverseCallers)];
            ctx->pc = getRegU32(ctx, 31);
        }

        void planeDistancesOverride(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            // bspVecTestIn consumes only the two distances in memory. The unused bspInterp helper still expects
            // the original register ABI, so calls from any other entry retain that complete implementation.
            if (!bozo::port::nativeGraphics(rt) || getRegU32(ctx, 31) != kBspDistanceReturn)
            { bspPlaneTest2Vec__FP6_planeP8_fvectorT1T1_0x201660(ram, ctx, rt); return; }
            const auto result = planeDistances(vector(rt, getRegU32(ctx, 4)), vector(rt, getRegU32(ctx, 5)), vector(rt, getRegU32(ctx, 6)));
            auto *out = const_cast<uint8_t *>(span(rt, getRegU32(ctx, 7), 16));
            if (check())
            {
                math::Vector saved, expected;
                std::memcpy(&saved, out, 16);
                R5900Context original = *ctx;
                bspPlaneTest2Vec__FP6_planeP8_fvectorT1T1_0x201660(ram, &original, rt);
                std::memcpy(&expected, out, 16); std::memcpy(out, &saved, 16);
                if (std::memcmp(&result, &expected, 8)) throw std::runtime_error("native hierarchy plane distances differ from original");
                ++statistics.checked;
            }
            std::memcpy(out, &result, 8); // Z/W were inherited registers; the BSP caller never reads them
            ++statistics.planes;
            ctx->pc = getRegU32(ctx, 31);
        }
    }
    void install(Registry &runtime)
    {
        runtime.registerFunction(kHeightSphere, &heightSphereOverride);
        runtime.registerFunction(kInverseDirection, &inverseDirectionOverride);
        runtime.registerFunction(kPlaneDistances, &planeDistancesOverride);
    }
}
