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
#include <numeric>
namespace tmb::math { void install(Registry &); }
namespace tmb::hierarchy { void install(Registry &); }
namespace tmb::graphics { void install(Registry &); }
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
        unsigned yields = 0, randomCalls = 0, weatherPoints = 0, weatherIdentities = 0;
        std::array<unsigned,5> spatialCalls{};
        std::array<unsigned,3> aiActions{};
        std::array<unsigned,5> vehicleLightActions{}, finalActions{};
        std::array<unsigned,9> rigidActions{};
        std::array<unsigned,5> driverActions{};
        std::array<uint32_t,6> driverQueryEndpoints{};
        uint32_t stop = 0x1eeafc, stop2 = 0xffffffff;
        // Keep checkpoints enabled; stop at the chosen boundary or let a complete continuation return.
        void yield(uint8_t *ram, R5900Context *ctx, Runtime *) override
        {
            require(++yields < 4096, "caller did not reach segment end");
            if (ctx->pc == 0x21d228) ++randomCalls;
            if (ctx->pc == 0x1da238) ++weatherPoints;
            if (ctx->pc == 0x1db0f0) ++weatherIdentities;
            constexpr uint32_t spatial[]={0x1b28cc,0x1b28e8,0x168ca0,0x1691b0,0x169378};
            if (ctx->pc == 0x1da298) for (unsigned i=0;i<std::size(spatial);++i)
                if (getRegU32(ctx,31)==spatial[i]) ++spatialCalls[i];
            constexpr uint32_t ai[]={0x128420,0x106010,0x1068e8};
            for(unsigned i=0;i<std::size(ai);++i) if(ctx->pc==ai[i]) ++aiActions[i];
            constexpr uint32_t vehicleLight[]={0x1dab78,0x1dbc60,0x1dbef0,0x18ad58,0x1da710};
            for(unsigned i=0;i<std::size(vehicleLight);++i) if(ctx->pc==vehicleLight[i]) ++vehicleLightActions[i];
            constexpr uint32_t finalHelpers[]={0x1da778,0x1daef0,0x1fd1b0,0x1fd190,0x140808};
            for(unsigned i=0;i<std::size(finalHelpers);++i) if(ctx->pc==finalHelpers[i]) ++finalActions[i];
            constexpr uint32_t rigidHelpers[]={0x1ed3d0,0x1ed3e0,0x1ed598,0x1ed608,0x1ed650,0x1edf78,0x1ee0a0,0x1ee150,0x1dc030};
            for(unsigned i=0;i<std::size(rigidHelpers);++i) if(ctx->pc==rigidHelpers[i]) ++rigidActions[i];
            if(ctx->pc==0x1d02b8) for(unsigned i=0;i<2;++i) {
                const auto at=bozo::GuestMemory::flatOffset(getRegU32(ctx,4+i),12);
                require(at>=0,"invalid driver query endpoint"); std::memcpy(driverQueryEndpoints.data()+3*i,ram+at,12);
            }
            constexpr uint32_t driverHelpers[]={0x1d02b8,0x1f2140,0x1d0538,0x1db230,0x18b6b8};
            for(unsigned i=0;i<std::size(driverHelpers);++i) if(ctx->pc==driverHelpers[i]) ++driverActions[i];
            if (ctx->pc == stop || ctx->pc == stop2) throw SegmentEnd{};
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
    constexpr uint32_t towVectors = 0x23d060, towVectorBytes = 0x50, towMaterials = 0x190c20, towMaterialBytes = 0x100;
    constexpr std::pair<uint32_t,uint32_t> regions[] = {{base,bytes},{globals,globalBytes},{debug,debugBytes},{0x66fcc4,4},
        {towVectors,towVectorBytes},{towMaterials,towMaterialBytes},{0x230010,64},{0x687680,0x130},{0x687f00,0xb800},{0x446560,0x80},{0x235280,0x10},{0x239480,0x400},{0x742f80,0x20}};
    constexpr size_t fixtureBytes = bytes + globalBytes + debugBytes + 4 + towVectorBytes + towMaterialBytes + 64 + 0x130 + 0xb800 + 0x80 + 0x10 + 0x400 + 0x20;
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
    constexpr uint32_t auditedInverseReturns[]={0x1161f4,0x116618,0x119090,0x11927c,0x11985c,0x119870,0x119e64,
        0x11afb0,0x11b018,0x11b9a8,0x11c38c,0x11c3c0,0x11c540,0x1ab558,0x1ab8f4,0x1abad4,
        0x1ed6c4,0x1ed8ec,0x1ed92c,0x1edaec,0x1edc04,0x1edd94,0x1ee030,
        0x1029f8,0x102c20,0x102d74,0x105c68,0x105eec,
        0x1265a8,0x1265d0,0x1288b0,0x1288c4,0x112118,
        0x18bc9c,0x1ac85c,0x1af120,0x1af148,0x1eb4d4,0x1eb58c,
        0x12d0b0,0x136ce8,0x13f814,0x13fe54,0x14037c,0x1e9778};
    std::array<unsigned,std::size(auditedInverseReturns)> auditedInverseCalls{};
    void countAuditedInverse(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(auditedInverseReturns);++i)
            if(getRegU32(ctx,31)==auditedInverseReturns[i]) ++auditedInverseCalls[i];
    }
    R5900Context forceInverseEntry, forceReferenceEntry; bool sawForceInverseEntry=false;
    RecompiledFunction nativeInverseImplementation = nullptr;
    void auditedInverse(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        if(getRegU32(ctx,31)==0x12d0b0) { forceInverseEntry=*ctx; sawForceInverseEntry=true; }
        countAuditedInverse(ctx); const auto before=*ctx;
        nativeInverseImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"audited native inverse changed VU state");
    }
    RecompiledFunction nativeDirectionImplementation = nullptr;
    R5900Context directionEntry;
    bool sawDirectionEntry = false;
    void rearDirection(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        directionEntry = *ctx; sawDirectionEntry = true;
        nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(directionEntry,*ctx),"rear-view native leaf changed its producer's VU state");
    }
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
    constexpr uint32_t rigidDirectionReturns[]={0x1ed478,0x1ed4c4,0x1ed4dc,0x1ed544,0x1ed55c,0x1ed708,0x1ed754,0x1ed76c,0x1edf38,0x1ee07c};
    std::array<unsigned,std::size(rigidDirectionReturns)> rigidDirectionCalls{};
    void countRigidDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(rigidDirectionReturns);++i) if(getRegU32(ctx,31)==rigidDirectionReturns[i]) ++rigidDirectionCalls[i];
    }
    void rigidDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countRigidDirection(ctx); const auto before=*ctx; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"rigid-body forward leaf changed VU state");
    }
    constexpr uint32_t driverDirectionReturns[]={0x126618,0x1267f0,0x1276a4,0x128704,0x128744,0x1287dc,0x12880c};
    std::array<unsigned,std::size(driverDirectionReturns)> driverDirectionCalls{};
    void countDriverDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(driverDirectionReturns);++i) if(getRegU32(ctx,31)==driverDirectionReturns[i]) ++driverDirectionCalls[i];
    }
    void driverDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countDriverDirection(ctx); const auto before=*ctx; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"driver forward leaf changed VU state");
    }
    struct SavedW { uint32_t offset, value; };
    std::vector<SavedW> referenceW;
    void referenceDirection(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        countRigidDirection(ctx); countDriverDirection(ctx);
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
        if(getRegU32(ctx,31)==0x12d0b0) forceReferenceEntry=*ctx;
        countAuditedInverse(ctx);
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
    nativeDirectionImplementation = direction; nativeInverseImplementation=inverse;
    require(inverse && direction, "missing hierarchy leaves");
    tmb::Registry graphics; tmb::graphics::install(graphics);
    const auto vehicleInstructions=graphics.instructionHandler(); const auto matrix=registry.find(0x1dcc08);
    require(vehicleInstructions && matrix,"missing coordinate-system consumer handlers");
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
        0x1f5414, 0x1f5458, 0x1f54f4, 0x1f5538, 0x1f55d4, 0x1f5618, 0x1f56a8, 0x1f5904,
        0x1888e8, 0x18b4c0, 0x1e4818, 0x1e42d8, 0x1e435c, 0x1e43b4, 0x1e43c8,
        0x15e1b8, 0x1b42c8, 0x1b430c, 0x1b43a8, 0x1b4434, 0x1b44c0,
        0x150bc8, 0x150cc0, 0x150d2c, 0x150d98, 0x108d6c, 0x109870, 0x1099a0, 0x1abd60,
        0x1137cc, 0x11398c, 0x113a78, 0x113b28, 0x113c2c, 0x113f28, 0x1142d8, 0x114394, 0x1fdd3c, 0x1e8a34,
        0x1b28cc, 0x1b28e8, 0x168ca0, 0x1691b0, 0x169378,
        0x1161f4,0x116618,0x119090,0x11927c,0x11985c,0x119870,0x119e64,
        0x11afb0,0x11b018,0x11b9a8,0x11c38c,0x11c3c0,0x11c540,0x1ab558,0x1ab8f4,0x1abad4,
        0x1ed6c4,0x1ed8ec,0x1ed92c,0x1edaec,0x1edc04,0x1edd94,0x1ee030,
        0x1029f8,0x102c20,0x102d74,0x105c68,0x105eec,
        0x1265a8,0x1265d0,0x1288b0,0x1288c4,0x112118,0x11112c,
        0x18bc9c,0x1ac85c,0x1af120,0x1af148,0x1eb4d4,0x1eb58c,
        0x12d0b0,0x136ce8,0x13f814,0x13fe54,0x14037c,0x1e9778,
        0x1ed478,0x1ed4c4,0x1ed4dc,0x1ed544,0x1ed55c,0x1ed708,0x1ed754,0x1ed76c,0x1edf38,0x1ee07c,
        0x126618,0x1267f0,0x1276a4,0x128704,0x128744,0x1287dc,0x12880c};
    const auto isInverse = [](unsigned kind) { return kind < 4 || kind == 12 || kind == 13 || (kind >= 24 && kind <= 30) || (kind >= 37 && kind <= 45) || (kind >= 50 && kind <= 57) || (kind >= 65 && kind <= 67) || (kind >= 76 && kind <= 96) || (kind >= 111 && kind <= 118) || (kind >= 123 && kind <= 125) || (kind >= 135 && kind <= 138) || (kind >= 155 && kind <= 187) || (kind >= 189 && kind <= 200); };
    const unsigned saved = _mm_getcsr(); unsigned cases = 0, memoryCases = 0; std::array<unsigned,2> aiShotPublications{};
    unsigned weaponJumpCases=0, lightingPublicationCases=0, rolloverPublicationCases=0;
    std::array<unsigned,4> finalPublications{};
    std::array<unsigned,4> driverPublications{};
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
            RecompiledFunction continuation; services.stop = services.stop2 = 0xffffffff; bool fromEntry = false;
            int expectedSelection = -1;
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
            else if (kind == 118)
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
            else if (kind == 119)
            {
                // Complete suspension consumer: transformed Z and selected bump determine ride height.
                constexpr uint32_t vehicle=0x18000;
                continuation = resume_0x188738; reg(4,vehicle+0x4a0); reg(5,vehicle+0x1d80); reg(6,sp);
                reg(16,vehicle+0x4a0); reg(17,vehicle); returnAt(0x30); word(sp+0x0c,bits());
                if (n < 16) {
                    std::memset(ram+vehicle+0x4a0,0,48); word(vehicle+0x4a0,0x3f800000); word(vehicle+0x4b4,0x3f800000); word(vehicle+0x4c8,0x3f800000);
                    constexpr uint32_t edge[] = {0,1,0x80000000,0x80000001,0x3f7fffff,0x3f800000,0x3f800001,0xbf800001};
                    word(vehicle+0x1d88,edge[n%8]); word(vehicle+0x1ddc,edge[(n>>1)%8]);
                }
            }
            else if (kind == 120)
            {
                // Tire reconstruction through real CS copy, smoothing/lower clamp and return; also complete entry paths.
                constexpr uint32_t vehicle=0x18000;
                continuation = resume_0x18b420;
                reg(4,vehicle+0xd0); reg(5,sp); reg(6,sp+0x10); reg(16,vehicle+0x1a90+(n%4)*0xf0);
                reg(17,vehicle); reg(18,vehicle+0x1d0); reg(19,sp+0x10); reg(20,vehicle+0x240);
                word(vehicle+0x2018,address(0x1c000)); word(sp+0x1c,bits()); returnAt(0x70);
                constexpr uint32_t heights[] = {0,0xc0400001,0xc0400000,0xc03fffff,0x3f800000,0xbf800000,1,0x80000001};
                word(vehicle+0x1e8,heights[n%8]); word(vehicle+0x1ed0,n & 8 ? 0xc1000000 : 0x41000000);
                word(gp-0x59f4,0x3f000000); word(gp-0x59f0,0x3dcccccd);
                if (n >= 64) {
                    fromEntry=true; initial.pc=0x18b420; setRegU32(&initial,31,0xbad0000); reg(4,vehicle);
                    ram[vehicle+0x490]=uint8_t(n & 1); word(vehicle+0x1ee4,n%4);
                    word(vehicle+0x1f0,n & 2 ? 0x3f800000 : 0xbf800000); word(vehicle+0x1f4,n & 4 ? 0x40000000 : 0x3f000000);
                    for (unsigned i=0;i<4;++i) {
                        const uint32_t tire=vehicle+0x1a90+i*0xf0; const float h=float((i+n)%4)/4.f;
                        std::memcpy(ram+tire+0x38,&h,4); word(tire+0xb8,(n>>i)&1);
                    }
                }
            }
            else if (kind == 121)
            {
                // In-place emitted direction through real position/velocity, bounds, lifetime and metadata publication.
                constexpr uint32_t particle=0x18000;
                const uint32_t slot=n%3;
                continuation = resume_0x1e4400;
                reg(4,particle+0x2ca0); reg(5,sp+0x40); reg(6,sp+0x40); reg(18,particle);
                reg(19,0x1d000+slot*16); reg(20,0x1d800+slot*16); reg(21,0x1c000); setRegU32(&initial,22,slot);
                reg(23,0x1c010); reg(30,sp+0x40); returnAt(0xf0); word(sp+0x4c,bits());
                word(sp+0x50,n&3); word(sp+0x54,address(0x1e000)); word(sp+0x5c,address(0x1e100));
                word(particle+0x2e70,slot+1);
                constexpr uint32_t types[]={1,2,3,7,8,9}; word(particle+0x2e40,types[(n/3)%6]);
                word(particle+0x2d98,0x3f000000); word(particle+0x2d9c,n&4 ? 0x3fc00000 : 0x3f000000);
                word(particle+0x2e24,n&8 ? 0x3dcccccd : 0); word(particle+0x2e28,30u<<16);
                word(particle+0x2da4,n&16 ? 0x3f800000 : 0);
                initial.fu[20]=0x3f000000; initial.fu[22]=0x40000000; initial.fu[23]=0x3f000000; initial.fu[24]=0x3e800000;
                if (n >= 64) {
                    // Enter before the actual range RNG branches; random direction generation is outside this fixture.
                    fromEntry=true; initial.pc=0x1e4714;
                    word(sp+0x58,address(particle+0x2ca0)); word(particle+0x2d8c,0x3f800000);
                    word(particle+0x2d78,0x3f000000); word(particle+0x2d7c,n&1 ? 0x3fc00000 : 0x3f000000);
                    word(particle+0x2d90,0x3e800000); word(particle+0x2d94,n&2 ? 0x3f400000 : 0x3e800000);
                }
            }
            else if (kind <= 125)
            {
                // Complete emitter-coordinate consumers, including full entry calls across all four attachment modes.
                constexpr uint32_t emitter=0x18000, attached=0x15000, relative=0x16000;
                const uint32_t output=n&16 ? 0x17300 : 0x1c000;
                continuation = resume_0x1e4298;
                reg(17,emitter); reg(18,sp+0x50); reg(19,output); reg(20,0x17300); returnAt(0xb0);
                word(emitter+0xd0,address(0x17000)); word(emitter+0xd4,address(attached));
                word(emitter+0xd8,address(0x17100)); word(emitter+0xdc,address(relative));
                word(emitter+0xf0,address(0x17200)); word(emitter+0xf4,address(0x19000));
                word(relative+0xc,bits()); word(emitter+0xc,bits()); word(output+0xc,bits());
                if (kind == 122) {
                    reg(4,attached); reg(5,0x17200); reg(6,sp+0x50); word(sp+0x5c,bits());
                    word(emitter+0xdc,n&8 ? address(relative) : 0);
                } else {
                    reg(4,relative); reg(5,kind == 125 ? 0x17300 : sp); reg(6,kind == 125 ? output : emitter);
                }
                if ((kind == 122 || kind == 124) && n >= 64) {
                    fromEntry=true; initial.pc=0x1e4298; setRegU32(&initial,31,0xbad0000);
                    reg(4,emitter); reg(5,0x17300); reg(6,output);
                    if (kind == 124) { word(emitter+0xd4,0); word(emitter+0xdc,n&8 ? address(relative) : 0); }
                }
            }
            else if (kind == 126)
            {
                // Vehicle shadow vertices: query staging, or the complete cached-height loop and center publication.
                constexpr uint32_t vehicle=0x18000;
                continuation = resume_0x15df38;
                reg(4,sp+0x10); reg(5,sp+0x50); reg(6,sp+0x60); reg(16,vehicle);
                reg(17,0x1a000); reg(18,0x19000); setRegU32(&initial,19,0); reg(20,sp+0x60);
                reg(21,sp+0x10); reg(22,0x1c000); reg(23,sp+0x50); setRegU32(&initial,30,1+n%4);
                returnAt(0x110); word(sp+0x6c,bits()); word(sp+0x70,n < 64 ? 1 : 0);
                constexpr uint32_t height[]={0,1,0x80000000,0x80000001,0xbf800000,0x3f7fffff,0x3f800000,0x40000000};
                initial.fu[21]=height[n%8];
                if (n < 64) services.stop=0x21d0b8; // Actual hdHatTest copies XYZ and stages its downward ray first.
                else { fromEntry=true; initial.pc=0x15e190; }
            }
            else if (kind <= 131)
            {
                // Weapon shadow motion/corners through world addition and real hdHatTest XYZ/downward-ray staging.
                constexpr uint32_t weapon=0x18000, cs=0x1a000;
                continuation = resume_0x1b41d0; services.stop=0x21d0b8;
                reg(4,cs+0x20); reg(5,kind == 127 ? sp : sp+0x10); reg(6,kind == 127 ? sp : sp+0x20);
                reg(16,weapon); reg(17,sp+0x10); reg(18,sp+0x20); returnAt(0xd0);
                word(weapon+0x130,address(cs)); word(cs+0x1c,bits()); word(sp+0x0c,bits()); word(sp+0x2c,bits());
                word(gp-0x4858,0x42c80000); initial.fu[26]=0; initial.fu[28]=0x41200000; initial.fu[25]=0x41700000;
                if (kind == 127 && n >= 64) {
                    // Actual movement-Z producer and in-place transform, then the first corner and height-query prefix.
                    fromEntry=true; initial.pc=0x1b429c; setRegU32(&initial,2,1+n%3);
                }
            }
            else if (kind <= 135)
            {
                // Complete tow-truck launcher tail: smoothing, local direction, normalization, matrix and material publication.
                constexpr uint32_t launcher=0x18000, vehicle=0x1b000, launcherCs=0x1c000, vehicleCs=0x1d000;
                constexpr uint32_t outputCs=0x1e000, attachment=0x1f000, material=0x20000;
                continuation=resume_0x150b60;
                reg(17,launcher); reg(20,vehicle); reg(18,sp+0x50); reg(21,sp+0x50);
                reg(22,launcher+0x360); reg(19,launcher+0x350); reg(16,kind == 135 ? sp+0x60 : sp+0x10);
                reg(23,0x240000); reg(30,0x240000); returnAt(0x100);
                reg(4,kind == 133 ? sp+0x10 : kind == 134 ? vehicleCs+0x20 : launcherCs+0x10);
                reg(5,kind == 132 ? 0x23d070 : kind == 133 ? 0x23d0a0 : kind == 134 ? launcher+0x350 : sp+0x50);
                reg(6,sp+0x50); initial.fu[20]=0x3e800000; initial.fu[21]=0x3c23d70a;
                word(launcher+0x2a0,address(launcherCs)); word(launcher+0x2a4,address(outputCs));
                word(launcher+0x34c,address(attachment)); word(vehicle+0x2018,address(vehicleCs));
                word(launcher, n&1 ? 1 : 2); word(launcher+0x188,n&2 ? 1 : n&8 ? 0xffffffff : 0); word(launcher+0x27c,0);
                ram[vehicle+0x475]=0; ram[vehicle+0x487]=uint8_t(n&4 ? 1 : 0);
                ram[attachment+0x0c]=0xa5; // Make both clearing and retaining the visibility byte observable.
                const uint32_t slot=(n%4)*4; word(gp-0x4154,slot);
                word(towMaterials+slot+0x60,address(material)); word(towMaterials+slot+0x80,address(material+0x10));
                word(gp-0x620c,0x3e800000); word(gp-0x6208,0xbe800000); word(gp-0x6204,0x3e800000);
                word(gp-0x6200,0x3e800000); word(gp-0x61fc,0x3c23d70a);
                word(gp-0x4824,0x38d1b717); word(gp-0x47fc,0x38d1b717);
                constexpr uint32_t tilt[]={0,0x80000000,0x3f000000,0xbf000000};
                word(vehicle+0x1f8,tilt[n%4]); word(vehicleCs+0x64,tilt[(n/4)%4]); word(vehicleCs+0x68,tilt[(n/16)%4]);
                word(sp+0x5c,bits()); word(launcherCs+0x1c,bits()); word(0x23d0ac,bits());
                // Fixed reference point used by the real vector subtraction; preserve its opaque fourth word.
                word(sp+0x60,0); word(sp+0x64,0); word(sp+0x68,0x40000000);
                if (kind == 135 && n < 64 && n%8 < 5) {
                    // Zero-length and vertical directions take the real normalization/orientation fallback branches.
                    for (unsigned column=0;column<3;++column) for (unsigned row=0;row<3;++row)
                        word(launcherCs+0x10+16*column+4*row,column == row ? 0x3f800000 : 0);
                    word(sp+0x50,n%8 == 1 || n%8 == 3 ? 0x3f800000 : 0);
                    word(sp+0x54,n%8 == 2 || n%8 == 3 ? 0x3f800000 : 0);
                    word(sp+0x58,n%8 == 4 ? 0 : 0x40000000);
                }
                if (n >= 64) {
                    fromEntry=true;
                    if (kind == 132 || kind == 135) {
                        initial.pc=0x150b60; reg(4,launcher); reg(5,vehicle); setRegU32(&initial,31,0xbad0000);
                        word(launcher+0x27c,kind == 135 ? 1 : 0); word(sp-0x120+0x5c,bits());
                        if (kind == 135) ram[vehicle+0x475]=uint8_t((n&24) == 24 ? 1 : 0);
                    } else if (kind == 133) initial.pc=0x150be0; // Actual tilt/clamp and rotation-matrix producer.
                    else { initial.pc=0x150d14; initial.fu[0]=tilt[n%4]; } // Actual final smoothed-Z publication.
                }
            }
            else if (kind <= 138)
            {
                // Attractor selection and world positions through their complete real consumers and producer entry calls.
                constexpr uint32_t object=0x18000, point=0x1b000, output=0x1d000, group=0x1a000;
                const auto rect = [&](unsigned id, uint32_t x, uint32_t y, int angle) {
                    const uint32_t at=group+id*0x100;
                    word(object+8+id*4,address(at)); word(at,x); word(at+4,y); word(at+8,0);
                    word(at+0x10,0x40800000); word(at+0x14,0x40800000); word(at+0x1c,uint32_t(angle));
                };
                reg(4,sp); reg(5,sp+0x40); reg(6,sp+0x50); reg(16,sp+0x40);
                word(sp+0x0c,bits()); word(sp+0x4c,bits()); word(sp+0x5c,bits()); word(output+0x0c,bits());
                word(gp-0x7c14,0x40490fdb); word(gp-0x7bfc,0x40490fdb); word(gp-0x7bf8,0x40490fdb);
                initial.fu[20]=0;
                if (kind == 136) {
                    continuation=resume_0x108c60; returnAt(0xf0);
                    reg(17,object+8+3*4); reg(18,object); reg(19,point); reg(23,object+8); reg(30,object+0x128);
                    setRegU32(&initial,20,3); setRegU32(&initial,21,0); setRegU32(&initial,22,0);
                    word(object+0x250,3); word(object+0x128,3); word(object+0x12c,1); word(object+0x130,2);
                    rect(3,0,0,(n/12)&1 ? -30 : 30); rect(1,0x41200000,0x41200000,0);
                    rect(2,0xc1200000,0xc1200000,-30);
                    word(point,0x40000000); word(point+4,0x40000000); word(point+8,0x3f800000);
                    if (n < 32) {
                        // Literal strict-boundary cases at a helper boundary, with an identity basis.
                        constexpr uint32_t xy[][2]={{0x40000000,0x40000000},{0,0x40000000},
                            {0x40800000,0x40000000},{0x40000000,0},{0x40000000,0x40800000},
                            {0xbf800000,0x40000000},{0x40a00000,0x40000000},{0x40000000,0xbf800000},
                            {0x40000000,0x40a00000},{0x80000000,0x40000000},
                            {0x407fffff,0x40000000},{0x40800001,0x40000000}};
                        for (unsigned c=0;c<3;++c) for (unsigned r=0;r<3;++r) word(sp+16*c+4*r,c==r ? 0x3f800000 : 0);
                        word(sp+0x40,xy[n%12][0]); word(sp+0x44,xy[n%12][1]); word(sp+0x48,0x3f800000);
                        expectedSelection=n%12 == 0 || n%12 == 10 ? 3 : 0;
                    }
                    if (n >= 64) {
                        fromEntry=true; initial.pc=0x108c60; reg(4,object); reg(5,point); setRegU32(&initial,31,0xbad0000);
                        const unsigned scenario=(n-64)%12; expectedSelection=0;
                        if (scenario==0 || scenario==9) word(object+0x250,scenario==0 ? 0 : 0xffffffff);
                        else if (scenario==1 || scenario==10) word(point+8,scenario==1 ? 0 : 0xbf800000);
                        else if (scenario==2 || scenario==7) { expectedSelection=3; if (scenario==7) rect(1,0,0,0); }
                        else if (scenario==3) { word(point,0); word(point+4,0); }
                        else if (scenario==4) { rect(3,0,0,0); word(point,0); }
                        else if (scenario==5 || scenario==11) {
                            rect(3,0,0,0); word(point,scenario==5 ? 0x40800000 : 0x407fffff);
                            word(point+4,0x40800000); expectedSelection=3;
                        } else if (scenario==6) { word(point,0x41400000); word(point+4,0x41400000); expectedSelection=1; }
                        else {
                            rect(3,0x42c80000,0x42c80000,30); rect(1,0xc2c80000,0xc2c80000,-30);
                            rect(2,0,0,30); expectedSelection=2;
                        }
                    }
                } else {
                    continuation=kind == 137 ? resume_0x1097c8 : resume_0x1098e8; returnAt(0x90);
                    reg(17,group); reg(18,output); word(object+8,address(group));
                    word(group,0x41200000); word(group+4,0x41a00000); word(group+8,0x41f00000);
                    constexpr uint32_t extent[]={0,0x3f800000,0x40800000,0xc0800000};
                    word(group+0x10,extent[n%4]); word(group+0x14,extent[(n/4)%4]);
                    word(group+0x1c,n%4 == 0 ? 0 : n&1 ? 30 : uint32_t(-30));
                    // The actual generated rand uses guest-backed state, restored with all other fixture memory.
                    word(0x4e429c,address(0x17000)); word(0x17058,bits());
                    if (n >= 64) {
                        fromEntry=true; initial.pc=kind == 137 ? 0x1097c8 : 0x1098e8;
                        reg(4,object); reg(5,output); setRegU32(&initial,6,0); setRegU32(&initial,31,0xbad0000);
                    }
                }
            }
            else if (kind == 139)
            {
                // Complete peel-out producer/tail, followed by the real production-native coordinate-system consumer.
                constexpr uint32_t vehicle=0x18000;
                continuation=resume_0x1abb18; reg(16,vehicle); reg(4,vehicle+0xd0); reg(5,sp); reg(6,vehicle+0x520);
                returnAt(0x20); word(vehicle+0x52c,bits()); word(vehicle+0x1dc,bits());
                word(vehicle+0x2018,address(n%3 == 0 ? 0x1c000 : vehicle+(n%3 == 1 ? 0x480 : 0x4c0)));
                word(vehicle+0x574,0x7fffffffu+(n&1)); word(vehicle+0x20c8,1+(n&1));
                constexpr uint32_t input[]={0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x3e800000,0xbe800000};
                word(sp,input[n%8]); word(sp+4,0); word(sp+8,0);
                if (n >= 64) {
                    fromEntry=true; initial.pc=0x1abb18; reg(4,vehicle); setRegU32(&initial,31,0xbad0000);
                    const unsigned scenario=(n-64)%16;
                    setRegU32(&initial,5,scenario>=8 ? 1 : 0);
                    ram[vehicle+0x48f]=uint8_t(scenario>=4 && scenario!=14);
                    ram[vehicle+0x491]=uint8_t(scenario==7 || scenario==10 || scenario==11 || scenario==15);
                    word(vehicle+0x2c4,scenario>=6 && scenario<=11 || scenario==15 ? 0 : 0x3f800000);
                    word(vehicle+0x318,0x3f800000); word(vehicle+0x1f8,scenario&1 ? 0xbf800000 : 0x3f800000);
                    word(vehicle+0x364,0x3f000000); word(vehicle+0x2cc,0x3e800000); word(vehicle+0x3b0,n&4 ? 0x40000000 : 0xbf800000);
                    word(vehicle+0x530,scenario==0 || scenario==4 || scenario==12 ? 0 : scenario&1 ? 0x3f000000 : 0xbf000000);
                    if (scenario==3) word(vehicle+0x530,0x3c23d70a);
                    if (scenario==14) word(vehicle+0x530,1);
                    word(vehicle+0x534,scenario==8 ? 0x3f000000 : scenario==9 ? 0xbf000000 : 0x3e000000);
                    word(vehicle+0x538,scenario==9 ? 0xbf000000 : 0x3e000000);
                    word(vehicle+0x53c,scenario==9 ? 0xbf800000 : 0x3f800000);
                    word(vehicle+0x540,scenario==10 ? 0x3f800000 : 0x3e800000);
                    word(vehicle+0x1ef8,scenario==1 ? 1 : 0); word(gp-0x2ef0,(n/16)&1);
                    for (uint32_t at=0x4db8;at>=0x4d74;at-=4) word(gp-at,0x3e800000);
                    word(gp-0x4db4,0xbe800000); word(gp-0x4dac,0xbe800000);
                    for (uint32_t at:{0x4da4u,0x4d9cu,0x4d94u,0x4d80u,0x4d78u}) word(gp-at,0x3c23d70a);
                    word(gp-0x4d8c,0xbe800000); word(gp-0x4d88,0x3f000000);
                    // Bound-angle and smoothing constants; these fixtures keep trigonometric inputs below half a radian.
                    word(gp-0x487c,0x40490fdb); word(gp-0x4878,0x49742400); word(gp-0x4874,0x3e22f983); word(gp-0x4870,0x40c90fdb);
                    word(gp-0x486c,0xc0490fdb); word(gp-0x4868,0xc9742400); word(gp-0x4864,0x40490fdb);
                    word(gp-0x4860,0x3e22f983); word(gp-0x485c,0x40c90fdb);
                    word(gp-0x46a8,0x3fc90fdb); word(gp-0x46a4,0xbfc90fdb); word(gp-0x46a0,0xbfc90fdb);
                }
            }
            else if (kind <= 148)
            {
                // Real camera producers and consumers through pose publication or the next actual query boundary.
                constexpr uint32_t camera=0x10000, vehicle=0x18000, cs=0x1c000;
                const unsigned slot=n%4; const uint32_t slot4=slot*4, slot16=slot*16;
                continuation=resume_0x1135d0; services.stop=0x1fdc90;
                reg(22,camera); reg(23,cs); setRegU32(&initial,21,slot4); setRegU32(&initial,30,slot4);
                word(sp+0xd0,slot); word(sp+0xd4,address(vehicle)); word(sp+0x104,address(cs+0x10));
                word(sp+0x108,address(cs+0x20)); word(sp+0xe4,address(sp+0x40)); word(sp+0xec,slot16);
                word(vehicle+0x2018,address(n&4 ? cs : 0x1d000));
                word(sp+0x100,address(camera+0x43d0)); word(sp+0x118,address(camera+0x42e0));
                word(sp+0x11c,address(camera+0x42f0)); word(sp+0xe8,address(camera+0x42d0));
                word(cs+0x4c,bits()); word(cs+0x1c,bits());
                word(camera+0x416c+slot16,bits());
                word(camera+0x43e4+slot4,1); word(sp+0x1b0,0xbad0000); word(sp+0x1b4,0);
                for (uint32_t at=0x7718;at<=0x77e8;at+=4) word(gp-at,0x3e000000);
                word(gp-0x46a8,0x3fc90fdb); word(gp-0x46a4,0xbfc90fdb); word(gp-0x46a0,0xbfc90fdb);
                for (uint32_t at:{cs+0x60,cs+0x64,cs+0x68}) word(at,n&1 ? 0x3e000000 : 0xbe000000);
                reg(4,cs+0x20); reg(5,sp+0x40); reg(6,sp+0x50); reg(17,sp+0x50);
                if (kind == 140) {
                    services.stop=0x1cec90; reg(16,cs+0x20); reg(18,sp+0x40); reg(19,sp+0x50);
                    reg(17,camera+0x4404+slot4); word(camera+0x4404+slot4,1);
                    word(sp+0xdc,address(cs+0x10)); word(sp+0xd8,address(vehicle+0x1d0));
                    word(gp-0x4154,0x18000u-0x1ae650u); word(0x18000,0);
                    if (n>=64) { fromEntry=true; initial.pc=0x113774; }
                } else if (kind == 141) {
                    reg(17,cs+0x20); reg(16,sp+0x70); reg(20,camera+slot4); reg(23,cs+0x10);
                    reg(5,sp+0x60); reg(6,sp+0x70);
                    if (n>=64) { fromEntry=true; initial.pc=0x113978; }
                } else if (kind == 142) {
                    reg(16,sp+0x40); setRegU32(&initial,18,slot16); reg(21,vehicle+0x1d0); reg(23,cs+0x10);
                    reg(20,camera+0x42d0+slot4); reg(19,camera+0x4300+slot4);
                    word(sp+0xe0,address(camera+0x4404+slot4));
                    if (n>=64) { fromEntry=true; initial.pc=0x113a50; }
                } else if (kind == 143) {
                    services.stop=0x1dbd58; reg(4,n&4 ? cs+0x20 : 0x1d020);
                    initial.fu[23]=0; word(vehicle+0x3e0,0);
                    if (n>=64) { fromEntry=true; initial.pc=0x113ae0; }
                } else if (kind == 144) {
                    reg(20,sp+0x40); reg(18,vehicle+0x1d0); setRegU32(&initial,19,slot16);
                    initial.fu[23]=0; initial.fu[21]=initial.fu[22]=0x3e000000;
                    if (n>=64) { fromEntry=true; initial.pc=0x113bf4; }
                } else if (kind == 145) {
                    services.stop=0x1d02b8; reg(16,camera+0x4160+slot16); reg(17,camera+slot16);
                    reg(18,camera+0x43c0); reg(19,camera+0x43d0); setRegU32(&initial,20,slot16);
                    word(sp+0xf0,address(vehicle+0x1d0)); word(sp+0xf4,address(camera+0x4340));
                    word(vehicle+0x33c,0x3f800000); ram[vehicle+0x491]=uint8_t(n&1);
                    reg(4,sp); reg(6,camera+0x4160+slot16);
                    if (n>=64) { fromEntry=true; initial.pc=0x113eb8; setRegU32(&initial,4,slot); }
                } else if (kind <= 147) {
                    services.stop=0x1d02b8; reg(18,vehicle+0x1d0); reg(20,vehicle+0xd0);
                    const uint32_t out=sp+(kind==146 ? 0x70 : 0xc0);
                    reg(16,out); reg(4,vehicle+0xd0); reg(6,out);
                    if (n>=64) { fromEntry=true; initial.pc=kind==146 ? 0x114298 : 0x11435c; reg(4,vehicle); }
                } else {
                    continuation=resume_0x1fdc90; services.stop=0xffffffff;
                    reg(17,cs); reg(16,sp+0x30); reg(4,sp+0x30); reg(5,sp); reg(6,sp+0x10); returnAt(0x90);
                    word(gp-0x3270,n&1); initial.fu[12]=n&2 ? 0x3f000000 : 0xbf000000;
                    if (n>=64) {
                        fromEntry=true; initial.pc=0x1fdc90; reg(4,cs); setRegU32(&initial,5,n%8==0 ? 1 : 0);
                        setRegU32(&initial,31,0xbad0000);
                    }
                }
            }
            else if (kind == 149)
            {
                // Complete weather viewport: real view accessors, fade/fog producers, grid placement and projection.
                constexpr uint32_t cs=0x1b000;
                const unsigned view=n%4, scenario=(n%64)/4, effect=scenario%3;
                const uint32_t projection=n&8 ? cs+0x20 : 0x1d000;
                continuation=resume_0x1e87a0;
                reg(4,cs+0x20); reg(5,sp+0x10); reg(6,sp+0x20); reg(16,sp+0x20);
                setRegU32(&initial,17,view); setRegU32(&initial,18,0x2e80);
                setRegU32(&initial,19,scenario&1 ? address(cs) : 0); initial.fu[20]=0x3f800000;
                returnAt(0x120); word(gp-0x34b8,effect); word(gp-0x34b4,address(0x1a180));
                word(gp-0x34b0,0x41a00000); word(gp-0x34a8,n*17);
                word(gp-0x2f58,address(0x1a040)); word(gp-0x2f54,address(0x1a100));
                word(gp-0x2f50,address(0x1a120)); word(gp-0x2f48,address(projection));
                word(0x4dfcd0+view*16,address(cs)); word(gp-0x2ef8,1+(n&1));
                word(gp-0x4728,0x3c4ccccd); word(gp-0x472c,0x442f0000);
                constexpr uint32_t counter[]={0,1,20,21,0xffffffff,2,0,21,1,0,20,21,1,2,21,0};
                word(0x687780+view*4,0x3f800000);
                word(0x687790+view*4,counter[scenario]); word(0x6877a0+view*4,counter[(scenario+5)%16]);
                word(gp-0x3684,0x43c80000); word(gp-0x3680,0x44480000);
                word(gp-0x367c,0x3f800000); word(gp-0x3678,0x3e800000);
                word(gp-0x366c,32); word(gp-0x3668,64); word(gp-0x3664,96);
                word(sp+0x2c,bits()); word(cs+0x1c,bits());
                if (n<16) {
                    // Exact cell boundaries and signed subnormal inputs retain the game's original quantization order.
                    constexpr uint32_t edge[]={0,0x80000000,1,0x80000001,0x421fffff,0x42200000,0x42200001,0xc2200000};
                    for (unsigned j=0;j<3;++j) for (unsigned i=0;i<4;++i) word(cs+0x20+16*j+4*i,i==j ? 0x3f800000 : 0);
                    for (unsigned j=0;j<3;++j) word(sp+0x10+4*j,edge[(n+j)%8]);
                }
                if (n>=64) {
                    fromEntry=true; initial.pc=0x1e87a0; setRegU32(&initial,4,view);
                    setRegU32(&initial,5,scenario&1 ? address(cs) : 0); setRegU32(&initial,6,scenario>=4);
                    setRegU32(&initial,31,0xbad0000);
                    if (scenario==0) word(gp-0x34b8,0xffffffff);
                }
            }
            else if (kind <= 154)
            {
                constexpr uint32_t vehicle=0x18000, weapon=0x15000, cs=0x1b000, scene=0x360000;
                const unsigned wheel=n%4, mark=(n/4)%4, scenario=(n%64)/8;
                const uint32_t tire=vehicle+0x1a90+wheel*0xf0, vertices=scene+0x163570+mark*64;
                word(gp-0x4154,scene); word(vehicle+0x2018,address(cs));
                word(vehicle+0x3c0,n&4 ? 0x40800000 : 0x40000000);
                word(vehicle+0x2c4,n&1 ? 0xbf800000 : 0x3f800000);
                word(vehicle+0x530,scenario&1 ? 0xbe800000 : 0x3e800000);
                word(tire+0xa4,mark); const uint16_t material=uint16_t(wheel);
                std::memcpy(ram+tire+0xa0,&material,2);
                word(tire+0xb8,0); word(tire+0xd4,0); word(tire+0xd8,0); word(tire+0xdc,scenario&1);
                for (uint32_t at:{0x4ef4a4u,0x4edd34u,0x4eae48u,0x4e3918u,0x4e67f8u,0x4e5088u,0x4e7f68u}) word(at+mark*4,0);
                word(0x4eae48+mark*4,material); word(gp-0x5f7c,0x3f800000);
                for (unsigned j=0;j<3;++j) {
                    word(tire+0x30+j*4,0); word(tire+0x40+j*4,0x3f800000);
                    for (unsigned v=0;v<4;++v) word(vertices+16*v+4*j,0);
                }
                for (unsigned v=0;v<4;++v) word(vertices+16*v+12,bits());
                word(cs+0x60,0); word(cs+0x68,0); word(cs+0x1c,bits());
                if (kind <= 151) {
                    continuation=resume_0x1b25b8; reg(16,weapon); reg(4,kind==150 ? weapon+0xd0 : cs+0x20);
                    reg(5,sp); reg(6,sp); returnAt(0x30);
                    word(weapon+0x130,address(cs)); word(weapon+0x14,scenario%4);
                    word(weapon+0x3c,kind==150); word(weapon+0x38,0);
                    word(weapon+0x70,0x3e800000); word(weapon+0x50,0x3e000000);
                    word(weapon+0x90,0x3e000000); word(weapon+0x94,scenario&1 ? 0x3e800000 : 0);
                    word(weapon+0x98,0xbe000000); word(gp-0x2ef8,1+(scenario%3));
                    word(gp-0x4ae8,0x3f800000); word(gp-0x4ae4,0x3e000000); word(gp-0x4ae0,0x3e800000);
                    if (n>=64) { fromEntry=true; initial.pc=0x1b25b8; reg(4,weapon); setRegU32(&initial,31,0xbad0000); }
                } else if (kind == 152) {
                    continuation=resume_0x168a98; reg(19,vehicle); reg(22,tire); reg(23,tire+0x30); reg(21,tire+0x40);
                    reg(18,sp+0x10); reg(4,vehicle+0x190); reg(5,sp); reg(6,sp+0x10);
                    setRegU32(&initial,17,mark); setRegU32(&initial,20,mark*4); setRegU32(&initial,30,0x188000); returnAt(0xb0);
                    word(vehicle+0x2110,scenario==0 ? 0 : 1); word(vehicle+0x20a8,0);
                    word(tire+0xa4,0xffffffff); word(gp-0x2ed0,scenario==3 ? 0x40000000 : 0);
                    word(gp-0x5f80,0x3f800000); word(tire+0xb8,scenario==2);
                    if (scenario==1) { const uint16_t invalid=29; std::memcpy(ram+tire+0xa0,&invalid,2); }
                    word(scene+0x18dd2c,scenario==7 ? 0xffffffff : mark);
                    word(scene+0x18dd28,scenario==7 ? mark : 0xffffffff);
                    word(scene+0x17f2c0+mark*12,1); word(scene+0x17f2c4+mark*12,0xffffffff); word(scene+0x17f2c8+mark*12,0xffffffff);
                    word(scene+0x17ac70+mark*12,scenario==7 ? 1 : 0);
                    word(scene+0x17ac74+mark*12,0xffffffff); word(scene+0x17ac78+mark*12,0xffffffff);
                    word(scene+0x18c5b8+mark*4,20);
                    initial.fu[1]=0x40000000;
                    if (n>=64) { fromEntry=true; initial.pc=0x168a98; reg(4,vehicle); setRegU32(&initial,5,wheel); setRegU32(&initial,31,0xbad0000); }
                } else {
                    continuation=resume_0x168ef0; reg(20,vehicle); reg(18,tire); reg(17,tire+0x30); reg(19,sp+0x20);
                    setRegU32(&initial,21,1); initial.fu[20]=0;
                    reg(4,kind==153 ? cs+0x20 : vehicle+0x190); reg(5,sp); reg(6,sp+0x20); returnAt(0x90);
                    if (kind==153) {
                        word(vehicle+0x2c4,0); word(0x4ef4a4+mark*4,scenario==0 ? 0 : 1);
                        word(vehicle+0x530,scenario&1 ? 0x3e800000 : 0xbe800000);
                        if (scenario==7) word(vehicle+0x530,0xbe800000);
                        word(tire+0xd4,scenario==4 || scenario==5); word(tire+0xd8,scenario==3 || scenario==5);
                        const uint32_t distance=scenario==6 ? 0x41a00000 : 0x3f800000;
                        word(vertices+0x10,distance); word(vertices+0x30,distance);
                    } else {
                        word(0x4eae48+mark*4,scenario==0 ? material+1 : material);
                        word(0x4ef4a4+mark*4,scenario==4);
                        word(0x4edd34+mark*4,scenario==5 ? n&1 : !(n&1));
                        const uint32_t distance=scenario==3 ? 0x41800000 : scenario==2 || scenario==6 ? 0x40a00000 : 0x3f800000;
                        word(vertices+0x10,distance); word(vertices+0x30,distance);
                        if (scenario==6) word(cs+0x68,0x40000000);
                    }
                    if (n>=64) { fromEntry=true; initial.pc=0x168ef0; reg(4,vehicle); reg(5,tire); setRegU32(&initial,31,0xbad0000); }
                }
            }
            else if (kind <= 170)
            {
                // Collision/drift inverses publish full XYZW, including meaningful first-basis W.
                // Resume actual producers for half the cases; never reconstruct VU state.
                constexpr uint32_t vehicle=0x18000, other=0x1c000, point=0x19000;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                uint32_t prefix=0;
                reg(4,vehicle+0xd0); reg(5,sp+0x20); reg(6,sp);
                for (unsigned o:{0x320u,0x3b0u,0x3b4u,0x3b8u}) f(vehicle+o,2.f);
                if (kind <= 156) {
                    continuation=resume_0x115f30; services.stop=0x1da298;
                    reg(16,vehicle); word(sp+0x40,address(vehicle+0xd0)); word(sp+0x3c,address(sp+0x20));
                    word(vehicle+0x1790,0); word(vehicle+0x1a64,0); word(vehicle+0x1a44,1); word(vehicle+0x1794,0);
                    fp(20,.5f); fp(21,0.f); fp(22,0.f);
                    prefix=kind==155 ? 0x1161dc : 0x116608;
                } else if (kind<=158) {
                    continuation=resume_0x118a10; reg(16,vehicle); reg(5,sp);
                    word(sp+0x50,address(point)); word(sp+0x40,address(point+0x40)); word(sp+0x44,address(point+0x60));
                    word(vehicle+0x1ef8,0); word(vehicle+0x1a58,0); word(vehicle+0x1a74,1);
                    word(vehicle+0x17d8,n%2); returnAt(0x100);
                    if(kind==157) { reg(6,point); prefix=0x119080; }
                    else { reg(6,sp+0x10); setRegU32(&initial,19,0); word(sp+0x4c,1); prefix=0x11926c; }
                } else if (kind<=161) {
                    continuation=resume_0x1195d8; reg(17,vehicle); reg(18,point); reg(19,vehicle+0x190);
                    word(vehicle+0x490,0); word(vehicle+0x17a0,0); word(vehicle+0x1a70,0); word(vehicle+0x17c4,0);
                    returnAt(0xc0); fp(20,0.f); fp(21,n%2 ? .5f : 0.f);
                    if(kind<=160) {
                        reg(5,point); reg(6,sp+0x40); services.stop=0x1daf28;
                        f(gp-0x7608,n%2 ? -100000.f : 100000.f);
                        prefix=kind==159 ? 0x119854 : 0x119864;
                    } else { reg(4,vehicle+0x190); reg(5,vehicle+0xe0); reg(6,vehicle+0x280); reg(20,vehicle+0xe0); prefix=0x119e54; }
                } else if (kind<=163) {
                    continuation=resume_0x11af08; reg(20,vehicle); reg(21,other); setRegU32(&initial,22,1);
                    reg(18,vehicle+0x1d0); reg(19,other+0x1d0); fp(22,.5f); fp(23,1.25f);
                    word(vehicle+0x588,n%2); word(other+0x588,(n+1)%2);
                    f(gp-0x7580,4.f); f(gp-0x757c,4.f); f(gp-0x7578,100000.f); f(gp-0x7574,100000.f);
                    returnAt(0x120);
                    if(kind==162) { reg(4,sp); reg(5,vehicle+0x1e0); reg(6,sp+0x90); reg(16,sp+0x80); reg(17,sp+0x90); services.stop=0x1dbef0; prefix=0x11af9c; }
                    else { reg(4,sp+0x40); reg(5,other+0x1e0); reg(6,sp+0xa0); reg(16,sp+0xa0); reg(17,sp+0x40); prefix=0x11b004; }
                } else if (kind==164) {
                    continuation=resume_0x11b7d0; reg(17,vehicle); reg(19,other); reg(21,vehicle+0x190);
                    reg(4,vehicle+0x190); reg(5,vehicle+0xe0); reg(6,vehicle+0x280);
                    services.stop=0x133dc0; prefix=0x11b980;
                } else if(kind<=166) {
                    continuation=resume_0x11c318; reg(18,vehicle); reg(17,vehicle+0x190); reg(19,sp+0x10); fp(20,0.f);
                    returnAt(0x60);
                    if(kind==165) { reg(4,vehicle+0x190); reg(5,vehicle+0xe0); reg(6,vehicle+0x280); prefix=0x11c37c; }
                    else { reg(5,sp); reg(6,sp+0x10); prefix=0x11c38c; }
                } else if(kind==167) {
                    continuation=resume_0x11c478; reg(18,vehicle); reg(21,sp+0x10); reg(23,sp+0x10);
                    reg(16,vehicle+0xb10); reg(19,vehicle+0xb20); reg(20,sp+0x20); setRegU32(&initial,17,1); setRegU32(&initial,22,0);
                    reg(5,sp+0x20); returnAt(0xb0); prefix=0x11c530;
                } else {
                    continuation=resume_0x1ab410; reg(17,vehicle); reg(18,vehicle+0x230); reg(19,vehicle+0x280);
                    reg(22,vehicle+0x190); reg(23,vehicle+0xe0); reg(30,vehicle+0x290);
                    reg(4,vehicle+0x190); reg(5,vehicle+0xe0); reg(6,kind==169 ? sp+0x40 : vehicle+0x280);
                    word(sp+0x60,address(vehicle+0x220)); returnAt(0x100); fp(20,.125f); fp(21,0.f);
                    word(vehicle+0x1ef8,1); word(vehicle+0x490,n%2); word(vehicle+0x474,1);
                    f(gp-0x4dc4,n%2 ? -.5f : .5f); f(gp-0x4dc0,.5f); f(gp-0x4dbc,.125f);
                    prefix=kind==168 ? 0x1ab54c : kind==169 ? 0x1ab8e4 : 0x1abac4;
                    if(kind==168 && n>=96) {
                        fromEntry=true; initial.pc=0x1ab410; reg(4,vehicle); setRegU32(&initial,31,0xbad0000);
                        word(vehicle+0x1a60,0); word(vehicle+0x17c8,0);
                        f(vehicle+0x32c,.25f); f(vehicle+0x560,.125f);
                        f(gp-0x4de0,n%2 ? 100.f : -100.f);
                    }
                    if(kind==170 && n>=64) {
                        fromEntry=true; initial.pc=0x1ab410; reg(4,vehicle); setRegU32(&initial,31,0xbad0000);
                        word(vehicle+0x474,0); // Full inactive-drift entry still publishes the inverse velocity.
                    }
                }
                const auto basis=bozo::GuestMemory::flatOffset(kind>=168 ? address(vehicle+0x190) : getRegU32(&initial,4),16);
                require(basis>=0,"invalid collision/drift basis"); word(uint32_t(basis)+12,bits());
                if(n>=64 && !fromEntry) { fromEntry=true; initial.pc=prefix; }
            }
            else if (kind <= 177)
            {
                // Complete rigid-body entries retain real cross/dot products, friction and impulse application.
                constexpr uint32_t body=0x18000, other=0x1c000, point=0x19000, normal=0x19040, velocity=0x19080, cs=0x22000;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                for(auto b:{body,other}) {
                    f(b,.5f); word(b+0x30,address(cs)); xyz(b+0x20,1.f,2.f,3.f);
                    xyz(b+0x40,0.f,0.f,0.f); xyz(b+0x60,0.f,0.f,0.f); xyz(b+0xc0,0.f,0.f,0.f);
                    xyz(b+0x70,1.f,0.f,0.f); xyz(b+0x80,0.f,2.f,0.f); xyz(b+0x90,0.f,0.f,3.f);
                    word(b+0x7c,bits());
                }
                xyz(cs+0x20,0.f,1.f,0.f); xyz(cs+0x30,-1.f,0.f,0.f); xyz(cs+0x40,0.f,0.f,1.f);
                xyz(body+0xb0,n%8 ? -2.f : 0.f,n%8 ? (n%3 ? 1.f : 8.f) : 0.f,0.f);
                xyz(other+0xb0,0.f,0.f,0.f); xyz(other+0x40,1.f,0.f,0.f);
                xyz(point,.5f,1.f,n%2 ? .25f : 0.f); xyz(normal,1.f,0.f,0.f); xyz(velocity,0.f,0.f,0.f);
                for(unsigned o:{0x46c0u,0x46bcu,0x46b8u,0x46b4u,0x46b0u,0x46acu}) f(gp-o,0.f);
                fp(20,-.5f); fp(21,1.f); fp(22,.5f); fp(23,-1.f); fp(24,.25f); fp(25,.75f); fp(26,1.f);
                uint32_t entry=0;
                if(kind==171) {
                    continuation=resume_0x1ed650; entry=0x1ed650; reg(17,body); reg(18,body+0x60);
                    reg(4,body+0x70); reg(5,body+0x60); reg(6,body+0xc0); returnAt(0x70);
                } else if(kind<=173) {
                    continuation=resume_0x1ed7a8; entry=0x1ed7a8;
                    reg(21,body); reg(22,other); reg(23,point); reg(19,normal); reg(20,sp+0x20); reg(18,sp+0x40); reg(16,sp+0x50);
                    reg(4,(kind==172 ? body : other)+0x70); reg(5,sp+0x40); reg(6,sp+0x50); returnAt(0xe0);
                } else if(kind<=175) {
                    continuation=resume_0x1ed9c8; entry=0x1ed9c8;
                    reg(20,body); reg(21,point); reg(22,body+0x40); reg(23,body+0xc0); reg(30,body+0xb0);
                    word(sp+0x60,address(body+0x70)); reg(18,sp+0x30); reg(19,sp+0x20); reg(17,normal); reg(16,sp+0x10);
                    reg(4,body+0x70); reg(5,sp+0x20); reg(6,sp+0x30); returnAt(0x100);
                    if(kind==175) { reg(17,sp+0x40); reg(16,sp+0x50); reg(5,sp+0x40); reg(6,sp+0x50); }
                } else if(kind==176) {
                    continuation=resume_0x1edc98; entry=0x1edc98; reg(19,body); reg(21,point); reg(20,normal);
                    reg(17,velocity); reg(18,sp+0x20); reg(16,sp+0x30);
                    reg(4,body+0x70); reg(5,sp+0x20); reg(6,sp+0x30); returnAt(0xa0);
                } else {
                    continuation=resume_0x1edf78; entry=0x1edf78; reg(16,body); reg(17,cs+0x20); reg(18,body+0x70); reg(19,body+0x60); reg(20,body+0xc0);
                    reg(4,body+0x70); reg(5,body+0x60); reg(6,body+0xc0); returnAt(0x90);
                }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry; setRegU32(&initial,31,0xbad0000); reg(4,body);
                    fp(12,float(n%3)*.5f); fp(13,.25f); fp(14,.75f);
                    if(kind==171) { reg(5,n%4 ? normal : body+0x60); reg(6,point); }
                    else if(kind<=173) { reg(5,other); reg(6,point); reg(7,normal); }
                    else if(kind<=175) { reg(5,point); reg(6,normal); }
                    else if(kind==176) { reg(5,n%4 ? velocity : body+0xb0); reg(6,point); reg(7,normal); }
                    // Separate separating-contact and small-speed gates from reached arithmetic paths.
                    if(kind>=172 && kind<=176 && n>=112) {
                        const unsigned offset=kind<=173 ? 0x46c0 : kind<=175 ? 0x46b8 : 0x46b0;
                        xyz(body+0xb0,n>=120 ? -.01f : 2.f,0.f,0.f);
                        if(kind==176) reg(5,velocity);
                        if(n>=120) f(gp-offset+4,-.1f);
                    }
                }
            }
            else if (kind <= 182)
            {
                constexpr uint32_t vehicle=0x18000, info=0x1c000, driver=0x20000, cs=0x22000, target=0x24000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                word(vehicle+0x2004,address(info)); word(vehicle+0x2008,address(driver)); word(vehicle+0x2018,address(cs));
                word(driver+0xb0,address(vehicle)); word(driver+0xb4,address(target)); word(driver+0x174,address(cs));
                xyz(vehicle+0xd0,n%4 ? 1.f : 0.f,n%4 ? 0.f : 1.f,0.f);
                xyz(vehicle+0xe0,n%4 ? 0.f : -1.f,n%4 ? 1.f : 0.f,0.f); xyz(vehicle+0xf0,0.f,0.f,1.f);
                word(vehicle+0xdc,bits());
                constexpr float lateral[]={-4.f,-3.f,-1.5f,0.f,1.5f,3.f,4.f,0.f};
                xyz(target+0x1e0,lateral[n%8],float(int(n%5)-2)*.5f,0.f); f(target+0x2d4,8.f);
                f(vehicle+0x2dc,0.f); f(vehicle+0x2cc,.5f); f(vehicle+0x1f8,n%2 ? .1f : -.1f);
                word(vehicle+0x20a8,scenario==6 ? 0x40 : 0); word(vehicle+0x2140,0); word(vehicle+0x224c,11);
                word(vehicle+0x1a54,0); word(vehicle+0x474,0); word(driver+0x270,1); word(driver+4,0);
                f(cs+0x68,n%2 ? 3.1f : -3.1f); f(driver+0x48,n%2 ? 3.f : -3.f); f(driver+0x54,n%2 ? 3.f : -3.f);
                f(driver+0x5c,.1f); f(driver+0x1a8,20.f); f(driver+0x1b4,10.f); word(driver+0x22c,n%3 ? 1 : 100);
                f(vehicle+0x2d4,25.f); f(info+0x20,scenario==4 ? 80.f : 20.f); f(info+0x24,n%2 ? .2f : -.2f);
                f(info+0x28,scenario==5 ? 1.f : 0.f); word(info+0x60c,1); word(info+0x614,scenario==3 ? 1 : 0);
                word(info+0x5b8,0); word(info+0x47c,n%6); word(info+0x480,1+n%11); word(info+0x5cc,n%2); word(info+0x5f8,0);
                word(info+0x4b0,0); f(info+0x404,1.f); word(info+0x414,0); word(info+0x488,0);
                for(unsigned i=0;i<6;++i) { f(info+0x208+4*i,i%2 ? 80.f : 20.f); f(info+0xf0+4*i,.75f); f(info+0x64+4*i,100.f); }
                for(unsigned i=0;i<11;++i) word(0x4e48f0+4*i,i%3==0 ? 0x105f58 : i%3==1 ? 0x105f64 : 0x105f70);
                word(gp-0x4154,address(0x310000)); f(0x4c3d84,n%3 ? 2.f : 0.f); word(0x4c3db0,4);
                for(auto o:{0x7da4u,0x7d9cu,0x7d98u,0x7d84u}) f(gp-o,.5f);
                f(gp-0x7da0,.5f); f(gp-0x7d88,.5f); f(gp-0x7d90,10.f); f(gp-0x7d8c,10.f);
                f(gp-0x71a0,.5f); f(gp-0x719c,.75f); f(gp-0x7d94,5.f);
                f(gp-0x7ca4,1.f); f(gp-0x7ca0,1.5f); f(gp-0x7c9c,.25f); f(gp-0x7c98,.5f);
                f(gp-0x7c94,100.f); f(gp-0x7c90,80.f); f(gp-0x7c8c,60.f); f(gp-0x7c88,.75f); f(gp-0x7c84,.5f);
                word(gp-0x487c,0x40490fdb); word(gp-0x4878,0x49742400); word(gp-0x4874,0x3e22f983); word(gp-0x4870,0x40c90fdb);
                word(gp-0x486c,0xc0490fdb); word(gp-0x4868,0xc9742400); word(gp-0x4864,0x40490fdb);
                word(gp-0x4860,0x3e22f983); word(gp-0x485c,0x40c90fdb);
                reg(4,vehicle+0xd0); reg(5,target+0x1e0); reg(6,kind==178 ? sp+0x10 : sp);
                reg(16,vehicle); setRegU32(&initial,17,1); setRegU32(&initial,18,0);
                uint32_t entry=0;
                if(kind==178) {
                    continuation=resume_0x102108; entry=0x102944; reg(18,vehicle); reg(20,sp+0x30);
                    setRegU32(&initial,23,n%2); setRegU32(&initial,30,0); f(sp+0x3c,10.f); returnAt(0xd0);
                } else if(kind==179) { continuation=resume_0x102b98; entry=0x102b98; f(sp+0x10,10.f); returnAt(0x30); }
                else if(kind==180) { continuation=resume_0x102c68; entry=0x102c68; fp(20,n%2 ? 25.f : -25.f); returnAt(0x30); }
                else if(kind==181) { continuation=resume_0x105bb0; entry=0x105bb0; fp(20,3.f); fp(21,10.f); fp(22,.5f); returnAt(0x40); }
                else { continuation=resume_0x105e88; entry=0x105e88; fp(20,.5f); fp(21,10.f); returnAt(0x40); }
                if(kind==178) {
                    if(scenario==1) word(info+0x60c,0); if(scenario==2) word(driver+0x270,0);
                    if(scenario==3) word(vehicle+0x1a54,1); if(scenario==4) word(driver+4,5);
                    if(scenario==6) f(info+0x20,800.f); if(scenario==7) f(info+0x24,1.f);
                } else if(kind<=180) {
                    if(scenario==1) { word(vehicle+0x20a8,0x40); word(vehicle+0x2140,1); }
                    if(scenario==2) { word(info+0x5b8,1); f(cs+0x68,0.f); f(info+0x24,0.f); }
                    if(scenario==3) word(vehicle+0x2140,5);
                } else {
                    if(scenario==1) f(vehicle+0x2dc,2.f); if(scenario==2) f(target+0x2d4,0.f);
                }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry; reg(4,vehicle); setRegU32(&initial,31,0xbad0000);
                    fp(12,scenario==7 ? 200.f : 10.f); fp(13,scenario==6 ? 1.f : .5f);
                }
            }
            else if (kind <= 188)
            {
                constexpr uint32_t driver=0x18000, vehicle=0x1c000, first=0x22000, second=0x24000, point=0x28000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool rotated) {
                    xyz(at,rotated ? 0.f : 1.f,rotated ? 1.f : 0.f,0.f);
                    xyz(at+0x10,rotated ? -1.f : 0.f,rotated ? 0.f : 1.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits());
                };
                word(0x4e429c,address(0x17000)); word(0x17058,bits()); // actual generated rand state
                if(kind<=184) {
                    continuation=resume_0x126500; reg(17,driver); reg(20,point); reg(16,sp+0x60); reg(19,sp+0x50); setRegU32(&initial,21,1);
                    word(driver+0xb8,address(0x27000)); word(0x27000,address(point)); word(driver+0x200,0); word(driver+0x208,0);
                    word(driver+0x270,1); word(driver+0x2a8,1); word(driver+0x170,address(first)); word(driver+0x174,address(second));
                    word(driver+4,5); word(driver+8,4); f(driver+0x40,0.f); f(driver+0x1f4,0.f);
                    basis(first+0x20,false); xyz(first+0x10,0.f,0.f,0.f); xyz(second+0x10,0.f,2.f,0.f);
                    xyz(point,1.f,scenario==1 ? 1.f : 3.f,0.f); xyz(sp+0x60,kind==183 ? 0.f : 1.f,kind==183 ? 2.f : (scenario==1 ? 1.f : 3.f),0.f);
                    f(sp+0x74,2.f); reg(4,first+0x20); reg(5,sp+0x60); reg(6,kind==183 ? sp+0x70 : sp+0x80); returnAt(0x160);
                    if(scenario==1) services.stop=0x1da298; // scalar comparison and 150-unit probe staging, before unrelated LOS
                    if(n>=64) {
                        fromEntry=true; initial.pc=0x126500; reg(4,driver); setRegU32(&initial,31,0xbad0000);
                        if(scenario==2) word(driver+0x270,0); if(scenario==3) word(driver+0x2a8,0);
                        if(scenario==4) word(driver+0x170,0); if(scenario==5) word(driver+0x174,0);
                        if(scenario==6) word(driver+4,0); if(scenario==7) word(driver+8,0);
                    }
                } else if(kind<=186) {
                    continuation=resume_0x128638; reg(16,driver); reg(18,sp+0x20); reg(19,sp+0x10); setRegU32(&initial,17,scenario==7 ? 0 : 1);
                    word(driver+0xb0,address(vehicle)); basis(vehicle+0xd0,n%4==0);
                    xyz(driver+0x110,scenario==6 ? 0.f : 1.f,scenario==6 ? 0.f : 1.f,0.f);
                    xyz(driver+0x140,scenario==1 ? 1.f : -1.f,scenario==1 ? 1.f : -1.f,0.f);
                    xyz(sp+0x10,scenario==6 ? 0.f : 1.f,1.f,0.f);
                    uint32_t flags=scenario==5 ? 8 : scenario==2 ? 0 : scenario==4 ? 2 : 1;
                    word(driver+0x23c,flags); word(driver+0x298,scenario==2 || scenario==3 ? 1 : 0); setRegU32(&initial,3,flags);
                    reg(4,vehicle+0xd0); reg(5,driver+(kind==185 ? 0x110 : 0x140)); reg(6,kind==185 ? sp+0x10 : sp+0x20); returnAt(0xc0);
                    if(n>=64) { fromEntry=true; initial.pc=0x128880; }
                } else {
                    constexpr uint32_t boss=0x18000, victim=0x24000;
                    basis(first+0x20,n%4==0); xyz(first+0x10,10.f,20.f,30.f);
                    const float dy=n%2 ? -2.f : 2.f; xyz(victim+0x1d0,13.f,20.f+dy,31.f);
                    word(boss+0xe0,address(first)); word(boss+0xe8,address(victim)); word(boss+0x38,1);
                    word(victim+0x588,0); word(victim+0x58c,n%2 ? 0x100 : 0);
                    reg(16,boss); reg(20,boss+0x70); fp(21,-10.f); fp(20,10.f); fp(22,0.f); fp(23,1.f);
                    if(kind==187) {
                        continuation=resume_0x111f80; services.stop=0x1e6610;
                        xyz(boss+0x70,3.f,dy,1.f); reg(4,first+0x20); reg(5,boss+0x70); reg(6,boss+0x70);
                        if(n>=64) { fromEntry=true; initial.pc=0x1120ec; }
                    } else {
                        continuation=resume_0x10f7c0; services.stop=0x111f80;
                        xyz(boss+0x70,n%4 ? 3.f : dy,n%4 ? dy : -3.f,1.f);
                        reg(17,boss); reg(16,sp+0x10); reg(4,first+0x20); reg(5,boss+0x70); reg(6,sp+0x10);
                        if(n>=64) { fromEntry=true; initial.pc=0x11110c; }
                    }
                }
            }
            else if(kind<=194)
            {
                constexpr uint32_t vehicle=0x18000, basisAt=0x22000, point=0x24000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool rotated) {
                    xyz(at,rotated ? 0.f : 1.f,rotated ? 1.f : 0.f,0.f);
                    xyz(at+0x10,rotated ? -1.f : 0.f,rotated ? 0.f : 1.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits()); word(at+28,bits()); word(at+44,bits());
                };
                f(gp-0x47fc,.0001f); // MatrixFromNormal threshold
                f(gp-0x481c,.0001f); f(gp-0x4818,0.f); f(gp-0x4814,.0001f);
                f(gp-0x480c,.0001f); f(gp-0x4808,1.57079637f); f(gp-0x4804,-1.57079637f);
                if(kind==189) {
                    continuation=resume_0x18bbe8; reg(19,vehicle); reg(17,vehicle+0xd0); reg(16,sp+0x40); reg(18,vehicle+0x200);
                    basis(vehicle+0x10,false); basis(vehicle+0xd0,n%4==0); xyz(sp+0x40,.125f,-.25f,.5f);
                    reg(4,vehicle+0xd0); reg(5,sp+0x40); reg(6,vehicle+0x200); returnAt(0xa0);
                    if(n>=64) {
                        fromEntry=true; initial.pc=0x18bbe8; reg(4,vehicle); setRegU32(&initial,5,scenario==7);
                        word(vehicle+0x580,1); word(vehicle+0x1790,2); setRegU32(&initial,31,0xbad0000);
                    }
                } else if(kind==190) {
                    continuation=resume_0x1ac7e8; reg(17,vehicle); reg(16,point);
                    basis(sp,false); xyz(point,scenario&1 ? -.25f : .25f,scenario==2 ? -1.f : scenario==3 ? .1f : 1.f,.25f);
                    xyz(vehicle+0xe0,0.f,1.f,0.f); f(vehicle+0xd8,scenario%3 ? 1.f : .1f);
                    f(vehicle+0x2c4,n&1 ? -.5f : .5f); f(vehicle+0x2cc,2.f); f(vehicle+0x214,.25f); f(vehicle+0x17b4,1.f);
                    word(vehicle+0x20a8,scenario==7 ? 0xc0 : 0); word(vehicle+0x243c,1); word(vehicle+0x580,scenario==4 ? 3 : 1);
                    const float constants[]={1.f,.5f,.2f,.5f,.3f,2.f,.5f,1.f,-1.f,.2f,.5f,.3f,2.f,.8f};
                    for(unsigned i=0;i<std::size(constants);++i) f(gp-0x4cbc+4*i,constants[i]);
                    reg(4,sp); reg(5,point); reg(6,sp+0x40); returnAt(0x70);
                    if(n>=64) { fromEntry=true; initial.pc=0x1ac7e8; reg(4,vehicle); reg(5,point); setRegU32(&initial,31,0xbad0000); }
                } else if(kind<=192) {
                    continuation=resume_0x1af088; reg(19,vehicle); reg(18,point); reg(16,sp+0x40);
                    setRegU32(&initial,20,scenario==4 ? 75 : 20); setRegU32(&initial,22,scenario==5);
                    fp(20,20.f); basis(vehicle+0xd0,n%4==0); basis(sp,false); xyz(vehicle+0xe0,0.f,1.f,0.f);
                    xyz(point,scenario==6 ? 0.f : scenario&1 ? -1.f : 1.f,scenario==6 ? 0.f : 1.f,scenario==5 ? 1.f : -.25f);
                    f(gp-0x4bd4,.5f); f(gp-0x4bd0,0.f); f(gp-0x4bcc,.1f); f(gp-0x4bc8,.5f);
                    for(unsigned i=0;i<4;++i) { word(vehicle+0x1a90+i*0xf0+0xb8,scenario==3); f(vehicle+0x1a90+i*0xf0+0x64,scenario==4 ? 1.f : 0.f); }
                    word(vehicle+0x1790,(kind==191) != (scenario==7) ? 1 : 3);
                    reg(4,kind==191 ? vehicle+0xd0 : sp); reg(5,point); reg(6,sp+0x40); returnAt(0xe0);
                    if(n>=64) { fromEntry=true; initial.pc=0x1af088; reg(4,vehicle); reg(5,point); setRegU32(&initial,6,scenario==4 ? 75 : 20); setRegU32(&initial,7,scenario==5); setRegU32(&initial,31,0xbad0000); }
                } else {
                    continuation=resume_0x1eb328;
                    constexpr uint32_t table=0x4ca230, light=0x4c99d0, output=0x4ca250;
                    basis(basisAt,n%4==0); xyz(point,0.f,0.f,0.f); xyz(0x26000,0.f,0.f,0.f); xyz(0x27000,0.f,0.f,0.f);
                    for(unsigned i=0;i<8;++i) {
                        word(table+4*i,i<(scenario==1 || scenario==2 ? 4u : scenario==7 ? 0u : 1u) ? i : 0xffffffff);
                        const uint32_t at=light+i*0x40; xyz(at,scenario==3 ? 10000.f : 1.f,0.f,0.f); f(at+12,10.f);
                        xyz(at+0x10,1.f,0.f,0.f); f(at+0x1c,.5f); xyz(at+0x20,.2f,.4f,.6f); f(at+0x2c,.8f); word(at+0x38,scenario==2);
                    }
                    word(gp-0x3458,scenario!=4); word(0x2800c,scenario==6 ? 0 : 0x01000000); fp(21,2.f); fp(22,1.f);
                    word(sp+0x40,address(basisAt)); word(sp+0x44,address(point)); word(sp+0x48,address(0x26000)); word(sp+0x4c,address(0x27000));
                    word(sp+0x50,address(output+0x30)); word(sp+0x54,address(output+0x20)); word(sp+0x58,address(output+0x10));
                    word(sp+0x5c,address(output+0x60)); word(sp+0x60,address(output+0x50)); word(sp+0x64,address(output+0x40)); word(sp+0x68,address(output+0xa0));
                    reg(16,light); reg(17,sp+0x30); reg(19,output); setRegU32(&initial,18,scenario==1 ? 12 : 0); setRegU32(&initial,20,scenario==1 ? 3 : 0); setRegU32(&initial,21,7);
                    reg(22,output+0x40); reg(23,output+0x50); reg(30,output+0x60);
                    xyz(sp+0x20,scenario==3 ? 10000.f : 1.f,0.f,0.f); xyz(sp,1.f,0.f,0.f); xyz(sp+0x10,-1.f,0.f,0.f);
                    reg(4,basisAt); reg(5,kind==193 ? sp+0x20 : light+0x10); reg(6,kind==193 ? sp : sp+0x30); returnAt(0x100);
                    if(n>=64) {
                        fromEntry=true; initial.pc=0x1eb328; reg(4,basisAt); reg(5,point); reg(6,0x26000); reg(7,0x27000); reg(8,0x28000);
                        fp(12,scenario==5 ? 5000.f : 2.f); setRegU32(&initial,31,0xbad0000);
                    }
                }
            }
            else if(kind<=200)
            {
                constexpr uint32_t hud=0x18000, vehicle=0x1c000, basisAt=0x22000, point=0x24000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at) {
                    xyz(at,n%4 ? 1.f : 0.f,n%4 ? 0.f : 1.f,0.f);
                    xyz(at+0x10,n%4 ? 0.f : -1.f,n%4 ? 1.f : 0.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits()); word(at+28,bits()); word(at+44,bits());
                };
                basis(basisAt);
                if(kind==195) {
                    continuation=resume_0x12d038; reg(17,0x500000); reg(16,0x4fc3a0);
                    xyz(0x4fc390,2.f,3.f,4.f); xyz(0x4fc3a0,2.f,3.f,4.f); xyz(point,0.f,0.f,0.f);
                    xyz(0x25000,0.f,0.f,0.f); f(0x25014,1.f); f(0x25018,1.f);
                    xyz(0x26000,scenario&1 ? -2.f : 2.f,3.f,4.f);
                    word(gp-0x44c4,n%4); for(unsigned i=0;i<4;++i) word(0x235280+4*i,4);
                    word(gp-0x3114,scenario!=6); word(gp-0x3124,address(point)); word(gp-0x3128,address(basisAt));
                    word(gp-0x3100,address(0x25000)); f(gp-0x310c,10.f);
                    reg(4,basisAt); reg(5,0x4fc3a0); reg(6,0x4fc3a0); returnAt(0x30);
                    if(n>=64) { fromEntry=true; initial.pc=0x12d038; reg(4,0x26000); if(scenario==7) setRegU32(&initial,4,0); setRegU32(&initial,31,0xbad0000); }
                } else if(kind==196) {
                    continuation=resume_0x136c70; const uint32_t slot=n%4;
                    word(gp-0x4480,slot); xyz(point,10.f,20.f,30.f);
                    for(unsigned i=0;i<4;++i) { xyz(0x239480+16*i,float(i+1),0.f,0.f); word(0x23948c+16*i,bits()); }
                    reg(17,basisAt); reg(20,point); reg(19,0x2394c0); reg(18,0x239490); setRegU32(&initial,16,3); setRegU32(&initial,21,0x70);
                    reg(4,basisAt); reg(5,0x239480); reg(6,sp); returnAt(0x70);
                    if(n>=64) { fromEntry=true; initial.pc=0x136c70; reg(4,point); reg(5,basisAt); setRegU32(&initial,31,0xbad0000); }
                } else if(kind<=198) {
                    continuation=resume_0x13eb40;
                    reg(17,hud); reg(16,vehicle); reg(23,sp+0x10); reg(18,0x28000); setRegU32(&initial,19,0); setRegU32(&initial,30,1);
                    setRegU32(&initial,20,32); setRegU32(&initial,21,48); setRegU32(&initial,22,0);
                    word(hud+0x5e0,address(0x20000)); basis(0x200d0); xyz(0x201d0,0.f,0.f,0.f);
                    const float distance=scenario==1 ? 1000.f : scenario==2 ? 2500.f : scenario==3 ? 600.f : 100.f;
                    xyz(vehicle+0x1d0,distance,0.f,0.f); xyz(sp+0x10,-distance,0.f,0.f);
                    word(hud+4,0); word(hud+0x8d4,0xffffffff); word(hud+0x598,0x00403020); word(hud+0x59c,scenario==6 ? 0 : 0x80605001);
                    word(vehicle+0x20a8,scenario==7 ? (kind==197 ? 0x200 : 0x1e0) : 1);
                    word(vehicle+0x580,scenario==5 ? 2 : 1); word(vehicle+0x20b4,0); word(vehicle+0x480,0);
                    word(sp+0x48,2); word(sp+0x4c,scenario==4 ? 10 : 3000); word(sp+0x50,0x100); word(sp+0x58,address(0x27000)); word(0x27000,1);
                    word(sp+0x30,100); word(sp+0x34,100); word(sp+0x38,32); word(sp+0x3c,32);
                    word(gp-0x4148,address(0x29000)); word(0x43360,1); word(0x2b354,0); word(gp-0x4154,address(0x34000)); word(0x3a650,0);
                    const double constants[]={2000.,1400.,98.}; for(unsigned i=0;i<3;++i) std::memcpy(ram+0x4e6e70+8*i,&constants[i],8);
                    services.stop=kind==197 ? 0x13f378 : 0x13fb10; services.stop2=kind==198 ? 0x141af0 : 0xffffffff;
                    reg(4,0x200d0); reg(5,sp+0x10); reg(6,sp+0x20);
                    if(n>=64) { fromEntry=true; initial.pc=kind==197 ? 0x13f7b0 : 0x13fd58; }
                } else if(kind==199) {
                    continuation=resume_0x1402a0; reg(17,hud); reg(18,point); reg(19,hud+0x534);
                    setRegU32(&initial,21,1); setRegU32(&initial,20,scenario==2 || scenario==3 ? 2 : 1);
                    fp(21,100.f); fp(22,100.f); fp(23,.5f);
                    word(hud,scenario!=6); word(hud+0x14,100); word(hud+0x18,100); word(hud+0x1c,3); word(hud+0x5e0,address(vehicle));
                    word(vehicle+0x20a8,scenario==7 ? 1 : 2); xyz(vehicle+0x1d0,0.f,0.f,0.f); basis(vehicle+0xd0);
                    xyz(point,scenario<=3 ? (scenario&1 ? 2000.f : 100.f) : 0.f,scenario<=3 ? 100.f : 0.f,scenario==4 ? 100.f : 0.f);
                    xyz(sp,scenario<=3 ? (scenario&1 ? 2000.f : 100.f) : 0.f,scenario<=3 ? 100.f : 0.f,scenario==4 ? 100.f : 0.f);
                    word(hud+0x4d0+3,15); word(hud+0x46c,0x01010101); f(gp-0x6c90,3.14159274f); f(gp-0x6c8c,1.57079637f);
                    reg(4,vehicle+0xd0); reg(5,sp); reg(6,sp+0x10); returnAt(0x80);
                    if(n>=64) { fromEntry=true; initial.pc=0x1402a0; reg(4,hud); setRegU32(&initial,5,1); reg(6,point); setRegU32(&initial,7,scenario==2 || scenario==3 ? 2 : 1); fp(12,.5f); setRegU32(&initial,31,0xbad0000); }
                } else {
                    continuation=resume_0x1e9540;
                    constexpr uint32_t particle=0x687f00, indices=0x26000, view=0x22000;
                    xyz(view+0x10,0.f,0.f,0.f); basis(view+0x20); word(0x4dfcd0,address(view)); word(sp+0x230,0);
                    word(gp-0x35c8,0); word(gp-0x35c4,0); word(gp-0x2f3c,address(0x28000));
                    for(unsigned i=0;i<2;++i) {
                        const auto at=particle+i*0x2e80;
                        const float x=scenario==2 ? 1000.f : 0.f, y=scenario==3 || (scenario==1 && i==1) ? 0.f : i ? 10.f : 6.f;
                        xyz(at+0x2c80,n%4 ? x : -y,n%4 ? y : x,scenario==4 ? 1000.f : 0.f);
                        f(at+0x2c8c,.5f); word(at+0x2e54,2); word(at+0x2e40,scenario==5 ? 10 : scenario==7 ? 7 : 2); word(at+0x2e70,scenario==6 ? 0 : 1); word(at+0x2e5c,0);
                        word(0x742f80+12*i+8,n>=64 && i==0 ? 1 : 0xffffffff);
                    }
                    reg(17,particle); setRegU32(&initial,18,0); reg(16,sp+0x210); reg(30,indices); setRegU32(&initial,23,n<64 && (n&4) ? 1 : 0);
                    fp(20,1.f); fp(21,1.f); fp(22,1.f); fp(24,1.f);
                    f(sp,12.f); word(indices,1);
                    const float x=scenario==2 ? 1000.f : 0.f, y=scenario==3 ? 0.f : 6.f;
                    xyz(sp+0x210,n%4 ? x : -y,n%4 ? y : x,scenario==4 ? 1000.f : 0.f);
                    services.stop=0x1f91e8; services.stop2=0x203c18;
                    reg(4,view+0x20); reg(5,sp+0x210); reg(6,sp+0x200);
                    if(n>=64) { fromEntry=true; initial.pc=0x1e9700; setRegU32(&initial,2,0x2e80); }
                }
            }
            else if(kind<=210)
            {
                constexpr uint32_t body=0x18000, force=0x19000, point=0x19040, cs=0x22000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                word(body+0x30,address(cs)); f(body,scenario==7 ? 0.f : .5f); xyz(body+0x20,1.f,2.f,3.f);
                xyz(body+0x10,1.f,2.f,3.f); xyz(body+0x40,n%4 ? 11.f : 8.f,n%4 ? 22.f : 21.f,33.f);
                xyz(cs+0x10,10.f,20.f,30.f); xyz(cs+0x20,n%4 ? 1.f : 0.f,n%4 ? 0.f : 1.f,0.f);
                xyz(cs+0x30,n%4 ? 0.f : -1.f,n%4 ? 1.f : 0.f,0.f); xyz(cs+0x40,0.f,0.f,1.f);
                for(uint32_t o:{0x2cu,0x3cu,0x4cu}) word(cs+o,bits());
                xyz(body+0x50,.25f,-.5f,.75f); xyz(body+0x60,.5f,-.25f,.125f);
                xyz(body+0xb0,.125f,-.25f,.375f); xyz(body+0xc0,.5f,-.5f,.375f);
                xyz(body+0x70,1.f,0.f,0.f); xyz(body+0x80,0.f,2.f,0.f); xyz(body+0x90,0.f,0.f,3.f); word(body+0x7c,bits());
                xyz(body+0xd0,.25f,.5f,.75f); xyz(body+0xe0,.5f,.25f,-.5f);
                xyz(force,scenario==2 ? 0.f : scenario&1 ? -1.f : 1.f,scenario==2 ? 0.f : 2.f,scenario==2 ? 0.f : scenario&1 ? -3.f : 3.f);
                xyz(point,2.f,0.f,1.f); xyz(0x190c0,3.f,4.f,5.f);
                const uint32_t source=scenario==5 ? body+0x50 : force, application=scenario==6 ? body+0x40 : point;
                uint32_t values[3]; std::memcpy(values,ram+source,12);
                if(n%4) std::memcpy(ram+sp,values,12);
                else { word(sp,values[1]^0x80000000u); word(sp+4,values[0]); word(sp+8,values[2]); }
                uint32_t entry=0;
                if(kind==201 || kind==206) {
                    continuation=kind==201 ? resume_0x1ed458 : resume_0x1ed6e8; entry=kind==201 ? 0x1ed458 : 0x1ed6e8;
                    reg(16,body); reg(4,cs+0x20); reg(5,source); reg(6,sp); returnAt(0x20);
                } else if(kind<=208) {
                    continuation=kind<=203 ? resume_0x1ed498 : kind<=205 ? resume_0x1ed518 : resume_0x1ed728;
                    entry=kind<=203 ? 0x1ed498 : kind<=205 ? 0x1ed518 : 0x1ed728;
                    reg(17,body); reg(18,application); reg(16,sp+0x10); returnAt(0x50);
                    const bool second=kind==203 || kind==205 || kind==208;
                    reg(4,cs+0x20); reg(5,second ? application : source); reg(6,second ? sp+0x10 : sp);
                } else if(kind==209) {
                    continuation=resume_0x1ede08; entry=0x1ede08; reg(16,body); reg(21,body+0x40); reg(17,sp+0x70);
                    reg(4,cs+0x20); reg(5,body+0x10); reg(6,sp+0x70); returnAt(0xf0);
                } else {
                    continuation=resume_0x1ee050; entry=0x1ee050; reg(16,body); reg(17,body+0x40);
                    reg(4,cs+0x20); reg(5,body+0x10); reg(6,body+0x40); returnAt(0x20);
                }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry; reg(4,body); reg(5,source); reg(6,application); setRegU32(&initial,31,0xbad0000);
                    fp(12,scenario==0 ? 0.f : .125f);
                }
            }
            else
            {
                constexpr uint32_t driver=0x18000, vehicle=0x1c000, target=0x20000, first=0x24000, second=0x25000, point=0x28000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool rotated) {
                    xyz(at,rotated ? 0.f : 1.f,rotated ? 1.f : 0.f,0.f);
                    xyz(at+0x10,rotated ? -1.f : 0.f,rotated ? 0.f : 1.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits()); word(at+28,bits()); word(at+44,bits());
                };
                word(driver+0xb0,address(vehicle)); word(driver+0xb4,address(target));
                word(driver+0x170,address(first)); word(driver+0x174,address(second));
                basis(first+0x20,false); basis(vehicle+0xd0,n%4==0); basis(target+0x190,n%4==0);
                xyz(first+0x10,10.f,20.f,30.f); xyz(second+0x10,10.f,22.f,30.f);
                xyz(vehicle+0x1d0,10.f,20.f,30.f); xyz(target+0x1d0,2010.f,20.f,30.f);
                xyz(vehicle+0x1e0,0.f,0.f,0.f); f(vehicle+0x2c4,scenario==3 ? 0.f : scenario==7 ? -5.f : 5.f); f(vehicle+0x2d4,20.f);
                f(vehicle+0x394,2.f); f(vehicle+0x3b0,4.f); f(target+0x2d4,10.f); f(target+0x2c4,scenario==7 ? -8.f : 8.f);
                word(driver+0xb8,address(0x27000)); word(0x27000,address(point)); word(driver+0x200,0); word(driver+0x208,0);
                xyz(point,11.f,21.f,30.f); ram[point+0x1c]=2; ram[point+0x1f]=1;
                word(driver+0x270,1); word(driver+0x2a8,1); word(driver+4,5); word(driver+8,4);
                f(driver+0x1f4,0.f); f(driver+0x40,0.f); f(driver+0x44,0.f); f(driver+0x58,0.f); f(driver+0x5c,0.f);
                f(gp-0x7230,-3.1415927f); f(gp-0x722c,1.5707964f); f(gp-0x7228,3.1415927f);
                word(gp-0x487c,0x40490fdb); word(gp-0x4878,0x49742400); word(gp-0x4874,0x3e22f983); word(gp-0x4870,0x40c90fdb);
                word(gp-0x486c,0xc0490fdb); word(gp-0x4868,0xc9742400); word(gp-0x4864,0x40490fdb);
                word(gp-0x4860,0x3e22f983); word(gp-0x485c,0x40c90fdb);
                // Actual empty terrain and a query outside the collision-grid X bounds. No query overrides.
                word(gp-0x33c0,0); word(gp-0x33c4,0); word(gp-0x3620,address(0x26000));
                f(0x2600c,10000.f); f(0x26014,11000.f); f(gp-0x4844,1000000.f); f(gp-0x4840,1e20f);
                f(0x4fb898,0.f); f(0x4fb89c,1.f); f(gp-0x4678,1e20f); f(gp-0x59e8,.5f);
                f(gp-0x47fc,.0001f); f(gp-0x481c,.0001f); f(gp-0x4818,0.f); f(gp-0x4814,.0001f);
                word(0x4e429c,address(0x17000)); word(0x17058,bits());
                uint32_t entry=0;
                if(kind<=212) {
                    continuation=resume_0x126500; entry=0x126500;
                    reg(17,driver); reg(20,point); reg(19,sp+0x50); setRegU32(&initial,21,1); returnAt(0x160);
                    if(kind==211) {
                        reg(18,sp+0xa0); reg(4,first+0x20); reg(5,sp+0x90); reg(6,sp+0xa0); xyz(sp+0x90,0.f,150.f,0.f);
                        if(n>=64) {
                            if(scenario==1) xyz(second+0x10,10.f,170.f,30.f);
                            if(scenario==2) word(driver+0x270,0); if(scenario==3) word(driver+0x2a8,0);
                            if(scenario==4) word(driver+0x170,0); if(scenario==5) word(driver+0x174,0);
                            if(scenario==6) word(driver+4,0); if(scenario==7) word(driver+8,0);
                        }
                    } else {
                        word(driver+0x270,0); f(driver+0x40,scenario&1 ? -2.f : 2.f);
                        if(scenario==1) f(driver+0x40,0.f); if(scenario==2) ram[point+0x1f]=0;
                        if(scenario==3) f(driver+0x1f4,.5f);
                        f(driver+0x58,scenario>=4 ? .25f : 0.f); f(driver+0x5c,scenario>=4 ? -.5f : 0.f);
                        basis(sp,n%4==0); reg(18,sp+0x40); reg(4,sp); reg(5,sp+0x40); reg(6,sp+0x50);
                        xyz(sp+0x40,scenario==2 ? 0.f : scenario==3 ? 1e-30f : 2.f,scenario==2 ? 0.f : -1.f,0.f);
                    }
                } else if(kind==213) {
                    continuation=resume_0x1275e8; entry=0x1275e8; reg(17,driver); reg(16,sp+0x10);
                    const uint32_t output=scenario==5 ? target+0x1d0 : scenario==6 ? vehicle+0x1d0 : 0x29000;
                    reg(18,output); returnAt(0x50); reg(4,target+0x190); reg(5,sp); reg(6,sp+0x10); xyz(sp,0.f,200.f,0.f);
                    if(n>=64) {
                        if(scenario==1) word(driver+0xb4,0); if(scenario==2) f(target+0x2d4,0.f);
                        if(scenario==3) xyz(target+0x1d0,1010.f,20.f,30.f); if(scenario==4) f(target+0x2c4,0.f);
                    }
                } else {
                    continuation=resume_0x128638; entry=0x128638;
                    const bool special=kind==214 || kind==216;
                    const unsigned counter=special ? 4 : scenario==7 ? 0 : 1+n%3;
                    word(driver+0x238,counter); word(driver+0x23c,scenario==4 ? 1 : 0); word(driver+0x298,scenario==5 ? 1 : 0);
                    word(driver+0x16c,address(point)); xyz(point,10.f,scenario==6 ? 30.f : 1020.f,30.f);
                    f(driver+0x178,-2.f); f(driver+0x17c,2.f); f(driver+0x180,6.f); f(driver+0x184,-6.f); f(driver+0x1d0,.5f);
                    basis(driver+0x60,n%4==0); xyz(driver+0x110,1.f,1.f,0.f); xyz(driver+0x140,-1.f,-1.f,0.f);
                    xyz(sp,2.f,4.f,3.f); xyz(sp+0x10,12.f,24.f,35.f);
                    reg(16,driver); reg(19,sp+0x10); reg(18,sp+0x20); fp(20,scenario==6 ? 10.f : 1000.f);
                    reg(4,special ? driver+0x60 : vehicle+0xd0); reg(5,sp); reg(6,kind<=215 ? sp+0x10 : sp+0x20); returnAt(0xc0);
                    if(n>=64) word(driver+0x238,special ? 3 : counter==0 ? 4 : counter-1);
                }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry; reg(4,driver); setRegU32(&initial,31,0xbad0000);
                    if(kind==213) reg(5,scenario==5 ? target+0x1d0 : scenario==6 ? vehicle+0x1d0 : 0x29000);
                }
            }
            snapshot(input.get());
            const auto stopAt=services.stop;
            const auto run = [&](bool native, R5900Context &ctx) {
                services.stop=stopAt; referenceW.clear(); services.yields = services.randomCalls = services.weatherPoints = services.weatherIdentities = 0;
                services.spatialCalls.fill(0); auditedInverseCalls.fill(0); services.aiActions.fill(0); services.vehicleLightActions.fill(0); services.finalActions.fill(0); services.rigidActions.fill(0); rigidDirectionCalls.fill(0); driverDirectionCalls.fill(0); services.driverActions.fill(0); services.driverQueryEndpoints.fill(0);
                rt->functions.setOverride(0x1da1e8, native ? (kind>=155 ? auditedInverse : inverse) : referenceInverse);
                rt->functions.setOverride(0x1da298, native ? (kind == 148 ? rearDirection : kind>=211 ? driverDirection : (kind>=201 && kind<=210) ? rigidDirection : direction) : referenceDirection);
                sawDirectionEntry=false; sawForceInverseEntry=false;
                host.instruction=kind == 139 ? vehicleInstructions : nullptr;
                if (kind == 141 && native) host.instruction=vehicleInstructions;
                rt->functions.setOverride(0x1dcc08,kind == 139 ? matrix : nullptr);
                rt->functions.setOverride(0x1da238,kind == 149 ? registry.find(0x1da238) : nullptr);
                rt->functions.setOverride(0x1db0f0,kind == 149 ? registry.find(0x1db0f0) : nullptr);
                auto helper = isInverse(kind) ? (native ? (kind>=155 ? auditedInverse : inverse) : referenceInverse) : (native ? (kind>=211 ? driverDirection : (kind>=201 && kind<=210) ? rigidDirection : direction) : referenceDirection);
                if (!fromEntry) { helper(ram, &ctx, rt.get()); require(ctx.pc == callers[kind], "wrong hierarchy return"); }
                try { continuation(ram, &ctx, rt.get()); }
                catch (const SegmentEnd &) {
                    require(services.stop != 0xffffffff,"unexpected segment end");
                    if(kind==187) {
                        require(services.stop==0x1e6610,"boss effect staging boundary");
                        // Consume the published local offset in the actual boss position-publication segment.
                        services.stop=0x111f80; setRegU32(&ctx,17,address(0x18000)); ctx.pc=0x11110c;
                        try { resume_0x10f7c0(ram,&ctx,rt.get()); require(false,"boss consumer did not reach collision boundary"); }
                        catch(const SegmentEnd &) { require(ctx.pc==0x111f80,"wrong boss publication boundary"); }
                    }
                    return;
                }
                require(services.stop == 0xffffffff && ctx.pc == 0xbad0000, "hierarchy consumer returned early");
                if (kind == 171 || kind == 177 || (kind>=201 && kind<=210)) {
                    // Read the published angular velocity through the real point-velocity consumer.
                    setRegU32(&ctx,4,address(0x18000)); setRegU32(&ctx,5,address(0x190c0));
                    setRegU32(&ctx,6,address(0x19000)); setRegU32(&ctx,31,0xbad0000); ctx.pc=0x1ee0a0;
                    getPointVelocity__9RigidBodyR8_fvectorT1_0x1ee0a0(ram,&ctx,rt.get());
                    require(ctx.pc==0xbad0000,"rigid-body point velocity returned early");
                }
                if (kind == 139) {
                    setRegU32(&ctx,4,address(0x18000)); ctx.pc=0x1ab250;
                    vehicleSetCsMat__7Vehicle_0x1ab250(ram,&ctx,rt.get());
                    require(ctx.pc==0xbad0000,"peel-out coordinate-system consumer returned early");
                }
            };
            const unsigned control = ((saved | 0x1f80u) & ~0xe040u) | (mode << 13u) | (n & 1 ? 0x8040u : 0u);
            auto a = initial, b = initial; _mm_setcsr(control);
            try { run(false, a); }
            catch (...) { std::fprintf(stderr, "hierarchy original kind %u case %u\n", kind, n); throw; }
            const auto refControl = _mm_getcsr(); const auto refYields = services.yields;
            const auto refRandomCalls = services.randomCalls;
            const auto refWeatherPoints=services.weatherPoints, refWeatherIdentities=services.weatherIdentities;
            const auto refSpatialCalls=services.spatialCalls;
            const auto refAuditedInverseCalls=auditedInverseCalls; const auto refAiActions=services.aiActions; const auto refVehicleLightActions=services.vehicleLightActions; const auto refFinalActions=services.finalActions; const auto refRigidActions=services.rigidActions; const auto refRigidDirections=rigidDirectionCalls; const auto refDriverDirections=driverDirectionCalls; const auto refDriverActions=services.driverActions; const auto refDriverEndpoints=services.driverQueryEndpoints;
            if(kind>=181 && kind<=182) aiShotPublications[kind-181]+=refAiActions[2]!=0;
            if(kind>=155 && kind<=187) {
                const unsigned scenario=(n%64)/8;
                const bool rejected=(kind>=172 && kind<=176 && fromEntry && n>=112) ||
                    (fromEntry && (kind==178 ? scenario!=0 : kind==180 || kind==181 ? scenario==1 || scenario==2 : kind==182 ? scenario==1 :
                        kind==183 || kind==184 ? scenario>=2 : kind==185 || kind==186 ? scenario>=2 && scenario<=4 : false));
                require(refAuditedInverseCalls[kind-155]==unsigned(!rejected),"audited inverse producer reach");
            }
            if(kind>=189 && kind<=194) {
                const unsigned scenario=(n%64)/8;
                const unsigned reached=!fromEntry ? 1 : kind<=192 ? unsigned(scenario!=7) :
                    scenario==1 || scenario==2 ? 4 : scenario==0 || (kind==193 && scenario==3) ? 1 : 0;
                require(refAuditedInverseCalls[kind-156]==reached,"vehicle/light inverse producer reach");
                if(kind==189 && reached) {
                    require(!std::memcmp(ram+0x18200,ram+0x18210,12),"tire rotation delta was not copied");
                    require(!std::memcmp(ram+0x1820c,ram+0x180dc,4),"tire rotation delta lost meaningful inverse W");
                    if(fromEntry) require(refVehicleLightActions[0]==1 && refVehicleLightActions[1]==1,"tire rotation producers not executed");
                }
                if(kind==190 && reached) {
                    require(!std::memcmp(ram+0x181f0,ram+0x18200,12) && !std::memcmp(ram+0x181f0,ram+0x18210,12),"rollover angular state not published");
                    if(fromEntry) { require(refVehicleLightActions[2]==1,"rollover normal producer not executed"); ++rolloverPublicationCases; }
                }
                if(kind==191 || kind==192) weaponJumpCases+=refVehicleLightActions[3]!=0;
                if(kind>=193 && fromEntry) {
                    uint32_t published; std::memcpy(&published,ram+gp-0x3410,4);
                    const bool illuminated=scenario<=2;
                    require(published==unsigned(illuminated),"object-light publication branch not reached");
                    lightingPublicationCases+=published!=0;
                    if(scenario==1) require(refVehicleLightActions[4]==4,"four-light scalar dot consumers not executed");
                }
            }
            if(kind>=195 && kind<=200) {
                const unsigned scenario=(n%64)/8;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                const unsigned reached=!fromEntry ? (kind==196 ? 4 : 1) : kind==195 ? unsigned(scenario<6) : kind==196 ? 4 :
                    kind==197 ? unsigned(scenario<6) : kind==198 ? unsigned(scenario!=6) : kind==199 ? unsigned(scenario<6) : scenario<=4 ? 2 : 0;
                require(refAuditedInverseCalls[kind-156]==reached,"final inverse producer reach");
                if(kind==195) {
                    require(read(gp-0x3118)==unsigned(reached!=0),"force-field hit flag not published");
                    if(reached) {
                        require(refFinalActions[0]==1,"force-field normalize consumer not executed");
                        require(read(0x4fc3ac)==read(0x2200c),"force-field lost meaningful inverse W");
                        ++finalPublications[0];
                    }
                }
                if(kind==196) {
                    require(refFinalActions[1]==4 && read(gp-0x4480)==n%4+1,"health-stand loop did not publish four vertices");
                    for(unsigned i=0;i<4;++i) {
                        const float expected[3]={n%4 ? 11.f+float(i) : 10.f,n%4 ? 20.f : 19.f-float(i),30.f};
                        require(!std::memcmp(ram+0x2394c0+(n%4)*0x70+i*16,expected,12),"health-stand transformed vertex differs");
                    }
                }
                if(kind==197) {
                    const bool packet=!fromEntry || scenario!=6;
                    require(read(0x18004)==(packet ? 5u : 0u),"HUD distance packet not published");
                    if(packet) { require(read(0x2801c)==(scenario==1 ? 100u : scenario==2 ? 30u : 128u),"HUD distance alpha differs"); ++finalPublications[1]; }
                }
                if(kind==198 && reached && scenario!=4 && scenario!=1 && scenario!=2 && scenario!=3) {
                    require(read(0x188d4)==0,"HUD nearest target not published"); ++finalPublications[2];
                }
                if(kind==199) require(read(0x1801c)==(reached ? 4u : 3u),"radar signal count not published");
                if(kind==200 && fromEntry) {
                    const unsigned visible=scenario==0 ? 2 : scenario==1 ? 1 : 0;
                    require(getRegU32(&a,23)==visible,"particle visibility/sort count differs");
                    if(visible==2) {
                        require(read(0x26000)==1 && read(0x26004)==0,"particle depth order not published");
                        const float depths[]={8.f,4.f}; require(!std::memcmp(ram+sp,depths,8),"particle sorted depths differ");
                        ++finalPublications[3];
                    }
                    if(visible==1) require(read(0x26000)==0,"single visible particle not published");
                    require(refFinalActions[2]==reached && refFinalActions[3]==reached,"particle camera getters not executed");
                }
            }
            if((kind>=201 && kind<=210)) {
                require(refRigidDirections[kind-201]==1,"rigid forward site not reached");
                const bool pair=kind==202 || kind==203 || kind==204 || kind==205 || kind==207 || kind==208;
                const bool second=kind==203 || kind==205 || kind==208;
                require(referenceW.size()==(pair && (fromEntry || !second) ? 2u : 1u),"rigid forward output count differs");
                const uint32_t frame=fromEntry ? sp-(kind==209 ? 0x110u : kind==201 || kind==206 || kind==210 ? 0x30u : 0x60u) : sp;
                for(auto patch:referenceW) {
                    const bool allowed=kind==209 ? patch.offset==frame+0x7c : kind==210 ? patch.offset==0x1804c :
                        pair ? patch.offset==frame+0x1c || ((!second || fromEntry) && patch.offset==frame+0xc) : patch.offset==frame+0xc;
                    require(allowed,"unexpected rigid forward-W exclusion");
                }
                const unsigned action=kind==201 ? 0 : kind<=203 ? 1 : kind<=205 ? 2 : kind==206 ? 3 : kind<=208 ? 4 : 5;
                if(kind<209 || (kind==209 && fromEntry)) require(refRigidActions[action]==1,"rigid world consumer not executed");
                if(kind==209 && fromEntry) require(refRigidActions[7]==1 && refRigidActions[8]==1,"rigid integration matrix producers not executed");
                if(fromEntry && (n%64)/8==0) {
                    const bool rotated=n%4==0;
                    const float force[3]={rotated ? -2.f : 1.f,rotated ? 1.f : 2.f,3.f};
                    const float moment[3]={rotated ? 5.f : -2.f,rotated ? -2.f : -5.f,4.f};
                    const auto vector=[&](uint32_t at,float x,float y,float z) {
                        const float expected[3]={x,y,z};
                        if(std::memcmp(ram+at,expected,12)) { float actual[3]; std::memcpy(actual,ram+at,12); std::fprintf(stderr,"rigid known kind %u case %u address %x actual %a %a %a expected %a %a %a\n",kind,n,at,actual[0],actual[1],actual[2],x,y,z); }
                        require(!std::memcmp(ram+at,expected,12),"rigid known state publication differs");
                    };
                    if(kind<=203) vector(0x180d0,.25f+force[0],.5f+force[1],.75f+force[2]);
                    if(kind>=202 && kind<=205) vector(0x180e0,.5f+moment[0],.25f+moment[1],-.5f+moment[2]);
                    if(kind>=206 && kind<=208) {
                        vector(0x18050,.25f+force[0],-.5f+force[1],.75f+force[2]);
                        vector(0x180b0,(.25f+force[0])*.5f,(-.5f+force[1])*.5f,(.75f+force[2])*.5f);
                        if(kind>=207) {
                            vector(0x18060,.5f+moment[0],-.25f+moment[1],.125f+moment[2]);
                            vector(0x180c0,.5f+moment[0],(-.25f+moment[1])*2.f,(.125f+moment[2])*3.f);
                        }
                    }
                    if(kind==209) { vector(0x18050,.25f,-.5f,.75f); vector(0x18060,.5f,-.25f,.125f); }
                    if(kind==210) vector(0x18040,rotated ? 8.f : 11.f,rotated ? 21.f : 22.f,33.f);
                }
            }
            if(kind>=211) {
                const unsigned scenario=(n%64)/8;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                const auto vector=[&](uint32_t at,float x,float y,float z) {
                    const float expected[3]={x,y,z}; require(!std::memcmp(ram+at,expected,12),"driver known state publication differs");
                };
                std::array<unsigned,7> reached{};
                if(kind==211) reached[0]=!fromEntry || scenario<=1;
                else if(kind==212) reached[1]=!fromEntry || (scenario!=1 && scenario!=2);
                else if(kind==213) reached[2]=!fromEntry || scenario==0 || scenario>=4;
                else if(kind==214 || kind==216) {
                    reached[3]=fromEntry || kind==214;
                    reached[5]=kind==216 && !fromEntry ? 1 : scenario!=6;
                } else { reached[4]=fromEntry || kind==215; reached[6]=1; }
                require(refDriverDirections==reached,"driver forward producer reach differs");
                const bool inversePair=(kind==214 || kind==216) && scenario==4;
                const unsigned outputCount=inversePair ? 0 : std::accumulate(reached.begin(),reached.end(),0u);
                require(referenceW.size()==outputCount,"driver forward output count differs");
                const uint32_t frame=fromEntry ? sp-(kind<=212 ? 0x180u : kind==213 ? 0x70u : 0xe0u) : sp;
                for(auto patch:referenceW) {
                    const bool allowed=kind==211 ? patch.offset==frame+0xac : kind==212 ? patch.offset==frame+0x5c :
                        kind==213 ? patch.offset==frame+0x1c : patch.offset==frame+0x1c || patch.offset==frame+0x2c;
                    require(allowed,"unexpected driver forward-W exclusion");
                }
                const unsigned queries=kind==211 ? unsigned(!fromEntry || scenario==0) : kind>=214 ? 1 : 0;
                require(refDriverActions[0]==queries && refDriverActions[1]==queries && refDriverActions[2]==queries,
                    "driver did not execute actual LOS/terrain/grid consumers");
                require(refDriverActions[3]==unsigned(kind==212 && fromEntry && reached[1]),"driver lateral rotation producer reach");
                require(refDriverActions[4]==0,"empty-world LOS must take its actual clear path");
                if(kind<=212 && fromEntry) {
                    if(kind==211 && scenario==0) { vector(0x18010,10.f,170.f,33.f); require(read(0x182a8)==0,"clear path probe flag not published"); ++driverPublications[0]; }
                    if(kind==211 && scenario==1) { vector(0x18010,11.f,21.f,30.f); require(read(0x182a8)==1,"near probe gate not retained"); }
                    if(kind==212 && scenario==0) { vector(0x18010,13.f,21.f,30.f); ++driverPublications[1]; }
                    if(kind==212 && (scenario==1 || scenario==2)) vector(0x18010,11.f,21.f,30.f);
                }
                if(kind==213 && fromEntry) {
                    if(scenario>=1 && scenario<=3) vector(0x29000,10.f,20.f,30.f);
                    if(scenario==0) { vector(0x29000,n%4==0 ? 1810.f : 2010.f,n%4==0 ? 20.f : 220.f,30.f); ++driverPublications[2]; }
                }
                if(kind>=214) {
                    const bool special=kind==214 || kind==216;
                    const unsigned counter=special ? 4 : scenario==7 ? 0 : 1+n%3;
                    require(read(0x18238)==counter,"driving LOS counter/wrap not retained");
                    if(special) require(read(0x18240)==1,"clear driving LOS state not published");
                    else require(read(0x1823c)==((scenario==4 ? 1u : 0u)&~(1u<<counter)),"clear driving LOS mask not published");
                    if(fromEntry && scenario==0 && special) {
                        const float expected[6]={10.f,24.f,35.f,10.f,64.f,35.f};
                        require(!std::memcmp(refDriverEndpoints.data(),expected,sizeof(expected)),"driving LOS known endpoints differ");
                        ++driverPublications[3];
                    }
                    if(inversePair) require(refAuditedInverseCalls[30]==1 && refAuditedInverseCalls[31]==1,"driving LOS inverse consumers not executed");
                }
            }
            if (kind == 138 && fromEntry) require(refRandomCalls == 2, "random-position producer must make two real rand calls");
            if (expectedSelection >= 0) require(getRegU32(&a,2) == uint32_t(expectedSelection), "original attractor boundary selection");
            // Tow-truck and attractor consumers need no forward-W exclusion.
            if (kind >= 132 && kind <= 138) require(referenceW.empty(), "complete consumer left opaque forward W unconsumed");
            if (kind == 139) for (auto patch:referenceW) require(patch.offset==0x1852c,"unexpected peel-out W exclusion");
            if (kind == 149) for (auto patch:referenceW)
                require(patch.offset==sp+0x2c || patch.offset==sp-0x140+0x2c,"unexpected weather W exclusion");
            if (kind == 149) {
                const unsigned scenario=(n%64)/4; const bool disabled=fromEntry && scenario==0;
                require(referenceW.size()==(disabled ? 0u : 1u),"weather producer did not reach its transform");
                require(refWeatherPoints==unsigned(!disabled && (scenario&1)) &&
                        refWeatherIdentities==unsigned(!disabled && !(scenario&1)),"weather publication branch not reached");
            }
            if (kind >= 150 && kind <= 154 && fromEntry) {
                const unsigned scenario=(n%64)/8;
                const bool reached=kind<=151 || (kind==152 ? scenario>=4 : kind==153 ? scenario!=0 && scenario!=5 && scenario!=6 :
                    scenario!=0 && scenario!=3 && scenario!=4 && scenario!=5 && scenario!=6);
                if (refSpatialCalls[kind-150]!=unsigned(reached))
                    std::fprintf(stderr,"producer reach kind %u case %u scenario %u: %u expected %u\n",kind,n,scenario,refSpatialCalls[kind-150],unsigned(reached));
                require(refSpatialCalls[kind-150]==unsigned(reached),"weapon/skid producer transform reach");
            }
            if (kind>=150 && kind<=154) for (auto patch:referenceW) {
                const uint32_t tire=0x18000+0x1a90+(n%4)*0xf0;
                const bool allowed=kind<=151 ? patch.offset==sp+0xc || patch.offset==sp-0x60+0xc :
                    kind==152 ? patch.offset==tire+0x3c || patch.offset==sp+0x1c || patch.offset==sp-0xc0+0x1c :
                    patch.offset==sp+0x2c || patch.offset==sp-0xb0+0x2c;
                require(allowed,"unexpected weapon/skid W exclusion");
            }
            if(kind==187 || kind==188) {
                require(referenceW.size()==1 && referenceW[0].offset==sp+0x1c,"unexpected boss forward-W exclusion");
                if(kind==187) require(refRandomCalls==3,"boss effect producer must use three actual rand calls");
                uint32_t actual[3]; std::memcpy(actual,ram+0x241d0,12);
                const float expected[3]={13.f,20.f+(n%2 ? -2.f : 2.f),31.f};
                require(!std::memcmp(actual,expected,12),"boss inverse-to-publication round trip");
            }
            if (((kind>=155 && kind<=186) || (kind>=189 && kind<=200)) && !referenceW.empty()) {
                std::fprintf(stderr,"unexpected forward W kind %u case %u offset %x\n",kind,n,referenceW.front().offset);
                require(false,"collision/drift inverse must compare full W");
            }
            // Normalize only forward-output W after all real consumers have run, exposing any W-dependent result.
            for (auto patch : referenceW) std::memcpy(ram + patch.offset, &patch.value, 4);
            // Weather's full-entry sample covers active fog updates and aliased projection output, not its disabled gate.
            const bool wholeCase=n < 4 || (fromEntry && (kind==168 ? n>=96 && n<100 : kind == 149 ? n>=92 && n<96 : kind==152 ? n>=96 && n<100 : kind>=153 && kind<=154 ? n>=72 && n<76 : n<68));
            snapshot(expected.get()); if (wholeCase) { std::memcpy(whole.get(), ram, bozo::kFlatBytes); ++memoryCases; }
            restore(); _mm_setcsr(control); run(true, b); const auto after = _mm_getcsr(); _mm_setcsr(saved);
            // Full rear-view entries retain the original rotation producer's identity-helper VU writes.
            const auto &expectedVu=kind == 148 && sawDirectionEntry ? directionEntry : kind==195 && sawForceInverseEntry ? forceInverseEntry : initial;
            if(kind==195 && sawForceInverseEntry) require(vuUntouched(forceReferenceEntry,forceInverseEntry),"force-field original producer effects differ");
            if (!cpuEqual(a,b) || !vuUntouched(expectedVu,b) || after != refControl || services.yields != refYields || services.randomCalls != refRandomCalls ||
                services.weatherPoints != refWeatherPoints || services.weatherIdentities != refWeatherIdentities || services.spatialCalls != refSpatialCalls || auditedInverseCalls != refAuditedInverseCalls || services.aiActions != refAiActions || services.vehicleLightActions != refVehicleLightActions || services.finalActions != refFinalActions || services.rigidActions != refRigidActions || rigidDirectionCalls != refRigidDirections || driverDirectionCalls != refDriverDirections || services.driverActions != refDriverActions || services.driverQueryEndpoints != refDriverEndpoints || !dataEqual() ||
                (wholeCase && std::memcmp(whole.get(),ram,bozo::kFlatBytes)))
            { std::fprintf(stderr,"FAIL hierarchy %x case %u mode %u CPU%d VU%d FP%d data%d\n",callers[kind],n,mode,cpuEqual(a,b),vuUntouched(initial,b),after==refControl,dataEqual()); throw std::runtime_error("hierarchy consumer differs"); }
            ++cases;
        }
    require(aiShotPublications[0] && aiShotPublications[1],"AI fixtures did not publish front/rear attack states");
    std::printf("AI attack publication cases: front %u rear %u\n",aiShotPublications[0],aiShotPublications[1]);
    require(weaponJumpCases && lightingPublicationCases && rolloverPublicationCases,"vehicle/light fixtures missed successful publication");
    std::printf("Vehicle/light publication cases: tire jumps %u lighting %u rollover %u\n",weaponJumpCases,lightingPublicationCases,rolloverPublicationCases);
    require(std::all_of(finalPublications.begin(),finalPublications.end(),[](unsigned n) { return n!=0; }),"final inverse fixtures missed successful publication");
    std::printf("Final inverse publication cases: force-field %u HUD packets %u nearest targets %u particle sorting %u\n",finalPublications[0],finalPublications[1],finalPublications[2],finalPublications[3]);
    require(std::all_of(driverPublications.begin(),driverPublications.end(),[](unsigned n) { return n!=0; }),"driver fixtures missed successful publication");
    std::printf("Driver forward publication cases: path probes %u lateral offsets %u lookahead %u LOS endpoints %u\n",driverPublications[0],driverPublications[1],driverPublications[2],driverPublications[3]);
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
    std::printf("hierarchy consumers: %u generated sequences, %u whole-memory checks; CPU/clock/FP exact outside retired VU/forward W; native leaves VU untouched, original producer effects retained; %u historical forward ABI cases (%u whole-memory)\n", cases, memoryCases, historical, historical / 16);
}
