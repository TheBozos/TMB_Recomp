// SPDX-License-Identifier: MIT
// Actual sphere/LOD decisions and hierarchy-helper consumers, compiled from the local game ELF.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "tmb_hierarchy_math.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace tmb::graphics { void install(Registry &); }
namespace tmb::math { void install(Registry &); }
namespace tmb::hierarchy { void install(Registry &); }
namespace
{
    void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
    uint32_t seed = 0x13bc91a7;
    uint32_t bits() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
    struct Host : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bool enabled = true;
        bozo::port::InstructionHandler instruction = nullptr;
        bool nativeInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc) override
        { return enabled && instruction && instruction(ram, ctx, rt, pc); }
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return enabled; }
        void frameEnd(Runtime *, R5900Context *) override { throw std::runtime_error("unexpected frame"); }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
    };
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
        void unknownTarget(uint8_t *, R5900Context *ctx, Runtime *, uint32_t pc) override
        { std::fprintf(stderr, "unexpected target %x return %x\n", pc, getRegU32(ctx, 31)); unexpected(); }
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
        void ioRead(R5900Context *ctx, Runtime *, uint32_t at, uint32_t, uint64_t[2]) override
        { std::fprintf(stderr, "unexpected read %x at %x return %x\n", at, ctx->pc, getRegU32(ctx, 31)); unexpected(); }
        void ioWrite(R5900Context *ctx, Runtime *, uint32_t at, uint32_t, const uint64_t[2]) override
        { std::fprintf(stderr, "unexpected write %x at %x return %x\n", at, ctx->pc, getRegU32(ctx, 31)); unexpected(); }
    };

    constexpr uint32_t base = 0x10000, bytes = 0x40000, globals = 0x510000, globalBytes = 0x20000;
    constexpr uint32_t gp = 0x40000, sp = 0x30000;
    constexpr size_t vuAt = offsetof(R5900Context, vu0_vf), vuBytes = offsetof(R5900Context, cycles) - vuAt;
    bool cpuEqual(R5900Context a, R5900Context b)
    {
        std::memset(reinterpret_cast<uint8_t *>(&a) + vuAt, 0, vuBytes);
        std::memset(reinterpret_cast<uint8_t *>(&b) + vuAt, 0, vuBytes);
        return !std::memcmp(&a, &b, sizeof(a));
    }
    bool vuUntouched(const R5900Context &initial, const R5900Context &after)
    { return !std::memcmp(reinterpret_cast<const uint8_t *>(&initial) + vuAt, reinterpret_cast<const uint8_t *>(&after) + vuAt, vuBytes); }
    void context(R5900Context &ctx, uint32_t alias)
    {
        for (unsigned r = 1; r < 32; ++r) for (auto &v : ctx.r[r].uw) v = bits();
        for (auto &v : ctx.vu0_vf) for (auto &b : v.u) b = bits();
        ctx.vu0_vf[0] = {}; ctx.vu0_vf[0].u[3] = 0x3f800000;
        for (auto &b : ctx.vu0_acc.u) b = bits();
        ctx.vu0_status = bits(); ctx.vu0_mac = bits(); ctx.vi[5] = bits();
        setRegU32(&ctx, 28, gp | alias); setRegU32(&ctx, 29, sp | alias); setRegU32(&ctx, 31, 0xbad0000);
        ctx.cycleLimit = 0;
    }
}

