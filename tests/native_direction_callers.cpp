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
        // Keep checkpoints enabled and stop just before the next unrelated result-buffer operation.
        void yield(uint8_t *, R5900Context *ctx, Runtime *) override
        {
            require(++yields < 64, "caller did not reach segment end");
            if (ctx->pc == 0x1eeafc) throw SegmentEnd{};
        }
        [[noreturn]] static void unexpected() { throw std::runtime_error("unexpected BSP caller service"); }
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
}
