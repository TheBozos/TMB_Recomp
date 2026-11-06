// SPDX-License-Identifier: MIT
// Native hierarchy records connected to scalar traversal and renderer consumers.
#include "tmb_port.h"
#include "tmb_scene.h"
#include "bozo/libs/libraries.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace tmb::scene
{
    namespace
    {
        struct PositionSite { uint32_t pc; unsigned address; uint32_t offset; };
        struct ReadSite { uint32_t pc; unsigned output, field; };
        struct GroupSite { uint32_t pc; unsigned group; bool put; };
        // bozo:native kUnportedPrograms
        constexpr uint32_t kUnportedPrograms[] = {
            0x1c1fa4,0x1c2080,0x1c20d4,0x1c2104,0x1c21ac,0x1c21cc,0x1cf7e0,
        };
        // Native instruction boundaries for the audited USA hierarchy.
        // bozo:native kSceneInstructions
        constexpr uint32_t kSceneInstructions[] = {
            0x1d2c30, 0x1d31c4, 0x1d4804, 0x1d4808, 0x1d480c, 0x1d4810, 0x1d4814, 0x1d481c,
            0x1d4820, 0x1d4824, 0x1d4840, 0x1d4844, 0x1d4848, 0x1d4850, 0x1d487c, 0x1d48d0,
            0x1d4940, 0x1d4944, 0x1d4948, 0x1d494c, 0x1d4950, 0x1d4958, 0x1d4af4, 0x1d507c,
            0x1d5080, 0x1d5094, 0x1d5098, 0x1d50a4, 0x1d50ac, 0x1d5390, 0x1d5394, 0x1d5398,
            0x1d53d8, 0x1d5404, 0x1d5440, 0x1d5444, 0x1d5448, 0x1d54ec, 0x1d54f0, 0x1d54fc,
            0x1d5578, 0x1d558c, 0x1d56b0, 0x1d56c4, 0x1d5788, 0x1d578c, 0x1d5790, 0x1d5794,
            0x1d5798, 0x1d579c, 0x1d57f8, 0x1d5808, 0x1d580c, 0x1d5810, 0x1d584c, 0x1d58d0,
            0x1d5924, 0x1d5928, 0x1d596c, 0x1d59a8, 0x1d59ec, 0x1d5a08, 0x1d5aa0, 0x1d5aa4,
            0x1d5aa8, 0x1d5aac, 0x1d5ab0, 0x1d5ab4, 0x1d5b08, 0x1d5b58, 0x1d5b5c, 0x1d5b60,
            0x1d5b64, 0x1d5b68, 0x1d5b6c, 0x1d5b70, 0x1d5b74, 0x1d5b78, 0x1eab4c, 0x1eab50,
            0x1eab54, 0x1eab58, 0x1eab78, 0x1eab7c, 0x1eab80, 0x1eae44, 0x1eae58, 0x1eae5c,
            0x1eae60, 0x1eae64, 0x1eae68, 0x1eae6c, 0x1eae70, 0x1eae74, 0x1eae78, 0x1eae7c,
            0x1eae80, 0x1eaf84, 0x1ec0b8, 0x1ec1f0, 0x1ec200, 0x1ec234, 0x1ec240, 0x1ec24c,
            0x201e9c, 0x201ea0, 0x201ea4, 0x201ea8, 0x201eac, 0x201ed8, 0x201edc, 0x201ee0,
            0x201efc, 0x201f00, 0x201f04, 0x201f08, 0x201f0c, 0x201f10, 0x201f14, 0x201f18,
            0x201f1c, 0x201f38, 0x201f3c, 0x201f40, 0x201f44, 0x201f48, 0x201f4c, 0x201f50,
            0x201f54, 0x201f58, 0x201f74, 0x201f78, 0x201f7c, 0x201f80, 0x201f84, 0x201f88,
            0x201f8c, 0x201f90, 0x201f94, 0x201fb0, 0x201fb4, 0x201fb8, 0x201fbc, 0x201fc0,
            0x201fc4, 0x201fc8, 0x201fcc, 0x201fd0, 0x201fec, 0x201ff0, 0x201ff4, 0x201ff8,
            0x201ffc, 0x202000, 0x202004, 0x202008, 0x20200c, 0x202028, 0x20202c, 0x202030,
            0x202034, 0x202038, 0x20203c, 0x202040, 0x202044, 0x202048, 0x202064, 0x202068,
            0x20206c, 0x202070, 0x202074, 0x202078, 0x20207c, 0x202080, 0x202084, 0x2020a0,
            0x2020a4, 0x2020a8, 0x2020ac, 0x2020b0, 0x2020b4, 0x2020b8, 0x2020bc, 0x2020c0,
            0x2020d4, 0x2020d8, 0x2020ec, 0x202100, 0x202104, 0x202108, 0x20210c, 0x20211c,
            0x202120, 0x202124, 0x202134, 0x202138, 0x20213c,
        };
        constexpr PositionSite positions[] = {
            {0x1d487c, 8, 0},
            {0x1d4af4, 29, 0},
            {0x1d5094, 4, 0},
            {0x1d5404, 17, 0},
            {0x1d58d0, 17, 0},
            {0x1d596c, 17, 0},
            {0x1d59a8, 17, 0},
            {0x1d59ec, 17, 0},
        };
        constexpr PositionSite loads[] = {
            {0x1d48d0, 6, 0},
            {0x1d50ac, 4, 0},
            {0x1d5a08, 17, 0},
        };
        constexpr ReadSite reads[] = {
            {0x1d4820, 2, 2},
            {0x1d4824, 3, 5},
            {0x1d5080, 2, 10},
            {0x1d53d8, 24, 2},
            {0x1d54ec, 3, 2},
            {0x1d54f0, 2, 5},
            {0x1eae44, 11, 14},
        };
        constexpr ReadSite gprs[] = {
            {0x1d57f8, 9, 4},
            {0x1d584c, 2, 6},
            {0x1d5b08, 9, 4},
            {0x1eaf84, 8, 30},
        };
        constexpr GroupSite groups[] = {
            {0x201f14, 0, false},
            {0x201f50, 1, false},
            {0x201f8c, 2, false},
            {0x201fc8, 3, false},
            {0x202004, 0, true},
            {0x202040, 1, true},
            {0x20207c, 2, true},
            {0x2020b8, 3, true},
        };

        uint8_t *span(Runtime *rt,uint32_t address,uint32_t bytes)
        {
            address &= ~15u;
            const auto offset=bozo::GuestMemory::flatOffset(address,bytes);
            if(offset<0 || uint64_t(offset)+bytes>bozo::kFlatVu0Offset)throw bozo::libs::BadAccess{address,bytes};
            return rt->memory.flat()+offset;
        }
        Vector read(Runtime *rt,uint32_t address)
        { Vector v;std::memcpy(&v,span(rt,address,16),16);return v; }
        Basis basis(Runtime *rt,uint32_t address)
        {return {read(rt,address),read(rt,address+16),read(rt,address+32)};}
        unsigned depth(uint32_t stride)
        {if(stride%12)throw std::runtime_error("invalid native hierarchy frame stride");return stride/12;}
        Basis &group(Frame &f,unsigned g)
        {return g==0?f.rotation:g==1?f.outer:g==2?f.inner:f.directions;}
        struct Statistics
        {
            uint64_t views{},roots{},coordinates{},children{},fov{},objects{},lights{},lod{};
            ~Statistics(){std::fprintf(stderr,"[native:scene] views %llu roots %llu coordinates %llu children %llu FOV %llu objects %llu lights %llu LOD %llu\n",
                (unsigned long long)views,(unsigned long long)roots,(unsigned long long)coordinates,(unsigned long long)children,
                (unsigned long long)fov,(unsigned long long)objects,(unsigned long long)lights,(unsigned long long)lod);}
        } statistics;
    }
    static bool handle(uint8_t *,R5900Context *ctx,Runtime *rt,uint32_t pc)
    {
        if(!bozo::port::nativeGraphics(rt))return false;
        if(std::binary_search(std::begin(kUnportedPrograms),std::end(kUnportedPrograms),pc))
        {
            const char *check=std::getenv("TMB_NATIVE_HIERARCHY_CHECK");
            if(pc==0x1cf7e0 && check && *check=='1')return false; // explicitly requested copied-context oracle only
            throw std::runtime_error("unported VU0 program reached the native TMB player");
        }
        if(!std::binary_search(std::begin(kSceneInstructions),std::end(kSceneInstructions),pc))return false;
        auto &s=get(rt);
        const auto arg=[&](unsigned reg){return getRegU32(ctx,reg);};
        if(pc==0x2020ec)
        {
            s.viewPosition=read(rt,arg(4)+0x20);s.viewOuter=basis(rt,arg(4)+0x70);s.viewInner=basis(rt,arg(4)+0xb0);
            s.viewDirections=basis(rt,arg(5));s.nearMargin=ctx->fu[12];
            for(auto &f:s.frames){f.valid=false;f.groups=0;}
            ++statistics.views;return true;
        }
        if(pc==0x1ec0b8){s.pointCount=s.pointWrites=s.ambientMask=0;return true;}
        if(pc==0x1ec1f0){s.ambientMask=arg(19);return true;}
        if(pc==0x1ec200){s.pointCount=arg(30);s.pointWrites=0;return true;}
        if(pc==0x1ec234)
        {
            if(s.pointWrites>=s.points.size())throw std::runtime_error("native point-light list overflow");
            s.points[s.pointWrites++]={read(rt,arg(3)),read(rt,arg(3)+16),read(rt,arg(3)+32)};return true;
        }
        if(pc==0x1d2c30||pc==0x1d31c4){s.root();++statistics.roots;return true;}
        if(pc==0x201eac){s.coordinate(basis(rt,arg(4)+0x20),read(rt,arg(4)+0x10));++statistics.coordinates;return true;}
        if(pc==0x1d481c){s.child(arg(5),basis(rt,arg(6)),read(rt,arg(6)+0x30));++statistics.children;return true;}
        if(pc==0x1d579c){s.child(depth(arg(15)),basis(rt,arg(21)+0x10),read(rt,arg(21)+0x40));++statistics.children;return true;}
        if(pc==0x1d5ab4){s.child(depth(arg(15)),basis(rt,arg(2)),read(rt,arg(2)+0x30));++statistics.children;return true;}
        if(pc==0x201ee0){s.fov(arg(5),read(rt,arg(4)));++statistics.fov;return true;}
        if(pc==0x1d5398||pc==0x1d5448){s.fov(depth(arg(13)),read(rt,arg(21)+0x10));++statistics.fov;return true;}
        if(pc==0x2020d8){s.project(arg(4));++statistics.objects;return true;}
        if(pc==0x1d54fc){s.project(s.selectedFrame);++statistics.objects;return true;}
        if(pc==0x1d4840)
        {
            for(unsigned i=0;i<4;++i)s.object.column[i]=read(rt,arg(4)+16*i);
            return true;
        }
        if(pc==0x1d5578||pc==0x1d56b0){s.lighting.enabled=false;return true;}
        if(pc==0x1d558c||pc==0x1d56c4){s.selectLights();++statistics.lights;return true;}
        if(pc==0x1d5810){s.scaledDistance(read(rt,arg(21)+0x10),read(rt,arg(16)+0x60).bits[1]);++statistics.lod;return true;}
        if(pc==0x1d5928){s.translate(read(rt,arg(21)+0x10),true);return true;}
        if(pc==0x1d50a4){s.translate(read(rt,arg(4)),true);return true;}
        if(pc==0x1d4958)
        {
            const math::ArithmeticScope arithmetic;
            const auto v=math::combineXYZ(read(rt,arg(4)),s.position,false),squared=math::multiply(v,v);
            const auto result=math::add(math::add(math::broadcast(squared.bits[0]),math::broadcast(squared.bits[1])),math::broadcast(squared.bits[2]));
            std::memcpy(span(rt,arg(5),16),result.bits.data(),4);return true;
        }
        if(pc==0x1d507c){std::memcpy(span(rt,arg(4),16),&s.nearRange,16);return true;}
        if(pc==0x1d5b70)
        {const auto snapshot=s.frame(arg(19)&0xffff).rotation;std::memcpy(span(rt,arg(16)+0x2c0,48),&snapshot,48);return true;}
        for(const auto &site:groups)if(pc==site.pc)
        {
            const unsigned index=arg(site.put?5:4);
            if(index>=s.frames.size())throw std::runtime_error("native hierarchy frame index exceeded");
            auto &f=s.frames[index];
            if(!site.put && !(f.groups&(1u<<site.group)))throw std::runtime_error("uninitialised native hierarchy frame component");
            auto &target=group(f,site.group);
            if(site.put){target=basis(rt,arg(4));f.groups|=1u<<site.group;f.valid=f.groups==15;}
            else {const auto snapshot=target;const uint32_t offset=site.group==0?0x2c0:site.group==1?0x320:site.group==2?0x350:0x380;std::memcpy(span(rt,0x70000000+offset,48),&snapshot,48);}
            return true;
        }
        for(const auto &site:positions)if(pc==site.pc)
        {std::memcpy(span(rt,arg(site.address)+site.offset,16),s.position.bits.data(),12);return true;}
        for(const auto &site:loads)if(pc==site.pc)
        {s.position=read(rt,arg(site.address)+site.offset);s.position.bits[3]=0;return true;}
        for(const auto &site:reads)if(pc==site.pc)
        {setRegU64(ctx,site.output,site.field==2?s.outside:site.field==5?s.inside:site.field==14?s.lighting.enabled:s.selectedFrame*12);return true;}
        for(const auto &site:gprs)if(pc==site.pc)
        {
            const Vector v=site.field==4?s.position:site.field==30?s.nearRange:Vector{{s.distance,0,0,0}};
            std::memcpy(&ctx->r[site.output],&v,16);return true;
        }
        // All remaining sites are retired register loads/operations, VU-memory transfers or packet-only stores.
        return true;
    }
    bool instruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc)
    {
        try{return handle(ram,ctx,rt,pc);}
        catch(const std::runtime_error &e){throw std::runtime_error(std::string(e.what())+" at native hierarchy site "+std::to_string(pc));}
    }
}
