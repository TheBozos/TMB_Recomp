// SPDX-License-Identifier: MIT
// The traversal's texture consumer reads game records directly; no packet image or address-keyed copy is kept.
#include "tmb_texture_inputs.h"
#include "bozo/libs/libraries.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace tmb::textureInputs
{
    namespace
    {
        // bozo:native kTextureInputs
        constexpr uint32_t kTextureInputs[] = {
            0x1f90cc, 0x1f9130, 0x1f9158, 0x1f91b4,
            0x1f93bc, 0x1f93c4, 0x1f93d4, 0x1f9494, 0x1f94a0, 0x1f988c,
        };
        static_assert(std::is_sorted(std::begin(kTextureInputs), std::end(kTextureInputs)));
        template <typename T> T read(Runtime *rt, uint32_t address)
        {
            const auto at = bozo::GuestMemory::flatOffset(address, sizeof(T));
            if (at < 0 || uint64_t(at) + sizeof(T) > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address, sizeof(T)};
            T value;
            std::memcpy(&value, rt->memory.flat() + at, sizeof(value));
            return value;
        }
        struct Statistics
        {
            uint64_t reads{}, patches{};
            ~Statistics()
            {
                std::fprintf(stderr, "[native:texture-inputs] direct reads %llu retired patches %llu\n",
                             (unsigned long long)reads, (unsigned long long)patches);
            }
        } statistics;

        // Both assembly traversals enqueue {object, texture-copy cursor, ...}. The helper preserves the
        // texture index at frame+0. Read the caller from its actual saved frame, so a deferred return/resume
        // needs no scoped binding, persistent packet bank or thread-local context.
        bool source(Runtime *rt, const R5900Context *ctx, uint32_t pc, uint32_t &address)
        {
            const uint32_t sp = getRegU32(ctx, 29);
            uint32_t frame = sp, queueOffset;
            const bool activation = pc >= 0x1f93bc && pc <= 0x1f94a0;
            const bool mip = pc == 0x1f988c;
            if (activation)
            {
                if (read<uint64_t>(rt, sp + 0x50) != 0x1f9100) return false;
                frame += 0x60;
            }
            else if (mip)
            {
                if (read<uint64_t>(rt, sp + 0xa0) != 0x1f9100) return false;
                frame += 0xb0;
            }
            const uint64_t caller = read<uint64_t>(rt, frame + 0x20);
            unsigned queueRegister;
            if (caller == 0x1d330c || caller == 0x1d3340) queueRegister = 18;
            else if (caller == 0x1d2d84 || caller == 0x1d2db8) queueRegister = 19;
            else return false; // C builders and other callers retain their complete original ABI.
            if (activation) queueOffset = read<uint32_t>(rt, sp + (queueRegister == 18 ? 0x20 : 0x30));
            else if (mip) queueOffset = read<uint32_t>(rt, sp + (queueRegister == 18 ? 0x30 : 0x40));
            else queueOffset = getRegU32(ctx, queueRegister);
            const uint32_t index = read<uint32_t>(rt, frame);
            const uint32_t object = read<uint32_t>(rt, 0x4c0a78 + queueOffset);
            // pktAddVu1ObjAsm copied each source qword with an aligned 128-bit load.
            address = (object + 0x70 + 48 * index) & ~15u;
            return true;
        }
    }
    bool owns(uint32_t pc)
    {
        return std::binary_search(std::begin(kTextureInputs), std::end(kTextureInputs), pc);
    }
    bool instruction(uint8_t *, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt) || !owns(pc)) return false;
        uint32_t address;
        if (!source(rt, ctx, pc, address)) return false;
        if (pc == 0x1f90cc || pc == 0x1f9130)
        {
            setRegU64(ctx, pc == 0x1f90cc ? 3 : 5, read<uint64_t>(rt, address + (pc == 0x1f90cc ? 0 : 16)));
            ++statistics.reads;
        }
        else if (pc == 0x1f93bc || pc == 0x1f9494)
        {
            setRegU64(ctx, 2, read<uint16_t>(rt, address));
            ++statistics.reads;
        }
        else ++statistics.patches; // Scalar results and game texture-manager writes still execute normally.
        return true;
    }
}
