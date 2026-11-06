// SPDX-License-Identifier: MIT
// Retire per-object shader packet output after the native mesh path takes its inputs from game records.
#include "tmb_port.h"
#include "tmb_object_packets.h"
#include "bozo/libs/libraries.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace tmb::packets
{
    namespace
    {
        // Output only. The traversal's texture consumer now reads the source object directly.
        // Keep cursor control+8 and scratchpad cache+94 bookkeeping.
        // bozo:native kObjectPacketOutput
        constexpr uint32_t kObjectPacketOutput[] = {
            0x1eaafc, 0x1eab08, 0x1eab44, 0x1eab48, 0x1eab74, 0x1eabb8, 0x1eabd0, 0x1eabd4,
            0x1eabd8, 0x1eabf4, 0x1eac2c, 0x1eac30, 0x1eac40, 0x1eac48, 0x1eac54, 0x1eac80,
            0x1eaca4, 0x1eacb4, 0x1eace4, 0x1eacec, 0x1ead0c, 0x1ead24, 0x1ead3c, 0x1ead54,
            0x1ead94, 0x1ead98, 0x1ead9c, 0x1eadc4, 0x1eae14, 0x1eae18, 0x1eae1c, 0x1eae20,
            0x1eae24, 0x1eae28, 0x1eae54, 0x1eaea0, 0x1eaea8, 0x1eaed0, 0x1eaf00, 0x1eaf68, 0x1eaf78,
        };
        // The original car-tint calculation read its three RGB inputs back from the output packet.
        // bozo:native kObjectTintReads
        constexpr uint32_t kObjectTintReads[] = {0x1ead74, 0x1ead7c, 0x1ead8c};
        static_assert(std::is_sorted(std::begin(kObjectPacketOutput), std::end(kObjectPacketOutput)));
        static_assert(std::is_sorted(std::begin(kObjectTintReads), std::end(kObjectTintReads)));
        struct Statistics
        {
            uint64_t stores{}, tintReads{};
            ~Statistics()
            {
                std::fprintf(stderr, "[native:object-packets] retired stores %llu direct tint reads %llu\n",
                             (unsigned long long)stores, (unsigned long long)tintReads);
            }
        } statistics;
        template <typename T> T read(Runtime *rt, uint32_t address)
        {
            const auto at = bozo::GuestMemory::flatOffset(address, sizeof(T));
            if (at < 0 || uint64_t(at) + sizeof(T) > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address, sizeof(T)};
            T value;
            std::memcpy(&value, rt->memory.flat() + at, sizeof(value));
            return value;
        }
    }
    bool owns(uint32_t pc)
    {
        return std::binary_search(std::begin(kObjectPacketOutput), std::end(kObjectPacketOutput), pc) ||
               std::binary_search(std::begin(kObjectTintReads), std::end(kObjectTintReads), pc);
    }
    bool instruction(uint8_t *, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!bozo::port::nativeGraphics(rt)) return false;
        if (std::binary_search(std::begin(kObjectPacketOutput), std::end(kObjectPacketOutput), pc))
        {
            ++statistics.stores;
            return true;
        }
        if (!std::binary_search(std::begin(kObjectTintReads), std::end(kObjectTintReads), pc)) return false;
        const unsigned component = pc == 0x1ead74 ? 0 : pc == 0x1ead7c ? 1 : 2;
        const uint32_t object = getRegU32(ctx, 21), instance = getRegU32(ctx, 23);
        // The object's base colour was copied by an aligned 128-bit source load.
        uint32_t address = ((object + 0x40) & ~15u) + 4 * component;
        if (instance)
        {
            const auto index = read<int16_t>(rt, object + 0x34 + 2 * component);
            if (index >= 0) address = read<uint32_t>(rt, instance + 4) + 4 * uint32_t(index);
        }
        ctx->fu[component == 0 ? 1 : 3] = read<uint32_t>(rt, address);
        ++statistics.tintReads;
        return true;
    }
}
