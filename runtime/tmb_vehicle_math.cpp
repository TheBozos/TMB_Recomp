// SPDX-License-Identifier: MIT
// Inline vehicle transforms, sums and differences over game records. Original bodies are diagnostics only.
#include "tmb_vehicle_math.h"
#include "tmb_native_math.h"
#include "tmb_vehicle_original.h"
#include "bozo/libs/libraries.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tmb::math
{
    namespace
    {
        // bozo:native kVehicleVectors
        constexpr uint32_t kVehicleVectors[] = {
            0x1aada8, 0x1aadac, 0x1aadb0, 0x1aadb4, 0x1aadb8, 0x1aadbc, 0x1aadc0, 0x1aadc4,
            0x1aadd0, 0x1aadd4, 0x1aadd8, 0x1aaddc,
            0x1aae38, 0x1aae3c, 0x1aae40, 0x1aae44,
            0x1aaf70, 0x1aaf74, 0x1aaf78, 0x1aaf7c, 0x1aaf80, 0x1aaf84, 0x1aaf88, 0x1aaf8c,
            0x1aaf94, 0x1aaf98, 0x1aaf9c, 0x1aafa0,
            0x1aafa8, 0x1aafac, 0x1aafb0, 0x1aafb4,
            0x1aafb8, 0x1aafbc, 0x1aafc0, 0x1aafc4,
            0x1aafdc, 0x1aafe0, 0x1aafe4, 0x1aafe8, 0x1aafec, 0x1aaff0, 0x1aaff4, 0x1aaff8,
            0x1aaffc, 0x1ab000, 0x1ab004, 0x1ab008,
            0x1ab010, 0x1ab014, 0x1ab018, 0x1ab01c,
            0x1ab124, 0x1ab128, 0x1ab12c, 0x1ab130, 0x1ab134, 0x1ab138, 0x1ab13c, 0x1ab140,
            0x1ab144, 0x1ab148, 0x1ab14c, 0x1ab150,
            0x1ab29c, 0x1ab2a0, 0x1ab2a4, 0x1ab2a8,
        };
        struct Statistics
        {
            uint64_t calls[13]{}, checked[13]{};
            ~Statistics()
            {
                for (unsigned i = 0; i < vehicleVectors.size(); ++i)
                    if (calls[i]) std::fprintf(stderr, "[native:vehicle] %06x: %llu transforms, %llu original XYZ comparisons\n",
                        vehicleVectors[i].last, static_cast<unsigned long long>(calls[i]), static_cast<unsigned long long>(checked[i]));
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

    bool vehicleInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt)) return false;
        for (unsigned i = 0; i < vehicleVectors.size(); ++i)
        {
            const auto &site = vehicleVectors[i];
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
            const Vector result = site.operation == VehicleOperation::Transform
                ? transformXYZ(basis, input, nullptr)
                : combineXYZ(input, translation, site.operation == VehicleOperation::Subtract);
            static const bool check = [] { const char *v = std::getenv("TMB_NATIVE_VEHICLE_CHECK"); return v && *v == '1'; }();
            if (check)
            {
                Vector saved, expected;
                std::memcpy(saved.bits.data(), out, 16);
                R5900Context original = *ctx;
                tmb_vehicle_originals[i](ram, &original, rt); // actual generated instruction bodies; no calls/checkpoints
                std::memcpy(expected.bits.data(), out, 16);
                std::memcpy(out, saved.bits.data(), 16);
                if (std::memcmp(result.bits.data(), expected.bits.data(), 12))
                    throw std::runtime_error("native vehicle XYZ differs from original block");
                ++statistics.checked[i];
            }
            std::memcpy(out, result.bits.data(), 12); // leave the destination's opaque fourth component untouched
            ++statistics.calls[i];
            return true;
        }
        return false;
    }
}
