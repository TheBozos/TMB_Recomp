// SPDX-License-Identifier: MIT
// Run the generated BSP collision setup through its real XYZ consumers. No copied game code is committed.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::math { void install(Registry &); }
namespace
{
    void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
    struct Host : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bool native = true;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return native; }
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
        [[noreturn]] static void unexpected() { throw std::runtime_error("unexpected direction caller service"); }
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
    uint32_t seed = 0x179ed461;
    uint32_t bits() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
    constexpr size_t vuAt = offsetof(R5900Context, vu0_vf), vuBytes = offsetof(R5900Context, cycles) - vuAt;
    bool cpuEqual(R5900Context a, R5900Context b)
    {
        // The native direction leaf deliberately has no VU side effects. Later original inverse leaves still do.
        std::memset(reinterpret_cast<uint8_t *>(&a) + vuAt, 0, vuBytes);
        std::memset(reinterpret_cast<uint8_t *>(&b) + vuAt, 0, vuBytes);
        return !std::memcmp(&a, &b, sizeof(a));
    }
}

void testDirectionCallers()
{
    auto rt = std::make_unique<Runtime>(); Host host; Services services;
    rt->port = &host; rt->services = &services;
    rt->functions.setGenerated(tmb_function_table, tmb_function_count);
    rt->functions.setResume(tmb_resume_table, tmb_resume_count);
    tmb::Registry registry; tmb::math::install(registry);
    const auto nativeDirection = registry.find(0x1da298);
    const auto originalDirection = mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298;
    auto *ram = rt->memory.flat();
    constexpr unsigned arenaBytes = 0x40000, globals = 0x525000, globalBytes = 0x3000;
    auto input = std::make_unique<uint8_t[]>(arenaBytes + globalBytes);
    auto expected = std::make_unique<uint8_t[]>(arenaBytes + globalBytes);
    auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const unsigned saved = _mm_getcsr(); unsigned cases = 0, memoryCases = 0;
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned n = 0; n < 128; ++n)
        {
            for (unsigned i = 0; i < arenaBytes + globalBytes; i += 4)
            {
                const float f = float(int(bits() % 4097) - 2048) / 128.f;
                std::memcpy(input.get() + i, &f, 4);
            }
            std::memcpy(ram + 0x10000, input.get(), arenaBytes);
            std::memcpy(ram + globals, input.get() + arenaBytes, globalBytes);
            const auto word = [&](uint32_t address, uint32_t value) { std::memcpy(ram + address, &value, 4); };
            const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t value) { return value | alias; };
            constexpr uint32_t sp = 0x30000, gp = 0x40000, output = sp + 0x40;
            word(gp - 0x2ff4, address(0x11000)); word(gp - 0x2ff8, address(0x14000));
            word(0x14000, address(0x15000));
            word(sp + 0x11c, address(sp + 0xf0)); word(sp + 0x120, address(sp + 0x100));
            word(sp + 0x12c, address(output)); word(sp + 0x130, address(sp + 0x50));
            word(sp + 0x138, address(sp + 0x90));
            const uint16_t matrix = n % 4; std::memcpy(ram + 0x527a28, &matrix, 2);
            // W is an opaque sentinel; none of the audited consumers is allowed to use it as XYZ.
            word(output + 12, bits());
            R5900Context initial;
            for (unsigned i = 1; i < 32; ++i) for (auto &v : initial.r[i].uw) v = bits();
            for (auto &v : initial.vu0_vf) for (auto &vbits : v.u) vbits = bits();
            initial.vu0_vf[0] = {}; initial.vu0_vf[0].u[3] = 0x3f800000;
            initial.vu0_status = bits(); initial.vu0_mac = bits(); initial.vi[5] = bits();
            for (auto &v : initial.vu0_acc.u) v = bits();
            setRegU32(&initial, 28, address(gp)); setRegU32(&initial, 29, address(sp));
            setRegU32(&initial, 16, 0x527a00); setRegU32(&initial, 17, 0x527a40);
            setRegU32(&initial, 18, 0x527a10); setRegU32(&initial, 23, address(output));
            setRegU32(&initial, 30, address(sp + 0x50));
            setRegU32(&initial, 4, address(0x15020)); setRegU32(&initial, 5, address(sp));
            setRegU32(&initial, 6, address(output));
            const uint32_t caller = tmb::math::directionReturns[kind + 1];
            setRegU32(&initial, 31, caller); initial.pc = 0x1da298; initial.cycleLimit = 0;
            const float radius = 6.5f; std::memcpy(&initial.fu[20], &radius, 4);
            std::memcpy(input.get(), ram + 0x10000, arenaBytes);
            std::memcpy(input.get() + arenaBytes, ram + globals, globalBytes);
            const unsigned control = ((saved | 0x1f80) & ~0xe040u) | (mode << 13) | (n & 1 ? 0x8040 : 0);
            const auto run = [&](RecompiledFunction direction, R5900Context &ctx)
            {
                services.yields = 0; rt->functions.setOverride(0x1da298, direction);
                direction(ram, &ctx, rt.get());
                require(ctx.pc == caller, "direction returned to wrong caller");
                try { resume_0x1cd538(ram, &ctx, rt.get()); }
                catch (const SegmentEnd &) { return; }
                throw std::runtime_error("BSP segment returned early");
            };
            _mm_setcsr(control); auto a = initial, b = initial; run(originalDirection, a);
            const unsigned referenceYields = services.yields;
            // Compare the documented native record contract: destination W stays as it was before direction.
            // All subsequent inverse-transform outputs and scalar consumers are left completely unmasked.
            std::memcpy(ram + output + 12, input.get() + output - 0x10000 + 12, 4);
            std::memcpy(expected.get(), ram + 0x10000, arenaBytes);
            std::memcpy(expected.get() + arenaBytes, ram + globals, globalBytes);
            if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++memoryCases; }
            std::memcpy(ram + 0x10000, input.get(), arenaBytes);
            std::memcpy(ram + globals, input.get() + arenaBytes, globalBytes);
            run(nativeDirection, b); const unsigned after = _mm_getcsr(); _mm_setcsr(saved);
            if (!cpuEqual(a, b) || after != control || referenceYields != services.yields ||
                std::memcmp(expected.get(), ram + 0x10000, arenaBytes) ||
                std::memcmp(expected.get() + arenaBytes, ram + globals, globalBytes) ||
                (n < 4 && std::memcmp(whole.get(), ram, bozo::kFlatBytes)))
            {
                std::fprintf(stderr, "FAIL BSP direction caller %x case %u mode %u CPU %d\n", caller, n, mode, cpuEqual(a, b));
                throw std::runtime_error("BSP caller differs from original");
            }
            // Historical host retains the complete original helper, even at the audited addresses.
            host.native = false; a = initial; b = initial;
            originalDirection(ram, &a, rt.get());
            uint8_t originalOut[16]; std::memcpy(originalOut, ram + output, 16);
            nativeDirection(ram, &b, rt.get()); host.native = true;
            require(!std::memcmp(&a, &b, sizeof(a)) && !std::memcmp(originalOut, ram + output, 16), "historical direction ABI");
            ++cases;
        }
    _mm_setcsr(saved);
    std::printf("BSP direction callers: %u generated continuation cases, %u whole-memory checks; XYZ consumers, CPU/clock/checkpoints and FP control exact; historical helper ABI exact\n", cases, memoryCases);

    // Vehicle point setup through its next loop boundary, complete remaining tire-height loops,
    // and light-node continuations through their epilogue (including normalization and attenuation).
    unsigned addedCases = 0, addedMemory = 0;
    constexpr uint32_t callers[] = {0x11708c, 0x1880e0, 0x1d4284, 0x1d42e8};
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < 4; ++kind)
        for (unsigned n = 0; n < 128; ++n)
        {
            for (unsigned i = 0; i < arenaBytes; i += 4)
            {
                const float f = float(int(bits() % 4097) - 2048) / 128.f;
                std::memcpy(ram + 0x10000 + i, &f, 4);
            }
            const auto word = [&](uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); };
            const auto wide = [&](uint32_t at, uint64_t value) { std::memcpy(ram + at, &value, 8); };
            const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t v) { return v | alias; };
            constexpr uint32_t vehicle = 0x10000, node = 0x13000, gp = 0x40000, sp = 0x30000;
            R5900Context initial;
            for (unsigned i = 1; i < 32; ++i) for (auto &v : initial.r[i].uw) v = bits();
            for (auto &v : initial.vu0_vf) for (auto &vbits : v.u) vbits = bits();
            initial.vu0_vf[0] = {}; initial.vu0_vf[0].u[3] = 0x3f800000;
            initial.vu0_status = bits(); initial.vu0_mac = bits(); initial.vi[5] = bits();
            for (auto &v : initial.vu0_acc.u) v = bits();
            setRegU32(&initial, 28, address(gp)); setRegU32(&initial, 29, address(sp));
            const uint32_t caller = callers[kind];
            setRegU32(&initial, 31, caller); initial.pc = 0x1da298; initial.cycleLimit = 0;
            uint32_t output = sp + 0x10;
            RecompiledFunction continuation;
            if (kind == 0)
            {
                continuation = resume_0x115f30; services.stop = 0x116e78;
                setRegU32(&initial, 16, address(vehicle)); setRegU32(&initial, 17, address(vehicle + 0x1a90));
                setRegU32(&initial, 18, n % 3); setRegU32(&initial, 19, address(vehicle + 0x720));
                setRegU32(&initial, 20, address(vehicle + 0x1ab0)); setRegU32(&initial, 21, address(vehicle + 0x7f0));
                setRegU32(&initial, 22, address(vehicle + 0x1ac0)); setRegU32(&initial, 23, address(output));
                setRegU32(&initial, 30, address(sp + 0x20));
                word(sp + 0x34, address(vehicle + 0x1d0)); word(sp + 0x3c, address(output));
                word(sp + 0x40, address(vehicle + 0xd0)); word(vehicle + 0x1b48, n & 1);
                word(vehicle + 0x2018, address(0x17000));
                setRegU32(&initial, 4, address(vehicle + 0xd0)); setRegU32(&initial, 5, address(vehicle + 0x1ab0));
            }
            else if (kind == 1)
            {
                continuation = resume_0x187db8; services.stop = 0xffffffff;
                setRegU32(&initial, 16, address(vehicle + 0x1ab0)); setRegU32(&initial, 17, address(vehicle));
                setRegU32(&initial, 18, address(vehicle + 0x1a90)); setRegU32(&initial, 19, n % 4);
                setRegU32(&initial, 20, address(vehicle + 0x4e0)); setRegU32(&initial, 21, address(output));
                initial.fu[21] = 0x3f000000; // positive and negative suspension clamp limits
                word(vehicle + 0x378, 0x40000000); word(vehicle + 0x1790, n % 3);
                for (unsigned j = 0; j < 4; ++j)
                {
                    word(vehicle + 0x1a90 + j * 0xf0, address(0x18000 + j * 0x100));
                    word(vehicle + 0x1b48 + j * 0xf0, n % 5 == j ? 1 : 0);
                }
                wide(sp + 0x80, 0xbad0000);
                setRegU32(&initial, 4, address(vehicle + 0x4e0)); setRegU32(&initial, 5, address(vehicle + 0x1ab0));
            }
            else
            {
                continuation = resume_0x1d4210; services.stop = 0xffffffff;
                const uint64_t flags = n % 3 == 0 ? (1ull << 39) : n % 3 == 1 ? 0 : (1ull << 35);
                wide(node, flags); wide(sp + 0x80, 0xbad0000);
                word(gp - 0x358c, address(0x16000)); word(gp - 0x3588, n % 73);
                word(gp - 0x4824, 0x358637bd); // normalization threshold
                for (unsigned view = 0; view < 4; ++view) word(0x4dfcd0 + view * 16, address(0x15000 + view * 0x100));
                setRegU32(&initial, 17, address(node)); setRegU32(&initial, 19, address(vehicle));
                if (kind == 2)
                {
                    setRegU32(&initial, 16, address(sp + 0x10)); setRegU32(&initial, 18, n % 4);
                    setRegU32(&initial, 5, address(sp));
                    // Exercise normalization's zero/short-vector branch as well as ordinary finite vectors.
                    if (n % 8 == 0) std::memset(ram + sp, 0, 12);
                }
                else
                {
                    output = sp + 0x30;
                    setRegU32(&initial, 16, address(output)); setRegU32(&initial, 18, address(sp + 0x20));
                    setRegU32(&initial, 5, address(node + 0x20));
                }
                setRegU32(&initial, 4, address(vehicle));
            }
            setRegU32(&initial, 6, address(output)); word(output + 12, bits());
            // Both light directions may execute in one continuation; both opaque W words must stay untouched.
            std::memcpy(input.get(), ram + 0x10000, arenaBytes);
            const unsigned control = ((saved | 0x1f80) & ~0xe040u) | (mode << 13) | (n & 1 ? 0x8040 : 0);
            const auto run = [&](RecompiledFunction direction, R5900Context &ctx)
            {
                services.yields = 0; rt->functions.setOverride(0x1da298, direction);
                direction(ram, &ctx, rt.get()); require(ctx.pc == caller, "new direction return");
                try { continuation(ram, &ctx, rt.get()); }
                catch (const SegmentEnd &) { require(kind == 0, "unexpected segment stop"); return; }
                require(kind != 0 && ctx.pc == 0xbad0000, "new direction continuation did not finish");
            };
            _mm_setcsr(control); auto a = initial, b = initial; run(originalDirection, a);
            const unsigned referenceYields = services.yields;
            const auto preserveW = [&](uint32_t at) { std::memcpy(ram + at + 12, input.get() + at - 0x10000 + 12, 4); };
            preserveW(output); if (kind == 2) preserveW(sp + 0x30);
            std::memcpy(expected.get(), ram + 0x10000, arenaBytes);
            if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++addedMemory; }
            const unsigned referenceControl = _mm_getcsr();
            std::memcpy(ram + 0x10000, input.get(), arenaBytes);
            _mm_setcsr(control); run(nativeDirection, b);
            const unsigned after = _mm_getcsr(); _mm_setcsr(saved);
            // Original scalar normalization can raise host exception flags; compare its complete FP effects too.
            if (!cpuEqual(a, b) || after != referenceControl || referenceYields != services.yields ||
                std::memcmp(expected.get(), ram + 0x10000, arenaBytes) ||
                (n < 4 && std::memcmp(whole.get(), ram, bozo::kFlatBytes)))
            {
                std::fprintf(stderr, "FAIL vehicle/light caller %x case %u mode %u CPU%d data%d FP%d yields %u/%u\n",
                    caller, n, mode, cpuEqual(a, b), std::memcmp(expected.get(), ram + 0x10000, arenaBytes), after == referenceControl,
                    referenceYields, services.yields);
                throw std::runtime_error("vehicle/light direction consumer differs");
            }
            host.native = false; a = initial; b = initial; originalDirection(ram, &a, rt.get());
            uint8_t originalOut[16]; std::memcpy(originalOut, ram + output, 16);
            nativeDirection(ram, &b, rt.get()); host.native = true;
            require(!std::memcmp(&a, &b, sizeof(a)) && !std::memcmp(originalOut, ram + output, 16), "new historical direction ABI");
            ++addedCases;
        }
    _mm_setcsr(saved);
    std::printf("Vehicle/light directions: %u generated continuation cases, %u whole-memory checks; collision point/inverse consumers, tire loops/clamps and light normalization/attenuation CPU/memory/clock/FP exact outside direction W; historical ABI exact\n", addedCases, addedMemory);
}
