// SPDX-License-Identifier: MIT
#include "fixtures/shader_inputs.h"
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <xmmintrin.h>

namespace old = tmb::shader_reference;
namespace native = tmb::shading;
namespace
{
    struct Vertex { float x{},y{},z{},u{},v{}; uint32_t rgba{}; float fog{}; };
    uint32_t seed = 0x179ebc63;
    uint32_t nextBits() { seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5; return seed; }
    float number() { return float(int(nextBits()%20001)-10000)/2048.f; }
    bool same(float a, float b) { return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b); }
    void require(bool okay, const char *what)
    { if (!okay) throw std::runtime_error(what); }
    void equal(const Vertex &a, const Vertex &b)
    {
        require(same(a.x,b.x)&&same(a.y,b.y)&&same(a.z,b.z)&&same(a.u,b.u)&&same(a.v,b.v)&&
                a.rgba==b.rgba&&same(a.fog,b.fog), "vertex differs");
    }
    void equal(const float *a, const float *b, unsigned n)
    { for (unsigned i=0;i<n;++i) require(same(a[i],b[i]), "matrix/fog differs"); }
}
int main()
{
    const unsigned saved = _mm_getcsr();
    uint64_t sprites=0, ribbonVertices=0, cases=0;
    for (unsigned mode=0;mode<4;++mode) for (unsigned sample=0;sample<8192;++sample)
    {
        _mm_setcsr((saved & ~0xe040u) | 0x1f80u | (mode<<13) | (sample&1?0x8040u:0));
        old::VuImage image;
        const auto fill = [&](unsigned address)
        { float v[4] = {number(),number(),number(),number()}; image.setQw(address,v); };
        for (unsigned i=0;i<22;++i) fill(i);
        for (unsigned i=165;i<=175;++i) fill(i);
        const float morph[4] = {number(),0,0,0}; image.setQw(31,morph);
        const auto p = shader_fixture::decode(image);
        float a[16],b[16],unscaled[16];
        for (bool depth: {false,true})
        {
            old::objectToScreen(image,a,depth); native::objectToScreen(p,b,depth); equal(a,b,16);
            if (!depth) std::memcpy(unscaled,b,sizeof(b));
        }
        const float position[4] = {number(),number(),number(),number()};
        const uint32_t normal[3] = {nextBits()&255,nextBits()&255,nextBits()&255};
        const uint32_t rgba[4] = {nextBits()&255,nextBits()&255,nextBits()&255,nextBits()&255};
        for (unsigned mode=0;mode<3;++mode)
            require(old::shade(static_cast<old::Shading>(mode),image,normal,rgba,position)==
                    native::shade(static_cast<native::Shading>(mode),p,normal,rgba,position), "strip colour differs");
        const float n[3] = {number(),number(),number()};
        for (bool lit: {false,true})
            require(old::skinShade(image,lit,n,rgba)==native::skinShade(p,lit,n,rgba), "skin colour differs");
        const float w = sample%3==0 ? 0.f : sample%3==1 ? -number()*number() : number()*number();
        require(same(old::fogAt(image,w),native::fogAt(p,w)), "fog depth differs");
        require(same(old::fogFactor(image,w),native::fogFactor(p,w)), "fog reciprocal differs");
        const int32_t uv[4][2] = {{0,0},{4096,0},{0,4096},{4096,4096}};
        Vertex va[4],vb[4];
        const bool visibleA=old::spriteQuad(image,unscaled,position,rgba,uv,va);
        const bool visibleB=native::spriteQuad(p,unscaled,position,rgba,uv,vb);
        require(visibleA==visibleB,"sprite clipping differs");
        if (visibleA) { for (unsigned i=0;i<4;++i) equal(va[i],vb[i]); ++sprites; }
        const int32_t ni[3] = {int32_t(normal[0]),int32_t(normal[1]),int32_t(normal[2])};
        const int32_t ci[4] = {int32_t(rgba[0]),int32_t(rgba[1]),int32_t(rgba[2]),int32_t(rgba[3])};
        for (bool water: {false,true})
        {
            const float terms[3] = {water?0.f:1.f,water?0.f:number(),water?0.f:number()};
            Vertex ra,rb;
            old::reflectVertex(image,unscaled,old::reflection(terms),position,ni,ci,ra);
            native::reflectVertex(p,unscaled,native::reflection(terms),position,ni,ci,rb); equal(ra,rb);
        }
        // Positive-W projection with a broad clip volume, plus every-other-case clipping: exercise emitted
        // ribbon triangles as well as the strip's parity, point/UV/colour indexing and clipping decisions.
        const float identity[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
        const float clip[4][4] = {{.1f,0,0,0},{0,.1f,0,0},{0,0,.1f,0},{0,0,0,1}};
        for (unsigned i=0;i<4;++i) { image.setQw(i,identity[i]); image.setQw(9+i,identity[i]); image.setQw(13+i,clip[i]); }
        if (sample&1) { const float wide[4]={1000,0,0,0}; image.setQw(13,wide); }
        const auto ribbonParameters=shader_fixture::decode(image);
        uint32_t rows[64][4] = {};
        rows[0][0] = 1+sample%8;
        for (unsigned i=1;i<64;++i) for (unsigned k=0;k<4;++k)
            rows[i][k] = std::bit_cast<uint32_t>(number()/8.f);
        const auto read = [&](uint32_t index, uint32_t *out)
        { require(index<64,"ribbon fixture extent"); std::memcpy(out,rows[index],16); };
        std::vector<Vertex> ta,tb;
        old::ribbonStrip<Vertex>(image,&identity[0][0],read,ta);
        native::ribbonStrip<Vertex>(ribbonParameters,&identity[0][0],read,tb);
        require(ta.size()==tb.size(),"ribbon topology differs");
        for (unsigned i=0;i<ta.size();++i) equal(ta[i],tb[i]);
        ribbonVertices+=ta.size(); ++cases;
    }
    _mm_setcsr(saved);
    require(sprites>0&&ribbonVertices>0,"fixtures did not emit geometry");
    std::printf("native shader: %llu parameter sets in four FP modes; strip/skin colours, matrix/fog, %llu visible sprites, "
                "%llu ribbon vertices and reflection/water agree bit-for-bit\n",
                (unsigned long long)cases,(unsigned long long)sprites,(unsigned long long)ribbonVertices);
}
