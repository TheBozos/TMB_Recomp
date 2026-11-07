// SPDX-License-Identifier: MIT
// Compare actual generated original blocks against the production instruction dispatcher, including ordered aliases.
#include "tmb_contact_math.h"
#include "tmb_native_math.h"
#include "tmb_contact_original.h"
#include "bozo/libs/libraries.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::graphics { void install(Registry &); }
namespace tmb::math { void install(Registry &); }
void testSphereLodCallers();
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
        bozo::port::InstructionHandler instruction = nullptr;
        bool nativeInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc) override
        { return enabled && instruction && instruction(ram, ctx, rt, pc); }
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return enabled; }
        void frameEnd(Runtime *, R5900Context *) override { throw "unexpected frame end"; }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw "unexpected library call"; }
    };
    void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
    struct SegmentEnd {};
    struct Services : bozo::RuntimeServices
    {
        unsigned yields = 0;
        uint32_t stop = 0x1eeafc;
        // Keep checkpoints enabled; stop at the chosen boundary or let a complete continuation return.
        void yield(uint8_t *, R5900Context *ctx, Runtime *) override
        {
            require(++yields < 64, "caller did not reach segment end");
            if (ctx->pc == stop) throw SegmentEnd{};
        }
        [[noreturn]] static void unexpected() { throw std::runtime_error("unexpected contact caller service"); }
        void unknownTarget(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void syscall(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void breakpoint(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void trap(uint8_t *, R5900Context *, Runtime *) override { unexpected(); }
        void unimplemented(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void eret(uint8_t *, R5900Context *, Runtime *) override { unexpected(); }
        uint32_t cop0Read(R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void cop0Write(R5900Context *, Runtime *, uint32_t, uint32_t) override { unexpected(); }
        bool cop0Condition(R5900Context *, Runtime *) override { unexpected(); }
        void vu0CallMicroprogram(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        uint32_t vu0ControlRead(R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void vu0ControlWrite(R5900Context *, Runtime *, uint32_t, uint32_t) override { unexpected(); }
        void ioRead(R5900Context *, Runtime *, uint32_t, uint32_t, uint64_t[2]) override { unexpected(); }
        void ioWrite(R5900Context *, Runtime *, uint32_t, uint32_t, const uint64_t[2]) override { unexpected(); }
    };
    constexpr size_t arenaBytes = 512;
    constexpr uint32_t special[] = {0, 0x80000000, 1, 0x80000001, 0x7fffff, 0x807fffff,
        0x800000, 0x80800000, 0x3f800000, 0xbf800000, 0x7f7fffff, 0xff7fffff,
        0x7f800000, 0xff800000, 0x7fffffff, 0xffffffff};

    void originalBlock(unsigned index, uint8_t *ram, R5900Context &ctx, Runtime *rt)
    {
        const auto &site = tmb::math::contactVectors[index];
        const auto offset = bozo::GuestMemory::flatOffset(getRegU32(&ctx, site.output) & ~15u, 16);
        // Original stores a stale register W. The native record contract leaves the destination payload untouched.
        uint32_t payload;
        std::memcpy(&payload, ram + offset + 12, 4);
        tmb_contact_originals[index](ram, &ctx, rt);
        std::memcpy(ram + offset + 12, &payload, 4);
    }
    void testContactCallers(Runtime &rt, Host &host, bozo::port::InstructionHandler handler)
    {
        auto *ram = rt.memory.flat();
        // Ordered aliases: a point output may overwrite the next point's input or a basis column.
        std::array<uint8_t, arenaBytes> input{}, expected{};
        for (unsigned n = 0; n < 1024; ++n)
        {
            for (unsigned i = 0; i < arenaBytes; i += 4)
            { const uint32_t v = randomBits(); std::memcpy(input.data() + i, &v, 4); }
            std::memcpy(ram + 0x10000, input.data(), arenaBytes);
            R5900Context initial;
            for (unsigned r = 1; r < 32; ++r) setRegU32(&initial, r, 0x10000 + 16 * (r % 15));
            setRegU32(&initial, 17, n & 1 ? getRegU32(&initial, 20) : getRegU32(&initial, 3));
            auto original = initial, native = initial;
            for (unsigned index : {17u, 18u}) originalBlock(index, ram, original, &rt);
            std::memcpy(expected.data(), ram + 0x10000, arenaBytes);
            std::memcpy(ram + 0x10000, input.data(), arenaBytes);
            for (unsigned index : {17u, 18u})
                for (auto pc = tmb::math::contactVectors[index].first; pc <= tmb::math::contactVectors[index].last; pc += 4)
                    require(handler(ram, &native, &rt, pc), "ordered contact block was not handled");
            require(!std::memcmp(expected.data(), ram + 0x10000, arenaBytes) &&
                    !std::memcmp(&native, &initial, sizeof(initial)), "ordered contact alias differs");
        }

        rt.functions.setGenerated(tmb_function_table, tmb_function_count);
        rt.functions.setResume(tmb_resume_table, tmb_resume_count);
        Services services; rt.services = &services;
        constexpr uint32_t base = 0x10000, bytes = 0x40000, globals = 0x525000, globalBytes = 0x3000;
        auto inputAll = std::make_unique<uint8_t[]>(bytes + globalBytes);
        auto expectedAll = std::make_unique<uint8_t[]>(bytes + globalBytes);
        auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
        const auto saved = _mm_getcsr();
        unsigned cases = 0, memoryCases = 0;
        for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < 7; ++kind)
            for (unsigned n = 0; n < 128; ++n)
            {
                const auto fill = [&](uint32_t at, unsigned count) {
                    for (unsigned i = 0; i < count; i += 4)
                    { const float f = float(int(randomBits() % 4097) - 2048) / 128.f; std::memcpy(ram + at + i, &f, 4); }
                };
                fill(base, bytes); fill(globals, globalBytes);
                const auto word = [&](uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); };
                constexpr uint32_t gp = 0x40000, sp = 0x30000, head = 0x11000;
                const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
                const auto address = [&](uint32_t at) { return at | alias; };
                word(head, address(0x12000));
                std::memset(ram + head + 8, 0, 16);
                word(gp - 0x2ff8, address(head)); word(gp - 0x2ff4, address(0x16000));
                word(gp - 0x2ff0, address(0x16000)); word(gp - 0x2fec, address(0x13000));
                word(gp - 0x2fe8, address(0x15000));
                std::memset(ram + 0x15000, 0, 0x100);
                R5900Context initial;
                for (unsigned r = 1; r < 32; ++r) for (auto &v : initial.r[r].uw) v = randomBits();
                for (auto &v : initial.vu0_vf) for (auto &b : v.u) b = randomBits();
                initial.vu0_vf[0] = {}; initial.vu0_vf[0].u[3] = 0x3f800000;
                for (auto &b : initial.vu0_acc.u) b = randomBits();
                initial.vu0_status = randomBits(); initial.vu0_mac = randomBits(); initial.vi[5] = randomBits();
                setRegU32(&initial, 28, address(gp)); setRegU32(&initial, 29, address(sp));
                setRegU32(&initial, 31, 0xbad0000); initial.cycleLimit = 0;
                RecompiledFunction continuation;
                std::array<uint32_t, 4> output{}; unsigned outputs = 0;
                if (kind == 0)
                {
                    continuation = resume_0x1cc7a0; initial.pc = 0x1cc7a0; services.stop = 0x1eeafc;
                    output[outputs++] = 0x5253b0;
                }
                else if (kind == 1)
                {
                    continuation = resume_0x1cc7a0; initial.pc = 0x1cca30; services.stop = 0x1f02b8;
                    setRegU32(&initial, 2, address(head)); setRegU32(&initial, 23, 0x520000);
                    setRegU32(&initial, 22, 0x520000); setRegU32(&initial, 19, address(n & 1 ? 0x5253b0 : 0x13000));
                    setRegU32(&initial, 20, address(n & 2 ? 0x5253b0 : 0x14000));
                    output[outputs++] = 0x5253b0; output[outputs++] = 0x5253c0;
                }
                else if (kind == 2)
                {
                    continuation = resume_0x1cb0d8; initial.pc = 0x1cb430; services.stop = 0x1da1e8;
                    setRegU32(&initial, 2, address(0x12000)); setRegU32(&initial, 20, address(0x13000));
                    word(sp + 0x60, address(n & 1 ? 0x12020 : 0x13080)); word(sp + 0x64, address(0x130a0));
                    output[outputs++] = n & 1 ? 0x12020 : 0x13080; output[outputs++] = 0x5253b0;
                }
                else if (kind == 3)
                {
                    continuation = resume_0x1cb690; initial.pc = 0x1cb690; services.stop = 0x1da1e8;
                    setRegU32(&initial, 4, n % 4); setRegU32(&initial, 5, address(0x14000));
                    output[outputs++] = sp - 0xf0; output[outputs++] = 0x5253b0;
                }
                else if (kind == 4)
                {
                    continuation = resume_0x1cb690; initial.pc = 0x1cb7c8; services.stop = 0x1da1e8;
                    setRegU32(&initial, 4, address(head)); setRegU32(&initial, 19, 0x70 + (n % 4) * 16);
                    setRegU32(&initial, 23, 0x5253b0); setRegU32(&initial, 18, 0x527a00);
                    output[outputs++] = sp; output[outputs++] = 0x5253c0;
                }
                else if (kind == 5)
                {
                    continuation = resume_0x1cbd00; initial.pc = 0x1cbd00; services.stop = 0x1da778;
                    setRegU32(&initial, 4, address(0x16000)); setRegU32(&initial, 5, address(0x17000));
                    setRegU32(&initial, 6, address(0x13000)); setRegU32(&initial, 7, address(0x14000));
                    output[outputs++] = sp - 0x210;
                }
                else
                {
                    // Complete ray-contact result construction, metadata writes and epilogue.
                    continuation = resume_0x1cbd00; initial.pc = 0x1cc020; services.stop = 0xffffffff;
                    setRegU32(&initial, 7, n % 4); setRegU32(&initial, 6, address(0x15000));
                    setRegU32(&initial, 20, address(0x14000)); setRegU32(&initial, 17, (n % 4) * 16);
                    setRegU32(&initial, 30, address(0x16000)); setRegU32(&initial, 23, address(0x17000));
                    setRegU32(&initial, 18, address(sp + 0x130));
                    word(sp + 0x140, address(0x13000)); word(sp + 0x144, address(sp + 0x20));
                    word(sp + 0x1e0, 0xbad0000); word(sp + 0x1e4, 0);
                    output[outputs++] = 0x15000 + (n % 4) * 0x40 + 0x10;
                    output[outputs++] = sp + 0x20; output[outputs++] = sp + 0x130;
                    output[outputs++] = 0x15000 + (n % 4) * 0x40;
                }
                std::memcpy(inputAll.get(), ram + base, bytes);
                std::memcpy(inputAll.get() + bytes, ram + globals, globalBytes);
                const auto payload = [&](uint32_t at) { return inputAll.get() + (at >= globals ? bytes + at - globals : at - base); };
                const auto run = [&](bool enabled, R5900Context &ctx) {
                    host.enabled = enabled; services.yields = 0;
                    try { continuation(ram, &ctx, &rt); }
                    catch (const SegmentEnd &) { return; }
                    if (kind == 6 && ctx.pc == 0xbad0000) return;
                    throw std::runtime_error("contact continuation did not reach query boundary");
                };
                const auto control = ((saved | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1 ? 0x8040u : 0u);
                auto a = initial, b = initial;
                _mm_setcsr(control); run(false, a); const auto refControl = _mm_getcsr(); const auto refYields = services.yields;
                for (unsigned i = 0; i < outputs; ++i) std::memcpy(ram + output[i] + 12, payload(output[i]) + 12, 4);
                std::memcpy(expectedAll.get(), ram + base, bytes);
                std::memcpy(expectedAll.get() + bytes, ram + globals, globalBytes);
                if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++memoryCases; }
                std::memcpy(ram + base, inputAll.get(), bytes);
                std::memcpy(ram + globals, inputAll.get() + bytes, globalBytes);
                _mm_setcsr(control); run(true, b); const auto after = _mm_getcsr(); _mm_setcsr(saved);
                constexpr auto vuAt = offsetof(R5900Context, vu0_vf), vuBytes = offsetof(R5900Context, cycles) - vuAt;
                const bool untouched = !std::memcmp(reinterpret_cast<uint8_t *>(&initial) + vuAt, reinterpret_cast<uint8_t *>(&b) + vuAt, vuBytes);
                std::memset(reinterpret_cast<uint8_t *>(&a) + vuAt, 0, vuBytes);
                std::memset(reinterpret_cast<uint8_t *>(&b) + vuAt, 0, vuBytes);
                if (std::memcmp(&a, &b, sizeof(a)) || !untouched || after != refControl || refYields != services.yields ||
                    std::memcmp(expectedAll.get(), ram + base, bytes) || std::memcmp(expectedAll.get() + bytes, ram + globals, globalBytes) ||
                    (n < 4 && std::memcmp(whole.get(), ram, bozo::kFlatBytes)))
                { std::fprintf(stderr, "FAIL contact continuation %u case %u mode %u CPU%d VU%d FP%d data%d globals%d\n",kind,n,mode,std::memcmp(&a,&b,sizeof(a)),untouched,after==refControl,std::memcmp(expectedAll.get(),ram+base,bytes),std::memcmp(expectedAll.get()+bytes,ram+globals,globalBytes)); throw std::runtime_error("contact continuation differs"); }
                ++cases;
            }
        host.enabled = true; rt.services = nullptr;
        std::printf("native contact callers: %u generated continuations, %u whole-memory checks; 1024 ordered aliases; complete CPU/clock/FP exact outside retired VU and output W; native VU untouched\n", cases, memoryCases);
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
    host.instruction = handler;
    tmb::math::install(registry);
    rt->functions.setOverride(0x1dcc08, registry.find(0x1dcc08));
    if (!handler) return 1;
    uint8_t *ram = rt->memory.flat();
    std::array<uint8_t, arenaBytes> input{}, expected{};
    auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const unsigned savedControl = _mm_getcsr();
    uint64_t cases = 0, wholeCases = 0;
    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned index = 0; index < tmb::math::contactVectors.size(); ++index)
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
                const auto &site = tmb::math::contactVectors[index];
                const uint32_t alias = base == 0x10000u ? (n % 3u == 0 ? 0x80000000u : n % 3u == 1 ? 0x20000000u : 0u) : 0u;
                const uint32_t out = base + 64 + 16 * (n % 11u);
                const uint32_t in = site.input == site.output ? out : base + 64 + 16 * (n % 9u);
                const uint32_t translation = site.translation == site.output ? out : base + 64 + 16 * (n % 13u);
                if (site.basis) setRegU32(&initial, site.basis, (base + 128u) | alias);
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
                    std::fprintf(stderr, "FAIL contact block %u case %u mode %u: memory %d CPU/VU %d host %d\n", index, n, mode,
                        std::memcmp(arena, expected.data(), arenaBytes), std::memcmp(&native, &initial, sizeof(initial)), after == control);
                    return 4;
                }
                host.enabled = false;
                if (tmb::math::contactInstruction(ram, &native, rt.get(), site.last)) return 5;
                host.enabled = true;
                ++cases;
            }
    testContactCallers(*rt, host, handler);
    testSphereLodCallers();
    std::printf("native contact blocks: %llu generated comparisons, %llu whole-memory checks; CPU/VU/host untouched, destination W preserved\n", (unsigned long long)cases, (unsigned long long)wholeCases);
    return 0;
}
