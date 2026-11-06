// SPDX-License-Identifier: MIT
// Native records compared with the existing hierarchy programs, using only synthetic records.
#include "tmb_port.h"
#include "tmb_scene.h"
#include "bozo/libs/libraries.h"
#include "bozo/libs/guest.h"
#include "tmb_vu1_shading.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace tmb::graphics { void install(tmb::Registry &); }

namespace
{
    struct NativeHost : bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bool enabled=true;
        bozo::port::InstructionHandler handler=nullptr;
        bozo::libs::Libraries &libraries() override{return libs;}
        bool nativeGraphics()const override{return enabled;}
        bool nativeInstruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc)override{return handler(ram,ctx,rt,pc);}
        void frameEnd(Runtime *,R5900Context *)override{throw std::runtime_error("unexpected frame end");}
        bool callLibrary(Runtime *,R5900Context *,uint32_t)override{throw std::runtime_error("unexpected library");}
    };
    uint32_t seed=0xa32c7819;
    uint32_t randomBits(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
    tmb::scene::Vector randomVector()
    {
        tmb::scene::Vector v;
        for(auto &b:v.bits)b=(randomBits()&0x807fffffu)|((110u+randomBits()%25u)<<23);
        return v;
    }
    tmb::scene::Basis randomBasis(){return {randomVector(),randomVector(),randomVector()};}
}
int main()
{
    using namespace tmb::scene;
    auto rt=std::make_unique<Runtime>();
    bozo::libs::Libraries libs;libs.vu0.image=bozo::libs::Vu0Image::Hierarchy;
    R5900Context context;
    bozo::libs::Guest guest(*rt,&context);
    auto put=[&](unsigned i,const Vector &v){guest.write(bozo::kVu0DataBase+i*16,v.bits.data(),16);};
    auto fetch=[&](unsigned i){Vector v;std::memcpy(v.bits.data(),guest.in(bozo::kVu0DataBase+i*16,16),16);return v;};
    auto vf=[&](unsigned i,const Vector &v){std::memcpy(context.vu0_vf[i].u,v.bits.data(),16);};
    auto call=[&](unsigned pc){libs.vu0Program(pc)->handler(libs,guest);};
    auto frameEqual=[&](const Frame &f,unsigned depth){
        const Basis *bases[]={&f.outer,&f.inner,&f.directions,&f.rotation};
        for(unsigned g=0;g<4;++g)for(unsigned c=0;c<3;++c){const auto v=fetch(50+12*depth+g*3+c);if(std::memcmp(&v,&(*bases[g])[c],16))return false;}return true;
    };
    uint64_t cases=0;
    const unsigned saved=_mm_getcsr();
    for(unsigned mode=0;mode<4;++mode)for(unsigned n=0;n<8192;++n)
    {
        Scene scene;
        scene.viewPosition=randomVector();scene.viewOuter=randomBasis();scene.viewInner=randomBasis();scene.viewDirections=randomBasis();
        scene.nearMargin=0x3f800000;
        put(10,scene.viewPosition);for(unsigned c=0;c<3;++c){put(11+c,scene.viewOuter[c]);put(15+c,scene.viewInner[c]);put(19+c,scene.viewDirections[c]);}
        put(0,Vector{{scene.nearMargin,0,0,0}});
        for(auto &v:context.vu0_vf)for(auto &b:v.u)b=randomBits();context.vu0_vf[0]={};context.vu0_vf[0].u[3]=0x3f800000;
        const unsigned control=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1?0x8040u:0);
        _mm_setcsr(control);
        call(0x5a8);scene.root();
        if(!frameEqual(scene.frames[0],0)||std::memcmp(scene.position.bits.data(),context.vu0_vf[4].u,12))return 1;
        Basis local=randomBasis();Vector offset=randomVector();
        for(unsigned c=0;c<3;++c)vf(11+c,local[c]);vf(19,offset);
        call(0x450);scene.coordinate(local,offset);
        if(!frameEqual(scene.frames[0],0)||std::memcmp(scene.position.bits.data(),context.vu0_vf[4].u,12))return 2;
        for(unsigned depth=0;depth<3;++depth)
        {
            local=randomBasis();offset=randomVector();
            for(unsigned c=0;c<3;++c)vf(1+c,local[c]);vf(5,offset);context.vi[10]=depth*12;
            call(0x670);scene.child(depth,local,offset);
            if(!frameEqual(scene.frames[depth+1],depth+1)||std::memcmp(scene.position.bits.data(),context.vu0_vf[4].u,12))return 3;
        }
        const auto bounds=randomVector();vf(5,bounds);context.vi[10]=36;
        call(0x278);scene.fov(3,bounds);
        if(scene.outside!=context.vi[2]||scene.inside!=context.vi[5]||std::memcmp(scene.nearRange.bits.data(),context.vu0_vf[30].u,16))return 4;
        call(0x410);scene.project(3);
        for(unsigned c=0;c<3;++c)if(std::memcmp(&scene.object.column[c],context.vu0_vf[1+c].u,16)||std::memcmp(&scene.objectDirections[c],context.vu0_vf[7+c].u,16))return 5;
        if(std::memcmp(&scene.object.column[3],context.vu0_vf[6].u,16))return 6;
        offset=randomVector();vf(1,offset);Vector scale{{0,0x3f000000,0,0}};vf(8,scale);
        call(0x3a8);scene.scaledDistance(offset,scale.bits[1]);
        if(scene.distance!=context.vu0_vf[6].u[0]||_mm_getcsr()!=control)return 7;
        _mm_setcsr(saved);++cases;
    }
    uint64_t lightCases=0,shades=0;
    for(unsigned mode=0;mode<4;++mode)for(unsigned n=0;n<8192;++n)
    {
        Scene scene;
        const auto number=[&](float range){return (float(int(randomBits()%2001)-1000)/1000.f)*range;};
        const auto vec=[&](float range){Vector v;for(unsigned i=0;i<4;++i)v.bits[i]=tmb::hierarchy::collision::bits(number(range));return v;};
        scene.viewPosition=vec(5);scene.position=vec(10);scene.position.bits[3]=0;
        scene.sphere=vec(2);scene.sphere.bits[3]=tmb::hierarchy::collision::bits(float(n%7));
        scene.object.column[0]=Vector{{0x3f800000,0,0,0}};scene.object.column[1]=Vector{{0,0x3f800000,0,0}};scene.object.column[2]=Vector{{0,0,0x3f800000,0}};
        if(n&1)for(unsigned c=0;c<3;++c){scene.object.column[c]=vec(1);scene.object.column[c].bits[3]=0;}
        for(unsigned c=0;c<3;++c)vf(1+c,scene.object.column[c]);vf(4,scene.position);vf(5,scene.sphere);
        scene.pointCount=scene.pointWrites=n%9;scene.ambientMask=randomBits()&255;
        put(9,Vector{{scene.pointCount,scene.ambientMask,0,0}});put(10,scene.viewPosition);
        for(unsigned i=0;i<8;++i)
        {
            scene.points[i]={vec(10),vec(1),vec(1)};
            scene.points[i].sphere.bits[3]=tmb::hierarchy::collision::bits(float((n+i)%30));
            scene.points[i].direction.bits[3]=(n+i)&1?0:0x3f000000;
            scene.points[i].colour.bits[3]=tmb::hierarchy::collision::bits(float((n+i)%100));
            put(22+i,scene.points[i].sphere);put(30+i,scene.points[i].direction);put(38+i,scene.points[i].colour);
            if(i<4)put(5+i,vec(20)); // unrelated old slots must not become native light state
        }
        const unsigned control=((saved|0x1f80u)&~0xe040u)|(mode<<13)|(n&1?0x8040u:0);
        _mm_setcsr(control);call(0x808);scene.selectLights();
        if(_mm_getcsr()!=control)return 13;
        const int registers[]={10,18,12,13,14,15,16,20,21,22,28};
        if(scene.lighting.enabled!=(context.vi[14]!=0))return 10;
        if(scene.lighting.enabled)
        {
            tmb::vu1::VuImage oldImage,newImage;
            for(unsigned q=0;q<11;++q)
            {
                oldImage.setQw(165+q,context.vu0_vf[registers[q]].u);newImage.setQw(165+q,&scene.lighting.data[q]);
                for(unsigned k=0;k<4;++k)
                {
                    if(q>=4&&q<=6&&(k==3||k>=scene.lighting.count))continue; // inactive positions/padding have no colour contribution
                    if(scene.lighting.data[q].bits[k]!=context.vu0_vf[registers[q]].u[k]){
                        std::fprintf(stderr,"FAIL lights n%u mode%u kept%u q%u lane%u %x/%x\n",n,mode,scene.lighting.count,q,k,scene.lighting.data[q].bits[k],context.vu0_vf[registers[q]].u[k]);return 11;
                    }
                }
            }
            const Vector tint{{0x3f800000,0x3f800000,0x3f800000,0x3f800000}};
            oldImage.setQw(5,&tint);newImage.setQw(5,&tint);
            _mm_setcsr(saved);
            for(unsigned j=0;j<4;++j)
            {
                const float position[]={number(100),number(100),number(100)};
                const uint32_t normal[]={randomBits()&255,randomBits()&255,randomBits()&255},rgba[]={128,128,128,128};
                if(tmb::vu1::shade(tmb::vu1::Shading::Point,oldImage,normal,rgba,position)!=tmb::vu1::shade(tmb::vu1::Shading::Point,newImage,normal,rgba,position))return 12;
                ++shades;
            }
        }
        _mm_setcsr(saved);++lightCases;
    }
    // Execute the actual generated helper entries through the production instruction dispatcher. No service
    // capable of answering a microprogram or device call is supplied: missed adaptations cannot fall back.
    {
        NativeHost host;tmb::Registry registry;tmb::graphics::install(registry);host.handler=registry.instructionHandler();rt->port=&host;
        Binding owner(rt.get());
        uint8_t *ram=rt->memory.flat();std::memset(ram+bozo::kFlatVu0Offset,0x5a,4096);
        auto initial=context;
        const Basis input=randomBasis();const Vector inputPosition=randomVector();
        std::memcpy(ram+0x10020,&inputPosition,16);
        for(unsigned c=0;c<3;++c){std::memcpy(ram+0x10070+16*c,&input[c],16);std::memcpy(ram+0x100b0+16*c,&input[c],16);std::memcpy(ram+0x10180+16*c,&input[c],16);}
        setRegU32(&context,4,0x10000);setRegU32(&context,5,0x10180);setRegU32(&context,31,0xbad0000);
        hierCacheForAsm_0x2020e4(ram,&context,rt.get());
        auto expected=get(rt.get());
        std::memcpy(ram+0x10210,&inputPosition,16);std::memcpy(ram+0x10220,&input,48);
        setRegU32(&context,4,0x10200);
        expected.coordinate(input,inputPosition);hierCsUpdateAsm_0x201e98(ram,&context,rt.get());
        if(std::memcmp(&expected.frames[0],&get(rt.get()).frames[0],4*sizeof(Basis))||!get(rt.get()).frames[0].valid)return 14;
        std::memcpy(ram+0x10300,&input,48);std::memcpy(ram+0x10330,&inputPosition,16);
        setRegU32(&context,4,0x10300);setRegU32(&context,5,0);
        expected.child(0,input,inputPosition);vu0HierRotateAsm__FPA3_A3_fi_0x1d47f8(ram,&context,rt.get());
        if(std::memcmp(&expected.frames[1],&get(rt.get()).frames[1],4*sizeof(Basis))||!get(rt.get()).frames[1].valid)return 15;
        setRegU32(&context,4,1);hierGetMatStackAsm_0x201eec(ram,&context,rt.get());
        if(getRegU32(&context,2)!=0x700002c0||std::memcmp(rt->memory.flat()+bozo::kFlatScratchpadOffset+0x2c0,&expected.frames[1].rotation,48))return 16;
        const RecompiledFunction setters[]={hierPutMatStackAsm_0x201fdc,hierPutFovMatAsm_0x202018,hierPutFov1MatAsm_0x202054,hierPutLightDirMatAsm_0x202090};
        for(unsigned i=0;i<4;++i)
        {
            setRegU32(&context,4,0x10300);setRegU32(&context,5,2);setters[i](ram,&context,rt.get());
            if(get(rt.get()).frames[2].valid!=(i==3))return 17;
        }
        const size_t start=offsetof(R5900Context,vu0_vf),size=offsetof(R5900Context,cycles)-start;
        if(std::memcmp((uint8_t*)&context+start,(uint8_t*)&initial+start,size))return 18;
        for(unsigned i=0;i<4096;++i)if(ram[bozo::kFlatVu0Offset+i]!=0x5a)return 19;
        bool rejected=false;try{host.handler(ram,&context,rt.get(),0x1c1fa4);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)return 20;
        host.enabled=false;if(host.handler(ram,&context,rt.get(),0x1d31c4))return 21;
        rt->port=nullptr;
    }
    // Each bound scene belongs to one runtime; reusing a runtime address starts a fresh owner.
    { Binding owner(rt.get());get(rt.get()).pointCount=7;auto other=std::make_unique<Runtime>();Binding second(other.get());if(get(other.get()).pointCount)return 8; }
    { Binding owner(rt.get());if(get(rt.get()).pointCount)return 9; }
    std::printf("native scene: %llu root/camera/three-child/FOV/object/LOD sequences; four FP modes, owner isolation pass\n",(unsigned long long)cases);
    std::printf("native lights: %llu original comparisons, %llu rendered shading cases; unused slots/padding excluded\n",(unsigned long long)lightCases,(unsigned long long)shades);
}
