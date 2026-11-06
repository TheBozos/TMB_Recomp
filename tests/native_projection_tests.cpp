// SPDX-License-Identifier: MIT
#include "tmb_port.h"
#include "tmb_projection_math.h"
#include "tmb_vehicle_math.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::math { void installProjection(tmb::Registry &); void install(tmb::Registry &); }
namespace
{
    struct Host : bozo::ee::Host, bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bool native = true;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return native; }
        void frameEnd(Runtime *,R5900Context *) override { throw std::runtime_error("unexpected frame"); }
        bool callLibrary(Runtime *,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected program"); }
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected read"); }
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected write"); }
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
    };
    uint32_t seed=0xc1583a67;
    uint32_t nextBits() { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return seed; }
    constexpr uint32_t special[]={0,0x80000000,1,0x80000001,0x7fffff,0x807fffff,0x800000,0x80800000,
        0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fffffff,0xffffffff};
    constexpr size_t vuAt=offsetof(R5900Context,vu0_vf),vuBytes=offsetof(R5900Context,cycles)-vuAt;
    bool cpu(R5900Context expected,R5900Context actual,const R5900Context &initial)
    {
        if (std::memcmp((uint8_t*)&actual+vuAt,(uint8_t*)&initial+vuAt,vuBytes)) return false;
        std::memset((uint8_t*)&actual+vuAt,0,vuBytes); std::memset((uint8_t*)&expected+vuAt,0,vuBytes);
        return !std::memcmp(&actual,&expected,sizeof(actual));
    }
    void require(bool okay,const char *what) { if(!okay) throw std::runtime_error(what); }
    R5900Context context()
    {
        R5900Context c;
        for (unsigned i=1;i<32;++i) for(auto &v:c.r[i].uw) v=nextBits();
        for(auto &v:c.vu0_vf) for(auto &b:v.u) b=nextBits();
        c.vu0_vf[0]={}; c.vu0_vf[0].u[3]=0x3f800000;
        c.vu0_status=nextBits(); c.vu0_mac=nextBits(); c.vi[5]=nextBits();
        return c;
    }
}
int main()
{
    Host host; bozo::ee::Dispatcher d(host); auto &rt=d.runtime(); rt.port=&host;
    tmb::Registry registry; tmb::math::installProjection(registry);
    auto *ram=rt.memory.flat(); auto full=std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    const auto point=registry.find(0x1da238),identity=registry.find(0x1db0f0);
    const unsigned saved=_mm_getcsr(); uint64_t cases=0,memoryCases=0;
    uint8_t input[512],expected[512];
    for(unsigned mode=0;mode<4;++mode) for(unsigned kind=0;kind<3;++kind) for(unsigned n=0;n<65536;++n)
    {
        for(unsigned i=0;i<512;i+=4)
        {
            uint32_t bits=nextBits();
            if(n<16384)bits=(bits&0x807fffff)|((110+bits%30)<<23);
            else if(n<32768)bits=special[bits%std::size(special)];
            std::memcpy(input+i,&bits,4);
        }
        const uint32_t base=n%5==0?0x70000000:0x10000;
        const uint32_t flat=uint32_t(bozo::GuestMemory::flatOffset(base,512));
        std::memcpy(ram+flat,input,512);
        const uint32_t alias=base==0x10000?(n%3==0?0x80000000:n%3==1?0x20000000:0):0;
        const uint32_t output=base+32+(n%25)*16;
        auto initial=context();
        setRegU32(&initial,4,(base+128+((n>>4)&15))|alias);
        setRegU32(&initial,5,(base+256+((n>>8)&15))|alias);
        setRegU32(&initial,6,(output+(n&15))|alias);
        const uint32_t caller=kind==0?0x1dc438:kind==1?0x1e8cfc:0x1e8c48;
        setRegU32(&initial,31,caller); initial.pc=kind==2?0x1db0f0:0x1da238;
        if(kind==2)setRegU32(&initial,4,(output+(n&15))|alias);
        const unsigned control=((saved|0x1f80)&~0xe040u)|(mode<<13)|(n&1?0x8040:0);
        _mm_setcsr(control); auto original=initial,native=initial;
        const auto fn=kind==2?mathfUnitMatrixAsm__FPA3_f_0x1db0f0:mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238;
        fn(ram,&original,&rt); std::memcpy(expected,ram+flat,512);
        if(n<16){std::memcpy(full.get(),ram,bozo::kFlatBytes);++memoryCases;}
        std::memcpy(ram+flat,input,512); (kind==2?identity:point)(ram,&native,&rt);
        const unsigned after=_mm_getcsr(); _mm_setcsr(saved);
        if(std::memcmp(expected,ram+flat,512)||!cpu(original,native,initial)||after!=control||
           (n<16&&std::memcmp(full.get(),ram,bozo::kFlatBytes)))
        {
            std::fprintf(stderr,"FAIL projection kind%u case%u mode%u data%d CPU%d FP%d\n",kind,n,mode,
                std::memcmp(expected,ram+flat,512),cpu(original,native,initial),after==control);return 1;
        }
        ++cases;
    }
    // Unknown callers and the historical host must retain the complete original ABI, including VU state.
    for(unsigned kind=0;kind<2;++kind) for(unsigned n=0;n<1024;++n)
    {
        std::memcpy(ram+0x10000,input,512); auto initial=context();
        setRegU32(&initial,4,0x10080);setRegU32(&initial,5,0x10100);setRegU32(&initial,6,0x10020);
        setRegU32(&initial,31,0xbad0000); auto a=initial,b=initial;
        const auto fn=kind?mathfUnitMatrixAsm__FPA3_f_0x1db0f0:mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238;
        fn(ram,&a,&rt);std::memcpy(expected,ram+0x10000,512);std::memcpy(ram+0x10000,input,512);
        (kind?identity:point)(ram,&b,&rt);
        require(!std::memcmp(&a,&b,sizeof(a))&&!std::memcmp(expected,ram+0x10000,512),"unknown caller ABI");
        host.native=false;setRegU32(&initial,31,kind?0x1e8c48:0x1dc438);a=initial;b=initial;
        fn(ram,&a,&rt);std::memcpy(expected,ram+0x10000,512);
        (kind?identity:point)(ram,&b,&rt);host.native=true;
        require(!std::memcmp(&a,&b,sizeof(a))&&!std::memcmp(expected,ram+0x10000,512),"historical host ABI");
    }
    d.setFunctions(tmb_function_table,tmb_function_count,tmb_resume_table,tmb_resume_count);
    auto arenaInput=std::make_unique<uint8_t[]>(0x40000),arenaExpected=std::make_unique<uint8_t[]>(0x40000);
    unsigned callers=0;
    for(unsigned mode=0;mode<4;++mode) for(unsigned kind=0;kind<2;++kind) for(unsigned n=0;n<256;++n)
    {
        for(unsigned i=0;i<0x40000;i+=4)
        {
            const float f=float(int(nextBits()%2001)-1000)/128.f;std::memcpy(ram+0x10000+i,&f,4);
        }
        const unsigned view=n%4;
        for(unsigned i=0;i<64;i+=4)
        { const float f=float(int(nextBits()%2001)-1000)/128.f;std::memcpy(ram+0x4e0230+64*view+i,&f,4); }
        for(unsigned i=0;i<16;i+=4)
        { const float f=float(int(nextBits()%2001)-1000)/128.f;std::memcpy(ram+0x4a1240+0xf0*view+i,&f,4); }
        auto initial=context();setRegU32(&initial,28,0x40000);setRegU32(&initial,29,0x30000);setRegU32(&initial,31,0xbad0000);
        RecompiledFunction caller=mathfTransform3dTo2d__FiP8_fvectorT1_0x1dc3a8;
        if(kind==0)
        {
            setRegU32(&initial,4,view);setRegU32(&initial,5,n%8==0?0:0x11000);setRegU32(&initial,6,n%8==1?0:0x12000);
        }
        else
        {
            // Actual rain-viewport continuation from coordinate setup through projection and epilogue.
            initial.pc=0x1e8c50;caller=resume_0x1e87a0;
            setRegU32(&initial,5,0x11000);setRegU32(&initial,19,0x12000);
            const uint32_t out=0x14000;std::memcpy(ram+0x40000-0x2f48,&out,4);
            const uint64_t ret=0xbad0000;std::memcpy(ram+0x30000+0x120,&ret,8);
        }
        d.updateLimit(&initial); std::memcpy(arenaInput.get(),ram+0x10000,0x40000);
        const unsigned control=((saved|0x1f80)&~0xe040u)|(mode<<13)|(n&1?0x8040:0);
        _mm_setcsr(control); auto a=initial,b=initial;
        rt.functions.setOverride(0x1da238,mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238);
        rt.functions.setOverride(0x1db0f0,mathfUnitMatrixAsm__FPA3_f_0x1db0f0);
        try{caller(ram,&a,&rt);}catch(const bozo::ee::StopGuest&){require(a.pc==0xbad0000,"reference caller stopped early");}
        std::memcpy(arenaExpected.get(),ram+0x10000,0x40000);if(n<8)std::memcpy(full.get(),ram,bozo::kFlatBytes);
        std::memcpy(ram+0x10000,arenaInput.get(),0x40000);
        rt.functions.setOverride(0x1da238,point);rt.functions.setOverride(0x1db0f0,identity);
        try{caller(ram,&b,&rt);}catch(const bozo::ee::StopGuest&){require(b.pc==0xbad0000,"native caller stopped early");}
        const unsigned after=_mm_getcsr();_mm_setcsr(saved);
        if(!cpu(a,b,initial)||std::memcmp(arenaExpected.get(),ram+0x10000,0x40000)||after!=control||
           (n<8&&std::memcmp(full.get(),ram,bozo::kFlatBytes)))
        {std::fprintf(stderr,"FAIL caller kind%u case%u mode%u CPU%d\n",kind,n,mode,cpu(a,b,initial));return 2;}
        ++callers;
    }
    _mm_setcsr(saved);
    // Actual cross-call padding chain: projection previously left its W in VF16; the unaudited peeling-out
    // direction helper writes that old W into its output. The audited vehicle offset consumer uses XYZ only.
    tmb::Registry math; tmb::math::install(math); const auto direction=math.find(0x1da298);
    for (unsigned n=0;n<1024;++n)
    {
        const auto basis=tmb::math::identityMatrix();
        auto projection=basis;projection.column[3].bits[3]=0x40000000u;
        const tmb::math::Vector vector{{0x3f800000,0x40000000,0x40400000,0}},position{{0x40800000,0x40a00000,0x40c00000,0}};
        std::memcpy(ram+0x10100,&projection,64);std::memcpy(ram+0x10200,&basis,64);
        std::memcpy(ram+0x10300,&vector,16);std::memcpy(ram+0x13000,&position,16);
        auto initial=context();initial.vu0_vf[16].u[3]=0;
        const auto sequence=[&](bool native)
        {
            auto c=initial;
            setRegU32(&c,4,0x10100);setRegU32(&c,5,0x10300);setRegU32(&c,6,0x10400);setRegU32(&c,31,0x1dc438);
            (native?point:mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238)(ram,&c,&rt);
            setRegU32(&c,4,0x10200);setRegU32(&c,5,0x10300);setRegU32(&c,6,0x11520);setRegU32(&c,31,0x1abd64);
            direction(ram,&c,&rt); // the original helper ABI is deliberately retained at this caller
            setRegU32(&c,6,0x13000);setRegU32(&c,3,0x11520);setRegU32(&c,4,0x14000);
            require(tmb::math::vehicleInstruction(ram,&c,&rt,0x1ab2a8),"vehicle consumer routing");
        };
        std::memcpy(arenaInput.get(),ram+0x10000,0x40000);
        sequence(false);std::memcpy(arenaExpected.get(),ram+0x10000,0x40000);
        std::memcpy(ram+0x10000,arenaInput.get(),0x40000);sequence(true);
        uint32_t originalW,nativeW;std::memcpy(&originalW,arenaExpected.get()+0x152c,4);std::memcpy(&nativeW,ram+0x1152c,4);
        require(originalW==0x40000000u&&nativeW==0,"projection-to-direction W chain");
        std::memcpy(arenaExpected.get()+0x152c,&nativeW,4);
        require(!std::memcmp(arenaExpected.get(),ram+0x10000,0x40000),"padding affected another game record");
    }
    std::printf("native projection: %llu helper cases, %llu whole-memory helper checks, %u generated screen/weather callers (64 whole-memory), "
                "4096 fallback ABI cases, 1024 padding-chain cases; native helpers leave VU/host state untouched\n",
                (unsigned long long)cases,(unsigned long long)memoryCases,callers);
}
