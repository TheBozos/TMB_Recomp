// SPDX-License-Identifier: MIT
// Compare actual generated original blocks against the production instruction dispatcher, including ordered aliases.
#include "tmb_hud_math.h"
#include "tmb_native_math.h"
#include "tmb_hud_original.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>

namespace tmb::graphics { void install(Registry &); }
namespace
{
    uint32_t randomState = 0x78ac2139;
    uint32_t randomBits()
    {
        randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
        return randomState;
    }
    struct Host : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bool enabled = true;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return enabled; }
        void frameEnd(Runtime *, R5900Context *) override { throw "unexpected frame end"; }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw "unexpected library call"; }
    };
    constexpr size_t arenaBytes = 512;
    constexpr uint32_t special[] = {0, 0x80000000, 1, 0x80000001, 0x7fffff, 0x807fffff,
        0x800000, 0x80800000, 0x3f800000, 0xbf800000, 0x7f7fffff, 0xff7fffff,
        0x7f800000, 0xff800000, 0x7fffffff, 0xffffffff};

    void originalBlock(unsigned index, uint8_t *ram, R5900Context &ctx, Runtime *rt)
    {
        const auto &site = tmb::math::hudTransforms[index];
        const auto offset = bozo::GuestMemory::flatOffset(getRegU32(&ctx, site.output) & ~15u, 16);
        // Original stores a stale register W. The native record contract leaves the destination payload untouched.
        uint32_t payload;
        std::memcpy(&payload, ram + offset + 12, 4);
        tmb_hud_originals[index](ram, &ctx, rt);
        std::memcpy(ram + offset + 12, &payload, 4);
    }
}

int main()
{
    auto rt = std::make_unique<Runtime>();
    Host host;
    rt->port = &host;
    tmb::Registry registry;
    tmb::graphics::install(registry);
    const auto handler = registry.instructionHandler();
    if (!handler) return 1;
    uint8_t *ram = rt->memory.flat();
    std::array<uint8_t, arenaBytes> input{}, expected{};
    auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const unsigned savedControl = _mm_getcsr();
    uint64_t cases = 0, wholeCases = 0;
    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned index = 0; index < tmb::math::hudTransforms.size(); ++index)
            for (unsigned n = 0; n < 8192; ++n)
            {
                for (unsigned i = 0; i < arenaBytes; i += 4)
                {
                    uint32_t bits = randomBits();
                    if (n < 2048) bits = (bits & 0x807fffffu) | ((110u + bits % 30u) << 23u);
                    else if (n < 4096) bits = special[bits % std::size(special)];
                    std::memcpy(input.data() + i, &bits, 4);
                }
                const uint32_t base = n % 7u == 0 ? bozo::kScratchpadBase : 0x10000u;
                const auto offset = bozo::GuestMemory::flatOffset(base, arenaBytes);
                uint8_t *arena = ram + offset;
                std::memcpy(arena, input.data(), arenaBytes);
                R5900Context initial;
                for (unsigned i = 1; i < 32; ++i)
                    for (unsigned j = 0; j < 4; ++j) initial.r[i].uw[j] = randomBits();
                for (auto &v : initial.vu0_vf)
                    for (auto &bits : v.u) bits = randomBits();
                initial.vu0_vf[0] = Vu0Vector{}; initial.vu0_vf[0].u[3] = 0x3f800000;
                for (auto &bits : initial.vu0_acc.u) bits = randomBits();
                initial.vu0_status = randomBits(); initial.vu0_mac = randomBits(); initial.vi[5] = randomBits();
                initial.pc = 0x135798; initial.cycles = randomBits(); initial.checkpoints = randomBits();
                const auto &site = tmb::math::hudTransforms[index];
                const uint32_t alias = base == 0x10000u ? (n % 3u == 0 ? 0x80000000u : n % 3u == 1 ? 0x20000000u : 0u) : 0u;
                const uint32_t out = base + 64 + 16 * (n % 11u);
                const uint32_t in = site.input == site.output ? out : base + 64 + 16 * (n % 9u);
                const uint32_t translation = site.translation == site.output ? out : base + 64 + 16 * (n % 13u);
                setRegU32(&initial, site.basis, (base + 128u) | alias);
                setRegU32(&initial, site.output, (out | alias) + (n & 15u));
                setRegU32(&initial, site.input, (in | alias) + ((n >> 4u) & 15u));
                if (site.translation) setRegU32(&initial, site.translation, (translation | alias) + ((n >> 8u) & 15u));
                R5900Context original = initial, native = initial;
                const unsigned control = ((savedControl | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1u ? 0x8040u : 0u);
                _mm_setcsr(control);
                originalBlock(index, ram, original, rt.get());
                std::memcpy(expected.data(), arena, arenaBytes);
                if (n < 8) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++wholeCases; }
                std::memcpy(arena, input.data(), arenaBytes);
                for (uint32_t pc = site.first; pc <= site.last; pc += 4)
                {
                    if (!handler(ram, &native, rt.get(), pc)) return 2;
                    if (pc != site.last && std::memcmp(arena, input.data(), arenaBytes)) return 3;
                }
                const unsigned after = _mm_getcsr();
                _mm_setcsr(savedControl);
                if (std::memcmp(arena, expected.data(), arenaBytes) || std::memcmp(&native, &initial, sizeof(initial)) ||
                    after != control || (n < 8 && std::memcmp(whole.get(), ram, bozo::kFlatBytes)))
                {
                    std::fprintf(stderr, "FAIL HUD block %u case %u mode %u: memory %d CPU/VU %d host %d\n", index, n, mode,
                        std::memcmp(arena, expected.data(), arenaBytes), std::memcmp(&native, &initial, sizeof(initial)), after == control);
                    return 4;
                }
                host.enabled = false;
                if (tmb::math::hudInstruction(ram, &native, rt.get(), site.last)) return 5;
                host.enabled = true;
                ++cases;
            }
    // Both blocks share current input/output/matrix memory: the first write changes what the second must read.
    for (unsigned n = 0; n < 1024; ++n)
    {
        uint8_t *arena = ram + 0x10000;
        for (unsigned i = 0; i < arenaBytes; i += 4)
        {
            const uint32_t bits = (randomBits() & 0x807fffffu) | ((120u + i % 7u) << 23u);
            std::memcpy(input.data() + i, &bits, 4);
        }
        std::memcpy(arena, input.data(), arenaBytes);
        R5900Context initial;
        setRegU32(&initial, 18, 0x10080); setRegU32(&initial, 16, 0x10080 + 16 * (n % 3u));
        setRegU32(&initial, 29, 0x10080 + 16 * ((n / 3u) % 4u));
        R5900Context original = initial, native = initial;
        originalBlock(0, ram, original, rt.get()); originalBlock(1, ram, original, rt.get());
        std::memcpy(expected.data(), arena, arenaBytes);
        std::memcpy(arena, input.data(), arenaBytes);
        for (unsigned index : {0u, 1u})
            for (uint32_t pc = tmb::math::hudTransforms[index].first; pc <= tmb::math::hudTransforms[index].last; pc += 4)
                if (!handler(ram, &native, rt.get(), pc)) return 6;
        if (std::memcmp(arena, expected.data(), arenaBytes) || std::memcmp(&native, &initial, sizeof(initial))) return 7;
    }
    std::printf("native HUD tests: %llu generated-block cases, %llu whole-memory cases, 1024 ordered overlap cases; CPU/VU untouched\n",
        static_cast<unsigned long long>(cases), static_cast<unsigned long long>(wholeCases));
}
