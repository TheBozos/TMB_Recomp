// Exact, independent-original comparisons: record values, overlapping memory, the complete CPU ABI and untouched VU state.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "bozo/libs/libraries.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>

namespace tmb::math { void install(Registry &); }
namespace
{
    uint32_t randomState = 0x1928d3a7u;
    uint32_t randomBits()
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }
    struct NativeHost : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::libs::Libraries &libraries() override { return libs; }
        void frameEnd(Runtime *, R5900Context *) override { throw "unexpected frame operation"; }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw "unexpected library operation"; }
        bool nativeGraphics() const override { return true; }
    };
    struct Helper { uint32_t address; RecompiledFunction original; const char *name; };
    constexpr Helper helpers[] = {
        {0x1dcc68, vu0MulMatrixTP3x3__FPA3_fT0T0_0x1dcc68, "transposed basis"},
        {0x1dcd50, vu0MulMatrix4x4__FPA3_fT0T0_0x1dcd50, "full"},
    };
    constexpr uint32_t special[] = {0, 0x80000000, 1, 0x80000001, 0x7fffff, 0x807fffff,
        0x800000, 0x80800000, 0x800001, 0x3f7fffff, 0x3f800000, 0x3f800001, 0xbf800000,
        0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000, 0x7fffffff, 0xffffffff, 0x3f000000};
}

int main()
{
    auto rt = std::make_unique<Runtime>();
    NativeHost host;
    rt->port = &host;
    tmb::Registry registry;
    tmb::math::install(registry);
    constexpr size_t arenaBytes = 512;
    constexpr uint32_t arenaBase = 0x10000;
    std::array<uint8_t, arenaBytes> input{}, expected{};
    auto wholeRam = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    uint64_t cases = 0, fullMemoryCases = 0;
    const unsigned savedControl = _mm_getcsr();
    for (unsigned mode = 0; mode < 4; ++mode)
        for (const Helper &helper : helpers)
            for (uint32_t n = 0; n < 65536; ++n)
            {
                // Finite game-like values, special/underflow/overflow cases, and arbitrary 32-bit input records.
                for (size_t i = 0; i < input.size(); i += 4)
                {
                    uint32_t value = randomBits();
                    if (n < 16384)
                        value = (value & 0x807fffffu) | ((110u + (value % 30u)) << 23);
                    else if (n < 32768)
                        value = special[value % std::size(special)];
                    std::memcpy(input.data() + i, &value, 4);
                }
                const uint32_t physicalBase = n % 7u == 0 ? bozo::kScratchpadBase : arenaBase;
                const int64_t offset = bozo::GuestMemory::flatOffset(physicalBase, arenaBytes);
                uint8_t *arena = rt->memory.flat() + offset;
                std::memcpy(arena, input.data(), input.size());
                R5900Context initial;
                for (unsigned i = 1; i < 32; ++i)
                    for (unsigned lane = 0; lane < 4; ++lane)
                        initial.r[i].uw[lane] = randomBits();
                for (auto &v : initial.vu0_vf)
                    for (auto &bits : v.u) bits = randomBits();
                initial.vu0_vf[0] = Vu0Vector{};
                initial.vu0_vf[0].u[3] = 0x3f800000u;
                initial.vu0_status = randomBits(); initial.vu0_mac = randomBits();
                initial.vu0_acc.u[0] = randomBits(); initial.vi[5] = randomBits();
                const uint32_t first = physicalBase + 128u;
                const uint32_t second = physicalBase + (n % 11u == 0 ? 144u : 256u);
                const uint32_t output = physicalBase + 64u + (n % 21u) * 16u;
                const uint32_t alias = physicalBase == arenaBase ? (n % 3u == 0 ? 0x80000000u : n % 3u == 1 ? 0x20000000u : 0u) : 0u;
                setRegU32(&initial, 4, (output | alias) + (n & 15u));
                setRegU32(&initial, 5, (first | alias) + ((n >> 4u) & 15u));
                setRegU32(&initial, 6, (second | alias) + ((n >> 8u) & 15u));
                setRegU32(&initial, 31, 0x0bad0000u);
                initial.pc = helper.address;
                R5900Context original = initial, native = initial;
                // Deliberately vary caller rounding and denormal settings. Native code must restore the whole word.
                const unsigned control = ((savedControl | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1u ? 0x8040u : 0u);
                _mm_setcsr(control);
                helper.original(rt->memory.flat(), &original, rt.get());
                std::memcpy(expected.data(), arena, expected.size());
                if (n < 32)
                {
                    std::memcpy(wholeRam.get(), rt->memory.flat(), bozo::kFlatBytes);
                    ++fullMemoryCases;
                }
                std::memcpy(arena, input.data(), input.size());
                registry.find(helper.address)(rt->memory.flat(), &native, rt.get());
                const unsigned after = _mm_getcsr();
                _mm_setcsr(savedControl);
                const size_t vuAt = offsetof(R5900Context, vu0_vf), vuBytes = offsetof(R5900Context, cycles) - vuAt;
                const bool untouched = std::memcmp(reinterpret_cast<uint8_t *>(&initial) + vuAt,
                                                   reinterpret_cast<uint8_t *>(&native) + vuAt, vuBytes) == 0;
                std::memset(reinterpret_cast<uint8_t *>(&original) + vuAt, 0, vuBytes);
                std::memset(reinterpret_cast<uint8_t *>(&native) + vuAt, 0, vuBytes);
                if (std::memcmp(arena, expected.data(), expected.size()) || std::memcmp(&original, &native, sizeof(native)) ||
                    !untouched || after != control || (n < 32 && std::memcmp(rt->memory.flat(), wholeRam.get(), bozo::kFlatBytes)))
                {
                    std::fprintf(stderr, "FAIL %s case %u host mode %u: data %d, CPU %d, untouched VU %d, host control %d\n",
                        helper.name, n, mode, std::memcmp(arena, expected.data(), expected.size()),
                        std::memcmp(&original, &native, sizeof(native)), untouched, after == control);
                    for (size_t i = 0; i < arenaBytes; i += 4)
                        if (std::memcmp(arena + i, expected.data() + i, 4))
                        {
                            uint32_t got, want; std::memcpy(&got, arena + i, 4); std::memcpy(&want, expected.data() + i, 4);
                            std::fprintf(stderr, "first differing record word +%zu: %#x != %#x\n", i, got, want); break;
                        }
                    return 1;
                }
                ++cases;
            }
    std::printf("native matrix tests: %llu original/native cases exact, %llu whole-memory cases; 4 host modes; VU state untouched\n",
        static_cast<unsigned long long>(cases), static_cast<unsigned long long>(fullMemoryCases));
    return 0;
}
