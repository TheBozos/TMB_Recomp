// SPDX-License-Identifier: MIT
// Inline terrain/contact transforms, sums and differences over game records. Original bodies are diagnostics only.
#include "tmb_contact_math.h"
#include "tmb_native_math.h"
#include "tmb_contact_original.h"
#include "bozo/libs/libraries.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tmb::math
{
    namespace
    {
        // bozo:native kContactVectors
        constexpr uint32_t kContactVectors[] = {
            0x1cb1ac, 0x1cb1b0, 0x1cb1b4, 0x1cb1b8,
            0x1cb1f0, 0x1cb1f4, 0x1cb1f8, 0x1cb1fc,
            0x1cb438, 0x1cb43c, 0x1cb440, 0x1cb444, 0x1cb448, 0x1cb44c, 0x1cb450, 0x1cb454,
            0x1cb468, 0x1cb46c, 0x1cb470, 0x1cb474,
            0x1cb4a8, 0x1cb4ac, 0x1cb4b0, 0x1cb4b4,
            0x1cb4d8, 0x1cb4dc, 0x1cb4e0, 0x1cb4e4, 0x1cb4e8, 0x1cb4ec, 0x1cb4f0, 0x1cb4f4,
            0x1cb508, 0x1cb50c, 0x1cb510, 0x1cb514,
            0x1cb534, 0x1cb538, 0x1cb53c, 0x1cb540,
            0x1cb708, 0x1cb70c, 0x1cb710, 0x1cb714, 0x1cb718, 0x1cb71c, 0x1cb720, 0x1cb724,
            0x1cb734, 0x1cb738, 0x1cb73c, 0x1cb740,
            0x1cb77c, 0x1cb780, 0x1cb784, 0x1cb788,
            0x1cb7d0, 0x1cb7d4, 0x1cb7d8, 0x1cb7dc, 0x1cb7e0, 0x1cb7e4, 0x1cb7e8, 0x1cb7ec,
            0x1cb7fc, 0x1cb800, 0x1cb804, 0x1cb808,
            0x1cb80c, 0x1cb810, 0x1cb814, 0x1cb818,
            0x1cb8dc, 0x1cb8e0, 0x1cb8e4, 0x1cb8e8,
            0x1cb8f8, 0x1cb8fc, 0x1cb900, 0x1cb904,
            0x1cc814, 0x1cc818, 0x1cc81c, 0x1cc820, 0x1cc824, 0x1cc828, 0x1cc82c, 0x1cc830, 0x1cc834, 0x1cc838,
            0x1cca40, 0x1cca44, 0x1cca48, 0x1cca4c, 0x1cca50, 0x1cca54, 0x1cca58, 0x1cca5c, 0x1cca60, 0x1cca64,
            0x1cca6c, 0x1cca70, 0x1cca74, 0x1cca78, 0x1cca7c, 0x1cca80, 0x1cca84, 0x1cca88, 0x1cca8c, 0x1cca90,
            0x1cbd48, 0x1cbd4c, 0x1cbd50, 0x1cbd54,
            0x1cbeb0, 0x1cbeb4, 0x1cbeb8, 0x1cbebc,
            0x1cbec0, 0x1cbec4, 0x1cbec8, 0x1cbecc,
            0x1cbef0, 0x1cbef4, 0x1cbef8, 0x1cbefc,
            0x1cc038, 0x1cc03c, 0x1cc040, 0x1cc044, 0x1cc048, 0x1cc04c, 0x1cc050, 0x1cc054,
            0x1cc060, 0x1cc064, 0x1cc068, 0x1cc06c,
            0x1cc074, 0x1cc078, 0x1cc07c, 0x1cc080, 0x1cc084, 0x1cc088, 0x1cc08c, 0x1cc090,
            0x1cc09c, 0x1cc0a0, 0x1cc0a4, 0x1cc0a8,
            0x1c6714, 0x1c6718, 0x1c671c, 0x1c6720,
            0x1c69bc, 0x1c69c0, 0x1c69c4, 0x1c69c8,
            0x1cadc4, 0x1cadc8, 0x1cadcc, 0x1cadd0,
        };
        struct Statistics
        {
            uint64_t calls[contactVectors.size()]{}, checked[contactVectors.size()]{};
            ~Statistics()
            {
                for (unsigned i = 0; i < contactVectors.size(); ++i)
                    if (calls[i]) std::fprintf(stderr, "[native:contact] %06x: %llu transforms, %llu original XYZ comparisons\n",
                        contactVectors[i].last, static_cast<unsigned long long>(calls[i]), static_cast<unsigned long long>(checked[i]));
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

    bool contactInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt)) return false;
        for (unsigned i = 0; i < contactVectors.size(); ++i)
        {
            const auto &site = contactVectors[i];
            if (pc < site.first || pc > site.last || (pc & 3u)) continue;
            if (pc != site.last) return true; // no register loads, arithmetic, ACC or flag transport
            Matrix basis{};
            if (site.basis)
            {
                const uint8_t *columns = span(rt, getRegU32(ctx, site.basis), 48);
                for (unsigned j = 0; j < 3; ++j) std::memcpy(basis.column[j].bits.data(), columns + 16u*j, 16);
            }
            Vector input, translation;
            std::memcpy(input.bits.data(), span(rt, getRegU32(ctx, site.input), 16), 16);
            if (site.translation) std::memcpy(translation.bits.data(), span(rt, getRegU32(ctx, site.translation), 16), 16);
            uint8_t *out = span(rt, getRegU32(ctx, site.output), 16);
            // Snapshot this block's current records before its write; earlier blocks may have changed an aliased input.
            const Vector result = site.operation == ContactOperation::Transform
                ? transformXYZ(basis, input, site.translation ? &translation : nullptr)
                : combineXYZ(input, translation, site.operation == ContactOperation::Subtract);
            static const bool check = [] { const char *v = std::getenv("TMB_NATIVE_CONTACT_CHECK"); return v && *v == '1'; }();
            if (check)
            {
                Vector saved, expected;
                std::memcpy(saved.bits.data(), out, 16);
                R5900Context original = *ctx;
                tmb_contact_originals[i](ram, &original, rt); // actual generated instruction bodies; no calls/checkpoints
                std::memcpy(expected.bits.data(), out, 16);
                std::memcpy(out, saved.bits.data(), 16);
                if (std::memcmp(result.bits.data(), expected.bits.data(), 12))
                    throw std::runtime_error("native contact XYZ differs from original block");
                ++statistics.checked[i];
            }
            std::memcpy(out, result.bits.data(), 12); // leave the destination's opaque fourth component untouched
            ++statistics.calls[i];
            return true;
        }
        return false;
    }
}
