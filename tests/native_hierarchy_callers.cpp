// SPDX-License-Identifier: MIT
// Real hierarchy helper consumers compiled from the local game ELF; no copied game implementation.
#include "tmb_port.h"
#include "tmb_native_math.h"
#include "tmb_hierarchy_math.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include <algorithm>
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
            require(++yields < 4096, "caller did not reach segment end");
            if (ctx->pc == stop) throw SegmentEnd{};
        }
        [[noreturn]] static void unexpected() { throw std::runtime_error("unexpected hierarchy caller service"); }
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

    constexpr uint32_t base = 0x10000, bytes = 0x40000, globals = 0x4d0000, globalBytes = 0x90000;
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
    struct SavedW { uint32_t offset, value; };
    std::vector<SavedW> referenceW;
    void referenceDirection(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        const auto offset = bozo::GuestMemory::flatOffset(getRegU32(ctx, 6) & ~15u, 16);
        require(offset >= 0, "invalid reference output");
        if (std::none_of(referenceW.begin(), referenceW.end(), [&](auto p) { return p.offset == uint32_t(offset) + 12; }))
        {
            uint32_t value; std::memcpy(&value, ram + offset + 12, 4);
            referenceW.push_back({uint32_t(offset) + 12, value});
        }
        // Leave the real original W and all VU effects intact through the complete consumer sequence.
        mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298(ram, ctx, rt);
    }
    void referenceInverse(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        const auto offset = bozo::GuestMemory::flatOffset(getRegU32(ctx, 6) & ~15u, 16);
        require(offset >= 0, "invalid inverse output");
        mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram, ctx, rt);
        // An inverse may reuse a forward record, replacing opaque W with meaningful basis[0].W.
        // Its full later write must be compared exactly, without the earlier forward-W exclusion.
        std::erase_if(referenceW, [&](auto patch) { return patch.offset == uint32_t(offset) + 12; });
    }
}

