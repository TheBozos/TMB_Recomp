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
        std::array<unsigned,10> collisionActions{};
        std::array<unsigned,11> attachmentActions{};
        std::array<unsigned,6> minionActions{};
        std::vector<std::array<uint32_t,3>> minionPoints;
        bool minionFixture=false, weaponFixture=false, launcherFixture=false, jetFixture=false, chainFixture=false, loopFixture=false;
        std::vector<uint32_t> weaponOutAddrs;
        // Weapon transform inputs/outputs and the actual consumers' published points.
        std::vector<std::array<uint32_t,3>> weaponInputs, weaponBases, weaponOutputs, weaponTargets;
        std::array<uint32_t,12> collisionVelocities{};
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
            constexpr uint32_t collisionHelpers[]={0x11ae70,0x11af08,0x11b210,0x1da778,0x1dbef0,0x1dbd98,0x11c0e0,0x1dbe58,0x1bffc8,0x21c300};
            for(unsigned i=0;i<std::size(collisionHelpers);++i) if(ctx->pc==collisionHelpers[i]) ++collisionActions[i];
            if(ctx->pc==0x11ae70) for(unsigned i=0;i<2;++i) {
                const auto at=bozo::GuestMemory::flatOffset(getRegU32(ctx,i ? 7 : 4),0x2d8);
                require(at>=0,"invalid collision vehicle");
                std::memcpy(collisionVelocities.data()+6*i,ram+at+0x1e0,12);
                for(unsigned j=0;j<3;++j) std::memcpy(collisionVelocities.data()+6*i+3+j,ram+at+(j==2 ? 0x2d4 : 0x2c4+4*j),4);
            }
            constexpr uint32_t attachmentHelpers[]={0x1ee1c8,0x1f6a50,0x1c89e8,0x1c87a0,0x201dd8,0x201e18,0x1cec90,0x1efa38,0x1cee88,0x155a68,0x1ee258};
            for(unsigned i=0;i<std::size(attachmentHelpers);++i) if(ctx->pc==attachmentHelpers[i]) ++attachmentActions[i];
            if(minionFixture) {
                constexpr uint32_t helpers[]={0x12d038,0x1dbb28,0x112358,0x1d4ff8,0x1b04d8,0x1010b8};
                for(unsigned i=0;i<std::size(helpers);++i) if(ctx->pc==helpers[i]) ++minionActions[i];
                if(ctx->pc==0x1dbb28) {
                    const auto at=bozo::GuestMemory::flatOffset(getRegU32(ctx,5),12);
                    require(at>=0,"invalid Minion hit point"); std::array<uint32_t,3> point;
                    std::memcpy(point.data(),ram+at,12); minionPoints.push_back(point);
                }
            }
            if(weaponFixture) {
                const auto capture=[&](unsigned r,std::vector<std::array<uint32_t,3>> &out) {
                    const auto at=bozo::GuestMemory::flatOffset(getRegU32(ctx,r),12);
                    require(at>=0,"invalid weapon vector"); std::array<uint32_t,3> v; std::memcpy(v.data(),ram+at,12); out.push_back(v);
                };
                // Chained transforms (one transform's output is the next one's input, no addition between): record that
                // output, read as the next input, with a zero base. Only the volley fixtures chain.
                if(chainFixture && ctx->pc==0x1da298 && weaponOutputs.size()<weaponInputs.size()) {
                    capture(5,weaponOutputs); weaponBases.push_back({0,0,0});
                }
                // A transform read only by DispatchHit (the torch hit points): its output, with a zero base.
                if(chainFixture && ctx->pc==0x142d98 && weaponOutputs.size()<weaponInputs.size()) {
                    capture(5,weaponOutputs); weaponBases.push_back({0,0,0});
                }
                if(ctx->pc==0x1da298) { capture(5,weaponInputs); weaponOutAddrs.push_back(getRegU32(ctx,6)&0x1fffffffu); }
                // LoopTank transforms both points of a segment before adding the tank's position to each (output first,
                // position second): pair each addition with the transform that wrote its first operand.
                if(loopFixture) {
                    if(ctx->pc==0x1daef0 && weaponOutputs.size()<weaponInputs.size() && (getRegU32(ctx,5)&0x1fffffffu)==weaponOutAddrs[weaponOutputs.size()])
                        { capture(6,weaponBases); capture(5,weaponOutputs); }
                } else if(ctx->pc==0x1daef0 && weaponOutputs.size()<weaponInputs.size()) { capture(5,weaponBases); capture(6,weaponOutputs); }
                if(ctx->pc==0x1dbe18) capture(5,weaponTargets);
                if(ctx->pc==0x1cec90) capture(4,weaponTargets);
                if(launcherFixture && ctx->pc==0x1dbd58) capture(5,weaponTargets);
                if(launcherFixture && ctx->pc==0x1b7590) capture(8,weaponTargets); // CreateStaticWeapon's position
                if(jetFixture && ctx->pc==0x1e6610) capture(4,weaponTargets); // particleCreateFx's point
            }
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
        {towVectors,towVectorBytes},{towMaterials,towMaterialBytes},{0x230010,64},{0x687680,0x130},{0x687f00,0xb800},{0x446560,0x80},{0x235280,0x10},{0x239480,0x400},{0x742f80,0x20},{0x4b0560,0x10},{0x360000,0x2600},{0x4ac0d0,0x20}};
    constexpr size_t fixtureBytes = bytes + globalBytes + debugBytes + 4 + towVectorBytes + towMaterialBytes + 64 + 0x130 + 0xb800 + 0x80 + 0x10 + 0x400 + 0x20 + 0x10 + 0x2600 + 0x20;
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
    constexpr uint32_t collisionDirectionReturns[]={0x11ad1c,0x11ada0,0x11b26c,0x11bba8,0x11bbe8,0x11c368};
    std::array<unsigned,std::size(collisionDirectionReturns)> collisionDirectionCalls{};
    void countCollisionDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(collisionDirectionReturns);++i) if(getRegU32(ctx,31)==collisionDirectionReturns[i]) ++collisionDirectionCalls[i];
    }
    void collisionDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countCollisionDirection(ctx); const auto before=*ctx; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"collision forward leaf changed VU state");
    }
    constexpr uint32_t attachmentDirectionReturns[]={0x129e58,0x137cac,0x1b2d1c,0x1b3d30,0x1b3e00};
    std::array<unsigned,std::size(attachmentDirectionReturns)> attachmentDirectionCalls{};
    void countAttachmentDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(attachmentDirectionReturns);++i) if(getRegU32(ctx,31)==attachmentDirectionReturns[i]) ++attachmentDirectionCalls[i];
    }
    void attachmentDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countAttachmentDirection(ctx); const auto before=*ctx; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"attachment forward leaf changed VU state");
    }
    constexpr uint32_t minionDirectionReturns[]={0x1ada84,0x1adb8c,0x1adba0,0x1adca8,0x1adcbc,0x1addc4,0x1addd8};
    std::array<unsigned,std::size(minionDirectionReturns)> minionDirectionCalls{};
    void countMinionDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(minionDirectionReturns);++i) if(getRegU32(ctx,31)==minionDirectionReturns[i]) ++minionDirectionCalls[i];
    }
    void minionDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countMinionDirection(ctx); const auto before=*ctx; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"Minion forward leaf changed VU state");
    }
    constexpr uint32_t weaponDirectionReturns[]={0x1b1700,0x1b1bbc,0x1b339c,0x1b3634,0x1b371c,0x1b3ef8,0x1b2fb0,0x1b3118,0x150efc,0x1511ec,0x1513f4,0x150094,0x1502cc,0x14d62c,0x14d678,0x12aad4,0x169ed0,0x12a180,0x1392e4,0x143468,0x10eca0,0x135cf8,0x136200,0x1362ec,0x10eee0,0x10efa0,0x143864,0x143940,0x1439bc,0x143a18,0x143b0c,0x143b60,0x129c48,0x10cc7c,0x1b79f8,0x1b7a14,0x18b10c,0x18b19c,0x1af700,0x1af790,0x1af890,0x1af8fc,0x1af980,0x14d294,0x1b3874,0x1b39a4,0x1b3b00,0x103534,0x106af0,0x1e7e08,0x1e77e8,0x10febc,0x110114,0x1101b0,0x1103e4,0x1104e4,0x110880,0x11091c,0x110a0c,0x110adc,0x110d9c,0x110e8c,0x110f74,0x111788,0x111cc4,0x14dc44,0x14dccc,0x14dea8,0x14df2c,0x14e020,0x14ee10,0x14ee2c,0x14ee48,0x14ef50,0x14ef80,0x14eb9c,0x14ec04,0x14ec70,0x14ed94,0x14e2d0,0x14e320,0x14e498,0x14e4dc,0x14e6a8,0x14e6ec,0x1b6cd8,0x1b6d3c,0x1b5168,0x1b52c0,0x1b5528,0x1b5630,0x12a6d8,0x12a930,0x12a9e8,0x11201c,0x14f190,0x14f1c4,0x14f3e8,0x14f654,0x14f68c,0x14f72c,0x14f954,0x14fb7c,0x14fbb8,0x14fc24,0x151c08,0x151c18};
    std::array<unsigned,std::size(weaponDirectionReturns)> weaponDirectionCalls{};
    void countWeaponDirection(const R5900Context *ctx) {
        for(unsigned i=0;i<std::size(weaponDirectionReturns);++i) if(getRegU32(ctx,31)==weaponDirectionReturns[i]) ++weaponDirectionCalls[i];
    }
    R5900Context weaponLeafEntry; bool sawWeaponLeaf=false;
    void weaponDirection(uint8_t *ram,R5900Context *ctx,Runtime *rt) {
        countWeaponDirection(ctx); const auto before=*ctx; weaponLeafEntry=before; sawWeaponLeaf=true; nativeDirectionImplementation(ram,ctx,rt);
        require(vuUntouched(before,*ctx),"weapon forward leaf changed VU state");
    }
    // Weapon owners reach real libm/matrix helpers that use VU0 themselves. Outside the original leaf's write set
    // (VF11-13, VF15, VF16, ACC, MAC/status flags) the native run must match the original exactly; inside it, each word
    // must be either the original's later real write or the value the native leaf found at its entry.
    bool weaponVuRetired(const R5900Context &initial, const R5900Context &original, const R5900Context &native)
    {
        const auto *i=reinterpret_cast<const uint8_t *>(sawWeaponLeaf ? &weaponLeafEntry : &initial), *o=reinterpret_cast<const uint8_t *>(&original), *n=reinterpret_cast<const uint8_t *>(&native);
        const auto leaf=[](size_t at) {
            for(unsigned r:{11u,12u,13u,15u,16u}) { const size_t f=offsetof(R5900Context,vu0_vf)+16*r; if(at>=f && at<f+16) return true; }
            const size_t acc=offsetof(R5900Context,vu0_acc);
            return (at>=acc && at<acc+16) || at==offsetof(R5900Context,vu0_status) || at==offsetof(R5900Context,vu0_mac);
        };
        for(size_t at=vuAt;at<vuAt+vuBytes;at+=4)
            if(std::memcmp(o+at,n+at,4) && !(leaf(at) && !std::memcmp(i+at,n+at,4))) return false;
        return true;
    }
    struct SavedW { uint32_t offset, value; };
    std::vector<SavedW> referenceW;
    void referenceDirection(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        countRigidDirection(ctx); countDriverDirection(ctx); countCollisionDirection(ctx); countAttachmentDirection(ctx); countMinionDirection(ctx); countWeaponDirection(ctx);
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
        0x126618,0x1267f0,0x1276a4,0x128704,0x128744,0x1287dc,0x12880c,
        0x11ad1c,0x11ada0,0x11b26c,0x11bba8,0x11bbe8,0x11c368,
        0x129e58,0x137cac,0x1b2d1c,0x1b3d30,0x1b3e00,
        0x1ada84,0x1adb8c,0x1adba0,0x1adca8,0x1adcbc,0x1addc4,0x1addd8,
        0x1b1700,0x1b1bbc,0x1b339c,0x1b3634,0x1b371c,0x1b3ef8,0x1b2fb0,0x1b3118,
        0x150efc,0x1511ec,0x1513f4,0x150094,0x1502cc,0x14d62c,0x14d678,0x12aad4,0x169ed0,0x12a180,0x1392e4,0x143468,0x10eca0,0x135cf8,0x136200,0x1362ec,0x10eee0,0x10efa0,0x143864,0x143940,0x1439bc,0x143a18,0x143b0c,0x143b60,0x129c48,0x10cc7c,0x1b79f8,0x1b7a14,0x18b10c,0x18b19c,0x1af700,0x1af790,0x1af890,0x1af8fc,0x1af980,0x14d294,0x1b3874,0x1b39a4,0x1b3b00,0x103534,0x106af0,0x1e7e08,0x1e77e8,0x10febc,0x110114,0x1101b0,0x1103e4,0x1104e4,0x110880,0x11091c,0x110a0c,0x110adc,0x110d9c,0x110e8c,0x110f74,0x111788,0x111cc4,0x14dc44,0x14dccc,0x14dea8,0x14df2c,0x14e020,0x14ee10,0x14ee2c,0x14ee48,0x14ef50,0x14ef80,0x14eb9c,0x14ec04,0x14ec70,0x14ed94,0x14e2d0,0x14e320,0x14e498,0x14e4dc,0x14e6a8,0x14e6ec,0x1b6cd8,0x1b6d3c,0x1b5168,0x1b52c0,0x1b5528,0x1b5630,0x12a6d8,0x12a930,0x12a9e8,0x11201c,0x14f190,0x14f1c4,0x14f3e8,0x14f654,0x14f68c,0x14f72c,0x14f954,0x14fb7c,0x14fbb8,0x14fc24,0x151c08,0x151c18};
    const auto isInverse = [](unsigned kind) { return kind < 4 || kind == 12 || kind == 13 || (kind >= 24 && kind <= 30) || (kind >= 37 && kind <= 45) || (kind >= 50 && kind <= 57) || (kind >= 65 && kind <= 67) || (kind >= 76 && kind <= 96) || (kind >= 111 && kind <= 118) || (kind >= 123 && kind <= 125) || (kind >= 135 && kind <= 138) || (kind >= 155 && kind <= 187) || (kind >= 189 && kind <= 200); };
    const unsigned saved = _mm_getcsr(); unsigned cases = 0, memoryCases = 0; std::array<unsigned,2> aiShotPublications{};
    unsigned weaponJumpCases=0, lightingPublicationCases=0, rolloverPublicationCases=0;
    std::array<unsigned,4> finalPublications{};
    std::array<unsigned,4> driverPublications{};
    std::array<unsigned,4> collisionPublications{}; unsigned collisionErrnoCases=0;
    std::array<unsigned,5> attachmentPublications{}; unsigned gridRemovals=0;
    std::array<unsigned,4> minionPublications{}, minionDestructions{}; unsigned minionBoundaries=0;
    std::array<unsigned,107> weaponPublications{};
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned kind = 0; kind < std::size(callers); ++kind)
        for (unsigned n = 0; n < 128; ++n)
        {
            fill(); const uint32_t alias = n % 3 == 0 ? 0x80000000 : n % 3 == 1 ? 0x20000000 : 0;
            const auto address = [&](uint32_t v) { return v | alias; };
            R5900Context initial; context(initial, alias);
            const auto reg = [&](unsigned r, uint32_t v) { setRegU32(&initial, r, address(v)); };
            const auto returnAt = [&](uint32_t offset) { word(sp + offset, 0xbad0000); word(sp + offset + 4, 0); };
            // JunkJet::UpdateDeath path after its first transform: 0 crash, 1 high without a roll, 2 high with one, 3 low.
            const auto fromEntryPath=[](unsigned site,unsigned scenario,unsigned n) -> unsigned {
                if(n<64 && site!=26) return site==27 ? 2 : site<=29 ? 3 : 0;
                if(scenario==1 || (n>=64 && site>=30)) return 0;
                if(site==26) return scenario==2 ? 1 : scenario==3 ? 2 : 3;
                return site==27 ? 2 : 3;
            };
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
            else if(kind<=217)
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
            else if(kind<=223)
            {
                constexpr uint32_t vehicle=0x18000, other=0x1c000, cs=0x22000, otherCs=0x23000, info=0x24000, point=0x25000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool rotated) {
                    xyz(at,rotated ? 0.f : 1.f,rotated ? 1.f : 0.f,0.f);
                    xyz(at+0x10,rotated ? -1.f : 0.f,rotated ? 0.f : 1.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits()); word(at+28,bits()); word(at+44,bits());
                };
                for(auto v:{vehicle,other}) {
                    basis(v+0xd0,n%4==0); basis(v+0x190,n%4==0); xyz(v+0x1d0,v==vehicle ? 0.f : 10.f,0.f,0.f);
                    xyz(v+0xe0,0.f,1.f,0.f); xyz(v+0x1e0,scenario==2 ? 0.f : 1.f,scenario==2 ? 0.f : 2.f,0.f);
                    xyz(v+0x220,0.f,0.f,0.f); word(v+0x20c8,scenario==4 ? 2 : 1); f(v+0x408,v==vehicle ? 1.f : 2.f);
                    f(v+0x2c4,scenario&1 ? -3.f : 3.f); f(v+0x2c8,1.f); f(v+0x2d4,3.f); f(v+0x394,0.f);
                    f(v+0x3b0,2.f); f(v+0x3b4,2.f); f(v+0x3b8,2.f); word(v+0x1a48,0); word(v+0x1a4c,1);
                    word(v+0x20a8,0); word(v+0x23bc,0); word(v+0x588,0); word(v+0x474,0); word(v+0x580,0);
                    for(unsigned i=0;i<4;++i) { xyz(v+0x600+i*0x30,1.f,1.f,0.f); xyz(v+0x660+i*0x30,-1.f,-1.f,0.f); }
                }
                basis(cs+0x20,n%4==0); basis(otherCs+0x20,false); xyz(cs+0x10,10.f,20.f,30.f); xyz(otherCs+0x10,10.f,21.f,30.f);
                f(cs+0x68,0.f); word(vehicle+0x2018,address(cs)); word(other+0x2018,address(otherCs));
                f(gp-0x47fc,.0001f); f(gp-0x481c,.0001f); f(gp-0x4818,0.f); f(gp-0x4814,.0001f);
                word(gp-0x487c,0x40490fdb); word(gp-0x4878,0x49742400); word(gp-0x4874,0x3e22f983); word(gp-0x4870,0x40c90fdb);
                word(gp-0x486c,0xc0490fdb); word(gp-0x4868,0xc9742400); word(gp-0x4864,0x40490fdb);
                word(gp-0x4860,0x3e22f983); word(gp-0x485c,0x40c90fdb);
                f(gp-0x7560,10.f); f(gp-0x755c,-10.f);
                word(0x4f5da0,1); word(0x4e429c,address(0x17000)); word(0x17000,0); // retail libm mode and actual errno storage
                uint32_t entry=0;
                if(kind<=219) {
                    continuation=resume_0x11ac30; entry=0x11ac30; services.stop=0x11b2e8;
                    reg(18,vehicle); reg(19,other); reg(4,(kind==218 ? vehicle : other)+0x190); reg(5,sp); reg(6,(kind==218 ? vehicle : other)+0x1e0);
                    xyz(sp,0.f,scenario==2 ? 0.f : scenario&1 ? -4.f : 4.f,0.f);
                    const uint32_t chosen=kind==218 ? vehicle : other;
                    word(chosen+0x20a8,0x1a0); word(chosen+0x23bc,1);
                    f(gp-0x7598,4.f); f(gp-0x7594,-4.f); f(gp-0x7590,4.f); f(gp-0x758c,-4.f);
                    f(gp-0x7588,.5f); f(gp-0x7584,.5f); f(gp-0x7580,100.f); f(gp-0x757c,100.f);
                    f(gp-0x7578,-100000.f); f(gp-0x7574,-100000.f); returnAt(0xa0);
                    if(n>=64) {
                        if(scenario==1) word(chosen+0x23bc,0); if(scenario==2) word(chosen+0x20a8,0);
                        if(scenario==3) f(chosen+0x2c4,-3.f); if(scenario==5) word(vehicle+0x1a48,1);
                        if(scenario==6) word(other+0x1a48,1);
                    }
                } else if(kind==220) {
                    continuation=resume_0x11b210; entry=0x11b210; reg(17,vehicle); reg(18,cs+0x20); reg(16,vehicle+0x1e0); reg(19,sp+0x10);
                    reg(4,cs+0x20); reg(5,sp); reg(6,sp+0x10); xyz(sp,0.f,scenario==2 ? 0.f : scenario==3 ? 1e-30f : scenario&1 ? -4.f : 4.f,0.f);
                    fp(20,scenario==1 ? 0.f : scenario==5 ? -1.f : .5f); returnAt(0x60);
                } else if(kind<=222) {
                    continuation=resume_0x11b7d0; entry=0x11ba50; services.stop=0x142d98;
                    reg(17,vehicle); reg(19,other); reg(20,info); reg(23,point); setRegU32(&initial,4,2); setRegU32(&initial,18,2);
                    reg(16,sp+0x10); reg(4,cs+0x20); reg(5,kind==221 ? sp+0x20 : sp+0x50); reg(6,sp+0x10);
                    xyz(sp+0x20,1.f,2.f,3.f); xyz(sp+0x30,-1.f,-2.f,0.f); xyz(sp+0x40,3.f,-1.f,2.f); xyz(sp+0x50,-3.f,1.f,-2.f);
                    for(unsigned i=0;i<4;++i) std::memcpy(ram+0x4e4e60+16*i,ram+sp+0x20+16*i,16);
                    word(info,address(vehicle+0x1d0)); xyz(point,1.f,2.f,3.f); f(other+0x284,2.f); word(other+0x48d,0);
                    word(other+0x20a8,0x1a0); word(other+0x23bc,1); word(vehicle+0x20a8,0x1e0);
                    f(gp-0x7548,.5f); f(gp-0x7544,2.5f); f(gp-0x7540,-1.f); f(gp-0x753c,-2.f); f(gp-0x7538,.5f); f(gp-0x7534,2.5f);
                    word(gp-0x44fc,8); word(gp-0x44f8,4); const double threshold=1.; std::memcpy(ram+0x4e4ea0,&threshold,8);
                    if(kind==222) xyz(otherCs+0x10,11.f,20.f,30.f);
                    if(kind==221 && scenario==4) xyz(otherCs+0x10,10.f,19.f,30.f);
                    if(kind==221 && scenario==5) xyz(otherCs+0x10,9.f,20.f,30.f);
                    if(n>=64) { if(scenario==1) word(other+0x23bc,0); if(scenario==2) word(vehicle+0x20a8,0); }
                    returnAt(0xf0);
                } else {
                    continuation=resume_0x11c318; entry=0x11c318; reg(18,vehicle); reg(17,vehicle+0x190); reg(16,sp+0x10); fp(20,0.f);
                    reg(4,vehicle+0x190); reg(5,sp); reg(6,sp+0x10); xyz(sp,0.f,scenario==2 ? 0.f : scenario&1 ? -3.f : 3.f,0.f); returnAt(0x60);
                }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry;
                    if(kind<=220 || kind==223) { reg(4,vehicle); reg(5,kind==220 ? cs+0x20 : point); reg(6,other); setRegU32(&initial,31,0xbad0000); }
                    if(kind==220) { fp(12,scenario==2 ? 0.f : scenario==3 ? 1e-30f : scenario&1 ? -4.f : 4.f); fp(13,scenario==1 ? 0.f : scenario==5 ? -1.f : .5f); }
                    if(kind==221 || kind==222) setRegU32(&initial,4,scenario==3 ? 0 : 2);
                }
            }
            else if(kind<=228 || kind>=236)
            {
                constexpr uint32_t object=0x18000, actor=0x19000, link=0x1a000, vehicle=0x1c000, cs=0x22000, parent=0x23000, node=0x24000, child=0x25000;
                constexpr uint32_t grid=0x26000, pool=0x27000, lists=0x28000, oldNode=0x29000, scene=0x360000;
                const unsigned scenario=(n%64)/8;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool rotated) {
                    xyz(at,rotated ? 0.f : 1.f,rotated ? 1.f : 0.f,0.f);
                    xyz(at+0x10,rotated ? -1.f : 0.f,rotated ? 0.f : 1.f,0.f); xyz(at+0x20,0.f,0.f,1.f);
                    word(at+12,bits()); word(at+28,bits()); word(at+44,bits());
                };
                for(auto c:{cs,parent}) {
                    word(c,address(node)); word(c+0xc,0x101); basis(c+0x20,n%4==0);
                    xyz(c+0x10,scenario==7 ? 600.f : 10.f,20.f,30.f); xyz(c+0x60,0.f,0.f,0.f);
                    for(unsigned i=0;i<4;++i) word(c+0x70+4*i,0xffffffff);
                }
                word(node,1); f(node+0x1c,1.f); word(gp-0x3620,address(grid));
                word(grid+4,0x00010002); f(grid+8,1000.f); f(grid+0xc,-500.f); f(grid+0x10,-500.f); f(grid+0x14,1500.f); f(grid+0x18,500.f); word(grid+0x1c,address(lists));
                for(unsigned i=0;i<4;++i) { word(lists+4*i,0); word(grid+0x20+4*i,0); } // empty resident collision-cell pointers
                for(unsigned i=0;i<8;++i) { word(pool+16*i,i==7 ? 0 : address(pool+16*(i+1))); word(pool+16*i+4,0); }
                word(gp-0x2eb0,address(pool)); word(gp-0x3608,0); word(gp-0x2ef8,scenario==4 ? 0 : scenario==5 ? 2 : 1);
                word(gp-0x4154,address(scene)); f(scene+0x1b3d78,0.f); word(scene+0x1b3db0,0);
                word(gp-0x33c0,1); f(gp-0x4858,100000.f); f(gp-0x35e8,0.f); // real disabled-terrain / empty height grid
                word(0x4f5da0,1); word(0x4e429c,address(0x17000)); word(0x17000,0);
                word(gp-0x487c,0x40490fdb); word(gp-0x4878,0x49742400); word(gp-0x4874,0x3e22f983); word(gp-0x4870,0x40c90fdb);
                word(gp-0x486c,0xc0490fdb); word(gp-0x4868,0xc9742400); word(gp-0x4864,0x40490fdb); word(gp-0x4860,0x3e22f983); word(gp-0x485c,0x40c90fdb);
                uint32_t entry=0;
                reg(16,object); reg(4,cs+0x20); reg(5,sp); reg(6,sp); xyz(sp,1.f,2.f,3.f);
                if(kind==224) {
                    continuation=resume_0x129e38; entry=0x129e38; returnAt(0x20);
                    basis(object+0x40,n%4==0); xyz(object+0x30,scenario==1 ? 0.f : scenario==6 ? -1.f : 1.f,scenario==1 ? 0.f : 2.f,scenario==1 ? 0.f : 3.f);
                    word(object+0x90,address(cs)); reg(4,object+0x40); reg(5,object+0x30);
                    f(gp-0x44e8,33.f); f(gp-0x70a0,.5f); f(gp-0x709c,10.f);
                    if(scenario==2) ram[cs+0xd]=0;
                } else if(kind==225) {
                    continuation=resume_0x137c58; entry=0x137c58; returnAt(0x30);
                    word(object,address(link)); word(link+8,address(cs)); word(object+4,0); word(object+8,0);
                    word(actor+0xa8,address(parent)); word(actor+0x14,scenario==5 ? 300 : scenario==6 ? 301 : 0);
                    xyz(0x4e56c0,1.f,2.f,3.f); word(0x4e56cc,bits());
                    if(scenario==1) ram[cs+0xc]=0; if(scenario==2) ram[cs+0xd]=0;
                    if(scenario==6) { services.stop=0x155a68; for(unsigned i=0;i<3;++i) word(0x4fc620+4*i,11+i); }
                    reg(17,object); reg(16,actor); reg(4,parent+0x20); reg(5,sp);
                    std::memcpy(ram+sp,ram+0x4e56c0,16);
                } else if(kind>=236) {
                    // Weapon lock-on, look-ahead, skill-satellite, rock and ricochet transforms through their real owners.
                    const unsigned site=kind-236;
                    const auto fp=[&](unsigned r,float v) { std::memcpy(&initial.fu[r],&v,4); };
                    constexpr uint32_t owners[]={0x1b16a8,0x1b1ae0,0x1b3350,0x1b3350,0x1b3350,0x1b3e28,0x1b2ee0,0x1b2ee0,0x150e68,0x151060,0x151358,0x14ff58,0x14ff58,0x14d3a0,0x14d3a0,0x12aa98,0x169d48,0x12a0f0,0x139250,0x143310,0x10eae8,0x135c20,0x135fd8,0x135fd8,0x10ee18,0x10ee18,0x143728,0x143728,0x143728,0x143728,0x143728,0x143728,0x1297e8,0x10cbf0,0x1b77d0,0x1b77d0,0x18b088,0x18b088,0x1af4c0,0x1af4c0,0x1af4c0,0x1af4c0,0x1af4c0,0x14d238,0x1b3778,0x1b3778,0x1b3778,0x1034c4,0x1069f8,0x1e7c00,0x1e74c0,0x10fe80,0x1100e8,0x11018c,0x1103d0,0x1104d0,0x110854,0x1108f8,0x1109f8,0x110ac8,0x110d78,0x110e78,0x110f60,0x111774,0x111cb4,0x14dae0,0x14dae0,0x14de6c,0x14de6c,0x14dae0,0x14edc8,0x14edc8,0x14edc8,0x14dae0,0x14dae0,0x14dae0,0x14dae0,0x14dae0,0x14dae0,0x14e2b8,0x14e308,0x14e3f8,0x14e3f8,0x14e608,0x14e608,0x1b6bf0,0x1b6bf0,0x1b50f0,0x1b52a4,0x1b5518,0x1b5614,0x12a6c4,0x12a918,0x12a9c8,0x111f80,0x14f17c,0x14f0d4,0x14f3d4,0x14f644,0x14f67c,0x14f718,0x14f940,0x14fb68,0x14fba4,0x14fc10,0x151bc8,0x151bc8};
                    constexpr RecompiledFunction resumes[]={resume_0x1b16a8,resume_0x1b1ae0,resume_0x1b3350,resume_0x1b3350,resume_0x1b3350,resume_0x1b3e28,resume_0x1b2ee0,resume_0x1b2ee0,resume_0x150e68,resume_0x151060,resume_0x151358,resume_0x14ff58,resume_0x14ff58,resume_0x14d3a0,resume_0x14d3a0,resume_0x12aa98,resume_0x169d48,resume_0x12a0f0,resume_0x139250,resume_0x143310,resume_0x10eae8,resume_0x135c20,resume_0x135fd8,resume_0x135fd8,resume_0x10ee18,resume_0x10ee18,resume_0x143728,resume_0x143728,resume_0x143728,resume_0x143728,resume_0x143728,resume_0x143728,resume_0x1297e8,resume_0x10cbf0,resume_0x1b77d0,resume_0x1b77d0,resume_0x18adb8,resume_0x18adb8,resume_0x1af4c0,resume_0x1af4c0,resume_0x1af4c0,resume_0x1af4c0,resume_0x1af4c0,resume_0x14d238,resume_0x1b3778,resume_0x1b3778,resume_0x1b3778,resume_0x103308,resume_0x1069f8,resume_0x1e7c00,resume_0x1e74c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x10f7c0,resume_0x111550,resume_0x111c50,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x1b6bf0,resume_0x1b6bf0,resume_0x1b50f0,resume_0x1b50f0,resume_0x1b50f0,resume_0x1b50f0,resume_0x12a580,resume_0x12a580,resume_0x12a580,resume_0x111f80,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x14dae0,resume_0x1517a8,resume_0x1517a8};
                    continuation=resumes[site]; entry=owners[site]; returnAt(site<2 ? 0x70 : site>=105 ? 0x240 : site>=95 ? 0x200 : site>=91 ? 0xb0 : site>=87 ? 0x140 : site>=85 ? 0x90 : site>=65 ? 0x200 : site>=63 ? 0x20 : site>=51 ? 0xa0 : site==49 ? 0xe0 : site==50 ? 0x120 : site==47 ? 0x40 : site==48 ? 0x50 : site==43 ? 0x40 : site>=44 ? 0x60 : site>=38 ? 0x50 : site>=36 ? 0x130 : site>=34 ? 0xa0 : site==33 ? 0x60 : site>=32 ? 0x50 : site>=26 ? 0x40 : site>=24 ? 0x50 : site==21 ? 0x80 : site>=22 ? 0xb0 : site==19 ? 0x40 : site==20 ? 0x30 : site>=17 ? 0x20 : site==15 ? 0x40 : site==16 ? 0x50 : site==8 ? 0x70 : site>=13 ? 0xd0 : site>=11 ? 0x80 : site>=9 ? 0xd0 : site>=6 ? 0xb0 : 0x30);
                    word(object+0x130,address(cs)); word(object+0x110,address(vehicle)); word(object+0x114,scenario==2 && n>=64 ? 0 : address(vehicle)); // no target only gates full entries
                    word(object+4,scenario==1 ? 4 : 9); word(object+0x34,scenario==3 ? 0 : 1); word(object+0x3c,n&1); word(object+0x40,0);
                    f(object+0x54,0.f); word(object+0x14,0); f(object+0x70,2.f); f(object+0x94,-5.f); f(object+0x98,scenario==4 ? .5f : 6.f);
                    xyz(object+0x90,4.f,-5.f,scenario==4 ? .5f : 6.f); xyz(object+0x120,1.f,2.f,3.f); word(object+0x12c,bits());
                    word(object+0x20,site==2 ? 0 : site==3 ? 1+(scenario&1) : 3);
                    basis(vehicle+0xd0,n%4==0); basis(object+0xd0,n%4==1); xyz(vehicle+0x1d0,100.f,200.f,300.f);
                    f(vehicle+0x2c4,2.f); word(vehicle+0x2018,address(child)); xyz(child+0x10,40.f,50.f,60.f); // look-ahead scales the basis column at +0xe0
                    f(vehicle+0x2378,scenario==5 || site==3 || site==4 ? 1000.f : -1000.f); xyz(vehicle+0x2370,5.f,6.f,7.f); // satellites keep zero angles: exact identity rebuild
                    f(gp-0x49cc,-1000.f); // rock impact effect/kill boundary stays on the original path, not reached
                    reg(16,object); reg(17,site==5 ? scene+0x190c20 : object+0x90); fp(20,10.f);
                    if(site<2) { reg(4,vehicle+0xd0); reg(5,object+0x120); reg(6,sp); }
                    else if(site==2) { reg(4,object+0xd0); reg(5,object+0x90); reg(6,sp); }
                    else if(site<6) { reg(4,site==5 ? object+0xd0 : cs+0x20); reg(5,sp); reg(6,sp); xyz(sp,3.f,-4.f,8.f); } // actual in-place transforms
                    else if(site>=105) {
                        // LoopTank::update, from the call-return label after drawInit (151bc8): six segments of the loop's table at
                        // frame +80 (two points each, +80 and +90 for the first) go through the loop's matrix at frame +40 in place,
                        // then the tank's position (coordinate system at +80) is added to each. The segment selected by +14 (here
                        // the first) is then marked (mathfSetFVector: the fixture stops there, after both points were consumed).
                        services.stop=0x1dc018;
                        word(object+0x80,address(cs)); word(object+0x14,0); basis(sp+0x40,n%4==0);
                        for(uint32_t at:{0x80u,0x90u}) { xyz(sp+at,at==0x80 ? 2.f : -3.f,at==0x80 ? -1.5f : 4.f,at==0x80 ? 3.f : .5f); word(sp+at+12,bits()); }
                        reg(18,object); reg(19,n>=64 ? sp+0x40 : 0); reg(20,sp+0x80); reg(16,sp+0x80);
                        reg(17,sp+0x90); reg(21,sp+0x40); reg(22,sp+0x140); reg(23,sp+0x20); // r19: the matrix at the entry, the segment counter at a boundary
                        reg(4,sp+0x40); reg(5,site==105 ? sp+0x80 : sp+0x90); reg(6,site==105 ? sp+0x80 : sp+0x90);
                        setRegU32(&initial,21,n>=64 ? 0x4e0000 : sp+0x40);
                    }
                    else if(site>=95) {
                        // Launcher::UpdateSpecial torches, each from the call-return label before its transform. Tank car (type 180):
                        // 95 a lock-on offset in frame +60 through the turret matrix at frame +80, 96 through the vehicle's system
                        // (distance at least 250), both plus the vehicle position and line-tested from it (hdLosCsTest: stop); 97 a
                        // hit's (0,5,0) in frame +0 through the vehicle's system, handed to DispatchHit (stop). Minion (type 1e0):
                        // 98 the nozzle offset at +3a0 through frame +80 into the launcher's +620, plus the vehicle position (stop
                        // at the following mathfMulMatrix); 99 the offset at +3b0 through frame +40 into frame +0, added into +620
                        // (stop at mathfRotationFromPointToPoint); 100 the flame offset in frame +20 through the torch matrix at
                        // +2b0, plus +620, line-tested from +620; 101 like 97. Tanker (type 220): 102 the table offset 23d1f0
                        // through frame +70 into +620, plus the vehicle position; 103 23d200 through frame +30 into frame +0,
                        // added into +620; 104 like 100.
                        constexpr uint32_t launcher=object;
                        services.stop=site==97 || site==101 ? 0x142d98 : site==98 || site==102 ? 0x1da318 : site==99 || site==103 ? 0x1dbd58 : 0x1d0288;
                        word(vehicle+0x2018,address(cs)); xyz(vehicle+0x1d0,100.f,200.f,300.f); word(vehicle+0x1dc,bits()); word(vehicle+0x20b0,2);
                        word(launcher+0x2f8,address(parent)); word(launcher+0x2f4,address(parent)); word(launcher+0x2a0,address(oldNode));
                        xyz(launcher+0x620,-7.f,8.f,9.f); word(launcher+0x62c,bits()); xyz(launcher+0x3a0,1.5f,-2.f,3.f); xyz(launcher+0x3b0,-2.5f,1.f,4.f);
                        xyz(0x23d1f0,2.f,-3.f,1.5f); xyz(0x23d200,-1.f,4.f,-2.5f); basis(launcher+0x2b0,n%4==0);
                        for(uint32_t at:{0x0u,0x20u,0x60u}) { xyz(sp+at,3.f,-1.5f,2.f); word(sp+at+12,bits()); }
                        reg(18,launcher); reg(19,vehicle); reg(22,sp+0x10); reg(23,vehicle+0x1d0); fp(0,1000.f);
                        uint32_t matrix=cs+0x20, in=sp, out=sp;
                        if(site<=96) { in=out=sp+0x60; reg(20,sp+0x70); reg(21,vehicle+0x1d0); reg(22,launcher+0x6e0); reg(17,out);
                            if(site==95) { matrix=sp+0x80; basis(matrix,n%4==0); reg(16,matrix); } }
                        else if(site==98 || site==102) { matrix=sp+(site==98 ? 0x80u : 0x70u); basis(matrix,n%4==0); in=site==98 ? launcher+0x3a0 : 0x23d1f0; out=launcher+0x620;
                            reg(17,site==98 ? matrix : vehicle+0x1d0); reg(16,matrix); reg(20,out); reg(21,sp+0x10); }
                        else if(site==99 || site==103) { matrix=sp+(site==99 ? 0x40u : 0x30u); basis(matrix,n%4==0); in=site==99 ? launcher+0x3b0 : 0x23d200;
                            reg(16,matrix); reg(17,matrix); reg(20,launcher+0x620); reg(21,sp+0x10); }
                        else if(site==100 || site==104) { matrix=launcher+0x2b0; in=out=sp+0x20; reg(17,matrix); reg(16,out); reg(20,launcher+0x620); reg(21,sp+0x10); }
                        reg(4,matrix); reg(5,in); reg(6,out);
                    }
                    else if(site>=91) {
                        // Ferris wheel and final boss hit tests. 91: the wheel's offset at +30 goes through its coordinate system
                        // (pointer at +90) into frame +0, added into the system's position, raised by 20 and height-tested
                        // (hdHatTest: stop); entry at the call-return label 12a6c4. 92: a random car offset in frame +10 into frame
                        // +0, plus the position, sphere-tested against the wheel (hdPntSphereTest: stop); entry 12a918, after the
                        // second random draw. 93: the seat offset in frame +20 the same way, after the first test's hit dispatch
                        // (entry 12a9c8). 94: FinalBoss::CheckCollision from its start, the first collision point at +710 through
                        // the boss's coordinate system (+e0) into frame +0, plus the position, then hdPntSphereTest (stop).
                        services.stop=site==91 ? 0x1cec90 : 0x1c4e70;
                        word(object+0x90,address(cs)); word(object+0xe0,address(cs)); word(object+0x10,1);
                        xyz(object+0x30,1.5f,-2.f,3.f); word(object+0x3c,bits()); xyz(object+0x710,-2.5f,4.f,1.f); word(object+0x71c,bits());
                        xyz(sp+0x10,3.f,-1.f,0.f); word(sp+0x1c,bits()); xyz(sp+0x20,-4.f,2.f,6.f); word(sp+0x2c,bits());
                        reg(16,object); reg(17,sp+0x30); reg(18,object+0x40); fp(20,20.f); fp(0,5.f);
                        reg(4,cs+0x20); reg(5,site==91 ? object+0x30 : site==92 ? sp+0x10 : site==93 ? sp+0x20 : object+0x710); reg(6,sp);
                    }
                    else if(site>=85) {
                        // Weapon hit tests. RockHD (85, 86): with +1c set and a nonzero fall speed at +98 (85), or with +1c clear (86), a
                        // fixed offset goes through the weapon's basis at +d0 into frame +0, plus the position of its coordinate
                        // system, and becomes hdPntSphereTest's point (the fixture stops there). RicoHD: 87 from the owner's start
                        // (frame +10, then hdPntSphereTest); 88 after the reflection branch's basis rebuild (entry 1b52a4), the
                        // direction at +90 into frame +10, added into the coordinate system's position, which goes to the
                        // ricochet bounce sound (stop); 89 after the no-hit offset (entry 1b5518, then hdPntSphereTest); 90 after
                        // the second branch's rebuild (entry 1b5614), the position becomes +80 plus frame +10, then the owner returns.
                        const bool rico=site>=87;
                        word(object+0x1c,site==86 ? 0 : 1);
                        xyz(object+0x80,-6.f,7.f,-8.f); word(object+0x8c,bits()); word(object+0x44,0); xyz(cs+0x10,10.f,20.f,30.f); word(cs+0x1c,bits());
                        services.stop=site==88 ? 0x170b70 : site==90 ? 0xffffffff : 0x1c4e70;
                        const uint32_t out=rico ? sp+0x10 : sp;
                        reg(16,object); reg(17,rico ? object : sp+0x10); reg(18,rico ? out : object+0x80); reg(19,sp+0x20);
                        reg(20,object+0x80); reg(21,object+0xd0); reg(22,scene+0x190c20); reg(23,out);
                        reg(4,object+0xd0); reg(5,site==88 || site==90 ? object+0x90 : out); reg(6,out);
                        xyz(out,2.5f,-3.f,4.f); word(out+12,bits());
                    }
                    else if(site>=78) {
                        // Launcher::UpdateSpecial. Site 78: type 100 with a shot held (+284) turns its turret (angle +290 stepped,
                        // bounded, node matrix rebuilt), finds no animation running, smooths its aim point at +350 toward
                        // (0, gp-629c, 0), sends it through the vehicle's coordinate system and adds the vehicle system's position
                        // into the aimed object at +34c (from the owner's start; it then returns). Sites 79..84: type a0's
                        // lock-on arc (entries start at the call-return label before each): a side offset in frame +0 through the
                        // vehicle's system into frame +20, plus the vehicle position, or a heading-selected table slot (frame +40,
                        // +50, +60, +70) through the target's coordinate system into frame +30, plus the target's position. The
                        // arc ends meet in particleElectricArc (the fixture stops there).
                        constexpr uint32_t launcher=object, node=oldNode, aimed=0x2b000, targetCar=0x2a000;
                        const unsigned shot=site-78;
                        word(vehicle+0x20a8,shot==0 ? 0x100 : 0xa0); word(vehicle+0x20c8,3); word(launcher+0x280,10); word(launcher+0x284,1);
                        f(launcher+0x290,.5f); word(launcher+0x2a0,address(node)); word(launcher+0x34c,address(aimed)); word(launcher+0x9ac,0);
                        word(vehicle+0x2018,address(cs)); xyz(vehicle+0x1d0,100.f,200.f,300.f); xyz(launcher+0x350,.5f,-1.f,2.f);
                        f(gp-0x62a4,.125f); f(gp-0x429c,10.f); f(gp-0x62a0,1.f); f(gp-0x629c,4.f);
                        word(targetCar+0x2018,address(parent)); f(parent+0x68,0.f); word(launcher+0x2f8,address(aimed));
                        for(unsigned i=0;i<4;++i) { xyz(sp+0x40+16*i,1.5f-i,-2.f+.5f*i,3.f); word(sp+0x4c+16*i,bits()); }
                        // Heading windows: |h|<.5 takes slot +40 (site 81/83); .25<h<3 past the side window takes +70 (82/84).
                        for(uint32_t base:{0x6318u,0x62fcu}) { f(gp-base,.5f); f(gp-base+4,2.f); f(gp-base+8,0.f); f(gp-base+12,-5.f); f(gp-base+16,.25f); f(gp-base+20,3.f); }
                        services.stop=shot==0 ? 0xffffffff : 0x1e74c0;
                        reg(18,launcher); reg(19,vehicle); reg(6,sp);
                        if(shot==0) { reg(4,cs+0x20); reg(5,launcher+0x350); }
                        else if(shot<=2) { reg(17,sp+0x20); reg(4,cs+0x20); reg(5,sp); reg(6,sp+0x20); xyz(sp,2.f,-3.f,0.f); word(launcher+0x2f4,0); }
                        else {
                            const bool second=shot==4 || shot==6, block=shot>=5;
                            word(launcher+0x2f4,address(targetCar)); fp(0,second ? 1.f : 0.f);
                            reg(16,block ? vehicle+0x1d0 : sp+0x30); reg(17,block ? sp+0x30 : sp+0x20);
                            reg(4,parent+0x20); reg(5,sp+(second ? 0x70u : 0x40u)); reg(6,sp+0x30);
                        }
                    }
                    else if(site>=75) {
                        // Launcher::UpdateSpecial, vehicle type 100 (no shot held at +284). While its turret angle at +290, stepped
                        // down by gp-62b0, stays above gp-42a0, the turret node's matrix (node at +2a0, matrix at +10) is rebuilt from
                        // it and the table offset at 23d070 goes through it into frame +0, plus 23d060, into frame +20; the global
                        // sweep angle at gp-3090 then builds a matrix at frame +30 that sends 23d0a0 into frame +0, added to frame
                        // +20. The launcher's aim point at +350 is smoothed toward frame +20, goes through the vehicle's coordinate
                        // system, and plus the vehicle position becomes the position of the object at +34c; then it returns. When
                        // the angle has run below the limit it fires from the table instead (CreateStaticWeapon: stop).
                        constexpr uint32_t launcher=object, node=oldNode, aimed=0x2b000;
                        const unsigned shot=site-75;
                        const bool fire=n>=64 && scenario==1;
                        word(vehicle+0x20a8,0x100); word(vehicle+0x20c8,3); word(launcher+0x280,10); word(launcher+0x284,0);
                        f(launcher+0x290,fire ? -20.f : 1.f); word(launcher+0x2a0,address(node)); word(launcher+0x34c,address(aimed));
                        word(launcher+0x2f4,0); word(vehicle+0x2018,address(cs)); xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        xyz(launcher+0x350,.5f,-1.f,2.f); f(gp-0x62b0,.25f); f(gp-0x42a0,-10.f); f(gp-0x62ac,1.f); f(gp-0x62a8,.125f); f(gp-0x3090,.5f);
                        for(unsigned i=0;i<5;++i) { xyz(0x23d060+16*i,1.5f-i,-2.f+.5f*i,3.f-.25f*i); word(0x23d06c+16*i,bits()); }
                        if(fire) services.stop=0x1b7590;
                        reg(18,launcher); reg(19,vehicle); fp(20,.5f); fp(21,1.f); reg(6,sp);
                        if(shot==0) { basis(node+0x10,n%4==0); reg(4,node+0x10); reg(5,0x23d070); }
                        else if(shot==1) { basis(sp+0x30,n%4==0); reg(16,sp+0x30); reg(17,sp+0x20); xyz(sp+0x20,-3.f,4.f,1.5f); reg(4,sp+0x30); reg(5,0x23d0a0); }
                        else { reg(4,cs+0x20); reg(5,launcher+0x350); }
                    }
                    else if(site>=70) {
                        // Launcher::UpdateSpecial, vehicle types 1c0 and 140. Type 1c0 (no countdown left at +280) fires a volley:
                        // each shot sends a table offset (23d150 with +4 set, 23d0b0 without) through the vehicle's coordinate
                        // system into frame +0, then through the vehicle part's basis (the part at +1ec8, its basis at +10) in
                        // place, adds the vehicle position into the launcher's +620 and calls Weapons::CreateWeapon (the fixture
                        // stops at the first shot). Type 140 builds its launch point in +620 itself: with +4 set from the constant
                        // offset at gp-6298, otherwise from the launcher's +400, through the vehicle's system, plus the vehicle
                        // position; then Weapons::CreateWeapon (stop). A launcher still counting down returns.
                        constexpr uint32_t launcher=object, part=child;
                        const unsigned shot=site-70;
                        const bool idle=n>=64 && scenario==1 && shot>=3; // 1c0 entries start past the countdown check
                        const bool flag=shot==0 || shot==3 || (shot==2 && (n&1));
                        word(vehicle+0x20a8,shot<3 ? 0x1c0 : 0x140); word(launcher+0x4,flag ? 1 : 0); word(launcher+0x9c,0); word(vehicle+0x20c8,3); // countdown step
                        word(launcher+0x280,idle ? 25 : 0); word(vehicle+0x2018,address(cs)); word(vehicle+0x1ec8,address(part)); basis(part+0x10,n%4==1);
                        xyz(vehicle+0x1d0,100.f,200.f,300.f); xyz(launcher+0x400,-1.5f,2.5f,1.f); word(launcher+0x40c,bits());
                        xyz(0x23d150,1.5f,-2.f,3.f); word(0x23d15c,bits()); xyz(0x23d0b0,-1.f,2.f,.5f); word(0x23d0bc,bits());
                        f(gp-0x6298,2.f); f(gp-0x6294,-3.f); f(gp-0x6290,4.5f);
                        if(!idle) services.stop=0x1b72f0;
                        reg(18,launcher); reg(19,vehicle); reg(17,launcher+0x620); reg(21,vehicle+0x1d0); setRegU32(&initial,16,0);
                        setRegU32(&initial,20,n>=64 && shot<3 ? 0x190000 : 0x190c20); reg(22,0x23d150); // entries OR in 0xc20 themselves reg(23,0x23d0b0); reg(30,launcher+0x910);
                        if(shot<2) { reg(4,cs+0x20); reg(5,shot==0 ? 0x23d150 : 0x23d0b0); reg(6,sp); }
                        else if(shot==2) { reg(4,part+0x10); reg(5,sp); reg(6,sp); xyz(sp,2.f,-3.f,4.5f); }
                        else {
                            reg(16,launcher+0x620); reg(4,cs+0x20); reg(6,launcher+0x620);
                            if(shot==3) { reg(5,launcher+0x620); xyz(launcher+0x620,2.f,-3.f,4.5f); } else reg(5,launcher+0x400);
                        }
                    }
                    else if(site>=65) {
                        // Launcher::UpdateSpecial, dispatched on the vehicle type at +20a8. Type 60: when the fire countdown at +98
                        // runs out, the table offset at 23cfa0 goes through the vehicle's coordinate system and is added to its
                        // position into the launcher's +620; the turret angles (+290 plus the vehicle's) build a matrix at frame +40
                        // and the offset at 23cfb0 goes through it, added to +620 again; then Weapons::CreateWeapon (stop). Type 120
                        // (entries start after its UpdateHearse) does the same with 23cfc0/23cfd0 and a matrix at frame +30, ending in
                        // CreateStaticWeapon from +620 (stop). Type c0 sends the launcher's +400 offset through the vehicle's system
                        // into +620, finds no lock-on and calls TheGame::CreateWeapon (stop). Without a fire countdown it returns.
                        constexpr uint32_t launcher=object;
                        const unsigned shot=site-65;
                        const bool noFire=n>=64 && scenario==1;
                        word(vehicle+0x20a8,shot<2 ? 0x60 : shot<4 ? 0x120 : 0xc0); word(vehicle+0x20c8,3); word(launcher+0x98,noFire ? 100 : 1);
                        word(launcher+0x280,10); word(launcher+0x284,0); word(launcher+0x2f4,0); word(vehicle+0x2018,address(cs));
                        xyz(launcher+0x290,.125f,-.25f,.375f); xyz(cs+0x60,.0625f,.125f,-.125f); xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        for(unsigned i=0;i<4;++i) { xyz(0x23cfa0+16*i,1.5f-i,-2.f+.5f*i,3.f); word(0x23cfac+16*i,bits()); }
                        xyz(launcher+0x400,-1.5f,2.5f,1.f); word(launcher+0x40c,bits());
                        word(scene+0x1b3db4,0); word(scene+0x1b3d70,0); word(launcher+0x6c4,0); // GetLockOn: nothing to lock
                        if(noFire) { if(shot>=2 && shot<4) services.stop=0x17c2d0; } // type 120 then updates its gun sound
                        else services.stop=shot<2 ? 0x1b72f0 : shot<4 ? 0x1b7590 : 0x1515b8;
                        reg(18,launcher); reg(19,vehicle); reg(17,launcher+0x620); reg(21,launcher+0x620); reg(6,sp);
                        if(shot==0 || shot==2 || shot==4) { reg(4,cs+0x20); reg(5,shot==4 ? launcher+0x400 : 0x23cfa0+0x20*(shot/2)); }
                        else { const uint32_t matrix=sp+(shot==1 ? 0x40u : 0x30u); basis(matrix,n%4==0); reg(16,matrix); reg(20,sp+(shot==1 ? 0x30u : 0x20u)); reg(4,matrix); reg(5,shot==1 ? 0x23cfb0 : 0x23cfd0); }
                    }
                    else if(site>=63) {
                        // FinalBoss movement. UpdateTrans (entries start after the forward-speed smoothing, whose result becomes
                        // the velocity's Y at +58) and UpdateDeath (entries start after the death roll rebuilds the coordinate-
                        // system matrix): the velocity at +50 goes through the boss's own basis at +a0 into frame +0 and is added
                        // to its coordinate-system position. UpdateTrans then reparents the boss in the grid; UpdateDeath probes
                        // the ground under the new position with hdHatTest (which only writes the frame vector). Both stop there.
                        constexpr uint32_t boss=object;
                        word(boss+0xe0,address(cs)); basis(boss+0xa0,n%4==0); xyz(boss+0x50,1.5f,-2.f,.5f);
                        services.stop=site==63 ? 0x1c89e8 : 0x1cec90;
                        reg(16,boss); reg(4,boss+0xa0); reg(5,boss+0x50); reg(6,sp);
                        if(site==63 && n>=64) fp(0,n&1 ? 3.f : -3.f); // the smoothed forward speed
                    }
                    else if(site>=51) {
                        // FinalBoss::UpdateBoss gun shots. Every attack state ends a shot the same way: a muzzle offset (the game's
                        // table at 235070/235090, or one just built with mathfSetFVector) goes through the boss's coordinate
                        // system into frame +10, plus the boss position; some aim first (mathfRotationFromPointToPoint toward
                        // its target), then the shot is created from that point. Entries start at the label right before each
                        // shot (after the offset's mathfSetFVector, after a 1-in-N roll entered with result 0, or after the
                        // burst-interval check entered with remainder 0); the fixture stops at CreateStaticWeapon.
                        constexpr uint32_t boss=object;
                        const unsigned shot=site-51;
                        constexpr uint32_t table[12]={0,0x235070,0x235090,0,0,0x235070,0x235090,0,0,0x235090,0,0};
                        word(boss+0xe0,address(cs)); word(boss+0xe8,address(vehicle)); xyz(vehicle+0x1d0,250.f,350.f,450.f);
                        word(gp-0x4538,1); // shots are created (rather than only sounding)
                        xyz(0x235070,1.5f,-2.f,3.f); word(0x23507c,bits()); xyz(0x235090,-1.5f,-2.f,3.f); word(0x23509c,bits());
                        xyz(sp+0x10,2.f,-3.f,4.5f); word(sp+0x1c,bits());
                        services.stop=0x1b7590;
                        reg(16,sp+0x10); reg(17,boss); reg(4,cs+0x20); reg(5,table[shot] ? table[shot] : sp+0x10); reg(6,sp+0x10);
                        if(n>=64) { setRegU32(&initial,2,shot==1 || shot==5 ? 1 : 0); setRegU32(&initial,4,0); } // remainder 0 / roll 0
                    }
                    else if(site>=49) {
                        // Particle arcs. Both take a free particle from the game's list (index 0 here, at 687f00) and build a basis
                        // from a Z direction. FlatArcs throws ten points: a random flat offset (scaled 20, Z 0) through the basis,
                        // plus the start point, then a fresh random W; ElectricArc walks segments from A toward B: a random
                        // offset on a ring through the basis, scaled, plus A. Both hand the particle to 1e22f0 (stop there).
                        constexpr uint32_t particle=0x687f00, start=object+0x500, finish=object+0x510;
                        const bool flat=site==49;
                        word(gp-0x2f34,n>=64 && scenario==1 ? 0xffffffff : 0); word(gp-0x2f38,0xffffffff); word(0x742f88,0xffffffff);
                        xyz(start,10.f,20.f,30.f); xyz(finish,10.f,20.f,40.f); f(gp-0x4760,.5f); f(gp-0x476c,.25f);
                        if(!(n>=64 && scenario==1)) services.stop=0x1e22f0; // no free particle: returns -1
                        if(flat) {
                            reg(19,start); reg(17,particle); reg(16,0x687f20); reg(18,0x687f10); setRegU32(&initial,20,0xffffffff);
                            reg(21,sp+0x10); setRegU32(&initial,22,0x3f800000); reg(23,sp+0x50); fp(20,20.f);
                            basis(sp+0x10,n%4==0); reg(4,sp+0x10); reg(5,sp); reg(6,0x687f20); xyz(sp,6.f,-8.f,0.f);
                            if(n>=64) xyz(finish,0.f,0.f,1.f); // entry arguments (start, Z direction) are set with the entry
                        } else {
                            word(sp+0x70,n>=64 && scenario==2 ? 0 : 1); word(sp+0x78,address(sp+0x20)); word(sp+0x7c,address(sp+0x10)); word(sp+0x80,address(sp+0x60));
                            reg(22,start); reg(16,finish); reg(17,0x687f10); reg(19,particle); setRegU32(&initial,23,0); setRegU32(&initial,20,0x1234);
                            fp(25,.5f); fp(24,.75f); fp(26,1.f); setRegU32(&initial,30,0x3f800000);
                            basis(sp+0x20,n%4==0); xyz(sp+0x10,.6f,.8f,1.f); reg(4,sp+0x20); reg(5,sp+0x10); reg(6,sp);
                            if(n>=64) {
                                reg(6,sp+0x200); reg(7,sp+0x210); setRegU32(&initial,8,n>=64 && scenario==2 ? 0 : 1); setRegU32(&initial,9,0x1234);
                                fp(12,.5f); fp(13,1.f); fp(14,1.f); fp(15,1.f);
                            }
                        }
                    }
                    else if(site>=47) {
                        // AI driving. Hunt (aiPerformBeadOnHuntCs, entries start at 1034c4 once its side offset (x,0,0) is built):
                        // close in on a big target or with a bead, the offset is used as is, otherwise clamped to +-20; it goes
                        // through the vehicle's basis into frame +10, is added to the driver's bead point and made relative to the
                        // vehicle for atan2f; with no target the driver then drives to attack range (the fixture stops there).
                        // Bait (aiUpdateBaitModeOld, from its start): with a target that has been away long enough and no point
                        // picked yet, (0,130,0) goes through the target's basis into frame +10, plus the target's position, and the
                        // closest attractor point (two here, one near) becomes the driver's bead; then it returns.
                        constexpr uint32_t info=actor, driver=link, target=0x2a000, attract=0x2d000, points=0x2e000;
                        word(vehicle+0x2004,address(info)); word(vehicle+0x2008,address(driver)); word(vehicle+0x20c8,3);
                        xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        if(site==47) {
                            word(driver+0xb4,0); word(driver+0x164,address(points)); xyz(points,40.f,50.f,60.f);
                            word(info+0x5b8,scenario==1 || scenario==2 ? 0 : 1); f(info+0x20,100.f);
                            xyz(sp,scenario==2 ? -35.f : 35.f,0.f,0.f); fp(20,0.f);
                            services.stop=0x102c68; services.stop2=0x102e28;
                            reg(16,sp+0x10); reg(17,vehicle); reg(4,vehicle+0xd0); reg(5,sp); reg(6,sp+0x10);
                        } else {
                            word(driver+0xb4,address(target)); word(driver+0xb8,address(attract)); word(driver+0x258,0);
                            word(driver+0x20c,n>=64 && scenario==1 ? 0 : 0xffffffff);
                            word(target+0x20d0,700); basis(target+0xd0,n%4==0); xyz(target+0x1d0,10.f,20.f,30.f);
                            word(info+0x464,0); word(info+0x5e4,1); f(info+0x34,0.f); f(gp-0x7c4c,1.f); word(info+0x434,1);
                            word(attract,address(points)); word(attract+0x248,2); f(gp-0x7c0c,1e12f);
                            xyz(points,10.f,150.f,30.f); ram[points+0x1d]=0; xyz(points+0x20,9000.f,9000.f,9000.f); ram[points+0x3d]=0;
                            reg(16,sp+0x10); reg(17,vehicle); setRegU32(&initial,18,0); reg(4,target+0xd0); reg(5,sp); reg(6,sp+0x10); xyz(sp,0.f,130.f,0.f);
                        }
                    }
                    else if(site>=44) {
                        // Weapon::UpdateHalTrans. State 0 (flying): the weapon's velocity at +90, scaled by the frames since the
                        // last update, goes through its own basis at +d0 and is added to its position; then it returns. State 1
                        // (attached to its target vehicle, +475 clear): when the vehicle is rolling (|cs+64| above the limit) the
                        // vehicle's offset at +2460 goes through the vehicle's basis, otherwise a copy of +2450 does; the result
                        // plus the vehicle's position is where the weapon is smoothed toward (Z directly), then its angles follow
                        // and its matrix is rebuilt. State 2 just drifts; no target returns early.
                        constexpr uint32_t weapon=object, carCs=parent;
                        word(weapon+0x20,n>=64 && scenario==1 ? 2 : site==44 ? 0 : 1); word(weapon+0xc,0);
                        xyz(weapon+0x90,.25f,-.5f,1.f);
                        word(vehicle+0x2018,address(carCs)); ram[vehicle+0x475]=0; word(vehicle+0x20a8,0);
                        xyz(carCs+0x60,.125f,0.f,.25f); f(carCs+0x64,site==45 ? 1.f : 0.f); // roll at +64 picks the branch
                        xyz(vehicle+0x2460,1.5f,-2.f,3.f); word(vehicle+0x246c,bits()); xyz(vehicle+0x2450,-2.f,2.5f,1.f); word(vehicle+0x245c,bits());
                        xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        for(uint32_t at=0x4a48u;at>=0x49fcu;at-=4) f(gp-at,.5f); f(gp-0x4a38,.25f); // smoothing and roll limit
                        reg(16,weapon); reg(19,scene+0x190c20); fp(20,.5f); fp(21,1.f); fp(22,1.f); fp(23,.5f); fp(24,.25f);
                        if(site==44) { reg(4,object+0xd0); reg(5,sp); reg(6,sp); xyz(sp,2.f,-3.f,4.5f); }
                        else if(site==45) { reg(4,carCs+0x20); reg(5,vehicle+0x2460); reg(6,sp); }
                        else { reg(17,sp+0x10); reg(4,carCs+0x20); reg(5,sp+0x10); reg(6,sp); std::memcpy(ram+sp+0x10,ram+vehicle+0x2450,16); }
                    }
                    else if(site==43) {
                        // Launcher::UpdateZoomies: every period-th update (+25c counter, period at gp-42f8) the launcher's muzzle
                        // offset at +4c0 goes through the vehicle's coordinate system and is added to its position into +620, then
                        // GetLockOn (nothing to lock) and CreateWeapon: the fixture stops there. Other updates check the burst
                        // length and return.
                        constexpr uint32_t launcher=object;
                        word(gp-0x42f8,10); word(gp-0x42fc,100); word(launcher+0x25c,n>=64 && scenario==1 ? 10 : 9);
                        word(vehicle+0x2018,address(cs)); xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        xyz(launcher+0x4c0,1.5f,-2.f,3.f); word(launcher+0x4cc,bits());
                        word(scene+0x1b3db4,0); word(scene+0x1b3d70,0); word(launcher+0x6c4,0); word(sp+0x10,0); word(sp+0x14,0);
                        if(!(n>=64 && scenario==1)) services.stop=0x1515b8;
                        reg(16,launcher); reg(17,vehicle); reg(4,cs+0x20); reg(5,launcher+0x4c0); reg(6,sp);
                    }
                    else if(site>=38) {
                        // Vehicle::vehicleCheckForCombos: the player's entered combo (first word of the controller block at +2000)
                        // launches a special. Combos 1 and 0x10304 first look for a lock-on (empty vehicle lists here), then send
                        // offsets (0,8,2) / (0,12,5) through the vehicle's coordinate system; combos 4 and 5 send the stored
                        // offset at +2620 through the basis at +d0 (combo 4 picks its branch by the controller's +20 count). Each
                        // result is added to the vehicle's position into +2760 and the weapon is created: the fixture stops at
                        // CreateWeapon. No combo just accumulates the idle counter and returns.
                        constexpr uint32_t controller=link;
                        const unsigned combo=n>=64 && scenario==1 ? 0 : site==38 ? 1 : site==39 ? 0x10304 : site<=40 || site==41 ? 4 : 5;
                        word(vehicle+0x580,1); word(vehicle+0x2000,address(controller)); word(controller,combo); word(controller+0x20,site==40 ? 6 : 3);
                        word(vehicle+0x2018,address(cs)); word(vehicle+0x2214,100); word(vehicle+0x2270,0); word(vehicle+0x2274,0);
                        xyz(vehicle+0x2620,1.5f,-2.f,3.f); word(vehicle+0x262c,bits()); xyz(vehicle+0x1d0,100.f,200.f,300.f);
                        for(uint32_t at:{0x39acu,0x3998u,0x39a8u}) word(gp-at,10); word(gp-0x3884,5); // combo costs; ammo threshold
                        word(scene+0x1b3db4,0); word(scene+0x1b3d70,0); word(vehicle+0x2140+0x6c4,0); // GetLockOn: nothing to lock
                        word(sp+0x20,0); word(sp+0x24,0);
                        if(combo) services.stop=0x1515b8;
                        reg(17,vehicle); reg(6,sp);
                        if(site<=39) { reg(4,cs+0x20); reg(5,sp); xyz(sp,0.f,site==38 ? 8.f : 12.f,site==38 ? 2.f : 5.f); }
                        else { reg(4,vehicle+0xd0); reg(5,vehicle+0x2620); }
                    }
                    else if(site>=36) {
                        // Vehicle::tireUpdateCarRotation, from its tire-pair corrections onward (entries start at 18b088, after
                        // the up-vector normalize, with the frame and saved registers as its prologue leaves them). For 4/5-wheel
                        // layouts each axle's height correction (0,0,-c) goes through the vehicle basis at +d0 into frame +50 and
                        // is added to that axle's tire midpoint into frame +70/+80; the owner then rebuilds the basis from the
                        // corrected points (averages, normalize, cross products, RPH tilt, matrix multiply) and returns.
                        const bool first=site==36;
                        const unsigned layout=n>=64 && scenario==1 ? 2 : n>=64 && scenario==2 ? 6 : 4+(n&1);
                        const bool skipOther=n>=64 && scenario==3;
                        word(vehicle+0x1790,layout); word(vehicle+0x580,0); word(vehicle+0x2018,address(cs));
                        f(vehicle+0x1e58,skipOther && !first ? 0.f : 2.5f); f(vehicle+0x1e68,skipOther && first ? 0.f : -1.5f);
                        xyz(vehicle+0x1ac0,40.f,60.f,5.f); xyz(vehicle+0x1bb0,-40.f,60.f,4.f); xyz(vehicle+0x1ca0,40.f,-60.f,6.f); xyz(vehicle+0x1d90,-40.f,-60.f,5.5f);
                        xyz(sp+0x60,.5f,0.f,1.f); word(sp+0x90,0); word(sp+0x94,address(vehicle+0x1ca0)); word(sp+0x98,address(vehicle+0x1d90)); word(sp+0x9c,address(sp+0x60));
                        for(uint32_t at=0x5a0cu;at>=0x59f8u;at-=4) f(gp-at,.25f); // per-layout tilt angles
                        reg(18,vehicle); reg(19,vehicle+0xd0); reg(20,vehicle+0xe0); reg(21,vehicle+0x1ac0); reg(22,vehicle+0x1bb0); reg(23,vehicle+0xf0); reg(30,vehicle+0xd0);
                        reg(17,sp+0x50); reg(4,vehicle+0xd0); reg(5,sp+0x40); reg(6,sp+0x50); xyz(sp+0x40,0.f,0.f,first ? -2.5f : 1.5f);
                    }
                    else if(site>=34) {
                        // Weapon::InitWeapon for weapon type 0 (the machine gun): the vehicle's alternating muzzle offsets at
                        // +2510/+2520 (toggle at +2230) go through the new weapon's coordinate-system basis and are added to its
                        // position, then the weapon is linked into the hierarchy (the fixture stops at hierSetCsEpNode).
                        // Scenarios cover the locked and unlocked aim paths and the ammo/firing-state branch.
                        constexpr uint32_t weapon=object;
                        const bool left=site==34;
                        word(0x4f1bd0,0x1b7834); // the game's weapon-type jump table: type 0
                        word(weapon+0x14,n>=64 && scenario==1 ? 0x23 : 0); word(weapon+0x18,0);
                        word(vehicle+0x2230,left ? 1 : 0); word(vehicle+0x2234,scenario==3 ? 0 : 1); if(scenario==3) word(weapon+0x114,0);
                        word(vehicle+0x22c4,scenario==2 ? 3 : 0); ram[vehicle+0x48c]=n&1;
                        xyz(vehicle+0x2510,2.f,-3.f,4.5f); word(vehicle+0x251c,bits()); xyz(vehicle+0x2520,-2.f,-3.f,4.5f); word(vehicle+0x252c,bits());
                        if(!(n>=64 && scenario==1)) services.stop=0x1d4ed8;
                        reg(17,weapon); reg(19,scene+0x190c20); reg(4,cs+0x20); reg(5,vehicle+(left ? 0x2510u : 0x2520u)); reg(6,sp);
                    }
                    else if(site==33) {
                        // AttractPed::updateTrans: a walking pedestrian's velocity at +40, scaled by 60/update-rate, goes through its
                        // coordinate-system basis and is added to its position; hdHatTest then settles its height (writing only
                        // into the frame vector), and the walk animation speed follows (the fixture stops at getDbId). A
                        // pedestrian whose coordinate system is switched off just drifts by +50 instead (stops at its grid reparent).
                        constexpr uint32_t ped=object;
                        word(ped+4,address(actor)); word(actor+4,address(cs)); ram[cs+0xe]=n>=64 && scenario==1 ? 0 : 1;
                        word(gp-0x2ef4,2); // 60/2: thirty frames of velocity per update
                        xyz(ped+0x40,.1f,.05f,0.f); xyz(ped+0x50,.25f,.5f,0.f); f(ped+0x44,.05f); f(ped+0x48,0.f);
                        word(ped+0x60,scenario==2 ? 1 : 0); word(ped+0x28,0); // walk-animation slot index read just before the stop
                        for(uint32_t at:{0x7ac8u,0x7ac4u,0x7ac0u}) f(gp-at,.5f);
                        services.stop=n>=64 && scenario==1 ? 0x1c89e8 : 0x10e0b8;
                        reg(16,sp+0x10); reg(17,cs+0x10); reg(18,ped); reg(19,cs); fp(20,5.f); fp(21,30.f);
                        reg(4,cs+0x20); reg(5,sp); reg(6,sp+0x10); xyz(sp,3.f,1.5f,0.f);
                    }
                    else if(site>=32) {
                        // EnvHeli::UpdateEnvHeli in attack mode: GetClosestCar picks the one live car in the game's vehicle array,
                        // the helicopter turns toward it (mathfRotationFromPointToPoint, UpdateRot rebuilds its matrix), and every
                        // 30th update fires: the muzzle offset from the game's table at 4e50d0 goes through the basis, plus the
                        // position, to CreateStaticWeapon (the fixture stops there). Other updates move on (UpdateTrans, which has
                        // its own native transform: stops there); a non-attacking mode goes straight to its sound (stops there).
                        constexpr uint32_t heli=object, car=scene, muzzle=0x4e50d0;
                        word(heli+0x90,address(cs)); word(heli+0x28c,address(vehicle)); word(heli+8,n>=64 && scenario==1 ? 2 : 1);
                        word(heli+0x18,n>=64 && scenario==2 ? 30 : 29); // incremented, then fires when divisible by 30
                        for(uint32_t at:{0x34u,0x84u,0x88u,0x8cu}) f(heli+at,.5f);
                        xyz(heli+0xa0,40.f,50.f,60.f); xyz(cs+0x60,.25f,0.f,.5f);
                        word(vehicle+0x2140,0); word(scene+0x1b3db4,1); word(car+0x580,1); xyz(car+0x1d0,250.f,350.f,450.f); f(gp-0x635c,1e6f);
                        for(uint32_t at=0x7100u;at>=0x70a0u;at-=4) f(gp-at,.5f); // bounded aim/turn smoothing
                        f(gp-0x7100,-10.f); f(gp-0x70e0,1e6f); // pitch floor; aim tolerance always met
                        xyz(muzzle,2.f,-3.f,4.5f); word(muzzle+12,bits());
                        services.stop=n>=64 && scenario==1 ? 0x178848 : n>=64 && scenario==2 ? 0x129e38 : 0x1b7590;
                        reg(16,sp+0x10); reg(17,heli); reg(4,cs+0x20); reg(5,sp+0x10); reg(6,sp+0x10);
                        std::memcpy(ram+sp+0x10,ram+muzzle,16);
                    }
                    else if(site>=26) {
                        // JunkJet::UpdateDeath: the falling jet steers toward its crash target, rebuilds its matrix and moves by
                        // the velocity at +20 through it. High up, a 1-in-7 roll throws one explosion point (+-80, random
                        // Y/Z); lower, two fixed points (0,300,-20) and (0,150,0) burn. On the ground it crashes: cars are
                        // damaged (the fixture stops at BlowCarsInJetCrash), then (0,200,-20) and (0,100,0) burn. Every point
                        // is the transform plus the position, handed to particleCreateFx; the particle system is off here
                        // (gp-2f40 = 0), so each call returns at once and the owner runs on to its real return.
                        constexpr uint32_t jet=object;
                        const unsigned path=fromEntryPath(site,scenario,n);
                        word(jet+0x70,address(cs)); xyz(jet+0x20,0.f,3.f,-1.f); f(jet+0x10,0.f); xyz(jet+0xe0,500.f,400.f,30.f); xyz(cs+0x60,.25f,0.f,.5f);
                        f(gp-0x6c00,1.f); f(gp-0x6bfc,.5f); f(gp-0x6bf8,1.f); f(gp-0x6bf4,1.f); f(gp-0x6bf0,.0625f);
                        word(gp-0x2f40,0); // no particle system: particleCreateFx returns -1 without reading its point
                        f(cs+0x18,path==0 ? -100.f : path<=2 ? 100.f : 30.f);
                        uint32_t seed=0x3579u+7919u*n;
                        const auto roll=[](uint32_t &state) { state=state*1103515245u+12345u; return (state&0x7fffffffu)&0x3fffu; };
                        for(;;++seed) { uint32_t state=seed; const uint32_t k=roll(state); if(path==2 ? k<2300 : k>=2400) break; }
                        word(0x17058,seed); // libc rand state behind 0x4e429c
                        if(path==0) services.stop=n>=64 || site==26 ? 0x143c38 : 0x178228; // crash: car damage, or the jet sound after the last effect
                        reg(16,jet); reg(17,jet+0x20); fp(20,12.f); fp(21,0.f); fp(22,1.f); fp(23,.5f);
                        reg(4,cs+0x20); reg(6,sp);
                        if(site==26) reg(5,jet+0x20);
                        else {
                            reg(5,sp);
                            const float points[6][3]={{0,0,0},{n&1 ? 80.f : -80.f,50.f,10.f},{0.f,300.f,-20.f},{0.f,150.f,0.f},{0.f,200.f,-20.f},{0.f,100.f,0.f}};
                            xyz(sp,points[site-26][0],points[site-26][1],points[site-26][2]);
                        }
                    }
                    else if(site>=24) {
                        // Blimp::UpdateDeath: while it is above the ground it drifts down and rolls; its velocity at +20 goes
                        // through the rebuilt basis into the static vector 0x4fc200, added to its position. A 1-in-9 roll
                        // throws an explosion point (+-80, random Y/Z) through the same basis, added to the position, into the
                        // effect and explosion sound: the fixture stops at particleCreateFx. Near the ground it crash-lands
                        // (effect first, no transform). Vehicle lists are empty, so no car is set on fire.
                        constexpr uint32_t blimp=object, explosion=0x4fc200;
                        word(blimp+0x30,address(cs)); word(blimp+0xc,5); xyz(blimp+0x20,1.5f,0.f,-2.f);
                        f(gp-0x7a54,.5f); f(gp-0x7a50,.25f); f(gp-0x7a4c,1.f); f(gp-0x7a48,.0625f); f(gp-0x7a44,1.f);
                        word(scene+0x1b3dc0,0); word(scene+0x1b3db8,0);
                        const bool landing=n>=64 && scenario==1, burst=site==25 || scenario==4;
                        f(gp-0x35e8,landing ? 0.f : -1000.f); // empty-grid ground height: 30 above it lands, 1030 drifts
                        uint32_t seed=0x1357u+7919u*n;
                        const auto roll=[](uint32_t &state) { state=state*1103515245u+12345u; return (state&0x7fffffffu)&0x3fffu; };
                        for(;;++seed) { uint32_t state=seed; const uint32_t k=roll(state); if(burst ? k<1800 : k>=1900) break; }
                        word(0x17058,seed); // libc rand state behind 0x4e429c
                        if(landing || burst) services.stop=0x1e6610;
                        basis(cs+0x20,n%4==0);
                        reg(16,explosion); reg(17,0x500000); reg(18,blimp); reg(4,cs+0x20); reg(6,explosion);
                        if(site==24) reg(5,blimp+0x20); else { reg(5,explosion); xyz(explosion,n&1 ? 80.f : -80.f,30.f,-10.f); }
                    }
                    else if(site>=21) {
                        // GasTank: two tanks per level, selected by +10, each with a home position in the game's table.
                        constexpr uint32_t tank=object;
                        const uint32_t homes=site==21 ? 0x4e55e0u : 0x4e5600u, side=n&1;
                        word(tank+0x10,side); word(tank+8,0); word(tank+0xc,0); word(tank+4,1); word(tank+0x84,address(cs));
                        for(unsigned i=0;i<2;++i) { xyz(homes+16*i,site==21 && scenario==3 ? 12.f : 1000.f,site==21 && scenario==3 ? 18.f : 1000.f,0.f); word(homes+16*i+12,bits()); }
                        if(site==21) {
                            // UpdateGasTank (rolling): the tank's velocity through its own stored basis is added to its position,
                            // its spin angles are accumulated and the matrix rebuilt. Below the floor it bounces (sparks and a bounce
                            // sound: the fixture stops at the effect); near its home it lands (switch/animation: stops there).
                            basis(tank+0x40,n%4==0); xyz(tank+0x30,2.f,-3.f,4.5f); xyz(tank+0x20,0.f,0.f,0.f);
                            f(gp-0x6f08,.5f); f(gp-0x6f04,-1.5f); f(gp-0x6f00,.5f); f(gp-0x6efc,.25f); f(gp-0x6ef8,1.f); f(gp-0x6ef4,0.f);
                            if(scenario==2) f(cs+0x18,-100.f);
                            if(n>=64 && scenario==1) word(tank+4,0); // inactive tank: no update
                            if(scenario==2) services.stop=0x1e6610; else if(scenario==3) services.stop=0x1d4ff8;
                            std::memcpy(ram+sp+0x20,ram+homes,0x20);
                            reg(16,tank); reg(18,sp+0x10); reg(4,tank+0x40); reg(5,tank+0x30); reg(6,sp+0x10);
                        } else {
                            // UpdateFalling: once the fall animation is over (none is loaded here: percent -1), a 1-in-16 roll
                            // throws a burst 140 up through a random heading; otherwise a 1-in-9 roll throws one 150 up. Both are
                            // added to the tank's home position and handed to particleCreateFx: the fixture stops there.
                            ram[0x49f484]=0; // no NGP files loaded: the animation handle stays empty
                            if(n>=64 && scenario==1) word(tank+8,1);
                            f(gp-0x6eec,6.f); f(gp-0x6ee8,6.f);
                            uint32_t seed=0x2468u+7919u*n;
                            const auto roll=[](uint32_t &state) { state=state*1103515245u+12345u; return (state&0x7fffffffu)&0x3fffu; };
                            for(;;++seed) {
                                uint32_t state=seed; const uint32_t first=roll(state), second=roll(state);
                                if(site==22 ? first<1000 : first>=1100 && (scenario==2 ? second>=1900 : second<1800)) break;
                            }
                            word(0x17058,seed); // libc rand state behind 0x4e429c
                            if(!(n>=64 && (scenario==1 || (site==23 && scenario==2)))) services.stop=0x1e6610;
                            basis(sp+0x40,n%4==0); xyz(sp+0x20,0.f,site==22 ? 140.f : 150.f,0.f);
                            std::memcpy(ram+sp,ram+homes,0x20);
                            reg(16,sp+0x40); reg(17,tank); reg(18,sp+0x20); reg(4,sp+0x40); reg(5,sp+0x20); reg(6,sp+0x20);
                            fp(20,0.f); fp(21,site==22 ? 15.f : 50.f); fp(22,10.f);
                        }
                    }
                    else if(site>=19) {
                        // JunkJet::UpdateJet / Blimp::UpdateBlimp: steer toward the current waypoint, rebuild the matrix from the
                        // smoothed angles, then move by the speed vector at +20 through that basis. The jet's engine sound and
                        // the blimp's grid reparent follow; fixtures stop there. The first scenario parks the flyer instead.
                        const bool jet=site==19;
                        word(object+(jet ? 0x70u : 0x30u),address(cs)); word(object+4,1); word(object+8,0);
                        if(jet) { word(object+4,0); word(object+0xc,n>=64 && scenario==1 ? 1 : 0); } // the jet flies while +4 is clear
                        else if(n>=64 && scenario==1) word(object+4,0);
                        word(object+(jet ? 0x74u : 0x34u),0); xyz(object+(jet ? 0x80u : 0x40u),scenario==2 ? 12.f : 500.f,scenario==2 ? 22.f : 400.f,30.f);
                        xyz(object+0x20,0.f,3.f,0.f); xyz(cs+0x60,.25f,0.f,.5f);
                        for(uint32_t at=jet ? 0x6c38u : 0x7a7cu;at>=(jet ? 0x6c14u : 0x7a58u);at-=4) f(gp-at,.5f); // steering/speed smoothing
                        if(!(n>=64 && scenario==1)) services.stop=jet ? 0x1782f0 : 0x1c89e8;
                        reg(16,object); reg(4,cs+0x20); reg(5,object+0x20); reg(6,sp);
                    }
                    else if(site>=17) {
                        // EnvHeli/Helicopter::UpdateDeath: while the death timer runs, the craft sinks and rolls; its smoothed
                        // fall velocity goes through its own stored basis and is added to its coordinate-system position.
                        // Debris bursts and the timeout's regeneration have their own transforms/effects: fixtures stop there.
                        const bool heli=site==18;
                        const uint32_t ownBasis=object+(heli ? 0x50u : 0x40u), timer=object+(heli ? 0xcu : 0x14u);
                        word(object+(heli ? 0xa8u : 0x90u),address(cs)); basis(ownBasis,n%4==0); f(object+0x34,-2.f);
                        for(uint32_t at:heli ? std::initializer_list<uint32_t>{0x6da8u,0x6da4u} : std::initializer_list<uint32_t>{0x7090u,0x708cu}) f(gp-at,.5f);
                        f(gp-(heli ? 0x6dacu : 0x7094u),.125f); // roll step
                        const bool timeout=n>=64 && scenario==1, burst=heli && scenario==3;
                        word(timer,timeout ? 119 : burst ? (n>=64 ? 59 : 60) : 10);
                        if(timeout) services.stop=heli ? 0x139390 : 0x10de38; // Regen / BlowDebris
                        if(burst) services.stop=0x10de38;                      // BlowDebris at frame 60
                        reg(16,object); reg(4,ownBasis); reg(5,object+0x30); reg(6,sp);
                        if(n<64) xyz(object+0x30,0.f,-1.5f,0.f);
                    }
                    else if(site>=15) {
                        if(site==15) {
                            // FerrisWheel::UpdateFalling: the falling car's offset through its coordinate system, added to the
                            // position; the resulting height (Z) selects the landing state at gp-312c.
                            word(object+0x90,address(cs)); xyz(object+0x30,2.f,-3.f,4.5f); word(gp-0x312c,0);
                            if(scenario==2) f(cs+0x18,-100.f);
                            reg(16,object); reg(4,cs+0x20); reg(5,object+0x30); reg(6,sp);
                        } else {
                            // Smashers::SmasherHit: a camera offset (0,-200,60) through the slot's camera basis, added to the hit
                            // vehicle's position, then handed to TheGame::DetachCamera, which copies it into that camera.
                            constexpr uint32_t smasher=object, cameraModes=0x4ac0d0;
                            const uint32_t slot=n%4;
                            word(smasher+0x38,n>=64 && scenario==1 ? 0 : 4); for(unsigned i=0;i<4;++i) word(smasher+4*i,n>=64 && scenario==3 && i==slot ? 1 : 0);
                            word(smasher+0x10,address(vehicle)); word(vehicle+0x20bc,n>=64 && scenario==2 ? 0 : 1); word(vehicle+0x20c0,slot);
                            for(unsigned i=0;i<4;++i) { word(0x4b0370+4*i,address(cs)); word(0x4b0380+4*i,address(child)); word(cameraModes+4*i,0); }
                            xyz(child+0x10,40.f,50.f,60.f);
                            reg(16,sp+0x10); reg(17,smasher); reg(18,vehicle); reg(4,cs+0x20); reg(5,sp); reg(6,sp+0x10); xyz(sp,0.f,-200.f,60.f);
                        }
                    }
                    else if(site>=8) {
                        // Launcher turret aim (Hearse, Minion, Tanker): the object is the launcher, the vehicle its owner.
                        // GetClosestCar walks the game's embedded vehicle array; one live candidate car is at the game base.
                        constexpr uint32_t launcher=object, car=scene, carCs=parent;
                        word(launcher,scenario==1 ? 5 : 1); word(launcher+0x10c,0xb); word(launcher+0x2f4,address(car)); word(launcher+0x2f8,address(carCs)); word(launcher+0x2a0,address(oldNode)); // turret node: real matrix publication at +0x10
                        xyz(launcher+0x3a0,1.f,-2.f,3.f); xyz(0x23cfc0,2.f,-3.f,4.f); f(0x23cfd8,.5f); xyz(0x23d1f0,-1.f,5.f,2.f);
                        word(vehicle+0x2018,address(cs)); f(vehicle+0x1ecc,.25f);
                        word(scene+0x1b3db4,1); word(scene+0x1b3db0,0); f(gp-0x635c,1e6f);
                        word(car+0x580,1); if(site>=11) xyz(car+0x1d0,250.f,350.f,450.f); else xyz(car+0x1d0,500.f,600.f,700.f); word(car+0x2018,address(carCs)); xyz(carCs+0x10,500.f,600.f,700.f); // Cop SUV laser range is 600
                        for(uint32_t at:{0x61e8u,0x61e4u,0x61e0u,0x61f8u,0x61f4u}) f(gp-at,.5f); // bounded turret angle smoothing
                        const uint32_t out=sp+0x10, matrix=sp+0x30;
                        if(site==8 || site==11 || site==12) { reg(16,out); reg(17,launcher); reg(18,vehicle); reg(19,vehicle+0x1d0); reg(4,cs+0x20); reg(5,out); xyz(out,2.f,-3.f,4.5f); }
                        else if(site>=13) {
                            // Environment gun (UpdateEnvWeapon): the game-mode jump table selects the turret case; every tenth
                            // update fires from one of two offset tables, toggled at +f0, and passes the world point to
                            // CreateStaticWeapon. The fixture stops there; the no-fire scenario returns normally.
                            word(scene+0x1b3db0,1); word(0x4e71c4,0x14d448); word(scene+0x1b2a3c,address(cs));
                            word(launcher+0x264,n>=64 && scenario==1 ? 12 : 11); word(launcher+0xf0,site==13 ? 1 : 0);
                            xyz(0x23d240,2.f,-3.f,4.5f); xyz(0x23d250,-1.5f,6.f,-2.f); xyz(cs+0x60,.25f,0.f,.5f);
                            for(uint32_t at:{0x6340u,0x6338u,0x6334u}) f(gp-at,.5f); f(gp-0x633c,-1e6f); // aim limits and smoothing
                            ram[vehicle+0x475]=0; reg(20,launcher); reg(30,vehicle); setRegU32(&initial,16,0x1b0000); // no special-weapon handoff in the epilogue
                            reg(4,cs+0x20); reg(5,site==13 ? 0x23d240 : 0x23d250); reg(6,sp);
                            if(!(n>=64 && scenario==1)) services.stop=0x1b7590;
                        }
                        else {
                            basis(matrix,n%4==0); reg(16,out); reg(17,matrix); reg(20,vehicle+0x1d0); reg(4,matrix); reg(6,out);
                            if(site==9) { reg(18,launcher); reg(19,vehicle); reg(5,launcher+0x3a0); }
                            else { reg(18,vehicle); reg(19,launcher); reg(5,0x23d1f0); }
                        }
                        if(site==8 || site==11 || site==12) reg(6,out);
                        if(site==11 || site==12) {
                            // Cop SUV: mode 0x14 with a live player state takes the first aim branch, otherwise the second.
                            // Both branches end in the laser-sound start/stop calls into the sound driver: the fixture stops there,
                            // after the aim point has been consumed.
                            ram[vehicle+0x483]=0; reg(20,vehicle+0x1d0); word(scene+0x1b3db0,site==11 ? 0x14 : 0);
                            word(scene+0x1ae650,1); word(scene+0x1ab500,address(carCs));
                            if(!(site==12 && n>=64 && scenario==1)) { services.stop=0x17baf0; services.stop2=0x17bba0; } // no target: the owner returns normally
                        }
                    }
                    else {
                        // Ricochet: s2 is the weapon. A low resulting height takes the real reflection branch to the second
                        // transform, whose basis the owner rebuilds from the reflected direction; frame count 0 skips it.
                        reg(18,object); reg(19,object+0xd0); reg(16,scene+0x190c20);
                        f(object+0x98,-25.f); word(object+0x20,1+(n&1)); word(object+0x24,0); f(gp-0x4a8c,-1e6f); f(gp-0x4a80,1e6f);
                        xyz(object+0xc0,.25f,.5f,0.f); f(gp-0x4a7c,.5f); f(gp-0x4a78,10.f); // bounded heading/pitch smoothing
                        reg(4,object+0xd0); reg(6,sp);
                        if(site==6) { reg(5,sp); xyz(sp,3.f,-4.f,-25.f); } else reg(5,object+0x90);
                    }
                } else {
                    continuation=kind==226 ? resume_0x1b2c40 : kind==227 ? resume_0x1b3c88 : resume_0x1b3d58;
                    entry=kind==226 ? 0x1b2c40 : kind==227 ? 0x1b3c88 : 0x1b3d58;
                    returnAt(kind==226 ? 0x30 : 0x20); word(object+0x130,address(cs));
                    const float y=scenario==6 ? -2.f : 2.f;
                    xyz(object+0x90,scenario==3 ? 1e-30f : 1.f,y,3.f); f(object+0x70,y); f(object+0x50,.5f);
                    for(auto offset:{0x4ab8u,0x49f8u,0x49ecu}) { f(gp-offset,1.f); f(gp-offset+4,.5f); f(gp-offset+8,10.f); }
                    if(kind==226) {
                        reg(17,scene+0x190c20); basis(object+0xd0,n%4==0); reg(4,object+0xd0);
                        word(object+0x20,scenario==1 ? 1 : 0); word(object+0x1c,scenario==2 || scenario==5 ? 1 : 0);
                        word(object+0x48,scenario==3 ? 0xffffffff : 0); word(object+4,scenario==5 ? 301 : 0); word(object+8,0); word(object+0x10,42);
                        word(object+0x110,address(vehicle)); word(vehicle+0x580,0); f(object+0xa0,0.f); f(object+0xa4,0.f);
                        f(gp-0x4aac,0.f); f(gp-0x4aa8,.5f); f(gp-0x4aa4,10.f); f(gp-0x4aa0,10.f); f(gp-0x4a9c,100000.f);
                        word(gp-0x38d0,1); word(scene+0x190c20+0xafa8,address(child)); xyz(child+0x10,-1.f,-1.f,-1.f);
                    }
                }
                if(kind<=225 && scenario==7) { word(cs+0x70,0); word(lists,address(oldNode)); word(oldNode,0); word(oldNode+4,address(cs)); }
                if(n>=64) {
                    fromEntry=true; initial.pc=entry; reg(4,object); setRegU32(&initial,31,0xbad0000);
                    if(kind==225) reg(5,actor);
                    if(kind>=244) reg(5,vehicle); // launcher and smasher owners take (object, vehicle)
                    if((kind>=274 && kind<=278) || kind==284) reg(4,vehicle); // the combo and AI owners take the vehicle
                    if(kind==285 || kind==286) { reg(4,object+0x500); reg(5,object+0x510); } // particle arcs: start, then Z direction / end
                }
            }
            else
            {
                constexpr uint32_t vehicle=0x18000, cs=0x22000, part=0x23000, impact=0x26000, hit=0x4b0560;
                const unsigned first=kind==229 ? 0 : (kind-228)/2;
                const unsigned target=n>=64 ? (n%64)/16 : first;
                const unsigned variant=(n%16)/4;
                const bool rotated=n%4==0;
                const auto f=[&](uint32_t at,float v) { uint32_t u; std::memcpy(&u,&v,4); word(at,u); };
                const auto xyz=[&](uint32_t at,float x,float y,float z) { f(at,x); f(at+4,y); f(at+8,z); };
                const auto basis=[&](uint32_t at,bool turn) {
                    xyz(at,turn ? 0.f : 1.f,turn ? 1.f : 0.f,0.f);
                    xyz(at+16,turn ? -1.f : 0.f,turn ? 0.f : 1.f,0.f); xyz(at+32,0.f,0.f,1.f);
                    for(unsigned i=0;i<3;++i) word(at+16*i+12,bits());
                };
                const std::array<std::array<float,3>,4> local{{{100.f,0.f,0.f},{0.f,100.f,0.f},{0.f,0.f,100.f},{-100.f,0.f,0.f}}};
                basis(cs+0x20,rotated); basis(part+0x10,true); xyz(cs+0x10,10.f,20.f,30.f); word(cs+0x1c,bits());
                word(vehicle+0x2018,address(cs)); word(vehicle+0x1ec8,address(part)); word(vehicle+0x20c0,n%4);
                for(unsigned i=0;i<4;++i) {
                    xyz(0x4f1a10+16*i,local[i][0],local[i][1],local[i][2]); word(0x4f1a1c+16*i,bits());
                    word(vehicle+0x3f8+4*i,variant==3 && i==target ? 10 : 100);
                    word(vehicle+0x2058+4*i,address(0x27000+16*i)); word(0x27000+16*i,6); ram[0x2700b+16*i]=2; ram[0x27008+16*i]=0;
                    std::memcpy(ram+sp+16+16*i,ram+0x4f1a10+16*i,16);
                }
                // Independent exact quarter-turn coordinates, including the second local basis for shields 1..3.
                auto point=local[target]; if(rotated) { const float x=point[0]; point[0]=-point[1]; point[1]=x; }
                if(target) { const float x=point[0]; point[0]=-point[1]; point[1]=x; }
                xyz(impact,point[0]+10.f+(variant==1 ? 50.f : variant==2 ? 1000.f : 0.f),point[1]+20.f,point[2]+30.f); word(impact+12,bits());
                word(hit,address(impact)); word(hit+8,0); word(gp-0x4154,address(0x360000));
                word(gp-0x3114,0); word(gp-0x44c4,0); word(0x235280,9); // actual inactive force-field bookkeeping
                word(0x50be50+0xa00*(n%4)+0x9d8,n%2 ? 2 : 0);
                services.stop=variant==3 ? 0x1b04d8 : 0xffffffff;
                continuation=resume_0x1ad9a0; returnAt(0xb0);
                reg(17,vehicle); reg(18,hit); setRegU32(&initial,19,20); setRegU32(&initial,20,0); setRegU32(&initial,21,0);
                reg(4,cs+0x20); reg(5,sp+16+16*first); reg(6,sp);
                // Second calls consume the actual first-basis result in place.
                if(kind>=231 && kind%2==1) {
                    auto v=local[first]; if(rotated) { const float x=v[0]; v[0]=-v[1]; v[1]=x; }
                    xyz(sp,v[0],v[1],v[2]); reg(4,part+0x10); reg(5,sp);
                }
                if(n>=64) { fromEntry=true; initial.pc=0x1ad9a0; reg(4,vehicle); setRegU32(&initial,5,20); setRegU32(&initial,31,0xbad0000); }
            }
            snapshot(input.get());
            const auto stopAt=services.stop;
            const auto run = [&](bool native, R5900Context &ctx) {
                services.stop=stopAt; referenceW.clear(); services.yields = services.randomCalls = services.weatherPoints = services.weatherIdentities = 0;
                services.spatialCalls.fill(0); auditedInverseCalls.fill(0); services.aiActions.fill(0); services.vehicleLightActions.fill(0); services.finalActions.fill(0); services.rigidActions.fill(0); rigidDirectionCalls.fill(0); driverDirectionCalls.fill(0); services.driverActions.fill(0); services.driverQueryEndpoints.fill(0); services.collisionActions.fill(0); services.collisionVelocities.fill(0); collisionDirectionCalls.fill(0); attachmentDirectionCalls.fill(0); services.attachmentActions.fill(0); minionDirectionCalls.fill(0); services.minionActions.fill(0); services.minionPoints.clear(); services.minionFixture=kind>=229 && kind<=235;
                weaponDirectionCalls.fill(0); sawWeaponLeaf=false; services.weaponFixture=kind>=236; services.launcherFixture=kind>=244; services.jetFixture=kind>=262; services.chainFixture=kind>=306 && kind<341; services.loopFixture=kind>=341; services.weaponOutAddrs.clear(); for(auto *v:{&services.weaponInputs,&services.weaponBases,&services.weaponOutputs,&services.weaponTargets}) v->clear();
                rt->functions.setOverride(0x1da1e8, native ? (kind>=155 ? auditedInverse : inverse) : referenceInverse);
                rt->functions.setOverride(0x1da298, native ? (kind == 148 ? rearDirection : (kind>=224 && kind<=228) ? attachmentDirection : kind>=236 ? weaponDirection : kind>=229 ? minionDirection : (kind>=218 && kind<=223) ? collisionDirection : (kind>=211 && kind<=217) ? driverDirection : (kind>=201 && kind<=210) ? rigidDirection : direction) : referenceDirection);
                sawDirectionEntry=false; sawForceInverseEntry=false;
                host.instruction=kind == 139 ? vehicleInstructions : nullptr;
                if (kind == 141 && native) host.instruction=vehicleInstructions;
                rt->functions.setOverride(0x1dcc08,kind == 139 ? matrix : nullptr);
                rt->functions.setOverride(0x1da238,kind == 149 ? registry.find(0x1da238) : nullptr);
                rt->functions.setOverride(0x1db0f0,kind == 149 ? registry.find(0x1db0f0) : nullptr);
                auto helper = isInverse(kind) ? (native ? (kind>=155 ? auditedInverse : inverse) : referenceInverse) : (native ? ((kind>=224 && kind<=228) ? attachmentDirection : kind>=236 ? weaponDirection : kind>=229 ? minionDirection : (kind>=218 && kind<=223) ? collisionDirection : (kind>=211 && kind<=217) ? driverDirection : (kind>=201 && kind<=210) ? rigidDirection : direction) : referenceDirection);
                if (!fromEntry && services.weaponFixture) {
                    const auto at=bozo::GuestMemory::flatOffset(getRegU32(&ctx,5),12); require(at>=0,"invalid weapon input");
                    std::array<uint32_t,3> v; std::memcpy(v.data(),ram+at,12); services.weaponInputs.push_back(v); services.weaponOutAddrs.push_back(getRegU32(&ctx,6)&0x1fffffffu);
                }
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
            const auto refAuditedInverseCalls=auditedInverseCalls; const auto refAiActions=services.aiActions; const auto refVehicleLightActions=services.vehicleLightActions; const auto refFinalActions=services.finalActions; const auto refRigidActions=services.rigidActions; const auto refRigidDirections=rigidDirectionCalls; const auto refDriverDirections=driverDirectionCalls; const auto refDriverActions=services.driverActions; const auto refDriverEndpoints=services.driverQueryEndpoints; const auto refCollisionActions=services.collisionActions; const auto refCollisionVelocities=services.collisionVelocities; const auto refCollisionDirections=collisionDirectionCalls; const auto refAttachmentDirections=attachmentDirectionCalls; const auto refAttachmentActions=services.attachmentActions; const auto refMinionDirections=minionDirectionCalls; const auto refMinionActions=services.minionActions; const auto refWeaponDirections=weaponDirectionCalls; const auto refWeaponInputs=services.weaponInputs, refWeaponBases=services.weaponBases, refWeaponOutputs=services.weaponOutputs, refWeaponTargets=services.weaponTargets; const auto refMinionPoints=services.minionPoints;
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
            if((kind>=211 && kind<=217)) {
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
            if((kind>=218 && kind<=223)) {
                const unsigned scenario=(n%64)/8;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                const auto vector=[&](uint32_t at,float x,float y,float z) {
                    const float expected[3]={x,y,z}; require(!std::memcmp(ram+at,expected,12),"collision known state publication differs");
                };
                std::array<unsigned,6> reached{};
                const bool active=!fromEntry || (kind<=219 ? scenario!=1 && scenario!=2 : kind<=222 && kind>=221 ? scenario<1 || scenario>3 : true);
                reached[kind-218]=active;
                if(kind<=219) reached[2]=2; // actual coupled force applications, enabled by fixture thresholds
                require(refCollisionDirections==reached,"collision forward producer reach differs");
                const uint32_t frame=kind<=219 && fromEntry ? sp-0xb0 : (kind==220 || kind==223) && fromEntry ? sp-0x80 : sp;
                const unsigned count=kind<=219 ? unsigned(active)+1 : kind==223 ? 0 : unsigned(active);
                require(referenceW.size()==count,"collision forward output count differs");
                for(auto patch:referenceW) {
                    const bool allowed=kind<=219 ? patch.offset==frame-0x1b4 || (active && patch.offset==(kind==218 ? 0x181ec : 0x1c1ec)) :
                        (kind>=220 && kind<=222) && patch.offset==frame+0x1c;
                    require(allowed,"unexpected collision forward-W exclusion");
                }
                if(kind<=219) {
                    require(refCollisionActions[0]==1 && refCollisionActions[1]==1 && refCollisionActions[2]==2,
                        "collision relative-mass/force consumers not executed");
                    require(refCollisionActions[3]==4 && refCollisionActions[4]==4 && refCollisionActions[5]==2,
                        "collision normalization/matrix/rotation consumers not executed");
                    require(refAuditedInverseCalls[7]==1 && refAuditedInverseCalls[8]==1,"collision force inverse consumers not executed");
                    require(a.pc==0x11b2e8,"collision effect dispatch boundary differs");
                    if(fromEntry && scenario==0) {
                        const float expected[6]={n%4==0 ? -4.f : 0.f,n%4==0 ? 0.f : 4.f,0.f,4.f,4.f,4.f};
                        require(!std::memcmp(refCollisionVelocities.data()+(kind==218 ? 0 : 6),expected,sizeof(expected)),
                            "collision reconstructed velocity/speed not published before force consumers");
                        ++collisionPublications[0];
                    }
                } else if(kind==220) {
                    require(refCollisionActions[3]==1 && refCollisionActions[4]==1 && refCollisionActions[5]==1,
                        "force normalization/matrix/rotation consumers not executed");
                    require(ram[0x18474]==1,"force state flag not published");
                    if(fromEntry && scenario==0) { vector(0x181e0,n%4==0 ? -3.5f : .5f,n%4==0 ? 1.f : 5.f,0.f); ++collisionPublications[1]; }
                } else if(kind<=222) {
                    require(a.pc==0x142d98,"impact hit-dispatch boundary differs");
                    require(refCollisionActions[7]==unsigned(fromEntry && active) && refCollisionActions[8]==unsigned(fromEntry && active),
                        "impact heading/angle producers not executed");
                    require(read(0x24000)==(active ? getRegU32(&initial,29)+0x10 : (0x181d0u|(getRegU32(&initial,29)&0xa0000000u))),
                        "impact take-hit point pointer not published");
                    if(fromEntry && scenario==0) {
                        const float local[2][3]={{1.f,2.f,3.f},{-3.f,1.f,-2.f}};
                        const auto &v=local[kind-221]; vector(sp+0x10,10.f+(n%4==0 ? -v[1] : v[0]),20.f+(n%4==0 ? v[0] : v[1]),30.f+v[2]);
                        ++collisionPublications[2];
                    }
                } else {
                    require(refCollisionActions[6]==1 && refAuditedInverseCalls[10]==1 && refAuditedInverseCalls[11]==1,
                        "morph inverse/hit-location consumers not executed");
                    require(read(0x1828c)==read(0x1819c) && read(frame+0x1c)==read(0x180dc),"morph lost meaningful inverse W");
                    require(read(0x1979c)>=1 && read(0x1979c)<=8 && read(0x197a0)==read(0x1979c),"morph hit classification not published");
                    if(fromEntry) ++collisionPublications[3];
                }
                if(refCollisionActions[9]) { require(read(0x17000)==33,"actual libm domain errno was not retained"); ++collisionErrnoCases; }
            }
            if(kind>=224 && kind<=228) {
                const unsigned scenario=(n%64)/8;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                const auto vector=[&](uint32_t at,float x,float y,float z) {
                    const float expected[3]={x,y,z}; require(!std::memcmp(ram+at,expected,12),"attachment known position differs");
                };
                const bool active=!fromEntry || !((kind==225 || kind==226) && scenario==1);
                std::array<unsigned,5> reached{}; reached[kind-224]=active;
                require(refAttachmentDirections==reached,"attachment forward producer reach differs");
                const uint32_t frame=fromEntry ? sp-(kind==225 ? 0x40u : kind==226 ? 0x60u : 0x30u) : sp;
                require(referenceW.size()==unsigned(active),"attachment forward output count differs");
                for(auto patch:referenceW) require(patch.offset==frame+0xc,"unexpected attachment forward-W exclusion");
                if(kind<=225) {
                    const bool reparent=kind==224 || active;
                    const bool grid=reparent && scenario!=2;
                    require(refAttachmentActions[2]==unsigned(reparent) && refAttachmentActions[3]==unsigned(grid) &&
                        refAttachmentActions[4]==unsigned(grid) && refAttachmentActions[5]==unsigned(grid && scenario==7),
                        "attachment grid reparent/insertion/removal consumers not executed");
                    if(grid) {
                        const uint32_t cell=scenario==7 ? 1 : 0, node=scenario==7 ? 0x29000 : 0x27000;
                        const uint32_t alias=getRegU32(&initial,29)&0xa0000000u;
                        require(read(0x22070)==cell && read(0x28000+4*cell)==(node|alias) && read(node+4)==(0x22000u|alias),
                            "attachment translated coordinate not linked into its real grid cell");
                        if(scenario==7) { require(read(0x28000)==0,"old attachment grid membership not removed"); ++gridRemovals; }
                    }
                    if(kind==224) require(refAttachmentActions[0]==1,"helicopter height smoothing not executed");
                    if(kind==225) {
                        if(!active) require(ram[0x19006]==2 && read(0x19014)==0,"disabled cable state not published");
                        else if(scenario==6) {
                            require(a.pc==0x155a68 && read(0x19014)==0 && read(0x18008)==1 && getRegU32(&a,6)==12,
                                "cable expiry timer/type-dispatch staging differs");
                        } else require(read(0x19014)==(scenario==5 ? 301 : 1),"cable lifetime increment differs");
                    }
                } else {
                    require(refAttachmentActions[1]==unsigned(fromEntry && active ? 3 : 0),"weapon frame-count producers not executed");
                    if(kind>=227 || active) require(refAttachmentActions[0]==unsigned(fromEntry),"weapon velocity smoothing not executed");
                    if(kind==226) {
                        const bool query=scenario!=2 && scenario!=3 && scenario!=5;
                        require(refAttachmentActions[6]==unsigned(query) && refAttachmentActions[7]==unsigned(query) &&
                            refAttachmentActions[8]==unsigned(query),"gascan actual height consumers not executed");
                        require(refAttachmentActions[10]==unsigned(fromEntry && !active ? 3 : 0),"parked gascan angle consumers not executed");
                        if(fromEntry && scenario==1) { vector(0x22010,10.f,20.f,30.f); vector(0x25010,10.f,20.f,1.f); }
                        if(scenario==5) require(read(0x18008)==1,"gascan lifetime state not published");
                    }
                }
                if(fromEntry && scenario==0) {
                    vector(0x22010,n%4==0 ? 8.f : 11.f,n%4==0 ? 21.f : 22.f,33.f);
                    if(kind==226) vector(0x25010,n%4==0 ? 8.f : 11.f,n%4==0 ? 21.f : 22.f,1.f);
                    ++attachmentPublications[kind-224];
                }
            }
            if(kind>=229 && kind<=235) {
                const unsigned first=kind==229 ? 0 : (kind-228)/2, target=fromEntry ? (n%64)/16 : first, variant=(n%16)/4;
                const unsigned start=fromEntry ? 0 : first, last=variant==0 || variant==3 ? target : 3;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                std::array<unsigned,7> reached{};
                for(unsigned i=start;i<=last;++i) { if(i==0) reached[0]=1; else reached[2*i-1]=reached[2*i]=1; }
                if(!fromEntry && kind>=231 && kind%2==1) reached[kind-230]=0;
                require(refMinionDirections==reached,"Minion transform producer reach differs");
                require(refMinionPoints.size()==last-start+1,"Minion distance input count differs");
                const std::array<std::array<float,3>,4> local{{{100.f,0.f,0.f},{0.f,100.f,0.f},{0.f,0.f,100.f},{-100.f,0.f,0.f}}};
                for(unsigned i=start;i<=last;++i) {
                    auto point=local[i]; if(n%4==0) { const float x=point[0]; point[0]=-point[1]; point[1]=x; }
                    if(i) { const float x=point[0]; point[0]=-point[1]; point[1]=x; }
                    point[0]+=10.f; point[1]+=20.f; point[2]+=30.f;
                    require(!std::memcmp(refMinionPoints[i-start].data(),point.data(),12),"Minion independent world hit point differs");
                }
                require(refMinionActions[0]==unsigned(fromEntry) && refMinionActions[1]==last-start+1,"Minion actual field/distance consumers not executed");
                require(referenceW.size()==1 && referenceW[0].offset==(fromEntry ? sp-0xd0 : sp)+12,"unexpected Minion forward-W exclusion");
                for(unsigned i=0;i<4;++i) {
                    const auto expected=variant==3 && i==target ? uint32_t(-10) : variant==0 && i==target ? 80u : 100u;
                    require(read(0x183f8+4*i)==expected,"Minion shield health publication differs");
                }
                if(variant==0) {
                    require(refMinionActions[2]==1 && refMinionActions[3]==0 && refMinionActions[4]==0,"Minion damage/message path differs");
                    require(read(0x50be50+0xa00*(n%4)+(n%2 ? 0x9cc : 0x9dc))==0xd2+target,"Minion actual HUD message publication differs");
                    require(getRegU32(&a,2)==1,"Minion retained shield return differs"); ++minionPublications[target];
                } else if(variant==3) {
                    require(refMinionActions[3]==1 && refMinionActions[4]==1 && a.pc==0x1b04d8 && ram[0x27008+16*target]==1,"Minion destroyed shield switch publication differs");
                    ++minionDestructions[target];
                } else {
                    require(refMinionActions[2]==0 && refMinionActions[3]==0,"Minion distance rejection modified shield/UI");
                    require(getRegU32(&a,2)==1,"Minion distance rejection return differs"); if(variant==1) ++minionBoundaries;
                }
            }
            if(kind>=236) {
                const unsigned site=kind-236, scenario=(n%64)/8;
                const auto read=[&](uint32_t at) { uint32_t v; std::memcpy(&v,ram+at,4); return v; };
                const bool active=!fromEntry || site>=2 || (scenario!=1 && scenario!=2 && scenario!=3);
                std::array<unsigned,107> reached{};
                std::vector<unsigned> order; // reached sites in execution order
                if(site>=105) { if(fromEntry || site==105) { reached[105]=1; order.push_back(105); } reached[106]=1; order.push_back(106); } // both points of a segment
                else if(site>=78) { reached[site]=1; order.push_back(site); } // UpdateSpecial arcs and weapon hit tests: one transform each
                else if(site>=75) {
                    if(!(fromEntry && scenario==1)) {
                        const unsigned start=fromEntry ? 75u : unsigned(site);
                        for(unsigned at=start;at<=77;++at) { reached[at]=1; order.push_back(at); }
                    }
                }
                else if(site>=70) {
                    const bool live=!(fromEntry && scenario==1 && site>=73);
                    if(live) {
                        if(site<73) {
                            const unsigned first=site==72 ? (fromEntry ? (n&1 ? 70u : 71u) : 0u) : site;
                            if(first) { reached[first]=1; order.push_back(first); }
                            reached[72]=1; order.push_back(72);
                        } else { reached[site]=1; order.push_back(site); }
                    }
                }
                else if(site>=65) {
                    const unsigned shot=site-65;
                    const bool fire=!(fromEntry && scenario==1), first=shot==0 || shot==2;
                    if(fire) {
                        if(shot==4) { reached[69]=1; order.push_back(69); }
                        else {
                            const unsigned pair=shot<2 ? 65 : 67;
                            if(first || fromEntry) { reached[pair]=1; order.push_back(pair); }
                            reached[pair+1]=1; order.push_back(pair+1);
                        }
                    }
                }
                else if(site>=51) { reached[site]=1; order.push_back(site); } // boss shots and movement: one transform each
                else if(site>=49) {
                    const bool live=!fromEntry || !(scenario==1 || (site==50 && scenario==2));
                    const unsigned count=site==49 && fromEntry ? 10u : 1u; // a full FlatArcs throws ten points
                    reached[site]=live ? count : 0; if(live) for(unsigned i=0;i<count;++i) order.push_back(site);
                }
                else if(site>=47) { const bool live=site==47 || !fromEntry || scenario!=1; reached[site]=live; if(live) order.push_back(site); }
                else if(site>=44) { const bool live=!fromEntry || !(scenario==1 || (site>=45 && scenario==2)); reached[site]=live; if(live) order.push_back(site); }
                else if(site==43) { const bool fire=!fromEntry || scenario!=1; reached[43]=fire; if(fire) order.push_back(43); }
                else if(site>=38) { const bool combo=!fromEntry || scenario!=1; reached[site]=combo; if(combo) order.push_back(site); }
                else if(site>=36) {
                    const unsigned layout=fromEntry && scenario==1 ? 2 : fromEntry && scenario==2 ? 6 : 4;
                    const bool tilted=layout==4, skipOther=fromEntry && scenario==3;
                    if(tilted && (site==36 ? true : fromEntry) && !(skipOther && site==37)) { reached[36]=1; order.push_back(36); }
                    if(tilted && !(skipOther && site==36)) { reached[37]=1; order.push_back(37); }
                }
                else if(site>=34) { const bool armed=!fromEntry || scenario!=1; reached[site]=armed; if(armed) order.push_back(site); }
                else if(site==33) { const bool walking=!fromEntry || scenario!=1; reached[33]=walking; if(walking) order.push_back(33); }
                else if(site>=32) { const bool fire=!fromEntry || scenario<1 || scenario>2; reached[32]=fire; if(fire) order.push_back(32); }
                else if(site>=26) {
                    const unsigned path=fromEntryPath(site,scenario,n);
                    if(fromEntry || site==26) { reached[26]=1; order.push_back(26); }
                    const bool start=fromEntry || site==26;
                    if(start ? path==2 : site==27) { reached[27]=1; order.push_back(27); }
                    if(start ? path==3 : site==28) { reached[28]=1; order.push_back(28); }
                    if(start ? path==3 : site==28 || site==29) { reached[29]=1; order.push_back(29); }
                    if(!start && site==30) { reached[30]=1; order.push_back(30); }
                    if(!start && (site==30 || site==31)) { reached[31]=1; order.push_back(31); }
                }
                else if(site>=24) {
                    const bool live=!(fromEntry && scenario==1);
                    if(live && (site==24 || fromEntry)) { reached[24]=1; order.push_back(24); }
                    if(live && (site==25 || scenario==4)) { reached[25]=1; order.push_back(25); }
                }
                else if(site>=21) { const bool thrown=!fromEntry || (scenario!=1 && !(site==23 && scenario==2)); reached[site]=thrown; if(thrown) order.push_back(site); }
                else if(site>=17) { const bool moving=!fromEntry || scenario!=1; reached[site]=moving; if(moving) order.push_back(site); }
                else if(site>=15) { const bool hit=site==15 || !fromEntry || scenario<1 || scenario>3; reached[site]=hit; if(hit) order.push_back(site); }
                else if(site>=8) { const bool aim=!fromEntry || scenario!=1 || site==11; reached[site]=aim; if(aim) order.push_back(site); }
                // (Environment gun: scenario 1 is the no-fire update count, which skips its transform as well.)
                else if(site<6) { reached[site]=active; if(active) order.push_back(site); }
                else {
                    // Ricochet: the first transform always runs from the entry; the reflection branch then reaches the second.
                    if(site==6 || fromEntry) { reached[6]=1; order.push_back(6); }
                    if(!fromEntry || scenario!=4) { reached[7]=1; order.push_back(7); }
                }
                require(refWeaponDirections==reached,"weapon forward producer reach differs");
                const uint32_t frame=fromEntry && (site<36 || (site>=38 && site!=47 && site<51) || (site>=65 && site!=67 && site!=68 && (site<70 || site>72) && (site<79 || (site>=85 && site<=87) || site==94))) ? sp-(site<2 ? 0x90u : site>=91 ? 0xe0u : site>=87 ? 0x160u : site>=85 ? 0xb0u : site>=65 ? 0x230u : site==49 ? 0x100u : site==50 ? 0x170u : site==48 ? 0x60u : site==43 ? 0x50u : site>=44 ? 0xa0u : site>=38 ? 0x70u : site==5 ? 0x60u : site==8 ? 0xa0u : site>=34 ? 0xd0u : site>=32 ? 0x80u : site>=24 ? 0x70u : site==21 ? 0xc0u : site>=22 ? 0xe0u : site==19 ? 0x60u : site==20 ? 0x50u : site>=17 ? 0x40u : site>=15 ? 0x60u : site>=13 ? 0x110u : site>=11 ? 0xb0u : site==9 ? 0x100u : site==10 ? 0xf0u : site>=6 ? 0xd0u : 0x50u) : sp;
                const uint32_t wAt=site>=105 ? frame+(fromEntry || site==105 ? 0x8cu : 0x9cu) : site>=95 ? (site==98 || site==102 ? 0x1862cu : frame+(site<=96 ? 0x6cu : site==100 || site==104 ? 0x2cu : 0xcu)) : site>=85 ? frame+(site>=87 && site<91 ? 0x1cu : 0xcu) : site>=79 ? frame+(site<=80 ? 0x2cu : 0x3cu) : site>=73 && site<75 ? 0x1862cu : site>=24 && site<26 ? 0x4fc20cu : frame+(site>=22 && site<24 ? 0x2cu : (site>=8 && site<13) || site==16 || site==21 || site==32 || site==33 || site==47 || site==48 || (site>=51 && site<63) ? 0x1cu : site==36 || site==37 ? 0x5cu : 0xcu);
                // FlatArcs writes each point's W itself after the transform: compare it exactly, with no W exclusion.
                if(site==49) referenceW.clear();
                else if(site>=105) {
                    // Both points of the segment leave their forward W in place: frame +8c and +9c.
                    require(referenceW.size()==order.size(),"unexpected LoopTank forward-W exclusion count");
                    for(auto patch:referenceW) require(patch.offset==frame+0x8c || patch.offset==frame+0x9c,"unexpected LoopTank forward-W exclusion");
                }
                else require(referenceW.size()==unsigned(!order.empty()) && (order.empty() || referenceW[0].offset==wAt),"unexpected weapon forward-W exclusion");
                require(refWeaponInputs.size()==order.size() && refWeaponOutputs.size()==order.size(),"weapon transform consumer not executed");
                for(size_t k=0;k<order.size();++k) {
                    // Independent quarter-turn/identity result where the basis is fixture-known, then the actual XYZ
                    // publication through mathfAddVector. Ricochet's second basis is rebuilt by the owner after its first.
                    const unsigned at=order[k];
                    const bool known=at==50 ? false : (at==66 || at==68 || at==75 || at==76) && (fromEntry || site!=at) ? false : at==9 || at==10 || at==13 || at==14 || (at>=19 && at!=21 && at<34) || at==49 ? !fromEntry : at!=7 || (site==7 && !fromEntry); // Minion/Tanker entries build their aim basis
                    const bool rotated=at==2 || (at>=5 && at<=7) || at==44 || at==72 || (at>=85 && at<91) ? n%4==1 : (at==3 || at==4) && fromEntry ? false : n%4==0;
                    // Bait passes the transform result as mathfAddVector's first operand and the target position second.
                    float in[3], out[3]; std::memcpy(in,refWeaponInputs[k].data(),12); std::memcpy(out,(at==48 || at==49 ? refWeaponBases : refWeaponOutputs)[k].data(),12);
                    if(known) require(out[0]==(rotated ? -in[1] : in[0]) && out[1]==(rotated ? in[0] : in[1]) && out[2]==in[2],"weapon independent transform differs");
                    std::array<uint32_t,3> sum;
                    for(unsigned i=0;i<3;++i) sum[i]=bozo::fadd(nullptr,refWeaponBases[k][i],refWeaponOutputs[k][i]);
                    if(at>=105) {
                        const uint32_t point=frame+(at==105 ? 0x80u : 0x90u);
                        require(a.pc==0x1dc018 && read(point)==sum[0] && read(point+4)==sum[1] && read(point+8)==sum[2],"LoopTank segment point differs");
                    } else if(at>=95) {
                        // Torches: the launcher's +620 sum, the line-test end point (and that address), or DispatchHit's point.
                        if(at==97 || at==101) require(a.pc==0x142d98 && (getRegU32(&a,5)&0x1fffffffu)==frame && !std::memcmp(ram+frame,refWeaponOutputs[k].data(),12),"torch hit point differs");
                        else if(at==98 || at==99 || at==102 || at==103) require(read(0x18620)==sum[0] && read(0x18624)==sum[1] && read(0x18628)==sum[2],"torch launch point differs");
                        else {
                            const uint32_t end=frame+(at<=96 ? 0x60u : 0x20u);
                            require(a.pc==0x1d0288 && (getRegU32(&a,5)&0x1fffffffu)==end && read(end)==sum[0] && read(end+4)==sum[1] && read(end+8)==sum[2],"torch line-test end differs");
                        }
                    } else if(at>=91) {
                        // Ferris wheel: the sum lands in the coordinate system's position, raised by 20 before hdHatTest. The
                        // other three sums stay in frame +0 as hdPntSphereTest's point.
                        if(at==91) {
                            require(a.pc==0x1cec90 && read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==bozo::fadd(nullptr,sum[2],0x41a00000u)
                                && (getRegU32(&a,4)&0x1fffffffu)==0x22010u,"Ferris wheel height-test position differs");
                        } else require(a.pc==0x1c4e70 && read(frame)==sum[0] && read(frame+4)==sum[1] && read(frame+8)==sum[2] && (getRegU32(&a,4)&0x1fffffffu)==frame,
                            "Ferris wheel/boss hit-test point differs");
                    } else if(at>=85) {
                        // RockHD and RicoHD: the sum is hdPntSphereTest's point, or the coordinate system's new position.
                        const uint32_t point=at==88 || at==90 ? 0x22010u : frame+(at>=87 ? 0x10u : 0u);
                        require(read(point)==sum[0] && read(point+4)==sum[1] && read(point+8)==sum[2],"weapon hit-test point differs");
                        if(at!=90) require(a.pc==(at==88 ? 0x170b70u : 0x1c4e70u) && (getRegU32(&a,at==88 ? 5 : 4)&0x1fffffffu)==point,"weapon hit-test consumer differs");
                    } else if(at>=78) {
                        if(at==78) require(read(0x2b010)==sum[0] && read(0x2b014)==sum[1] && read(0x2b018)==sum[2],"special held aim position differs");
                        else {
                            const uint32_t end=frame+(at<=80 ? 0x20u : 0x30u);
                            require(a.pc==0x1e74c0 && read(end)==sum[0] && read(end+4)==sum[1] && read(end+8)==sum[2],"special arc end differs");
                        }
                    } else if(at>=75) {
                        // The sweep point sums stay in frame +20; the last sum is the aimed object's position.
                        if(at==76) require(read(frame+0x20)==sum[0] && read(frame+0x24)==sum[1] && read(frame+0x28)==sum[2],"special sweep point differs");
                        if(at==77) require(read(0x2b010)==sum[0] && read(0x2b014)==sum[1] && read(0x2b018)==sum[2],"special aimed position differs");
                    } else if(at>=70) {
                        if(k+1==order.size())
                            require(a.pc==0x1b72f0 && read(0x18000+0x620)==sum[0] && read(0x18000+0x624)==sum[1] && read(0x18000+0x628)==sum[2],"special volley launch point differs");
                    } else if(at>=65) {
                        // Only the last transform's sum survives in the launcher's +620; type 120 also hands it to CreateStaticWeapon.
                        if(k+1==order.size()) {
                            require(read(0x18000+0x620)==sum[0] && read(0x18000+0x624)==sum[1] && read(0x18000+0x628)==sum[2],"special launch point differs");
                            require(a.pc==(at<=66 ? 0x1b72f0u : at<=68 ? 0x1b7590u : 0x1515b8u),"special weapon creation not reached");
                            if(at==68) require(!refWeaponTargets.empty() && refWeaponTargets.back()==sum,"special static weapon origin differs");
                        }
                    } else if(at>=63) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2] && a.pc==(at==63 ? 0x1c89e8u : 0x1cec90u),
                            "boss movement position differs");
                    } else if(at>=51) {
                        require(a.pc==0x1b7590 && !refWeaponTargets.empty() && refWeaponTargets.back()==sum && read(frame+0x10)==sum[0] && read(frame+0x14)==sum[1] && read(frame+0x18)==sum[2],
                            "boss shot origin differs");
                    } else if(at>=49) {
                        // FlatArcs points sit 0x20 apart from 687f20; ElectricArc's first segment end is at 687f10.
                        const uint32_t point=at==49 ? 0x687f20u+0x20u*unsigned(k) : 0x687f10u;
                        require(a.pc==0x1e22f0 && read(point)==sum[0] && read(point+4)==sum[1] && read(point+8)==sum[2],"particle arc point differs");
                    } else if(at==47) {
                        // Bead point plus offset, made relative to the vehicle (100,200,300) for atan2f.
                        const float vehicleAt[3]={100.f,200.f,300.f};
                        for(unsigned i=0;i<3;++i) { uint32_t v; std::memcpy(&v,&vehicleAt[i],4); require(read(frame+0x10+4*i)==bozo::fsub(nullptr,sum[i],v),"hunt heading vector differs"); }
                        require(a.pc==0x102c68,"hunt drive-to-attack boundary not reached");
                    } else if(at==48) {
                        require(read(frame+0x10)==sum[0] && read(frame+0x14)==sum[1] && read(frame+0x18)==sum[2],"bait point differs");
                        require(read(0x1a000+0x20c)==0 && read(0x1a000+0x164)==(0x2e000u|(getRegU32(&initial,29)&0xa0000000u)),"closest attractor bead differs");
                    } else if(at==43) {
                        require(a.pc==0x1515b8 && read(0x18000+0x620)==sum[0] && read(0x18000+0x624)==sum[1] && read(0x18000+0x628)==sum[2],"Zoomies launch position differs");
                    } else if(at==44) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"flying weapon position differs");
                    } else if(at>=45) {
                        require(read(0x22018)==sum[2],"attached weapon height differs"); // stored directly; X/Y are smoothed toward it
                    } else if(at>=38) {
                        require(a.pc==0x1515b8 && read(0x1c000+0x2760)==sum[0] && read(0x1c000+0x2764)==sum[1] && read(0x1c000+0x2768)==sum[2],"combo launch position differs");
                        if(at==39) require(read(0x1c000+0x2270)==1,"combo 0x10304 flag not published");
                        if(at==40 || at==42) require(read(0x1c000+0x2274)==1,"combo special flag not published");
                    } else if(at>=36) {
                        // Each axle's corrected tire midpoint stays in the owner's frame (+70 front pair, +80 rear pair).
                        const uint32_t point=frame+(at==36 ? 0x70u : 0x80u);
                        require(read(point)==sum[0] && read(point+4)==sum[1] && read(point+8)==sum[2],"tire-pair corrected point differs");
                    } else if(at>=34) {
                        require(a.pc==0x1d4ed8 && read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2] &&
                            read(0x1c000+0x2230)==(at==34 ? 0u : 1u),"machine-gun weapon position/barrel toggle differs");
                    } else if(at==33) {
                        // Position X/Y; hdHatTest receives it after the +5 probe lift, then the height is settled.
                        require(a.pc==0x10e0b8 && read(0x22010)==sum[0] && read(0x22014)==sum[1] && refWeaponTargets.size()==1 &&
                            refWeaponTargets[0][0]==sum[0] && refWeaponTargets[0][1]==sum[1],"walking pedestrian position differs");
                    } else if(at==32) {
                        // CreateStaticWeapon's origin (the last launcher capture; entries first capture the aim start).
                        require(a.pc==0x1b7590 && refWeaponTargets.size()==(fromEntry ? 2u : 1u) && refWeaponTargets.back()==sum,"helicopter gun origin differs");
                    } else if(at==26) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"falling jet position differs");
                        if(fromEntryPath(site,scenario,n)==0) require(a.pc==0x143c38,"jet crash boundary not reached");
                    } else if(at==31) {
                        const size_t effect=k-(order[0]==26 ? 1 : 0);
                        require(effect<refWeaponTargets.size() && refWeaponTargets[effect]==sum && a.pc==0x178228,"jet crash point/sound boundary differs");
                    } else if(at>=27) {
                        // Each later point is the effect's position: the k-th particleCreateFx call after the first transform.
                        const size_t effect=k-(order[0]==26 ? 1 : 0);
                        require(effect<refWeaponTargets.size() && refWeaponTargets[effect]==sum,"jet explosion point differs");
                    } else if(at==24) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==bozo::fsub(nullptr,sum[2],0x3f000000),
                            "sinking blimp position differs"); // then sinks by 0.5
                    } else if(at==25) {
                        require(a.pc==0x1e6610 && read(0x4fc200)==sum[0] && read(0x4fc204)==sum[1] && read(0x4fc208)==sum[2],"blimp explosion point differs");
                    } else if(at==21) {
                        const uint32_t tank=0x18000;
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1],"gas tank position differs");
                        if(scenario==2) require(a.pc==0x1e6610 && read(0x22018)==0 && read(tank+0x38)==0x3f400000,"gas tank bounce publication differs");
                        else require(read(0x22018)==sum[2],"gas tank height differs");
                        if(scenario==3) require(a.pc==0x1d4ff8 && read(tank+0xc)==1,"gas tank landing publication differs");
                    } else if(at>=22) {
                        // The burst point is the sum; its height is then randomized before the effect.
                        require(a.pc==0x1e6610 && read(frame+0x20)==sum[0] && read(frame+0x24)==sum[1],"gas tank burst point differs");
                    } else if(at>=19) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"flyer position differs");
                        require(a.pc==(at==19 ? 0x1782f0u : 0x1c89e8u),"flyer sound/grid boundary not reached");
                    } else if(at>=17) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"falling helicopter position differs");
                        if(at==18 && scenario==3) require(a.pc==0x10de38,"helicopter debris boundary not reached");
                    } else if(at==15) {
                        float z; std::memcpy(&z,&sum[2],4);
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2] && read(gp-0x312c)==(z>10.f ? 0u : 1u),
                            "Ferris wheel falling position/state publication differs");
                    } else if(at==16) {
                        const uint32_t slot=n%4, alias=getRegU32(&initial,29)&0xa0000000u;
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"smasher camera position differs");
                        require(read(0x18000+4*slot)==(0x1c000u|alias) && read(0x4ac0d0+4*slot)==5,"smasher camera slot/mode publication differs");
                    } else if(at>=8) {
                        // The aim point is consumed by the real mathfRotationFromPointToPoint toward the chosen car.
                        require(refWeaponTargets.size()==1 && refWeaponTargets[0]==sum,"launcher aim point differs");
                        if((at==8 || at==12) && fromEntry) require(read(0x3624bc)==1,"launcher target flag not published"); // set before the transform
                        if(at==11 || at==12) require(a.pc==0x17baf0 || a.pc==0x17bba0,"Cop SUV laser-sound boundary not reached");
                        if(at>=13) require(a.pc==0x1b7590 && (!fromEntry || read(0x18000+0xf0)==(at==13 ? 0u : 1u)),"environment gun fire/toggle publication differs");
                    } else if(at==6) {
                        require(refAttachmentActions[6]==1 && refWeaponTargets.size()==1 && refWeaponTargets[0][0]==sum[0] && refWeaponTargets[0][1]==sum[1],
                            "ricochet height-test position differs");
                    } else if(at==7) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"ricochet published position differs");
                    } else if(at<2) {
                        require(!refWeaponTargets.empty() && refWeaponTargets[0]==sum,"weapon lock target point differs");
                        if(site==0) require(refWeaponBases[0][0]==0x42c80000 && refWeaponBases[0][1]==0x43480000 && refWeaponBases[0][2]==0x43960000,"lock-on target position differs");
                    } else if(site<5) {
                        require(read(0x22010)==sum[0] && read(0x22014)==sum[1] && read(0x22018)==sum[2],"skill-satellite published position differs");
                    } else {
                        require(refAttachmentActions[6]==1 && refWeaponTargets.size()==1 && refWeaponTargets[0][0]==sum[0] && refWeaponTargets[0][1]==sum[1],
                            "rock height-test position differs");
                    }
                    ++weaponPublications[at];
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
            if (!cpuEqual(a,b) || !(kind>=236 ? weaponVuRetired(initial,a,b) : vuUntouched(expectedVu,b)) || after != refControl || services.yields != refYields || services.randomCalls != refRandomCalls ||
                services.weatherPoints != refWeatherPoints || services.weatherIdentities != refWeatherIdentities || services.spatialCalls != refSpatialCalls || auditedInverseCalls != refAuditedInverseCalls || services.aiActions != refAiActions || services.vehicleLightActions != refVehicleLightActions || services.finalActions != refFinalActions || services.rigidActions != refRigidActions || rigidDirectionCalls != refRigidDirections || driverDirectionCalls != refDriverDirections || services.driverActions != refDriverActions || services.driverQueryEndpoints != refDriverEndpoints || services.collisionActions != refCollisionActions || services.collisionVelocities != refCollisionVelocities || collisionDirectionCalls != refCollisionDirections || attachmentDirectionCalls != refAttachmentDirections || services.attachmentActions != refAttachmentActions || minionDirectionCalls != refMinionDirections || services.minionActions != refMinionActions || services.minionPoints != refMinionPoints || weaponDirectionCalls != refWeaponDirections || services.weaponInputs != refWeaponInputs || services.weaponBases != refWeaponBases || services.weaponOutputs != refWeaponOutputs || services.weaponTargets != refWeaponTargets || !dataEqual() ||
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
    require(std::all_of(collisionPublications.begin(),collisionPublications.end(),[](unsigned n) { return n!=0; }) && collisionErrnoCases,"collision fixtures missed publication/domain paths");
    std::printf("Collision forward publication cases: velocities %u force %u impact points %u morph locations %u; real errno cases %u\n",collisionPublications[0],collisionPublications[1],collisionPublications[2],collisionPublications[3],collisionErrnoCases);
    require(std::all_of(attachmentPublications.begin(),attachmentPublications.end(),[](unsigned n) { return n!=0; }) && gridRemovals,"attachment fixtures missed position/grid publication");
    std::printf("Attachment forward publication cases: helicopter %u cable %u gascan %u saw %u taxi %u; real grid removals %u\n",attachmentPublications[0],attachmentPublications[1],attachmentPublications[2],attachmentPublications[3],attachmentPublications[4],gridRemovals);
    require(std::all_of(minionPublications.begin(),minionPublications.end(),[](unsigned n) { return n!=0; }) &&
        std::all_of(minionDestructions.begin(),minionDestructions.end(),[](unsigned n) { return n!=0; }) && minionBoundaries,"Minion fixtures missed damage/destruction/distance boundaries");
    std::printf("Minion forward publication cases: damage %u/%u/%u/%u destruction %u/%u/%u/%u exact distance boundaries %u\n",minionPublications[0],minionPublications[1],minionPublications[2],minionPublications[3],minionDestructions[0],minionDestructions[1],minionDestructions[2],minionDestructions[3],minionBoundaries);
    require(std::all_of(weaponPublications.begin(),weaponPublications.end(),[](unsigned n) { return n!=0; }),"weapon fixtures missed transform publication");
    std::printf("Weapon forward publication cases: lock-on %u look-ahead %u skill-satellite %u/%u/%u rock %u ricochet %u/%u launcher aim hearse %u minion %u tanker %u cop SUV %u/%u environment gun %u/%u; Ferris wheel fall %u, smasher camera %u, falling env heli %u helicopter %u, junk jet %u blimp %u, gas tank roll %u bursts %u/%u, sinking blimp %u explosion %u, falling jet %u high %u low %u/%u crash %u/%u, helicopter gun %u, walking pedestrian %u, machine-gun muzzles %u/%u, tire-pair corrections %u/%u, combo launches %u/%u/%u/%u/%u, Zoomies %u, HalTrans weapon %u/%u/%u, AI hunt %u bait %u, flat arcs %u electric arc %u, boss shots %u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u, boss move %u death %u, specials 60 %u/%u 120 %u/%u c0 %u 1c0 %u/%u/%u 140 %u/%u 100 %u/%u/%u held %u, a0 arcs %u/%u/%u/%u/%u/%u, hit tests rock %u/%u rico %u/%u/%u/%u Ferris %u/%u/%u boss %u, torches tank car %u/%u/%u Minion %u/%u/%u/%u Tanker %u/%u/%u, LoopTank %u/%u\n",weaponPublications[0],weaponPublications[1],weaponPublications[2],weaponPublications[3],weaponPublications[4],weaponPublications[5],weaponPublications[6],weaponPublications[7],weaponPublications[8],weaponPublications[9],weaponPublications[10],weaponPublications[11],weaponPublications[12],weaponPublications[13],weaponPublications[14],weaponPublications[15],weaponPublications[16],weaponPublications[17],weaponPublications[18],weaponPublications[19],weaponPublications[20],weaponPublications[21],weaponPublications[22],weaponPublications[23],weaponPublications[24],weaponPublications[25],weaponPublications[26],weaponPublications[27],weaponPublications[28],weaponPublications[29],weaponPublications[30],weaponPublications[31],weaponPublications[32],weaponPublications[33],weaponPublications[34],weaponPublications[35],weaponPublications[36],weaponPublications[37],weaponPublications[38],weaponPublications[39],weaponPublications[40],weaponPublications[41],weaponPublications[42],weaponPublications[43],weaponPublications[44],weaponPublications[45],weaponPublications[46],weaponPublications[47],weaponPublications[48],weaponPublications[49],weaponPublications[50],weaponPublications[51],weaponPublications[52],weaponPublications[53],weaponPublications[54],weaponPublications[55],weaponPublications[56],weaponPublications[57],weaponPublications[58],weaponPublications[59],weaponPublications[60],weaponPublications[61],weaponPublications[62],weaponPublications[63],weaponPublications[64],weaponPublications[65],weaponPublications[66],weaponPublications[67],weaponPublications[68],weaponPublications[69],weaponPublications[70],weaponPublications[71],weaponPublications[72],weaponPublications[73],weaponPublications[74],weaponPublications[75],weaponPublications[76],weaponPublications[77],weaponPublications[78],weaponPublications[79],weaponPublications[80],weaponPublications[81],weaponPublications[82],weaponPublications[83],weaponPublications[84],weaponPublications[85],weaponPublications[86],weaponPublications[87],weaponPublications[88],weaponPublications[89],weaponPublications[90],weaponPublications[91],weaponPublications[92],weaponPublications[93],weaponPublications[94],weaponPublications[95],weaponPublications[96],weaponPublications[97],weaponPublications[98],weaponPublications[99],weaponPublications[100],weaponPublications[101],weaponPublications[102],weaponPublications[103],weaponPublications[104],weaponPublications[105],weaponPublications[106]);
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