void testSphereLodCallers()
{
    auto rt = std::make_unique<Runtime>(); Host host; Services services;
    rt->port = &host; rt->services = &services;
    rt->functions.setGenerated(tmb_function_table, tmb_function_count);
    rt->functions.setResume(tmb_resume_table, tmb_resume_count);
    tmb::Registry registry; tmb::graphics::install(registry); tmb::math::install(registry); tmb::hierarchy::install(registry);
    host.instruction = registry.instructionHandler(); require(host.instruction, "no native instruction handler");
    auto *ram = rt->memory.flat();
    auto input = std::make_unique<uint8_t[]>(bytes + globalBytes), expected = std::make_unique<uint8_t[]>(bytes + globalBytes);
    auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const auto word = [&](uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); };
    const auto fill = [&] {
        for (auto region : {std::pair{base, bytes}, std::pair{globals, globalBytes}})
            for (uint32_t i = 0; i < region.second; i += 4)
            { const float f = float(int(bits() % 8193) - 4096) / 128.f; std::memcpy(ram + region.first + i, &f, 4); }
    };
    const auto snapshot = [&](uint8_t *out) { std::memcpy(out, ram + base, bytes); std::memcpy(out + bytes, ram + globals, globalBytes); };
    const auto restore = [&] { std::memcpy(ram + base, input.get(), bytes); std::memcpy(ram + globals, input.get() + bytes, globalBytes); };
    const auto dataEqual = [&] { return !std::memcmp(expected.get(), ram + base, bytes) && !std::memcmp(expected.get() + bytes, ram + globals, globalBytes); };
    const unsigned saved = _mm_getcsr(); unsigned cases = 0, memoryCases = 0;
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned n = 0; n < 512; ++n)
        {
            fill(); const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t v) { return v | alias; };
            R5900Context initial; context(initial, alias);
            setRegU32(&initial, 4, address(0x13000)); setRegU32(&initial, 5, address(0x14000));
            initial.fu[12] = n % 8 == 0 ? 0 : n % 8 == 1 ? 0xbf800000 : 0x40a00000 + (n % 3) - 1;
            initial.fu[13] = n % 3 == 0 ? 0x4360ffff : n % 3 == 1 ? 0x43610000 : 0x43610001;
            RecompiledFunction function;
            if (kind == 0)
            {
                initial.pc = 0x1c6710; function = resume_0x1c6710;
                word(gp - 0x3614, (n >> 2) & 1); word(gp - 0x3028, address(0x11000));
                word(0x11000, address(0x12000)); word(0x12000, address(0x12500));
                word(0x12500, n % 4); word(0x1251c, 0x40000000);
            }
            else if (kind == 1)
            {
                initial.pc = 0x1cadb8; function = resume_0x1cadb8; word(gp - 0x2fc8, 0);
            }
            else
            {
                initial.pc = 0x1c69a0; function = resume_0x1c69a0;
                setRegU32(&initial, 4, address(0x12000)); setRegU32(&initial, 5, address(0x13000));
                const uint16_t count = n % 4; std::memcpy(ram + 0x12004, &count, 2);
                word(0x13000, address(0x14000)); word(0x14000, (n & 1 ? 0x1340000u : 0u) | (n & 2 ? 0x12380u : 0u));
                for (unsigned i = 0; i < 3; ++i)
                { const float limit = float((n % 6) * 100 + i * 500); uint32_t value; std::memcpy(&value, &limit, 4); word(0x12024 + i * 16, value); word(0x12028 + i * 16, address(0x15000 + i * 0x100)); }
                if (n % 4 == 1)
                {
                    std::memset(ram + 0x512740, 0, 12);
                    word(0x12010, 0x40400000); word(0x12014, 0x40800000); word(0x12018, 0);
                    word(0x12024, 0x41c80000 + (n % 3) - 1); // squared distance 25, adjacent LOD limits
                }
            }
            // Exact radius and LOD boundaries: equality and neighbouring radius representations.
            if (n % 4 == 0 && kind != 2)
            {
                std::memset(ram + 0x13000, 0, 12); word(0x14000, 0x40400000); word(0x14004, 0x40800000); word(0x14008, 0);
            }
            snapshot(input.get()); auto a = initial, b = initial;
            const unsigned control = ((saved | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1 ? 0x8040u : 0u);
            host.enabled = false; services.yields = 0; services.stop = 0xffffffff;
            _mm_setcsr(control);
            try { function(ram, &a, rt.get()); }
            catch (...) { std::fprintf(stderr, "sphere/LOD original kind %u case %u\n", kind, n); throw; }
            const auto refControl = _mm_getcsr(); const auto refYields = services.yields;
            // Each complete function uses only XYZ from its local difference record.
            std::memcpy(ram + sp - 4, input.get() + sp - base - 4, 4);
            snapshot(expected.get()); if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++memoryCases; }
            restore(); host.enabled = true; services.yields = 0;
            _mm_setcsr(control); function(ram, &b, rt.get()); const auto after = _mm_getcsr(); _mm_setcsr(saved);
            if (!cpuEqual(a, b) || !vuUntouched(initial, b) || after != refControl || services.yields != refYields || !dataEqual() ||
                (n < 4 && std::memcmp(whole.get(), ram, bozo::kFlatBytes)))
            { std::fprintf(stderr, "FAIL sphere/LOD kind %u case %u mode %u CPU%d VU%d FP%d data%d\n", kind,n,mode,cpuEqual(a,b),vuUntouched(initial,b),after==refControl,dataEqual()); throw std::runtime_error("sphere/LOD caller differs"); }
            require(b.pc == 0xbad0000 && getRegU32(&b, 29) == address(sp), "sphere/LOD return ABI"); ++cases;
        }
    std::printf("sphere/LOD: %u complete generated callers, %u whole-memory checks; complete CPU/clock/FP/selection exact outside retired VU/local W; native VU untouched\n", cases, memoryCases);

    const auto nativeInverse = registry.find(0x1da1e8), nativeDirection = registry.find(0x1da298);
    const auto originalInverse = mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8;
    const auto originalDirection = mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298;
    unsigned helperCases = 0, helperMemory = 0;
    constexpr uint32_t callers[] = {0x1cb19c, 0x1cb1e4, 0x1cb4a4, 0x1cb530, 0x1cb770, 0x1cb834};
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < 6; ++kind)
        for (unsigned n = 0; n < 128; ++n)
        {
            fill(); const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t v) { return v | alias; };
            word(gp - 0x2ff8, address(0x11000)); word(0x11000, address(0x12000));
            word(gp - 0x2ff4, address(0x16000)); word(gp - 0x2ff0, address(0x16000));
            word(gp - 0x2fe8, address(0x15000)); word(gp - 0x2fd8, 0); word(0x14008, 0);
            const uint16_t slot = 0; std::memcpy(ram + 0x527a28, &slot, 2);
            R5900Context initial; context(initial, alias);
            const auto caller = callers[kind]; initial.pc = kind == 0 ? 0x1da298 : 0x1da1e8;
            setRegU32(&initial, 31, caller); setRegU32(&initial, 4, address(0x13000));
            setRegU32(&initial, 5, address(sp + 0x20)); setRegU32(&initial, 6, address(sp + 0x30));
            setRegU32(&initial, 17, 0x527a10); setRegU32(&initial, 18, 0x527a40);
            setRegU32(&initial, 16, 0x527a00);
            setRegU32(&initial, 21, address(0x14000)); setRegU32(&initial, 22, address(sp + 0x50)); setRegU32(&initial, 23, address(sp + 0x40));
            word(sp + 0x60, address(sp + 0x20)); word(sp + 0x64, address(sp + 0x30));
            RecompiledFunction continuation = kind < 4 ? resume_0x1cb0d8 : resume_0x1cb690;
            services.stop = kind < 2 ? 0x1dc008 : kind < 4 ? 0x1cc2b8 : kind == 4 ? 0x1dc008 : 0x1cba48;
            if (kind == 0) setRegU32(&initial, 6, address(sp + 0x20));
            if (kind >= 4)
            {
                setRegU32(&initial, 6, address(sp + 0x10)); setRegU32(&initial, 16, address(sp + 0x10));
                setRegU32(&initial, 17, n % 4); setRegU32(&initial, 18, 0x527a00); setRegU32(&initial, 19, 0x70 + (n % 4) * 16);
                setRegU32(&initial, 20, address(sp + 0x20)); setRegU32(&initial, 21, 0x527a40); setRegU32(&initial, 23, 0x5253b0);
                setRegU32(&initial, 22, address(0x14000));
                setRegU32(&initial, 30, 0x520000);
                ram[0x11000 + n % 4 + 0x3a] = 1;
            }
            if (kind == 5 && n % 8 < 4)
            {
                // Zero, short-normalization and exact/adjacent unit-box boundaries in the real scalar tail.
                std::memset(ram + 0x13000, 0, 48);
                word(0x13000, 0x3f800000); word(0x13014, 0x3f800000); word(0x13028, 0x3f800000);
                word(0x1300c, bits());
                std::memset(ram + sp + 0x20, 0, 12);
                constexpr uint32_t edge[] = {0, 0x3f000000, 0x3f800000, 0x3f7fffff};
                word(sp + 0x20, edge[n % 8]);
            }
            snapshot(input.get());
            const auto run = [&](bool native, R5900Context &ctx) {
                rt->functions.setOverride(0x1da1e8, native ? nativeInverse : originalInverse);
                rt->functions.setOverride(0x1da298, native ? nativeDirection : originalDirection);
                services.yields = 0;
                auto helper = kind == 0 ? (native ? nativeDirection : originalDirection) : (native ? nativeInverse : originalInverse);
                helper(ram, &ctx, rt.get()); require(ctx.pc == caller, "hierarchy helper return");
                // The pre-existing forward-direction contract preserves its destination's opaque W.
                if (kind == 0) std::memcpy(ram + sp + 0x2c, input.get() + sp - base + 0x2c, 4);
                try { continuation(ram, &ctx, rt.get()); }
                catch (const SegmentEnd &) { return; }
                throw std::runtime_error("hierarchy helper continuation did not reach its query boundary");
            };
            auto a = initial, b = initial;
            const auto control = ((saved | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1 ? 0x8040u : 0u);
            _mm_setcsr(control);
            try { run(false, a); }
            catch (...) { std::fprintf(stderr, "hierarchy original kind %u case %u\n", kind, n); throw; }
            const auto refControl = _mm_getcsr(); const auto refYields = services.yields;
            snapshot(expected.get()); if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++helperMemory; }
            restore(); _mm_setcsr(control); run(true, b); const auto after = _mm_getcsr(); _mm_setcsr(saved);
            if (!cpuEqual(a,b) || !vuUntouched(initial,b) || after != refControl || services.yields != refYields || !dataEqual() ||
                (n < 4 && std::memcmp(whole.get(),ram,bozo::kFlatBytes)))
            { std::fprintf(stderr,"FAIL hierarchy helper %x case %u mode %u CPU%d VU%d FP%d data%d\n",caller,n,mode,cpuEqual(a,b),vuUntouched(initial,b),after==refControl,dataEqual()); throw std::runtime_error("hierarchy helper consumer differs"); }
            ++helperCases;
        }
    _mm_setcsr(saved);
    std::printf("hierarchy helper consumers: %u generated sequences, %u whole-memory checks; inverse XYZW/scalar consumers CPU/clock/FP exact; native VU untouched\n", helperCases, helperMemory);

    // The new forward return must still execute the complete original ABI on the historical host.
    host.enabled = false;
    for (unsigned n = 0; n < 64; ++n)
    {
        fill(); R5900Context initial; context(initial, 0);
        setRegU32(&initial, 4, 0x13000 + (n & 15)); setRegU32(&initial, 5, 0x14000 + ((n >> 2) & 15));
        setRegU32(&initial, 6, 0x15000 + (n & 15)); setRegU32(&initial, 31, 0x1cb19c);
        snapshot(input.get()); auto a = initial, b = initial;
        originalDirection(ram, &a, rt.get()); snapshot(expected.get());
        if (n < 4) std::memcpy(whole.get(), ram, bozo::kFlatBytes);
        restore(); nativeDirection(ram, &b, rt.get());
        require(!std::memcmp(&a, &b, sizeof(a)) && dataEqual() &&
                (n >= 4 || !std::memcmp(whole.get(), ram, bozo::kFlatBytes)), "historical collision direction ABI");
    }
    host.enabled = true;
    std::printf("collision direction: 64 historical full-ABI cases, 4 whole-memory checks\n");
}
