// SPDX-License-Identifier: MIT
// USA matrix helper ABI: a0 output, a1 first matrix, a2 second matrix. Inputs are ordinary game records.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "bozo/libs/libraries.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace tmb::math
{
    namespace
    {
        constexpr uint32_t kTransposedProduct = 0x001dcc68u, kFullProduct = 0x001dcd50u;
        struct Statistics
        {
            uint64_t calls[2]{}, checked[2]{};
            uint64_t mismatches = 0;
            ~Statistics()
            {
                if (calls[0] || calls[1])
                    std::fprintf(stderr, "[native:math] transposed %llu, full %llu calls; original comparisons %llu/%llu, mismatches %llu\n",
                        static_cast<unsigned long long>(calls[0]), static_cast<unsigned long long>(calls[1]),
                        static_cast<unsigned long long>(checked[0]), static_cast<unsigned long long>(checked[1]),
                        static_cast<unsigned long long>(mismatches));
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
            std::memcpy(&first, span(rt, getRegU32(ctx, 5), inputBytes), inputBytes);
            std::memcpy(&second, span(rt, getRegU32(ctx, 6), inputBytes), inputBytes);
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
    }

    void install(Registry &runtime)
    {
        runtime.registerFunction(kTransposedProduct, &invoke<Product::TransposedBasis, vu0MulMatrixTP3x3__FPA3_fT0T0_0x1dcc68>);
        runtime.registerFunction(kFullProduct, &invoke<Product::Full, vu0MulMatrix4x4__FPA3_fT0T0_0x1dcd50>);
    }
}
