// SPDX-License-Identifier: MIT
// A scheduler may enter a flare leaf after particleDraw's native C++ scope has unwound.
#include "tmb_port.h"
#include "tmb_host.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::particles { void install(tmb::Registry &); }
namespace
{
    void require(bool okay, const char *message) { if (!okay) throw std::runtime_error(message); }
    struct Host : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return true; }
        void frameEnd(Runtime *, R5900Context *) override { throw std::runtime_error("unexpected frame"); }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
    };
    struct Services : bozo::RuntimeServices
    {
        [[noreturn]] static void unexpected() { throw std::runtime_error("native flare entered generated code or a runtime service"); }
        void unknownTarget(uint8_t *, R5900Context *, Runtime *, uint32_t) override { unexpected(); }
        void yield(uint8_t *, R5900Context *, Runtime *) override { unexpected(); }
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
}
int main()
{
    tmb::host::setEnvDefault("TMB_NATIVE", "1");
    auto rt = std::make_unique<Runtime>(); Host host; Services services;
    rt->port = &host; rt->services = &services;
    tmb::Registry registry; tmb::particles::install(registry);
    const auto test = registry.find(0x1dfe00), render = registry.find(0x1e01c8);
    require(test && render, "flare overrides missing");
    auto *ram = rt->memory.flat();
    const auto word = [&](uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); };
    const auto half = [&](uint32_t at, uint16_t v) { std::memcpy(ram + at, &v, 2); };
    const auto value = [&](uint32_t at, float v) { std::memcpy(ram + at, &v, 4); };
    const auto read = [&](uint32_t at) { uint32_t v; std::memcpy(&v, ram + at, 4); return v; };
    unsigned cases = 0;
    // No particleDraw call, recent frame marker, or live C++ native-flare scope exists in this process.
    // The native backend must answer these valid leaf entries directly, just as after a scheduler resume.
    for (unsigned view = 0; view < 4; ++view) for (unsigned visible = 0; visible < 2; ++visible)
    {
        constexpr uint32_t gp = 0x40000, list = 0x20000, viewport = 0x21000, chain = 0x22000;
        std::memset(ram + list, 0, 0x1000); std::memset(ram + viewport, 0, 0x100);
        std::memset(ram + chain, 0x5a, 256);
        word(gp - 0x2f28 + (view & 1) * 4, 1); word(gp - 0x2f20 + (view & 1) * 4, list);
        word(0x4dfcd0 + view * 16 + 4, viewport);
        half(viewport + 4, 640); half(viewport + 6, 224);
        word(viewport + 0x18, 2048); word(viewport + 0x1c, 2048);
        std::memset(ram + 0x4a1240 + view * 0xf0, 0, 16);
        const uint32_t matrix = 0x4e0230 + view * 64;
        std::memset(ram + matrix, 0, 64);
        for (unsigned row = 0; row < 4; ++row) value(matrix + row * 20, 1.f);
        value(matrix + 12, 2048.f); value(matrix + 28, 2048.f); value(matrix + 44, 10.f);
        value(list, visible ? 0.f : 10000.f); value(list + 8, 5.f);
        word(list + 0x10, 0x80); value(list + 0x2c, 1.f);
        std::memset(ram + gp - 0x34c0, 0, 8);
        std::memset(ram + 0x7435c0, 0xa5, 64);
        R5900Context ctx;
        for (unsigned reg = 1; reg < 32; ++reg) for (auto &lane : ctx.r[reg].uw) lane = 0xa5c31987u ^ reg;
        for (unsigned reg = 1; reg < 32; ++reg) for (auto &lane : ctx.vu0_vf[reg].u) lane = 0x7f812345u ^ reg;
        ctx.vu0_acc.u[0] = 0xdeadbeef; ctx.vi[5] = 0x1234; ctx.vu0_status = 0xabcdef;
        ctx.vu0_mac = 0xdead; ctx.vu0_clip_flags = 0xbeef; ctx.cycles = 123; ctx.checkpoints = 9;
        setRegU32(&ctx, 4, view); setRegU32(&ctx, 28, gp); setRegU32(&ctx, 29, 0x30000);
        setRegU32(&ctx, 31, 0xbad0000); ctx.pc = 0x1dfe00;
        const auto before = ctx;
        try { test(ram, &ctx, rt.get()); }
        catch (const std::exception &e) { std::fprintf(stderr, "FAIL native resumed flare test: %s\n", e.what()); return 1; }
        require(ctx.pc == 0xbad0000 && getRegU32(&ctx, 2) == visible, "resumed flare test result");
        auto expected = before; setRegU32(&expected, 2, visible); expected.r[2].ud[1] = 0; expected.pc = 0xbad0000;
        require(!std::memcmp(&expected, &ctx, sizeof(ctx)), "native flare test CPU ABI");
        require(read(gp - 0x2f28 + (view & 1) * 4) == visible, "flare compaction count");
        require(!std::memcmp(before.vu0_vf, ctx.vu0_vf, sizeof(ctx.vu0_vf)) && ctx.cycles == before.cycles &&
                ctx.checkpoints == before.checkpoints, "native flare changed VU or clocks");
        if (visible)
        {
            require(read(list) == 0x45000000 && read(list + 4) == 0x45000000 && read(list + 8) == 0x41700000 &&
                    read(list + 0x14) == 0x42c80000, "native flare projection record");
            for (unsigned i = 0; i < 16; ++i) require(read(0x7435c0 + i * 4) == 0, "missing native depth must stay far");
        }
        setRegU32(&ctx, 4, chain); setRegU32(&ctx, 5, view); ctx.pc = 0x1e01c8;
        expected = ctx; setRegU32(&expected, 2, chain); expected.r[2].ud[1] = 0; expected.pc = 0xbad0000;
        try { render(ram, &ctx, rt.get()); }
        catch (const std::exception &e) { std::fprintf(stderr, "FAIL native resumed flare render: %s\n", e.what()); return 2; }
        require(ctx.pc == 0xbad0000 && getRegU32(&ctx, 2) == chain, "resumed flare render result");
        require(!std::memcmp(&expected, &ctx, sizeof(ctx)), "native flare render CPU ABI");
        if (visible) require(read(list + 0x2c) == 0x3f800000, "native flare visibility");
        for (unsigned i = 0; i < 256; ++i) require(ram[chain + i] == 0x5a, "flare wrote a legacy output packet");
        ++cases;
    }
    std::printf("native particles: %u flare test/render continuations without a live particleDraw scope or frame marker; four views, culling/projection/depth/visibility, VU/clocks and untouched packet output pass\n", cases);
}
