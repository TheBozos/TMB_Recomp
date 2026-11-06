// SPDX-License-Identifier: MIT
// Inline HUD point/direction transforms over game records. Original instruction bodies are diagnostics only.
#include "tmb_hud_math.h"
#include "tmb_native_math.h"
#include "tmb_hud_original.h"
#include "bozo/libs/libraries.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tmb::math
{
    namespace
    {
        // bozo:native kHudTransforms
        constexpr uint32_t kHudTransforms[] = {
            0x13ce74, 0x13ce78, 0x13ce7c, 0x13ce80, 0x13ce84, 0x13ce88, 0x13ce8c, 0x13ce90, 0x13ce94, 0x13ce98,
            0x13ceb4, 0x13ceb8, 0x13cebc, 0x13cec0, 0x13cec4, 0x13cec8, 0x13cecc, 0x13ced0, 0x13ced4, 0x13ced8,
            0x13d03c, 0x13d040, 0x13d044, 0x13d048, 0x13d04c, 0x13d050, 0x13d054, 0x13d058, 0x13d05c, 0x13d060,
            0x13d210, 0x13d214, 0x13d218, 0x13d21c, 0x13d220, 0x13d224, 0x13d228, 0x13d22c,
            0x13d308, 0x13d30c, 0x13d310, 0x13d314, 0x13d318, 0x13d31c, 0x13d320, 0x13d324,
            0x13d408, 0x13d40c, 0x13d410, 0x13d414, 0x13d418, 0x13d41c, 0x13d420, 0x13d424,
        };
        struct Statistics
        {
            uint64_t calls[6]{}, checked[6]{};
            ~Statistics()
            {
                for (unsigned i = 0; i < hudTransforms.size(); ++i)
                    if (calls[i]) std::fprintf(stderr, "[native:hud] %06x: %llu transforms, %llu original XYZ comparisons\n",
                        hudTransforms[i].last, static_cast<unsigned long long>(calls[i]), static_cast<unsigned long long>(checked[i]));
            }
        } statistics;

        uint8_t *span(Runtime *rt, uint32_t address, uint32_t bytes)
        {
            address &= ~15u;
            const int64_t offset = bozo::GuestMemory::flatOffset(address, bytes);
            if (offset < 0 || uint64_t(offset) + bytes > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address, bytes};
            return rt->memory.flat() + offset;
        }
    }

    bool hudInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt)) return false;
        for (unsigned i = 0; i < hudTransforms.size(); ++i)
        {
            const auto &site = hudTransforms[i];
            if (pc < site.first || pc > site.last || (pc & 3u)) continue;
            if (pc != site.last) return true; // no register loads, arithmetic, ACC or flag transport
            Matrix basis{};
            const uint8_t *columns = span(rt, getRegU32(ctx, site.basis), 48);
            for (unsigned j = 0; j < 3; ++j) std::memcpy(basis.column[j].bits.data(), columns + 16u*j, 16);
            Vector input, translation;
            std::memcpy(input.bits.data(), span(rt, getRegU32(ctx, site.input), 16), 16);
            if (site.translation) std::memcpy(translation.bits.data(), span(rt, getRegU32(ctx, site.translation), 16), 16);
            uint8_t *out = span(rt, getRegU32(ctx, site.output), 16);
            // Snapshot this block's current records before its write; earlier blocks may have changed an aliased input.
            const Vector result = transformXYZ(basis, input, site.translation ? &translation : nullptr);
            static const bool check = [] { const char *v = std::getenv("TMB_NATIVE_HUD_CHECK"); return v && *v == '1'; }();
            if (check)
            {
                Vector saved, expected;
                std::memcpy(saved.bits.data(), out, 16);
                R5900Context original = *ctx;
                tmb_hud_originals[i](ram, &original, rt); // actual generated instruction bodies; no calls/checkpoints
                std::memcpy(expected.bits.data(), out, 16);
                std::memcpy(out, saved.bits.data(), 16);
                if (std::memcmp(result.bits.data(), expected.bits.data(), 12))
                    throw std::runtime_error("native HUD XYZ differs from original block");
                ++statistics.checked[i];
            }
            std::memcpy(out, result.bits.data(), 12); // leave the destination's opaque fourth component untouched
            ++statistics.calls[i];
            return true;
        }
        return false;
    }
}
