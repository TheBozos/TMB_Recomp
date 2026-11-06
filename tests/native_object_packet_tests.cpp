// SPDX-License-Identifier: MIT
// Execute the actual generated builder with the published native adaptations, then with packet retirement.
#include "tmb_port.h"
#include "tmb_object_packets.h"
#include "tmb_scene.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>

namespace tmb::graphics { void install(tmb::Registry &); }
namespace
{
    struct Host : bozo::ee::Host, bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::port::InstructionHandler handler{};
        bool batch=false, graphics=true;
        std::set<uint32_t> reached;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return graphics; }
        bool nativeInstruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc) override
        {
            if(!graphics)return false;
            if(tmb::packets::owns(pc)) { reached.insert(pc); if(!batch)return false; }
            return handler(ram,ctx,rt,pc);
        }
        void frameEnd(Runtime *,R5900Context *) override { throw std::runtime_error("unexpected frame"); }
        bool callLibrary(Runtime *,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected program"); }
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
    };
    uint32_t seed=0x371b6d29;
    uint32_t nextBits(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
    void require(bool okay,const char *what){if(!okay)throw std::runtime_error(what);}
    template<typename T> void put(uint8_t *ram,uint32_t address,T value)
    {const auto at=bozo::GuestMemory::flatOffset(address,sizeof(T));require(at>=0,"fixture address");std::memcpy(ram+at,&value,sizeof(value));}
    uint32_t word(uint8_t *ram,uint32_t address)
    {uint32_t v;std::memcpy(&v,ram+bozo::GuestMemory::flatOffset(address,4),4);return v;}
}
int main()
{
    Host host;bozo::ee::Dispatcher d(host);auto &rt=d.runtime();rt.port=&host;
    tmb::Registry registry;tmb::graphics::install(registry);host.handler=registry.instructionHandler();
    tmb::scene::Binding binding(&rt);auto *ram=rt.memory.flat();
    constexpr uint32_t control=0x11000,objectBase=0x12000,instance=0x13000,table=0x15000,car=0x16000;
    constexpr uint32_t chains=0x17000,extra=0x18000,packet=0x20000,scratch=0x70000000;
    constexpr size_t arenaBytes=0x18000,packetBytes=0x4000;
    const auto spr=bozo::GuestMemory::flatOffset(scratch,0x1000);
    auto input=std::make_unique<uint8_t[]>(arenaBytes),expected=std::make_unique<uint8_t[]>(arenaBytes);
    auto full=std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    std::array<uint8_t,0x1000> inputScratch{},expectedScratch{};
    const unsigned saved=_mm_getcsr();unsigned cases=0,poisonCases=0,memoryCases=0;
    for(unsigned mode=0;mode<4;++mode)for(unsigned n=0;n<2048;++n)
    {
        const uint32_t object=objectBase+4*((n/13+n/256)%4);
        std::memset(ram+0x10000,0,arenaBytes);std::memset(ram+spr,0,0x1000);std::memset(ram+packet,0xa5,packetBytes);
        for(unsigned i=0;i<0x220;i+=4)put(ram,scratch+i,nextBits());
        const uint16_t flags=uint16_t(nextBits());
        const bool inst=n&1,skinned=inst&&(n&2),hasCar=n&4,colour=n&8,fog=n&16,points=n&32,changed=n&64,additional=n&128;
        const uint16_t textures=(n/7)%5;
        put(ram,object+0xc,flags);put(ram,object+4,0x19000u);put(ram,object+0x5a,textures);
        put(ram,object+0x64,skinned?0x1a000u:0u);put(ram,object+0x68,n%4);
        put(ram,instance+4,table);put(ram,instance+0x18,chains);
        for(unsigned i=0;i<4;++i)put(ram,chains+4*i,0x1b000u+0x100*i);
        for(unsigned i=0;i<0x100;i+=4)put(ram,table-16+i,float(int(nextBits()%2001)-1000)/256.f);
        for(unsigned i=0;i<8;++i)put(ram,object+0x30+2*i,int16_t((n/5+i)%4==0?-1:int((n+3*i)%12)));
        for(unsigned i=0;i<4;++i)
        {
            put(ram,object+0x20+4*i,float(int(nextBits()%1001)-500)/128.f);
            put(ram,object+0x40+4*i,float(nextBits()%1001)/256.f);
            put(ram,car+0x80+4*i,float(nextBits()%1001)/512.f);
        }
        for(unsigned i=0;i<4*48;i+=4)put(ram,object+0x70+i,nextBits());
        for(unsigned i=0;i<64;i+=4)put(ram,extra+i,nextBits());
        put(ram,extra+0xc,uint16_t((n/11)%4));
        put(ram,control+8,packet);put(ram,scratch+0x38,hasCar?car:0u);
        put(ram,scratch+0x8c,colour?1u:0u);put(ram,scratch+0x90,fog?1u:0u);put(ram,scratch+0x9c,points?1u:0u);
        put(ram,scratch+0x94,packet);tmb::scene::get(&rt).lighting.enabled=(n&256)!=0;
        R5900Context initial;
        for(unsigned i=1;i<32;++i)for(auto &v:initial.r[i].uw)v=nextBits();
        for(auto &v:initial.vu0_vf)for(auto &b:v.u)b=nextBits();
        initial.vu0_vf[0]={};initial.vu0_vf[0].u[3]=0x3f800000;
        initial.vu0_status=nextBits();initial.vu0_mac=nextBits();initial.vi[14]=nextBits();
        initial.f[20]=.75f;initial.f[21]=n&512?.5f:1.f;initial.f[22]=.25f;
        setRegU32(&initial,4,control);setRegU32(&initial,7,changed);setRegU32(&initial,16,scratch);
        setRegU32(&initial,20,additional?extra:0u);setRegU32(&initial,21,object);setRegU32(&initial,22,n%3);
        setRegU32(&initial,23,inst?instance:0u);setRegU32(&initial,30,flags);setRegU32(&initial,31,n&1024?0x1d55b0:0x1d56d0);
        d.updateLimit(&initial);std::memcpy(input.get(),ram+0x10000,arenaBytes);std::memcpy(inputScratch.data(),ram+spr,0x1000);
        const unsigned csr=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1?0x8040u:0);
        _mm_setcsr(csr);auto baseline=initial,native=initial;host.batch=false;
        pktAddVu1ObjAsm_0x1eaae8(ram,&baseline,&rt);
        const uint32_t cursor=word(ram,control+8),cache=word(ram,scratch+0x94);
        std::memcpy(expected.get(),ram+0x10000,arenaBytes);std::memcpy(expectedScratch.data(),ram+spr,0x1000);
        require(std::memcmp(ram+packet,input.get()+packet-0x10000,packetBytes)!=0,"reference did not emit a packet");
        // Retain the three texture payload copies: the later texture activation helper still consumes them.
        // Every other packet byte stays untouched; cursor/cache metadata, CPU/FPU state and all other bytes match.
        const uint32_t textureStart=textures?cache-packet:0,textureBytes=48*textures;
        for(unsigned i=0;i<packetBytes;++i)if(i<textureStart||i>=textureStart+textureBytes)
            expected[packet-0x10000+i]=input[packet-0x10000+i];
        if(n%257==0){std::memcpy(full.get(),ram,bozo::kFlatBytes);std::memcpy(full.get()+packet,expected.get()+packet-0x10000,packetBytes);++memoryCases;}
        std::memcpy(ram+0x10000,input.get(),arenaBytes);std::memcpy(ram+spr,inputScratch.data(),0x1000);host.batch=true;
        pktAddVu1ObjAsm_0x1eaae8(ram,&native,&rt);
        const unsigned after=_mm_getcsr();_mm_setcsr(saved);
        if(std::memcmp(&baseline,&native,sizeof(native))||std::memcmp(expected.get(),ram+0x10000,arenaBytes)||
           std::memcmp(expectedScratch.data(),ram+spr,0x1000)||after!=csr||(n%257==0&&std::memcmp(full.get(),ram,bozo::kFlatBytes)))
        {
            std::fprintf(stderr,"FAIL object packet mode%u case%u flags%x CPU%d RAM%d scratch%d FP%d\n",mode,n,flags,
                std::memcmp(&baseline,&native,sizeof(native)),std::memcmp(expected.get(),ram+0x10000,arenaBytes),
                std::memcmp(expectedScratch.data(),ram+spr,0x1000),after==csr);
            for(unsigned at=0;at<arenaBytes;++at)if(expected[at]!=ram[0x10000+at]){std::fprintf(stderr,"first byte %x expected%x actual%x\n",0x10000+at,expected[at],ram[0x10000+at]);break;}
            return 1;
        }
        ++cases;
        if(!textures&&(n%8==0 || (hasCar&&colour&&!(flags&0x10)&&n%7==0)))
        {
            // An inaccessible output pointer proves neither a retired store nor tint readback can reach a device.
            constexpr uint32_t poison=0x10000000;
            std::memcpy(ram+0x10000,input.get(),arenaBytes);std::memcpy(ram+spr,inputScratch.data(),0x1000);
            put(ram,control+8,poison);auto poisoned=initial;_mm_setcsr(csr);
            pktAddVu1ObjAsm_0x1eaae8(ram,&poisoned,&rt);const unsigned poisonCsr=_mm_getcsr();_mm_setcsr(saved);
            require(word(ram,control+8)-poison==cursor-packet,"poison cursor extent changed");
            require(word(ram,scratch+0x94)==cache,"poison cache changed without textures");
            require(!std::memcmp(poisoned.fu,baseline.fu,sizeof(baseline.fu))&&poisoned.fcr31==baseline.fcr31&&poisonCsr==csr,
                    "poison pointer changed tint/FPU results");
            require(!std::memcmp(ram+packet,input.get()+packet-0x10000,packetBytes),"poison fixture changed packet storage");
            ++poisonCases;
        }
    }
    require(host.reached.size()==41,"not all 38 stores and three tint reads were exercised");
    require(!tmb::packets::owns(0x1eabd0)&&!tmb::packets::owns(0x1eabd4)&&!tmb::packets::owns(0x1eabd8),
            "texture producer retired before its consumer");
    host.graphics=false;R5900Context c;
    require(!tmb::packets::instruction(ram,&c,&rt,0x1eaafc),"historical graphics guard");
    require(!tmb::packets::instruction(ram,&c,&rt,0x1ead74),"historical tint guard");
    std::printf("native object packets: %u full-builder comparisons, %u full-memory checks outside retired output, %u inaccessible-output cases; "
                "all 41 sites reached, CPU/FPU/cursor/cache exact; shader output untouched, texture copies retained\n",cases,memoryCases,poisonCases);
}
