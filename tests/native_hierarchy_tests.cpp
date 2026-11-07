// SPDX-License-Identifier: MIT
// Differential checks against complete generated helpers, including the original VCALLMS library path.
#include "tmb_port.h"
#include "tmb_hierarchy_math.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::hierarchy { void install(Registry &); }
void testHierarchyHelperCallers();
namespace
{
    struct Host : bozo::ee::Host, bozo::port::Host
    {
        bozo::libs::Libraries libs;
        uint64_t microCalls{};
        bool native = true;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return native; }
        void frameEnd(Runtime *, R5900Context *) override { throw std::runtime_error("unexpected frame"); }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &d, R5900Context *ctx, uint32_t pc) override
        {
            if (pc != 0xab0) throw std::runtime_error("unexpected program");
            ++microCalls;
            bozo::libs::Guest guest(d.runtime(), ctx);
            libs.vu0Program(pc)->handler(libs, guest);
        }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    uint32_t seed = 0x846a32b9;
    uint32_t randomBits() { seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5; return seed; }
    constexpr uint32_t special[] = {0,0x80000000,1,0x80000001,0x7fffff,0x807fffff,0x800000,0x80800000,
        0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fffffff,0xffffffff};
    constexpr size_t vuAt = offsetof(R5900Context, vu0_vf), vuBytes = offsetof(R5900Context, cycles) - vuAt;
    bool compareCpu(R5900Context original, R5900Context native, const R5900Context &initial)
    {
        if (std::memcmp((uint8_t *)&native + vuAt, (const uint8_t *)&initial + vuAt, vuBytes)) return false;
        std::memset((uint8_t *)&original + vuAt, 0, vuBytes);
        std::memset((uint8_t *)&native + vuAt, 0, vuBytes);
        return !std::memcmp(&original, &native, sizeof(native));
    }
    void referencePlane(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        const auto at=bozo::GuestMemory::flatOffset(getRegU32(ctx,7)&~15u,16);
        uint8_t saved[8];std::memcpy(saved,ram+at+8,8);
        bspPlaneTest2Vec__FP6_planeP8_fvectorT1T1_0x201660(ram,ctx,rt);
        std::memcpy(ram+at+8,saved,8); // only XY belongs to the BSP caller's distance record
    }
}
int main()
{
    Host host;
    bozo::ee::Dispatcher d(host);
    host.libs.attach(d);
    host.libs.vu0.image = bozo::libs::Vu0Image::Hierarchy;
    auto &rt = d.runtime(); rt.port = &host;
    tmb::Registry registry; tmb::hierarchy::install(registry);
    auto *ram = rt.memory.flat();
    const auto sphere = registry.find(0x1cf7c0), inverse = registry.find(0x1da1e8), plane = registry.find(0x201660);
    const unsigned saved = _mm_getcsr();
    std::array<uint8_t,512> input{}, expected{};
    auto full = std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    uint64_t cases{}, memoryCases{}, nativeMicros{};
    for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned mode = 0; mode < 4; ++mode)
            for (unsigned n = 0; n < 65536; ++n)
            {
                for (unsigned i = 0; i < input.size(); i+=4)
                {
                    auto bits = randomBits();
                    if (n < 16384) bits = (bits & 0x807fffffu) | ((110u + bits%30u)<<23);
                    else if (n < 32768) bits = special[bits%std::size(special)];
                    // Collision positions/radii are finite game records. IEEE NaN payload/sign propagation in the
                    // retired microprogram is not a geometric contract. Inverse transforms retain arbitrary bits.
                    if (!kind && (bits & 0x7f800000u) == 0x7f800000u) bits &= ~0x00800000u;
                    std::memcpy(input.data()+i, &bits, 4);
                }
                const uint32_t base = n%7 == 0 ? bozo::kScratchpadBase : 0x10000;
                const uint32_t alias = base == 0x10000 ? (n%3 == 0 ? 0x80000000 : n%3 == 1 ? 0x20000000 : 0) : 0;
                auto *arena = ram + bozo::GuestMemory::flatOffset(base, input.size());
                std::memcpy(arena,input.data(),input.size());
                R5900Context initial;
                for (unsigned i = 1; i < 32; ++i) for (auto &v : initial.r[i].uw) v = randomBits();
                for (auto &v : initial.vu0_vf) for (auto &b : v.u) b = randomBits();
                initial.vu0_vf[0] = {}; initial.vu0_vf[0].u[3] = 0x3f800000;
                for (auto &v : initial.vu0_acc.u) v = randomBits();
                for (auto &v : initial.vi) v = randomBits(); initial.vi[0] = 0;
                initial.fcr31 = randomBits(); initial.fu[0] = randomBits(); initial.fu[12] = randomBits();
                initial.vu0_q_bits = randomBits(); initial.vu0_i_bits = randomBits();
                initial.vu0_mac = randomBits(); initial.vu0_status = randomBits(); initial.vu0_clip_flags = randomBits();
                initial.cycles = randomBits(); initial.checkpoints = randomBits();
                setRegU32(&initial,4,(base+128+alias)+(n&15));
                setRegU32(&initial,5,(base+128+16*(n%10)+alias)+((n>>4)&15));
                setRegU32(&initial,6,(base+64+16*(n%20)+alias)+((n>>8)&15));
                setRegU32(&initial,7,(base+64+16*(n%19)+alias)+((n>>9)&15));
                setRegU32(&initial,28,base+0x4854); // threshold is first word of input arena
                setRegU32(&initial,31,kind == 2 ? 0x20134c : kind ? tmb::hierarchy::inverseCallers[n%std::size(tmb::hierarchy::inverseCallers)] : 0xbad0000);
                initial.pc = kind == 2 ? 0x201660 : kind ? 0x1da1e8 : 0x1cf7c0;
                d.updateLimit(&initial); // the dispatcher supplies this scheduler cache before executing a helper
                R5900Context original=initial,native=initial;
                const unsigned control = ((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1 ? 0x8040u : 0);
                _mm_setcsr(control);
                if (kind == 2) bspPlaneTest2Vec__FP6_planeP8_fvectorT1T1_0x201660(ram,&original,&rt);
                else if (kind) mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram,&original,&rt);
                else hdHatSphereTestAsm__FP8_fvectorfT0T0_0x1cf7c0(ram,&original,&rt);
                if (kind == 2) {
                    const auto outOffset=(getRegU32(&initial,7)&~15u)-(base|alias);
                    std::memcpy(arena+outOffset+8,input.data()+outOffset+8,8);
                }
                std::memcpy(expected.data(),arena,input.size());
                if (n<32) { std::memcpy(full.get(),ram,bozo::kFlatBytes); ++memoryCases; }
                std::memcpy(arena,input.data(),input.size());
                const auto before=host.microCalls;
                (kind == 2 ? plane : kind ? inverse : sphere)(ram,&native,&rt);
                nativeMicros += host.microCalls-before;
                const unsigned after=_mm_getcsr(); _mm_setcsr(saved);
                if (!compareCpu(original,native,initial) || std::memcmp(arena,expected.data(),input.size()) || after!=control ||
                    (n<32 && std::memcmp(full.get(),ram,bozo::kFlatBytes)))
                {
                    if(!kind) {
                        for(unsigned g:{4u,5u,6u}) {const auto off=bozo::GuestMemory::flatOffset(getRegU32(&initial,g)&~15u,16);auto*v=(uint32_t*)(ram+off);std::fprintf(stderr,"arg%u %08x %08x %08x %08x\n",g,v[0],v[1],v[2],v[3]);}
                        for(unsigned v:{14u,15u,17u,18u,20u,21u,22u,23u,24u,27u})std::fprintf(stderr,"vf%u %08x %08x %08x %08x\n",v,original.vu0_vf[v].u[0],original.vu0_vf[v].u[1],original.vu0_vf[v].u[2],original.vu0_vf[v].u[3]);
                    }
                    std::fprintf(stderr,"FAIL kind %u mode %u case %u: CPU %d data %d host %d return %x/%x\n",kind,mode,n,
                        compareCpu(original,native,initial),std::memcmp(arena,expected.data(),input.size()),after==control,
                        getRegU32(&native,2),getRegU32(&original,2));
                    for (size_t i=0;i<sizeof(native);++i) if ((i<vuAt || i>=vuAt+vuBytes) && ((uint8_t *)&original)[i]!=((uint8_t *)&native)[i])
                        std::fprintf(stderr,"CPU byte %zu original %02x native %02x\n",i,((uint8_t *)&original)[i],((uint8_t *)&native)[i]);
                    return 1;
                }
                ++cases;
            }
    // Exact boundaries and degenerate segments, with both denormal controls. Compare the actual wrapper so the
    // scalar bypass cannot hide a geometric test. The radius-zero cases also exercise the reciprocal-zero branch.
    constexpr float edge[] = {-2.f,-1.00000011920928955078125f,-1.f,-0.999999940395355224609375f,
        -0.f,0.f,0.999999940395355224609375f,1.f,1.00000011920928955078125f,2.f};
    unsigned edges=0;
    for (unsigned mode=0;mode<4;++mode)
        for (unsigned n=0;n<2000;++n)
        {
            float start[4]={edge[n%10],edge[(n/10)%10],0.f,n%3==0 ? 0.f : 1.f};
            float centre[4]={0,0,0,0},end[4]={edge[(n/100)%10],0.f,edge[(n/1000)%2],0};
            std::memcpy(ram+0x10080,start,16);std::memcpy(ram+0x10100,centre,16);std::memcpy(ram+0x10140,end,16);
            const uint32_t threshold=0x7f7fffff;std::memcpy(ram+0x10000,&threshold,4);
            R5900Context initial;
            setRegU32(&initial,4,0x10080);setRegU32(&initial,5,0x10100);setRegU32(&initial,6,0x10140);
            setRegU32(&initial,28,0x14854);setRegU32(&initial,31,0xbad0000);d.updateLimit(&initial);
            auto original=initial,native=initial;
            const unsigned control=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1 ? 0x8040u : 0);
            _mm_setcsr(control);
            hdHatSphereTestAsm__FP8_fvectorfT0T0_0x1cf7c0(ram,&original,&rt);
            const auto before=host.microCalls;sphere(ram,&native,&rt);
            const auto after=_mm_getcsr();_mm_setcsr(saved);
            if (!compareCpu(original,native,initial) || after!=control || host.microCalls!=before) {
                std::fprintf(stderr,"FAIL sphere edge %u mode %u return %x/%x\n",n,mode,getRegU32(&native,2),getRegU32(&original,2));return 5;
            }
            ++edges;
        }
    // Full BSP callers: actual recursive branch order, FPU scalar interpolation, classification writes, saved GPRs
    // and checkpoints. Reference writes use the same audited XY/preserved-ZW output contract.
    d.setFunctions(tmb_function_table,tmb_function_count,tmb_resume_table,tmb_resume_count);
    constexpr size_t callerBytes=0x40000;
    auto callerInput=std::make_unique<uint8_t[]>(callerBytes),callerExpected=std::make_unique<uint8_t[]>(callerBytes);
    unsigned callers=0;
    for(unsigned mode=0;mode<4;++mode) for(unsigned n=0;n<256;++n)
    {
        std::memset(ram+0x10000,0,callerBytes);
        for(unsigned k=0;k<3;++k) {
            const uint32_t node=0x12000+64*k;
            const uint32_t child=k==0 && n%2 ? node+64 : 0;std::memcpy(ram+node,&child,4);
            const uint32_t back=k<2 && n%3 ? node+64 : 0;std::memcpy(ram+node+4,&back,4);
            const uint16_t frontClass=n&1 ? 0x200 : 1,backClass=n&2 ? 0x200 : 2;
            std::memcpy(ram+node+12,&frontClass,2);std::memcpy(ram+node+14,&backClass,2);
            const float planeRecord[4]={k==0 ? 1.f:0.f,k==1 ? 1.f:0.f,k==2 ? 1.f:0.f,float(int(n%9)-4)/8};
            std::memcpy(ram+node+16,planeRecord,16);
        }
        const float first[4]={float(int(n%13)-6)/4,float(int(n%11)-5)/3,float(int(n%7)-3)/2,1};
        const float last[4]={-first[0],-first[1],-first[2],1};
        std::memcpy(ram+0x11000,first,16);std::memcpy(ram+0x11020,last,16);
        constexpr uint32_t gp=0x40000,result=0x18000;std::memcpy(ram+gp-0x31b8,&result,4);
        constexpr float epsilon=0.001f,negativeEpsilon=-epsilon;
        std::memcpy(ram+gp-0x45ec,&epsilon,4);std::memcpy(ram+gp-0x45e8,&negativeEpsilon,4);
        std::memcpy(ram+gp-0x45e4,&epsilon,4);std::memcpy(ram+gp-0x45e0,&negativeEpsilon,4);
        std::memcpy(callerInput.get(),ram+0x10000,callerBytes);
        R5900Context initial;
        setRegU32(&initial,4,0x12000);setRegU32(&initial,5,0x11000);setRegU32(&initial,6,0x11020);
        setRegU32(&initial,28,gp);setRegU32(&initial,29,0x30000);setRegU32(&initial,31,0xbad0000);
        for(auto &v:initial.vu0_vf)for(auto &b:v.u)b=randomBits();initial.vu0_vf[0]={};initial.vu0_vf[0].u[3]=0x3f800000;
        d.updateLimit(&initial);auto original=initial,native=initial;
        const unsigned control=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1 ? 0x8040u : 0);
        _mm_setcsr(control);
        rt.functions.setOverride(0x201660,&referencePlane);
        try { bspVecTestIn__FP8_hierbspP8_fvectorT1_0x201318(ram,&original,&rt); }
        catch(const bozo::ee::StopGuest &) { if(original.pc!=0xbad0000)throw; } // recursive root return to dispatcher
        std::memcpy(callerExpected.get(),ram+0x10000,callerBytes);
        if(n<8)std::memcpy(full.get(),ram,bozo::kFlatBytes);
        std::memcpy(ram+0x10000,callerInput.get(),callerBytes);
        rt.functions.setOverride(0x201660,plane);
        try { bspVecTestIn__FP8_hierbspP8_fvectorT1_0x201318(ram,&native,&rt); }
        catch(const bozo::ee::StopGuest &) { if(native.pc!=0xbad0000)throw; }
        const unsigned after=_mm_getcsr();_mm_setcsr(saved);
        if(!compareCpu(original,native,initial)||std::memcmp(callerExpected.get(),ram+0x10000,callerBytes)||after!=control||
            (n<8&&std::memcmp(full.get(),ram,bozo::kFlatBytes))||native.pc!=0xbad0000) {
            std::fprintf(stderr,"FAIL complete BSP caller %u mode %u CPU %d data %d pc %x\n",n,mode,compareCpu(original,native,initial),std::memcmp(callerExpected.get(),ram+0x10000,callerBytes),native.pc);return 6;
        }
        ++callers;
    }
    // A shortcut must avoid all three invalid vector pointers. Other inverse callers preserve the complete old ABI.
    for (unsigned n=0;n<1024;++n)
    {
        R5900Context initial;
        for (auto &v:initial.vu0_vf) for(auto &b:v.u)b=randomBits();
        initial.vu0_vf[0]={};initial.vu0_vf[0].u[3]=0x3f800000;
        setRegU32(&initial,4,0x10080);setRegU32(&initial,5,0x10100);setRegU32(&initial,6,0x10040);setRegU32(&initial,31,0xbad0000);
        R5900Context original=initial,native=initial;
        mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram,&original,&rt);
        std::memcpy(expected.data(),ram+0x10000,512);
        inverse(ram,&native,&rt);
        if(std::memcmp(&original,&native,sizeof(native)) || std::memcmp(expected.data(),ram+0x10000,512))return 2;
        setRegU32(&initial,7,0x10180);
        original=initial;native=initial;
        bspPlaneTest2Vec__FP6_planeP8_fvectorT1T1_0x201660(ram,&original,&rt);
        std::memcpy(expected.data(),ram+0x10000,512);
        plane(ram,&native,&rt);
        if(std::memcmp(&original,&native,sizeof(native)) || std::memcmp(expected.data(),ram+0x10000,512))return 7;
    }
    R5900Context shortcut;
    setRegU32(&shortcut,28,0x14854);setRegU32(&shortcut,31,0xbad0000);
    setRegU32(&shortcut,4,0x12000000);setRegU32(&shortcut,5,0x10000000);setRegU32(&shortcut,6,0x11000000);
    std::memset(ram+0x10000,0,4);shortcut.fu[12]=0x3f800000;
    sphere(ram,&shortcut,&rt);
    if(getRegU32(&shortcut,2)!=1 || nativeMicros)return 3;
    host.native=false;
    unsigned historicalInverse=0;
    for (const uint32_t caller:tmb::hierarchy::inverseCallers) for (unsigned n=0;n<64;++n)
    {
        for(unsigned i=0;i<512;i+=4) {const auto v=randomBits();std::memcpy(ram+0x10000+i,&v,4);}
        R5900Context initial;
        for(unsigned i=1;i<32;++i) for(auto &v:initial.r[i].uw)v=randomBits();
        for(auto &v:initial.vu0_vf)for(auto &b:v.u)b=randomBits();
        initial.vu0_vf[0]={};initial.vu0_vf[0].u[3]=0x3f800000;
        for(auto &v:initial.vu0_acc.u)v=randomBits();
        initial.vu0_status=randomBits();initial.vu0_mac=randomBits();initial.vi[5]=randomBits();
        initial.cycles=randomBits();initial.checkpoints=randomBits();
        const uint32_t alias=n%3==0?0x80000000:n%3==1?0x20000000:0;
        setRegU32(&initial,4,(0x10080|alias)+(n&15));setRegU32(&initial,5,(0x10100|alias)+((n>>4)&15));
        setRegU32(&initial,6,(0x10040|alias)+(n&15));setRegU32(&initial,31,caller);
        auto a=initial,b=initial;
        mathfMulVec__FPA3_fP8_fvectorT1_0x1da1e8(ram,&a,&rt);
        std::memcpy(expected.data(),ram+0x10000,512);
        if(!n)std::memcpy(full.get(),ram,bozo::kFlatBytes);
        inverse(ram,&b,&rt);
        if(std::memcmp(&a,&b,sizeof(a))||std::memcmp(expected.data(),ram+0x10000,512)||
           (!n&&std::memcmp(full.get(),ram,bozo::kFlatBytes)))return 8;
        ++historicalInverse;
    }
    std::memset(ram+0x10000,0,4); // restore the shortcut threshold after the inverse ABI arena
    auto original=shortcut,native=shortcut;
    hdHatSphereTestAsm__FP8_fvectorfT0T0_0x1cf7c0(ram,&original,&rt);sphere(ram,&native,&rt);
    if(std::memcmp(&original,&native,sizeof(native)))return 4;
    testHierarchyHelperCallers();
    std::printf("native hierarchy: %llu helper cases, %llu whole-memory cases, %u geometric edges, %u complete BSP callers (32 whole-memory), 2048 other-caller ABI cases; %u historical inverse ABI cases (%zu whole-memory); no native micro calls; CPU/VU/host checks pass\n",
        (unsigned long long)cases,(unsigned long long)memoryCases,edges,callers,historicalInverse,std::size(tmb::hierarchy::inverseCallers));
}
