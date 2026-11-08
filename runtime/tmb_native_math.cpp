// SPDX-License-Identifier: MIT
// USA matrix helper ABI: a0 output, a1 first matrix, a2 second matrix. Inputs are ordinary game records.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "bozo/libs/libraries.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace tmb::math
{
    void installProjection(Registry &);
    namespace
    {
        constexpr uint32_t kTransposedProduct = 0x001dcc68u, kFullProduct = 0x001dcd50u;
        constexpr uint32_t kBasisProduct = 0x001dcc08u;
        constexpr uint32_t kDirectionProduct = 0x001da298u;
        // bozo:native kCameraPosition
        constexpr uint32_t kCameraPosition[] = {0x11399c, 0x1139a0, 0x1139a4, 0x1139a8};
        struct Statistics
        {
            uint64_t calls[3]{}, checked[3]{};
            uint64_t directions = 0, directionsChecked = 0;
            uint64_t mismatches = 0;
            std::map<uint32_t, uint64_t> directionCallers;
            ~Statistics()
            {
                if (calls[0] || calls[1] || calls[2])
                    std::fprintf(stderr, "[native:math] transposed %llu, full %llu, basis %llu calls; original comparisons %llu/%llu/%llu, mismatches %llu\n",
                        static_cast<unsigned long long>(calls[0]), static_cast<unsigned long long>(calls[1]),
                        static_cast<unsigned long long>(calls[2]),
                        static_cast<unsigned long long>(checked[0]), static_cast<unsigned long long>(checked[1]),
                        static_cast<unsigned long long>(checked[2]),
                        static_cast<unsigned long long>(mismatches));
                if (directions)
                    std::fprintf(stderr, "[native:math] direction %llu calls; original XYZ comparisons %llu\n",
                        static_cast<unsigned long long>(directions), static_cast<unsigned long long>(directionsChecked));
                for (const auto &[caller, count] : directionCallers)
                    std::fprintf(stderr, "[native:direction-trace] return %x calls %llu\n",
                        caller, static_cast<unsigned long long>(count));
            }
        } statistics;

        uint8_t *span(Runtime *rt, uint32_t address, uint32_t bytes)
        {
            address &= ~15u; // the original quadword loads/stores ignore the address's low four bits
            const int64_t offset = bozo::GuestMemory::flatOffset(address, bytes);
            if (offset < 0 || uint64_t(offset) + bytes > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address, bytes};
            return rt->memory.flat() + offset;
        }

        template <Product Kind, RecompiledFunction Original>
        void invoke(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt)) // the historical checker retains the original context semantics
            {
                Original(ram, ctx, rt);
                return;
            }
            constexpr unsigned index = static_cast<unsigned>(Kind);
            constexpr uint32_t inputBytes = Kind == Product::Full ? 64u : 48u;
            Matrix first{}, second{};
            // Take both inputs before writing anything: identical and partially overlapping matrices are valid.
            const uint8_t *firstData = span(rt, getRegU32(ctx, 5), inputBytes);
            const uint8_t *secondData = span(rt, getRegU32(ctx, 6), inputBytes);
            for (unsigned j = 0; j < inputBytes / sizeof(Vector); ++j)
            {
                std::memcpy(first.column[j].bits.data(), firstData + 16u*j, 16);
                std::memcpy(second.column[j].bits.data(), secondData + 16u*j, 16);
            }
            uint8_t *out = span(rt, getRegU32(ctx, 4), sizeof(Matrix));
            const Matrix result = product(first, second, Kind);
            static const bool check = [] { const char *v = std::getenv("TMB_NATIVE_MATH_CHECK"); return v && *v == '1'; }();
            if (check)
            {
                Matrix saved, expected;
                std::memcpy(&saved, out, sizeof(saved));
                R5900Context original = *ctx;
                Original(ram, &original, rt); // these leaf helpers do no runtime/device/timing calls
                std::memcpy(&expected, out, sizeof(expected));
                std::memcpy(out, &saved, sizeof(saved));
                if (std::memcmp(&result, &expected, sizeof(result)) != 0)
                {
                    ++statistics.mismatches;
                    throw std::runtime_error("native matrix product differs from original helper " + std::to_string(index));
                }
                ++statistics.checked[index];
            }
            std::memcpy(out, &result, sizeof(result));
            ++statistics.calls[index];
            ctx->pc = getRegU32(ctx, 31);
        }

        void direction(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            static const bool trace = [] { const char *v = std::getenv("TMB_NATIVE_DIRECTION_TRACE"); return v && *v == '1'; }();
            const uint32_t caller = getRegU32(ctx, 31);
            if (trace) ++statistics.directionCallers[caller];
            // These callers consume XYZ only. Other consumers still need their W/flag contract audited.
            if (!bozo::port::nativeGraphics(rt) ||
                std::find(directionReturns.begin(), directionReturns.end(), caller) == directionReturns.end())
            {
                mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298(ram, ctx, rt);
                return;
            }
            Matrix basis{};
            const auto *columns = span(rt, getRegU32(ctx, 4), 48);
            for (unsigned j = 0; j < 3; ++j) std::memcpy(basis.column[j].bits.data(), columns + 16u*j, 16);
            Vector input;
            std::memcpy(input.bits.data(), span(rt, getRegU32(ctx, 5), 16), 16);
            auto *out = span(rt, getRegU32(ctx, 6), 16);
            const Vector result = transformXYZ(basis, input, nullptr);
            static const bool check = [] { const char *v = std::getenv("TMB_NATIVE_MATH_CHECK"); return v && *v == '1'; }();
            if (check)
            {
                Vector saved, expected;
                std::memcpy(saved.bits.data(), out, 16);
                R5900Context original = *ctx;
                mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298(ram, &original, rt);
                std::memcpy(expected.bits.data(), out, 16);
                std::memcpy(out, saved.bits.data(), 16);
                if (std::memcmp(result.bits.data(), expected.bits.data(), 12))
                    throw std::runtime_error("native direction differs from original XYZ");
                ++statistics.directionsChecked;
            }
            std::memcpy(out, result.bits.data(), 12);
            ++statistics.directions;
            ctx->pc = getRegU32(ctx, 31);
        }
    }

    bool cameraInstruction(uint8_t *, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt) ||
            std::find(std::begin(kCameraPosition), std::end(kCameraPosition), pc) == std::end(kCameraPosition)) return false;
        if (pc != 0x1139a8) return true;
        Vector position, offset, basisTail;
        std::memcpy(&position, span(rt, getRegU32(ctx, 2), 16), 16);
        std::memcpy(&offset, span(rt, getRegU32(ctx, 16), 16), 16);
        // The immediately preceding transform loads this basis column into VF13. Its W survives VADD.xyz.
        // Publish that record field directly, without retaining any register state between instructions.
        std::memcpy(&basisTail, span(rt, getRegU32(ctx, 17) + 32, 16), 16);
        auto result = combineXYZ(position, offset, false);
        result.bits[3] = basisTail.bits[3];
        std::memcpy(span(rt, getRegU32(ctx, 5), 16), &result, 16);
        return true;
    }

    void install(Registry &runtime)
    {
        installProjection(runtime);
        runtime.registerFunction(kTransposedProduct, &invoke<Product::TransposedBasis, vu0MulMatrixTP3x3__FPA3_fT0T0_0x1dcc68>);
        runtime.registerFunction(kFullProduct, &invoke<Product::Full, vu0MulMatrix4x4__FPA3_fT0T0_0x1dcd50>);
        runtime.registerFunction(kBasisProduct, &invoke<Product::Basis, vu0MulMatrix3x3__FPA3_fT0T0_0x1dcc08>);
        runtime.registerFunction(kDirectionProduct, &direction);
    }
}
