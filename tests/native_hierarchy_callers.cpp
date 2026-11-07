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

    constexpr uint32_t base = 0x10000, bytes = 0x40000, globals = 0x4c0000, globalBytes = 0xa0000;
    constexpr uint32_t debug = 0x7484f0, debugBytes = 0x220; // actual terrain line/GS-line producer records
    constexpr std::pair<uint32_t,uint32_t> regions[] = {{base,bytes},{globals,globalBytes},{debug,debugBytes},{0x66fcc4,4}};
    constexpr size_t fixtureBytes = bytes + globalBytes + debugBytes + 4;
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
    auto input = std::make_unique<uint8_t[]>(fixtureBytes), expected = std::make_unique<uint8_t[]>(fixtureBytes);
    auto whole = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const auto word = [&](uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); };
    const auto fill = [&] {
        for (auto region : regions)
            for (uint32_t i = 0; i < region.second; i += 4)
            { const float f = float(int(bits() % 8193) - 4096) / 128.f; std::memcpy(ram + region.first + i, &f, 4); }
    };
    const auto snapshot = [&](uint8_t *out) {
        for (auto [at,size] : regions) { std::memcpy(out,ram + at,size); out += size; }
    };
    const auto restore = [&] {
        const auto *in=input.get(); for (auto [at,size] : regions) { std::memcpy(ram + at,in,size); in += size; }
    };
    const auto dataEqual = [&] {
        const auto *in=expected.get(); for (auto [at,size] : regions) { if (std::memcmp(in,ram + at,size)) return false; in += size; } return true;
    };
    constexpr uint32_t callers[] = {0x1d0de0, 0x1d0dfc, 0x1d12d8, 0x1d4b94,
        0x1cfd18, 0x1cfde8, 0x1d0224, 0x1d0250, 0x1d25ec, 0x1d261c, 0x1d2784, 0x1d2804,
        0x1cd400, 0x1cd768, 0x1cbbc8, 0x1cbc2c, 0x1cc634, 0x1cc6b4,
        0x1ccec8, 0x1ccef8, 0x1cd884, 0x1cd8b4, 0x1d41c8, 0x1d9cac,
        0x1cdf80, 0x1ce028, 0x1ce220, 0x1ce644, 0x1ce674, 0x1ce7b4, 0x1cea40,
        0x1cdf40, 0x1cdfe4, 0x1ce1cc, 0x1ce37c, 0x1ce3a4, 0x1cdd3c,
        0x1c0684, 0x1c0884, 0x1c0a70, 0x1c5d0c, 0x1c639c, 0x1c6bf4, 0x1c6c94, 0x1c6f04,
        0x1c7620, 0x1c6bac, 0x1c6c54, 0x1c7134, 0x1c7160,
        0x1c7860, 0x1c7904, 0x1c7ae4, 0x1c7d00, 0x1c9984, 0x1c9b3c, 0x1ca3b8, 0x1caa48,
        0x1c7818, 0x1c78c4, 0x1c7a90, 0x1c7bc8, 0x1c7bf8, 0x1c7de8, 0x1c7e14,
        0x1c4fe0, 0x1c51dc, 0x1c53b4, 0x1c8094, 0x1c80ac, 0x1c8120, 0x1c8150,
        0x1c85cc, 0x1c85e0, 0x1c8608, 0x1c863c,
        0x1ef318, 0x1ef44c, 0x1ef484, 0x1ef4c0, 0x1ef4fc, 0x1ef68c, 0x1ef6d4,
        0x1efc5c, 0x1efca0, 0x1efef8, 0x1f06b4,
        0x1f11a8, 0x1f11ec, 0x1f1b88, 0x1f1c38,
        0x1f2480, 0x1f24c0, 0x1f2e4c, 0x1f2ef8, 0x1f3110, 0x1f315c,
        0x1c8f10, 0x1c8f3c, 0x1c8fd8, 0x1c9000, 0x1c908c, 0x1c90b4,
        0x1c9140, 0x1c9168, 0x1c91f4, 0x1c921c, 0x1c959c, 0x1c96e8, 0x1d4514, 0x1d4560,
        0x1f5414, 0x1f5458, 0x1f54f4, 0x1f5538, 0x1f55d4, 0x1f5618, 0x1f56a8, 0x1f5904};
    const auto isInverse = [](unsigned kind) { return kind < 4 || kind == 12 || kind == 13 || (kind >= 24 && kind <= 30) || (kind >= 37 && kind <= 45) || (kind >= 50 && kind <= 57) || (kind >= 65 && kind <= 67) || (kind >= 76 && kind <= 96) || kind >= 111; };
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
            else if (kind < 37)
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
            else if (kind < 40)
            {
                continuation = resume_0x1c0300;
                reg(18,0x14000); reg(20,0x16000); word(0x14000,0x40000);
                if (kind < 39)
                {
                    services.stop = 0x2bad000; setRegU32(&initial,21,services.stop);
                    reg(4,0x16000); reg(5,sp + 0xa0); reg(6,sp);
                    if (kind == 37) { reg(17,0x14000); reg(22,sp + 0xa0); reg(30,0x17000); }
                    else { reg(16,sp + 0xa0); reg(19,0x15000); reg(22,0x17000); }
                }
                else
                {
                    services.stop = 0x1e6980; reg(4,0x15000); reg(5,0x14010); reg(6,sp + 0x20);
                    reg(16,0x14010); reg(20,0x15000); reg(22,sp + 0x20); reg(30,sp + 0x30);
                }
            }
            else if (kind == 40 || kind == 41 || kind == 45)
            {
                continuation = kind == 40 ? resume_0x1c5470 : kind == 41 ? resume_0x1c60e8 : resume_0x1c73a0;
                word(gp - 0x3004,kind == 45 ? (n % 3 == 0 ? 1199 : n % 3 == 1 ? 1200 : 0) : n & 1 ? 1199 : 0);
                if (kind == 40) { services.stop = 0x1da428; reg(21,0x15000); }
                else if (kind == 41)
                {
                    services.stop = 0x1da428; reg(18,0x18000); reg(19,0x15000); reg(30,0x510000);
                    std::memcpy(ram + 0x18028,&slot,2);
                }
                else
                {
                    reg(17,0x14000); reg(18,0x15000); reg(19,0x18000);
                    std::memcpy(ram + 0x18028,&slot,2); word(gp - 0x2ffc,1); returnAt(0xc0);
                }
            }
            else if (kind < 50)
            {
                word(gp - 0x3028,address(0x17000)); word(gp - 0x3024,address(0x12000));
                word(gp - 0x3020,address(0x13000)); word(gp - 0x301c,address(0x15000));
                std::memcpy(ram + 0x512758,&slot,2); ram[0x1703a] = uint8_t(n & 1);
                reg(5,kind == 46 || kind == 48 ? 0x16000 : kind == 47 || kind == 49 ? 0x17000 : sp);
                reg(6,kind >= 46 ? sp : sp + 0x10);
                setRegU32(&initial,20,0);
                if (kind >= 48)
                {
                    continuation = resume_0x1c70c0; services.stop = 0x21d0b8;
                    reg(18,0x510110); setRegU32(&initial,19,0);
                }
                else
                {
                    continuation = resume_0x1c6ad0; services.stop = 0x1da710;
                    reg(16,sp + 0x10); reg(17,0x512730); reg(19,0x19000); reg(21,sp + 0x20);
                    reg(22,0x512770); reg(23,0x14000); reg(30,0x512740);
                    setRegU32(&initial,18,0);
                    word(sp + 0x40,address(0x19000)); word(0x14008,1);
                    if (kind == 44) { setRegU32(&initial,18,0); reg(21,0x510000); setRegU32(&initial,30,1); }
                    if (kind == 43 && n < 16)
                    {
                        std::memset(ram + 0x15000,0,48); word(0x15000,0x3f800000); word(0x15014,0x3f800000); word(0x15028,0x3f800000);
                        word(0x1500c,bits()); std::memset(ram + sp,0,12);
                        constexpr uint32_t edge[] = {0,0x3f000000,0x3f800000,0x3f7fffff}; word(sp,edge[n % 4]);
                    }
                }
            }
            else if (kind >= 54 && kind <= 57)
            {
                // Root continuations publish packed stack state before actual traversal dispatch.
                if (kind < 56)
                {
                    continuation = resume_0x1c9858; services.stop = 0x1cae18;
                    reg(6,0x525490); word(gp - 0x2fe4,address(0x14000));
                    if (kind == 54) reg(16,0x525490);
                    else { reg(18,0x525490); reg(19,0x525480); }
                }
                else
                {
                    continuation = kind == 56 ? resume_0x1c9bf8 : resume_0x1ca798;
                    services.stop = 0x1da428; word(gp - 0x35ec,n & 1 ? 1199 : 0);
                    if (kind == 56) reg(21,0x15000);
                    else { reg(18,0x18000); reg(19,0x15000); reg(30,0x520000); std::memcpy(ram + 0x18028,&slot,2); }
                }
            }
            else if (kind < 65)
            {
                continuation = resume_0x1c7758;
                const bool publish = kind >= 61;
                services.stop = publish ? (kind < 63 ? 0x1c77d8 : 0xffffffff) : 0x201650;
                word(gp - 0x3028,address(0x17000)); word(gp - 0x3024,address(0x12000));
                word(gp - 0x3020,address(0x13000)); word(gp - 0x301c,address(0x15000));
                word(gp - 0x3018,address(0x11000)); std::memcpy(ram + 0x512758,&slot,2);
                word(sp + 0x90,address(0x14000)); word(0x1400c,address(0x18000));
                word(sp + 0x94,address(sp + 0x80)); word(sp + 0x98,address(sp + 0x50));
                word(sp + 0x9c,64); word(sp + 0xa0,0x70); word(sp + 0xa4,0);
                word(0x17004,3); ram[0x1703a] = uint8_t(n & 1);
                reg(16,0x512730); reg(17,sp + 0x30); reg(18,sp + 0x20); reg(19,sp + 0x70);
                reg(20,0x512770); reg(21,0x510110); setRegU32(&initial,22,64); setRegU32(&initial,23,0);
                reg(30,0x512740); reg(5,sp + 0x20); reg(6,sp + 0x30);
                if (kind == 58 || kind == 59) { reg(5,kind == 58 ? 0x16000 : 0x17070); reg(6,sp + 0x20); }
                if (kind == 52 || kind == 60)
                {
                    reg(16,sp + 0x80); reg(19,sp + 0x70); reg(20,sp + 0x40);
                    if (kind == 60) reg(5,sp + 0x20);
                }
                if (kind == 53)
                { reg(17,0x512730); reg(19,sp + 0x30); reg(5,0x510100); }
                if (publish)
                {
                    reg(20,sp + 0x40); reg(21,0x512730);
                    reg(5,kind == 61 || kind == 63 ? sp + 0x50 : sp + 0x20);
                    reg(6,kind == 61 || kind == 63 ? 0x11010 + (kind == 61 ? 64 : 0) : sp + 0x30);
                    if (kind < 63) { reg(16,0x512730); setRegU32(&initial,23,0); }
                    else { reg(17,0x512730); reg(18,sp + 0x40); reg(19,sp + 0x30); reg(20,0x512770); setRegU32(&initial,21,0xfffc0000); returnAt(0x140); }
                    ram[sp + 0x6e]=uint8_t(n);
                }
                if (kind == 51 && n < 16)
                {
                    std::memset(ram + 0x15000,0,48); word(0x15000,0x3f800000); word(0x15014,0x3f800000); word(0x15028,0x3f800000);
                    word(0x1500c,bits()); std::memset(ram + sp + 0x20,0,12);
                    constexpr uint32_t edge[] = {0,0x3f000000,0x3f800000,0x3f7fffff}; word(sp + 0x20,edge[n % 4]);
                }
            }
            else if (kind < 68)
            {
                continuation = kind == 65 ? resume_0x1c4ec0 : resume_0x1c50a0;
                services.stop = 0x1c6818; reg(5,sp); reg(6,0x5101c0);
                word(gp - 0x3014,address(0x14000)); word(0x14000,address(0x18000));
                word(0x18000,0x40000 | (n << 7));
                if (kind == 65) { reg(16,0x5101c0); reg(17,0x15000); reg(18,0x14000); }
                else
                {
                    reg(4,0x14020);
                    if (kind == 66) reg(16,0x5101c0);
                    else { reg(18,0x5101c0); reg(19,0x5101b0); }
                }
            }
            else if (kind < 76)
            {
                // Complete point/ray result tails: scalar reconstruction, world sums and packed metadata.
                const bool point = kind < 72, indexed = kind % 2 == 0, normal = kind < 70 || (kind >= 72 && kind < 74);
                continuation = point ? resume_0x1c7eb0 : resume_0x1c8280;
                word(gp - 0x3614,indexed); word(gp - 0x3018,address(0x11000));
                word(gp - 0x3024,address(0x12000));
                const uint32_t index = n % 4, offset = (n % 4)*16;
                if (point)
                {
                    reg(16,0x14030); reg(17,sp + 0x20); setRegU32(&initial,18,index*4);
                    reg(19,0x14000); setRegU32(&initial,20,index); reg(21,sp + 0x10);
                    reg(22,0x16000); reg(23,0x15000); reg(30,0x510000);
                    reg(5,normal ? 0x14030 : sp + 0x10);
                    reg(6,normal ? 0x11010 + (indexed ? index*64 : 0) : sp + 0x20); returnAt(0xc0);
                }
                else
                {
                    setRegU32(&initial,16,offset); reg(18,sp + 0x120); reg(20,0x14000);
                    reg(23,0x17000); reg(30,0x16000);
                    word(sp + 0x140,address(0x15000)); word(sp + 0x144,index); word(sp + 0x148,address(sp + 0x110));
                    reg(5,normal ? 0x14030 + offset : sp + 0x110);
                    reg(6,normal ? 0x11010 + (indexed ? index*64 : 0) : sp + 0x120); returnAt(0x1e0);
                }
            }
            else if (kind == 76)
            {
                continuation = resume_0x1ef090; services.stop = n & 1 ? 0x1eec38 : 0xffffffff;
                reg(5,sp); reg(6,0x18000); reg(16,0x18000); reg(17,0x14000); reg(18,0x15000); reg(20,0x19000);
                word(0x14004,1 + (n & 1)); word(0x140cc,address(0x16000)); returnAt(0xb0);
            }
            else if (kind < 83)
            {
                // Complete terrain-node tails: four corners, real height scan, extrema and aggregate bounds.
                continuation = resume_0x1ef3c0;
                reg(4,0x14010); reg(5,sp + 0x20); reg(6,sp + 0x10);
                reg(16,0x14000); reg(17,sp + 0x10); reg(18,0x14010); reg(19,sp + 0x20);
                reg(20,0x4d0000); reg(21,sp + 0x50); reg(22,sp + 0x60); returnAt(0x100);
                const uint16_t width = n % 4, height = (n >> 2) % 4;
                std::memcpy(ram + 0x1409c,&width,2); std::memcpy(ram + 0x1409e,&height,2); std::memcpy(ram + 0x140a0,&width,2);
                word(0x14090,address(0x19000)); word(gp - 0x33c4,1 + (n & 1));
                for (unsigned i=0;i<16;++i)
                { constexpr uint16_t heights[] = {0,0x561e,0x561f,0xffff}; const uint16_t h=heights[(i+n)%4]; std::memcpy(ram+0x19000+i*8,&h,2); }
                const float x = kind == 78 || kind == 80 ? float(int(width)-1) : 0.f;
                const float y = kind == 79 || kind == 80 ? float(int(height)-1) : 0.f;
                std::memcpy(ram + sp + 0x20,&x,4); std::memcpy(ram + sp + 0x24,&y,4); word(sp + 0x28,0);
                word(sp + 0x80,width ? width-1 : 0); word(sp + 0x84,height ? height-1 : 0); word(sp + 0x88,0xffff);
                initial.fu[20]=0x42000000; initial.fu[21]=0xc2000000; initial.fu[22]=0x42000000; initial.fu[23]=0xc2000000; initial.fu[24]=0xbf800000;
            }
            else if (kind < 85)
            {
                continuation = resume_0x1efb98; services.stop = 0x1f2370;
                word(gp - 0x33c8,address(0x14000)); reg(16,0x18000); reg(18,0x16000); reg(19,sp + 0x30); reg(20,sp + 0x20);
                reg(4,kind == 83 ? 0x14010 : 0x14040); reg(5,sp); reg(6,kind == 83 ? sp + 0x20 : sp + 0x30);
            }
            else if (kind < 87)
            {
                // Keep valid grid coordinates around integer boundaries through actual row-stride dispatch.
                continuation = kind == 85 ? resume_0x1efe10 : resume_0x1f0550; services.stop = 0x217ae8;
                word(gp - 0x33c8,address(0x14000)); word(gp - 0x33a0,9); word(gp - 0x339c,0);
                const uint16_t dimension=9; std::memcpy(ram+0x1408c,&dimension,2); std::memcpy(ram+0x1408e,&dimension,2);
                reg(4,0x14040); reg(5,0x16000); reg(6,0x4ca390); reg(19,0x16000); reg(20,0x4ca390);
                if (kind == 85) { reg(16,0x4d0000); setRegU32(&initial,18,0); }
                else { reg(30,0x4d0000); setRegU32(&initial,22,0); }
                std::memset(ram+0x14040,0,48); word(0x14040,0x3f800000); word(0x14054,0x3f800000); word(0x14068,0x3f800000);
                constexpr uint32_t edge[] = {0,1,0x3f7fffff,0x3f800000,0x3f800001,0x40dfffff,0x40e00000,0x40e00001};
                word(0x16000,edge[n%8]); word(0x16004,edge[(n>>3)%8]); word(0x16008,0x3f800000); word(0x1402c,0);
            }
            else if (kind < 89)
            {
                // Paired LOS inputs, including the context translations, through segment subtraction dispatch.
                continuation = resume_0x1f10c0; services.stop = 0x1daf28;
                word(gp - 0x33c8,address(0x14000));
                word(sp + 0x16c,address(0x16000)); word(sp + 0x1c8,address(sp + 0x10));
                word(sp + 0x1cc,address(sp + 0x20));
                reg(4,0x14040); reg(5,kind == 87 ? 0x15000 : 0x16000); reg(6,kind == 87 ? sp : sp + 0x10);
            }
            else if (kind < 91)
            {
                // World position, complete normal normalization/copy and distance publication before debug draw.
                continuation = resume_0x1f10c0; services.stop = 0x1f50f0;
                word(gp - 0x33c8,address(0x14000)); word(gp - 0x33d8,1);
                word(sp + 0x170,address(0x18000)); word(sp + 0x1c0,address(sp + 0x110));
                reg(4,0x14000); reg(5,kind == 89 ? sp + 0x90 : sp + 0xb0); reg(6,sp + 0x110);
                reg(16,0x18010); reg(22,sp + 0xc0); reg(30,sp + 0xa0);
                initial.fu[20]=0x41200000; initial.fu[21]=0x40800000;
                initial.fu[22]=0x3f000000; initial.fu[23]=0x3f800000;
                initial.fu[24]=0xbf000000; initial.fu[25]=0xbf800000;
            }
            else if (kind < 93)
            {
                // Paired cell BSP endpoints, real scalar segment records and BSP pointer lookup.
                continuation = resume_0x1f2370; services.stop = 0x201650;
                reg(4,0x14000); reg(5,kind == 91 ? 0x15000 : 0x16000); reg(6,kind == 91 ? sp + 0xd0 : sp + 0xe0);
                reg(20,0x14000); reg(21,0x17000); reg(22,0x15000); reg(23,0x19000);
                word(sp + 0x140,address(0x16000)); word(0x14088,address(0x18000));
                const uint16_t index=n%4; std::memcpy(ram + 0x17002,&index,2);
                for (unsigned i=0;i<4;++i) word(0x18000+4*i,address(0x1a000+16*i));
            }
            else if (kind < 95)
            {
                // Complete cell-result tail, retaining meaningful transformed W in the result copy.
                continuation = resume_0x1f2370;
                reg(4,0x14000); reg(5,kind == 93 ? sp + 0x90 : sp + 0xb0); reg(6,sp + 0xd0);
                reg(16,sp + 0xd0); reg(20,0x14000); reg(23,0x18000); returnAt(0x1f0);
                word(gp - 0x33d8,0); word(gp - 0x33e0,0); word(gp - 0x33dc,0);
                word(sp + 0x144,n & 1); initial.fu[20]=0x41200000; initial.fu[21]=0x40800000;
                initial.fu[22]=0x3f000000; initial.fu[23]=0x3f800000;
                initial.fu[24]=0xbf000000; initial.fu[25]=0xbf800000;
            }
            else if (kind < 97)
            {
                // Paired ray endpoints plus translation through segment subtraction dispatch.
                continuation = resume_0x1f30a0; services.stop = 0x1daf28;
                reg(4,0x14040); reg(5,kind == 95 ? 0x15000 : 0x16000); reg(6,kind == 95 ? sp : sp + 0x10);
                reg(16,0x14040); reg(17,sp + 0x10); reg(23,0x14000);
                word(sp + 0x54,address(0x16000));
            }
            else if (kind < 109)
            {
                // Real car debug-line producer: transformed corners, world sums, colour and XYZW records.
                continuation = resume_0x1c8db0; services.stop = 0x1c1e00;
                const unsigned which=kind-97, pair=which/2;
                constexpr uint32_t inputs[] = {0xb0,0xc0,0xc0,0xd0,0xd0,0xe0,0xe0,0xb0,0xf0,0x100};
                word(gp - 0x2ff8,address(0x14000)); word(gp - 0x2ff4,address(0x16000));
                word(gp - 0x2fe8,address(0x18000)); word(0x14000,address(0x15000));
                word(sp + 0x70,address(sp + 0x20)); word(sp + 0x60,address(0x17000));
                reg(4,0x15020); reg(30,sp + 0x10); reg(22,sp + 0x10);
                reg(5,0x14000 + (which < 10 ? inputs[which] : 0x160 + (n%4)*16));
                reg(6,which < 10 && !(which & 1) ? sp : sp + 0x10);
                if (which < 10) ram[0x18000 + 0x130 + pair*0x40]=uint8_t(n & 1);
                word(sp + 0x0c,bits()); word(sp + 0x1c,bits());
            }
            else if (kind < 111)
            {
                // Actual view lookup and XYZ addition; initialized nodes return, new nodes reach FX creation.
                continuation = resume_0x1d4498; services.stop = kind == 109 ? 0xffffffff : 0x1e6980;
                reg(5,sp); reg(6,sp + 0x10); reg(16,sp + 0x10); reg(17,0x14000);
                setRegU32(&initial,19,n%4); word(0x4dfcd0 + (n%4)*16,address(0x18000));
                word(sp + 0x1c,bits()); returnAt(0x70);
            }
            else if (kind < 117)
            {
                // Paired inverse/world sums through actual line, GS-line or box vertex construction.
                const unsigned which=kind-111, group=which/2;
                continuation = group == 0 ? resume_0x1f53e0 : group == 1 ? resume_0x1f54b8 : resume_0x1f55a0;
                services.stop = 0x1c1e00;
                reg(4,0x14000); reg(5,which & 1 ? 0x16000 : 0x15000); reg(6,which & 1 ? sp + 0x10 : sp);
                reg(16,0x14000); reg(17,0x16000); reg(18,sp + 0x10); reg(19,0x17000); reg(20,0x18000);
                word(0x1400c,bits()); word(0x1700c,bits()); word(0x1800c,bits());
            }
            else if (kind == 117)
            {
                // Complete terrain edit tail: integer cell selection, protection flag and real no-input polls.
                continuation = resume_0x1f5678; returnAt(0x20);
                reg(4,0x14040); reg(5,0x16000); reg(6,sp); word(gp - 0x33c8,address(0x14000));
                std::memset(ram + 0x14040,0,48); word(0x14040,0x3f800000); word(0x14054,0x3f800000); word(0x14068,0x3f800000);
                word(0x14080,address(0x19000)); const uint16_t stride=9; std::memcpy(ram + 0x14090,&stride,2);
                for (unsigned i=0;i<81;++i) {
                    const uint16_t height=uint16_t(n*521+i), flags=n & 1 ? 0x100 : 0;
                    std::memcpy(ram + 0x19000+i*8,&height,2); std::memcpy(ram + 0x19006+i*8,&flags,2);
                }
                word(0x66fcc4,0); // actual inputGetInput disabled-device branch, no synthetic completion
                constexpr uint32_t edge[] = {0,1,0x3f7fffff,0x3f800000,0x3f800001,0x40dfffff,0x40e00000,0x40e00001};
                word(0x16000,edge[n%8]); word(0x16004,edge[(n>>3)%8]); word(0x16008,0x3f800000);
            }
            else
            {
                // Real wave-type table: bound builders stop at timer input, special types at their actual subcall.
                continuation = resume_0x1f5898;
                const unsigned type=n%6, index=(n>>3)%4; const uint32_t wave=0x4ce890+index*0x3b0;
                services.stop = type == 2 ? 0x1f5c38 : type == 3 ? 0x1dc248 : 0x1f6a28;
                reg(4,0x14040); reg(5,0x16000); reg(6,wave+0x10); reg(16,wave); reg(20,0x4d0000);
                setRegU32(&initial,17,type); setRegU32(&initial,18,index); word(wave+0x60,address(0x14000)); word(wave+4,type);
                constexpr uint32_t targets[] = {0x1f5994,0x1f5994,0x1f595c,0x1f5974,0x1f5994,0x1f5994};
                for (unsigned i=0;i<6;++i) word(0x4f34d0+4*i,targets[i]);
                constexpr uint32_t radii[] = {0,0x3e800000,0x3f000000,0x3f800000,0x41180000,0x41280000};
                initial.fu[20]=radii[(n>>2)%6]; initial.fu[21]=0x3f000000;
                if (n < 32) {
                    std::memset(ram + 0x14040,0,48); word(0x14040,0x3f800000); word(0x14054,0x3f800000); word(0x14068,0x3f800000);
                    constexpr uint32_t edge[] = {0,0x3f7fffff,0x3f800000,0x3f800001,0xbf7fffff,0xbf800000,0xbf800001,0x3f000000};
                    word(0x16000,edge[n%8]); word(0x16004,edge[(n>>2)%8]);
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