void testHierarchyHelperCallers()
{
    auto rt = std::make_unique<Runtime>(); Host host; Services services;
    rt->port = &host; rt->services = &services;
    rt->functions.setGenerated(tmb_function_table, tmb_function_count);
    rt->functions.setResume(tmb_resume_table, tmb_resume_count);
    tmb::Registry registry; tmb::math::install(registry); tmb::hierarchy::install(registry);
    const auto inverse = registry.find(0x1da1e8), direction = registry.find(0x1da298);
    require(inverse && direction, "missing hierarchy leaves");
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
    constexpr uint32_t callers[] = {0x1d0de0, 0x1d0dfc, 0x1d12d8, 0x1d4b94,
        0x1cfd18, 0x1cfde8, 0x1d0224, 0x1d0250, 0x1d25ec, 0x1d261c, 0x1d2784, 0x1d2804,
        0x1cd400, 0x1cd768, 0x1cbbc8, 0x1cbc2c, 0x1cc634, 0x1cc6b4,
        0x1ccec8, 0x1ccef8, 0x1cd884, 0x1cd8b4, 0x1d41c8, 0x1d9cac,
        0x1cdf80, 0x1ce028, 0x1ce220, 0x1ce644, 0x1ce674, 0x1ce7b4, 0x1cea40,
        0x1cdf40, 0x1cdfe4, 0x1ce1cc, 0x1ce37c, 0x1ce3a4, 0x1cdd3c};
    const auto isInverse = [](unsigned kind) { return kind < 4 || kind == 12 || kind == 13 || (kind >= 24 && kind <= 30); };
    const unsigned saved = _mm_getcsr(); unsigned cases = 0, memoryCases = 0;
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < std::size(callers); ++kind)
        for (unsigned n = 0; n < 128; ++n)
        {
            fill(); const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t v) { return v | alias; };
            R5900Context initial; context(initial, alias);
            const auto reg = [&](unsigned r, uint32_t v) { setRegU32(&initial, r, address(v)); };
            const auto returnAt = [&](uint32_t offset) { word(sp + offset, 0xbad0000); word(sp + offset + 4, 0); };
            reg(4, 0x15000); reg(5, sp + 0x20); reg(6, 0x19000);
            setRegU32(&initial, 31, callers[kind]); initial.pc = isInverse(kind) ? 0x1da1e8 : 0x1da298;
            word(gp - 0x2fb4, address(0x11000)); word(gp - 0x2fbc, address(0x12000)); word(gp - 0x2fb8, address(0x12000));
            word(gp - 0x2f94, address(0x11000)); word(gp - 0x2f9c, address(0x12000)); word(gp - 0x2fa0, address(0x13000));
            word(gp - 0x2f98, address(0x12000)); word(gp - 0x2f8c, 0);
            const uint16_t slot = n % 4;
            std::memcpy(ram + 0x53cc58, &slot, 2); std::memcpy(ram + 0x551e68, &slot, 2);
            word(0x14000, n & 1 ? 0x40000 | (n << 7) : 0); ram[0x14006] = uint8_t(n);
            // Arbitrary opaque payloads must survive native forward outputs without affecting XYZ consumers.
            word(0x1101c, bits()); word(sp + 0x1c, bits()); word(sp + 0x2c, bits()); word(sp + 0x4c, bits());
            RecompiledFunction continuation; services.stop = 0xffffffff;
            if (kind < 2)
            {
                continuation = resume_0x1d0988; services.stop = 0x1da428;
                reg(16, sp + 0x20); reg(17, sp + 0x30); reg(18, 0x18000); reg(19, 0x19000); reg(21, 0x15000);
                if (kind == 1) reg(6, sp + 0x30);
            }
            else if (kind == 2)
            {
                continuation = resume_0x1d0988; services.stop = 0x1d1bc0;
                reg(5, sp); reg(16, sp + 0x20); reg(17, 0x15000); reg(19, 0x14000);
                reg(22, 0x19000); reg(23, 0x18000); reg(30, 0x1a000);
            }
            else if (kind == 3)
            {
                continuation = resume_0x1d4ab8; services.stop = 0x201310;
                reg(5, sp + 0x50); reg(6, sp + 0x60); reg(16, sp + 0x50); reg(20, sp + 0x10);
                reg(21, 0x18000); setRegU32(&initial, 18, 0); word(0x18000, address(0x14000));
            }
            else if (kind < 6)
            {
                continuation = resume_0x1cfb10; reg(22, 0x540000); reg(23, 0x540000);
                if (kind == 4)
                { reg(5, sp + 0x20); reg(6, 0x11010); reg(16, sp + 0x20); services.stop = 0x219b38; }
                else { reg(5, sp + 0x30); reg(6, sp + 0x40); reg(16, sp + 0x40); returnAt(0xd0); }
            }
            else if (kind < 8)
            {
                continuation = resume_0x1d0190;
                reg(16, 0x53cc70); reg(17, kind == 6 ? sp + 0x20 : sp + 0x10);
                reg(18, 0x53cc30); reg(19, 0x53cc40); returnAt(0x90);
                reg(5, kind == 6 ? sp + 0x30 : sp); reg(6, kind == 6 ? 0x11010 : sp + 0x10);
            }
            else if (kind < 10)
            {
                continuation = resume_0x1d2560;
                reg(16, 0x551e80); reg(17, sp + 0x30); reg(18, sp + 0x40);
                reg(19, 0x551e40); reg(20, 0x14000); returnAt(0xa0);
                reg(5, kind == 8 ? sp + 0x10 : sp + 0x30); reg(6, kind == 8 ? 0x11010 : sp + 0x40);
            }
            else if (kind < 12)
            {
                continuation = resume_0x1d26f0;
                reg(16, sp + 0x20); reg(17, kind == 10 ? sp + 0x30 : sp + 0x10);
                reg(18, 0x54f870); reg(19, 0x551e80); reg(20, 0x550000); reg(21, 0x550000);
                reg(22, 0x551e40); reg(23, 0x14000); returnAt(0xb0);
                reg(5, kind == 10 ? sp + 0x30 : sp + 0x10); reg(6, kind == 10 ? 0x11010 : sp + 0x20);
                initial.fu[20] = n & 1 ? 0x3f800000 : 0xbf800000;
            }
            else if (kind == 12)
            {
                continuation = resume_0x1cd180;
                reg(17, 0x14000); reg(18, 0x15000); reg(19, 0x18000);
                word(gp - 0x35ec, n % 3 == 0 ? 1199 : n % 3 == 1 ? 1200 : 0);
                word(gp - 0x2fc0, 1); returnAt(0xc0);
            }
            else if (kind == 13)
            {
                continuation = resume_0x1cd538; services.stop = 0x201650;
                reg(19, sp + 0x60); reg(6, sp + 0x50);
                word(sp + 0x120, address(sp + 0x100)); word(sp + 0x11c, address(sp + 0xf0));
                word(sp + 0x130, address(sp + 0x50)); word(sp + 0x110, address(0x14000));
                word(0x1400c, address(0x17000)); word(sp + 0x128, address(sp + 0x20));
                initial.fu[20] = 0x3f800000;
            }
            else if (kind < 18)
            {
                const bool point = kind < 16, first = kind % 2 == 0;
                continuation = point ? resume_0x1cba48 : resume_0x1cc588;
                word(gp - 0x2fdc, n % 4); word(gp - 0x2fe8, address(0x11000));
                word(0x14000, n & 1 ? 0x40000 | (n << 7) : 0); ram[0x14006] = uint8_t(n);
                word(0x527a28, n % 4); reg(23, 0x520000); returnAt(point ? 0xb0 : 0xc0);
                if (point)
                {
                    reg(16, sp + 0x20); reg(17, 0x16000); reg(18, 0x14000); reg(19, sp + 0x30);
                    reg(20, sp + 0x40); reg(21, 0x18000); reg(22, 0x15000);
                    reg(5, first ? 0x16000 : sp + 0x30); reg(6, first ? 0x19010 : sp + 0x40);
                    word(0x16000, n & 2 ? 0xbf800000 : 0x3f800000);
                }
                else
                {
                    reg(17, first ? sp + 0x30 : sp + 0x20); reg(18, 0x18000); reg(19, 0x527a40);
                    reg(20, 0x5253e0); reg(21, first ? 0x520000 : sp + 0x10); reg(22, 0x520000);
                    reg(23, 0x14000); reg(30, 0x527a00);
                    reg(5, first ? sp + 0x30 : sp + 0x10); reg(6, first ? 0x19010 : sp + 0x20);
                    initial.fu[20] = n & 2 ? 0x3f800000 : 0xbf800000;
                }
            }
            else if (kind < 20)
            {
                continuation = resume_0x1cce58; services.stop = 0x1eeafc;
                word(gp - 0x2ff4, address(0x12000)); word(gp - 0x2ff8, address(0x17000));
                word(gp - 0x2fec, address(0x15000)); word(gp - 0x2ff0, address(0x13000));
                word(gp - 0x2fe8, address(0x11000));
                for (unsigned r : {16u, 17u, 18u, 21u}) setRegU32(&initial, r, 0);
                reg(19, 0x520000); reg(20, 0x520000);
                reg(22, 0x5253c0); reg(5, 0x17000); reg(6, sp);
                if (n < 16)
                {
                    std::memset(ram + 0x15000, 0, 48);
                    word(0x15000, 0x3f800000); word(0x15014, 0x3f800000); word(0x15028, 0x3f800000);
                    std::memset(ram + 0x17000, 0, 12);
                    constexpr uint32_t edge[] = {0, 0x3f000000, 0x3f800000, 0x3f7fffff};
                    word(0x17000, edge[n % 4]);
                }
            }
            else if (kind < 22)
            {
                continuation = resume_0x1cd538; services.stop = 0x1cd5d0;
                reg(17, 0x527a00); setRegU32(&initial, 18, (n % 4) * 64); reg(19, sp + 0x60);
                setRegU32(&initial, 20, 0); reg(23, sp + 0x40); reg(30, sp + 0x90);
                reg(5, kind == 20 ? sp + 0x70 : sp + 0x40); reg(6, kind == 20 ? 0x19010 : sp + 0x90);
                word(0x527a28, n % 4); word(sp + 0x11c, address(sp + 0x20));
                word(sp + 0x110, address(0x14000)); word(sp + 0x114, 2);
                word(0x14000, n & 1 ? 0x40000 | (n << 7) : 0);
                word(gp - 0x2fe8, address(0x11000));
            }
            else if (kind == 22)
            {
                continuation = resume_0x1d4188; services.stop = 0x1e38f0;
                reg(16, sp + 0x10); setRegU32(&initial,18,n % 4); reg(19,0x14000);
                reg(5,sp); reg(6,sp + 0x10); word(0x4dfcd0 + (n % 4) * 16, address(0x18000));
                word(0x14000,n << 7);
            }
            else if (kind == 23)
            {
                continuation = resume_0x1d9c30;
                setRegU32(&initial,16,2u << (n % 3)); setRegU32(&initial,17,2u << ((n >> 2) % 3));
                reg(18,0x14000); reg(19,0x16000); reg(20,0x17000); reg(5,sp + 0x60); reg(6,sp);
                word(0x14008,n & 1); word(0x14040,0x3f000000); word(0x14044,0xbf000000); returnAt(0xa0);
                if (n < 16)
                {
                    std::memset(ram + 0x15000,0,48); word(0x15000,0x3f800000); word(0x15014,0x3f800000); word(0x15028,0x3f800000);
                    constexpr uint32_t edge[] = {0,0x3effffff,0x3f000000,0x3f000001};
                    word(sp + 0x60,edge[n % 4]); word(sp + 0x64,edge[(n >> 2) % 4]);
                }
                for (unsigned i=0;i<64;++i) word(0x17000+i*4,bits());
            }
            else if (kind >= 27 && kind <= 30)
            {
                // Complete crossing tails, including the real point emitter and its capacity/duplicate guards.
                continuation = kind < 29 ? resume_0x1ce588 : kind == 29 ? resume_0x1ce738 : resume_0x1ce958;
                constexpr unsigned axes[] = {4,6,8,9}; const auto axis = axes[n % 4], index = n % 4;
                word(gp - 0x2fe8,address(0x11000)); word(gp - 0x2ff4,address(0x12000));
                word(gp - 0x2ff8,address(0x17000)); word(0x17000,address(0x15000));
                word(gp - 0x2fcc,n % 3 == 0 ? 0 : n % 3 == 1 ? 48 : 49); word(gp - 0x35f4,0);
                word(0x525470 + index * 4,(n >> 4) & 1);
                reg(5,0x16000); reg(6,kind == 28 ? sp + 0x10 : sp);
                reg(16,sp + 0x20); reg(18,0x19000);
                if (kind < 29)
                { setRegU32(&initial,17,index*64); setRegU32(&initial,19,axis); returnAt(0x90); }
                else if (kind == 29)
                { setRegU32(&initial,17,axis); setRegU32(&initial,19,index); returnAt(0x60); }
                else
                { setRegU32(&initial,17,index); setRegU32(&initial,19,axis); returnAt(0x90); }
                if (kind >= 29 && n < 48)
                {
                    std::memset(ram+0x15000,0,48); word(0x15000,0x3f800000); word(0x15014,0x3f800000); word(0x15028,0x3f800000);
                    word(0x1500c,bits()); std::memset(ram+0x19000,0,12);
                    constexpr uint32_t edge[] = {0,0x409fffff,0x40a00000,0x40a00001,0xc0a00000,0xc0a00001};
                    word(0x16000,edge[(n >> 2) % 6]); word(0x16004,edge[(n >> 2) % 6]);
                }
            }
            else if (kind == 36)
            {
                continuation = resume_0x1cd538; services.stop = 0x1cdcb8;
                reg(16,0x15000); reg(17,0x14000); reg(19,sp + 0x60); reg(5,sp + 0x50); reg(6,sp + 0x60);
                setRegU32(&initial,21,(n % 4)*64); setRegU32(&initial,22,0); setRegU32(&initial,30,0);
                word(gp - 0x2fe8,address(0x11000));
            }
            else
            {
                continuation = resume_0x1cdeb8;
                const bool publish = kind == 34 || kind == 35;
                services.stop = publish ? 0xffffffff : 0x201650;
                setRegU32(&initial,16,0); setRegU32(&initial,20,64);
                setRegU32(&initial,21,(n % 4)*64); setRegU32(&initial,30,(n % 4)*64);
                reg(17,0x527a00); reg(18,sp + 0x30); reg(19,sp + 0x20);
                reg(22,0x527a40); reg(23,sp + 0x70);
                word(0x527a28,n % 4); word(gp - 0x2ff8,address(0x17000));
                word(gp - 0x2fec,address(0x15000)); word(gp - 0x2ff4,address(0x12000));
                word(gp - 0x2ff0,address(0x13000)); word(gp - 0x2fe8,address(0x11000));
                ram[0x1703a] = uint8_t(n & 1);
                word(sp + 0x90,address(0x14000)); word(0x1400c,address(0x18000));
                word(0x14000,n & 1 ? 0x40000 | (n << 7) : 0);
                if (kind == 24 || kind == 25) reg(6,sp + 0x30);
                if (kind == 31 || kind == 32)
                { reg(5,kind == 31 ? 0x16000 : 0x17040); reg(6,sp + 0x20); }
                if (kind == 26 || kind == 33)
                {
                    reg(17,sp + 0x80); reg(20,sp + 0x40);
                    reg(6,sp + 0x30); if (kind == 33) reg(5,0x16000);
                }
                if (publish)
                {
                    reg(16,0x527a40); reg(20,sp + 0x40);
                    reg(5,kind == 34 ? sp + 0x50 : sp + 0x20);
                    reg(6,kind == 34 ? 0x11010 + (n % 4)*64 : sp + 0x30);
                    ram[sp+0x6e]=uint8_t(n); returnAt(0x130);
                }
                if (kind == 25 && n < 16)
                {
                    std::memset(ram+0x15000,0,48);word(0x15000,0x3f800000);word(0x15014,0x3f800000);word(0x15028,0x3f800000);
                    word(0x1500c,bits());std::memset(ram+sp+0x20,0,12);
                    constexpr uint32_t edge[] = {0,0x3f000000,0x3f800000,0x3f7fffff};word(sp+0x20,edge[n % 4]);
                }
            }
            snapshot(input.get());
            const auto run = [&](bool native, R5900Context &ctx) {
                referenceW.clear(); services.yields = 0;
                rt->functions.setOverride(0x1da1e8, native ? inverse : referenceInverse);
                rt->functions.setOverride(0x1da298, native ? direction : referenceDirection);
                auto helper = isInverse(kind) ? (native ? inverse : referenceInverse) : (native ? direction : referenceDirection);
                helper(ram, &ctx, rt.get()); require(ctx.pc == callers[kind], "wrong hierarchy return");
                try { continuation(ram, &ctx, rt.get()); }
                catch (const SegmentEnd &) { require(services.stop != 0xffffffff, "unexpected segment end"); return; }
                require(services.stop == 0xffffffff && ctx.pc == 0xbad0000, "hierarchy consumer returned early");
            };
            const unsigned control = ((saved | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1 ? 0x8040u : 0u);
            auto a = initial, b = initial; _mm_setcsr(control);
            try { run(false, a); }
            catch (...) { std::fprintf(stderr, "hierarchy original kind %u case %u\n", kind, n); throw; }
            const auto refControl = _mm_getcsr(); const auto refYields = services.yields;
            // Normalize only forward-output W after all real consumers have run, exposing any W-dependent result.
            for (auto patch : referenceW) std::memcpy(ram + patch.offset, &patch.value, 4);
            snapshot(expected.get()); if (n < 4) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++memoryCases; }
            restore(); _mm_setcsr(control); run(true, b); const auto after = _mm_getcsr(); _mm_setcsr(saved);
            if (!cpuEqual(a,b) || !vuUntouched(initial,b) || after != refControl || services.yields != refYields || !dataEqual() ||
                (n < 4 && std::memcmp(whole.get(),ram,bozo::kFlatBytes)))
            { std::fprintf(stderr,"FAIL hierarchy %x case %u mode %u CPU%d VU%d FP%d data%d\n",callers[kind],n,mode,cpuEqual(a,b),vuUntouched(initial,b),after==refControl,dataEqual()); throw std::runtime_error("hierarchy consumer differs"); }
            ++cases;
        }
    host.enabled = false; unsigned historical = 0;
    for (unsigned kind = 4; kind < std::size(callers); ++kind) for (unsigned n = 0; n < 64; ++n)
    {
        if (isInverse(kind)) continue;
        fill(); R5900Context initial; context(initial, 0);
        setRegU32(&initial,4,0x15000+(n&15)); setRegU32(&initial,5,0x16000+((n>>2)&15));
        setRegU32(&initial,6,0x17000+(n&15)); setRegU32(&initial,31,callers[kind]);
        snapshot(input.get()); auto a=initial,b=initial;
        mathfMulTransVec__FPA3_A3_fP8_fvectorT1_0x1da298(ram,&a,rt.get()); snapshot(expected.get());
        if (n < 4) std::memcpy(whole.get(),ram,bozo::kFlatBytes);
        restore(); direction(ram,&b,rt.get());
        require(!std::memcmp(&a,&b,sizeof(a)) && dataEqual() && (n >= 4 || !std::memcmp(whole.get(),ram,bozo::kFlatBytes)), "historical hierarchy direction ABI");
        ++historical;
    }
    _mm_setcsr(saved);
    std::printf("hierarchy consumers: %u generated sequences, %u whole-memory checks; CPU/clock/FP exact outside retired VU/forward W; native VU untouched; %u historical forward ABI cases (%u whole-memory)\n", cases, memoryCases, historical, historical / 16);
}
