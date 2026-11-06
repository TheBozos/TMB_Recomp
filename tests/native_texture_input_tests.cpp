// SPDX-License-Identifier: MIT
// Execute the actual texture consumer, allocator, activation, mip and tag builders with source-record inputs.
#include "tmb_port.h"
#include "tmb_texture_inputs.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <xmmintrin.h>

namespace tmb::graphics { void install(tmb::Registry &); }
namespace
{
    struct Host : bozo::ee::Host, bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::port::InstructionHandler handler{};
        bool batch=false,graphics=true;
        std::set<uint32_t> reached;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool nativeGraphics() const override { return graphics; }
        bool nativeInstruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc) override
        {
            if(!graphics)return false;
            if(tmb::textureInputs::owns(pc)){if(!batch)return false;reached.insert(pc);}
            return handler(ram,ctx,rt,pc);
        }
        void frameEnd(Runtime *,R5900Context *) override {throw std::runtime_error("unexpected frame");}
        bool callLibrary(Runtime *,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected library");}
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected library");}
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected program");}
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override {throw std::runtime_error("unexpected device read");}
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override {throw std::runtime_error("unexpected device write");}
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override {throw std::runtime_error(s);}
    };
    void require(bool okay,const char *what){if(!okay)throw std::runtime_error(what);}
    uint32_t seed=0x932f817a;
    uint32_t nextBits(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
    template<typename T>void put(uint8_t *ram,uint32_t at,T value){std::memcpy(ram+at,&value,sizeof(value));}
    uint32_t word(const uint8_t *ram,uint32_t at){uint32_t value;std::memcpy(&value,ram+at,4);return value;}
    constexpr uint32_t packet=0x30000,control=0x31000,stack=0x40000,gp=0x500000;
    constexpr size_t bytes=0x510000;
    constexpr uint32_t callers[]={0x1d330c,0x1d3340,0x1d2d84,0x1d2db8};
}
int main()
{
    Host host;bozo::ee::Dispatcher d(host);auto &rt=d.runtime();rt.port=&host;auto *ram=rt.memory.flat();
    tmb::Registry registry;tmb::graphics::install(registry);host.handler=registry.instructionHandler();
    auto input=std::make_unique<uint8_t[]>(bytes),expected=std::make_unique<uint8_t[]>(bytes);
    auto full=std::make_unique<uint8_t[]>(bozo::kFlatBytes);
    unsigned cases=0,memoryCases=0,poisonCases=0,fallbackCases=0;
    const unsigned saved=_mm_getcsr();
    for(unsigned mode=0;mode<4;++mode)for(unsigned n=0;n<2048;++n)
    {
        std::memset(ram,0,bytes);
        const uint32_t object=0x12000+4*((n/7)%4),index=(n/11)%4,offset=16*((n/13)%8);
        const uint32_t source=(object+0x70+48*index)&~15u;
        const uint32_t id=n%9==0?0:40+8*((n/17)%8),mips=(n/19)%8;
        const uint32_t formats[]={0,0x13,0x14,0x1b,0x24,0x2c,0x30};
        const uint64_t base=(uint64_t(nextBits())<<32)|(nextBits()&~uint32_t(0x3f00000))|uint64_t(formats[n%7])<<20;
        for(unsigned i=0;i<48;i+=4)put(ram,source+i,nextBits());
        put(ram,source,base);
        put(ram,0x4c0a78+offset,object);
        put(ram,control+8,0x32000u);
        put(ram,gp-0x32bc,0x26000u); // texture bin descriptors
        put(ram,gp-0x32b8,nextBits()); // usage serial
        put(ram,gp-0x32ac,nextBits()); // zero-texture base
        put(ram,gp-0x329c,n&1);       // mip filtering enabled
        put(ram,gp-0x32a0,nextBits());put(ram,gp-0x32a4,nextBits());put(ram,gp-0x3298,nextBits());
        for(unsigned bin=0;bin<4;++bin)
        {
            put(ram,0x26000+8*bin,int32_t((n/23)%5));
            for(unsigned slot=0;slot<4;++slot)
            {
                const uint32_t at=0x4d57b0+0x960*bin+8*slot;
                put(ram,at,n&2?nextBits()|1u:0u);
                put(ram,at+4,uint16_t(200+slot));put(ram,at+6,uint16_t(0x100+bin*64+slot*8));
                put(ram,0x4d1930+16*(200+slot)+4,0x80000000u);
            }
        }
        for(unsigned level=0;level<9;++level)
        {
            const uint32_t info=0x4d1930+16*(id+level),data=0x15000+0x100*level;
            const uint32_t bin=(n+level)%4;
            const uint32_t flags=((n>>(level%4))&1?0x80000000u:0u)|(level==0?mips<<25:0)|(bin<<20)|
                                 (((n+level)%64)<<14)|(0x100+level*8);
            put(ram,info,data);put(ram,info+4,flags);put(ram,info+8,0u);
            for(unsigned i=0;i<0x70;i+=4)put(ram,data+i,nextBits());
        }
        for(unsigned i=0;i<0x200;i+=4)put(ram,stack-0x180+i,nextBits());
        std::memset(ram+packet,0xa5,48);
        R5900Context initial;
        for(unsigned r=1;r<32;++r)for(auto &v:initial.r[r].uw)v=nextBits();
        for(auto &v:initial.fu)v=nextBits();
        for(auto &v:initial.vu0_vf)for(auto &b:v.u)b=nextBits();
        initial.vu0_vf[0]={};initial.vu0_vf[0].u[3]=0x3f800000;
        const uint32_t caller=callers[n%4];
        setRegU32(&initial,4,id);setRegU32(&initial,5,packet);setRegU32(&initial,6,control);
        setRegU32(&initial,16,index);setRegU32(&initial,caller<0x1d3000?19:18,offset);
        setRegU32(&initial,28,gp);setRegU32(&initial,29,stack);setRegU32(&initial,31,caller);
        d.updateLimit(&initial);std::memcpy(input.get(),ram,bytes);
        std::memcpy(ram+packet,ram+source,48); // Published producer, before retirement.
        auto baseline=initial,native=initial;host.batch=false;
        const unsigned csr=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1?0x8040u:0);
        _mm_setcsr(csr);texmGetTextureAddrs__FiP6QwDataP9Vu1DmaPkt_0x1f90b0(ram,&baseline,&rt);
        require(_mm_getcsr()==csr,"original changed host FP control");
        std::memcpy(expected.get(),ram,bytes);std::memcpy(expected.get()+packet,input.get()+packet,48);
        if(n%257==0){std::memcpy(full.get(),ram,bozo::kFlatBytes);std::memcpy(full.get()+packet,input.get()+packet,48);++memoryCases;}
        std::memcpy(ram,input.get(),bytes);host.batch=true;
        texmGetTextureAddrs__FiP6QwDataP9Vu1DmaPkt_0x1f90b0(ram,&native,&rt);
        const unsigned after=_mm_getcsr();_mm_setcsr(saved);
        if(std::memcmp(&baseline,&native,sizeof(native))||std::memcmp(expected.get(),ram,bytes)||after!=csr||
           (n%257==0&&std::memcmp(full.get(),ram,bozo::kFlatBytes)))
        {
            std::fprintf(stderr,"FAIL texture input mode%u n%u caller%x mips%u CPU%d memory%d\n",mode,n,caller,mips,
                std::memcmp(&baseline,&native,sizeof(native)),std::memcmp(expected.get(),ram,bytes));
            for(unsigned i=0;i<bytes;++i)if(expected[i]!=ram[i]){std::fprintf(stderr,"first byte%x expected%x actual%x\n",i,expected[i],ram[i]);break;}
            return 1;
        }
        ++cases;
        if(n%31==0)
        {
            std::memcpy(ram,input.get(),bytes);auto poisoned=initial;setRegU32(&poisoned,5,0x10000000);
            _mm_setcsr(csr);texmGetTextureAddrs__FiP6QwDataP9Vu1DmaPkt_0x1f90b0(ram,&poisoned,&rt);
            const unsigned poisonCsr=_mm_getcsr();_mm_setcsr(saved);
            require(!std::memcmp(ram+0x4c0000,expected.get()+0x4c0000,0x50000),"inaccessible output changed manager state");
            require(!std::memcmp(ram+0x15000,expected.get()+0x15000,0x1200),"inaccessible output changed loaded texture state");
            require(word(ram,control+8)==word(expected.get(),control+8),"inaccessible output changed upload cursor");
            require(!std::memcmp(ram+packet,input.get()+packet,48)&&poisonCsr==csr,"inaccessible output touched packet/host FP");
            ++poisonCases;
        }
        if(n%127==0)
        {
            // Unknown caller and historical host still execute every original load/patch.
            for(bool historical:{false,true})
            {
                host.graphics=!historical;
                std::memcpy(ram,input.get(),bytes);std::memcpy(ram+packet,ram+source,48);
                auto original=initial;setRegU32(&original,31,0x1d3afc);auto unchanged=original;
                host.batch=false;texmGetTextureAddrs__FiP6QwDataP9Vu1DmaPkt_0x1f90b0(ram,&original,&rt);
                std::memcpy(expected.get(),ram,bytes);
                std::memcpy(ram,input.get(),bytes);std::memcpy(ram+packet,ram+source,48);host.batch=true;
                texmGetTextureAddrs__FiP6QwDataP9Vu1DmaPkt_0x1f90b0(ram,&unchanged,&rt);
                require(!std::memcmp(&original,&unchanged,sizeof(original))&&!std::memcmp(expected.get(),ram,bytes),"fallback ABI changed");
                ++fallbackCases;
            }
            host.graphics=true;
        }
    }
    require(host.reached.size()==10,"not all ten texture read/patch sites reached");
    unsigned resumedCases=0;
    Host otherHost;bozo::ee::Dispatcher other(otherHost);other.runtime().port=&otherHost;
    constexpr uint32_t sites[]={0x1f90cc,0x1f9130,0x1f9158,0x1f91b4,0x1f93bc,0x1f93c4,0x1f93d4,0x1f9494,0x1f94a0,0x1f988c};
    // Reconstructed suspended contexts need only their actual saved stack and owning runtime's source data.
    // There is deliberately no begin/end binding or previously executed producer/consumer for these cases.
    for(Runtime *owner:{&rt,&other.runtime()})for(uint32_t caller:callers)for(unsigned alignment=0;alignment<4;++alignment)
    {
        auto *mem=owner->memory.flat();const uint32_t object=0x13000+4*alignment,offset=0x30,index=2;
        const uint32_t source=(object+0x70+48*index)&~15u,frame=stack-0x30;
        const uint64_t base=owner==&rt?0x81763399cafe1234ull:0x99762431dabc4567ull,filter=base^0xffaa006633225588ull;
        put(mem,0x4c0a78+offset,object);put(mem,source,base);put(mem,source+16,filter);
        put(mem,frame,index);put(mem,frame+0x20,uint64_t(caller));
        for(uint32_t pc:sites)
        {
            R5900Context resumed;const unsigned queue=caller<0x1d3000?19:18;
            uint32_t sp=frame;setRegU32(&resumed,queue,offset);
            if(pc>=0x1f93bc&&pc<=0x1f94a0)
            {
                sp-=0x60;put(mem,sp+0x50,uint64_t(0x1f9100));
                put(mem,sp+(queue==18?0x20:0x30),uint64_t(offset));setRegU32(&resumed,queue,0xdead0000);
            }
            else if(pc==0x1f988c)
            {
                sp-=0xb0;put(mem,sp+0xa0,uint64_t(0x1f9100));
                put(mem,sp+(queue==18?0x30:0x40),uint64_t(offset));setRegU32(&resumed,queue,0xbeef0000);
            }
            setRegU32(&resumed,29,sp);auto wanted=resumed;
            if(pc==0x1f90cc)setRegU64(&wanted,3,base);
            if(pc==0x1f9130)setRegU64(&wanted,5,filter);
            if(pc==0x1f93bc||pc==0x1f9494)setRegU64(&wanted,2,uint16_t(base));
            std::memcpy(input.get(),mem,bytes);
            require(tmb::textureInputs::instruction(mem,&resumed,owner,pc),"reconstructed context was not handled");
            require(!std::memcmp(&wanted,&resumed,sizeof(wanted))&&!std::memcmp(input.get(),mem,bytes),"resume/owner source contract changed");
            ++resumedCases;
        }
    }
    std::printf("native texture inputs: %u full-consumer cases, %u full-memory checks, %u inaccessible-output cases, %u fallback cases; "
                "%u reconstructed-context/owner cases; all ten sites reached, CPU/context/manager/source/cursors exact outside retired texture output\n",
                cases,memoryCases,poisonCases,fallbackCases,resumedCases);
}
