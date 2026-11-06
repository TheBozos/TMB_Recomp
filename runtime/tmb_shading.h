// SPDX-License-Identifier: MIT
// Native TMB mesh shading. Inputs are named game/scene parameters, with no VU memory layout.
#pragma once

#include "tmb_shader_parameters.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tmb::shading
{
    enum class Shading
    {
        Unlit,       // 0xcc8 and 0xd08: colour * VU 5 + VU 20
        Directional, // 0xce8: three directional lights
        Point,       // 0x1700: three point lights
    };

    // The fog factor a microprogram writes in XYZF2's F, over 255: clamp(VU 4.w + VU 21.x + VU 21.w / w, 0, VU 21.y)
    // (the same in the strip and sprite programs; checked on 3000 strip vertices of the match trace).
    inline float fogFactor(const Parameters &parameters, float q)
    {
        return std::min(std::max(parameters.fogOffset + parameters.fog.base + parameters.fog.inverseDepth * q, 0.0f), parameters.fog.limit) / 255.0f;
    }

    // A vertex's fog at its w. Behind the camera (w <= 0, a corner of a triangle that reaches past it: the ground under a
    // car seen from above, which the microprograms never draw) the fog it tends to as w goes to 0 from in front (q very
    // large): not fully fogged, which the triangle's visible part would fade toward, angle by angle.
    inline float fogAt(const Parameters &parameters, float w)
    {
        return fogFactor(parameters, w > 0.0f ? 1.0f / w : 1.0e30f);
    }

    inline uint32_t clampByte(float v)
    {
        if (!(v > 0.0f))
            return 0u;
        return v >= 255.0f ? 255u : static_cast<uint32_t>(v);
    }

    // The colour a strip's microprogram writes for one vertex (tools/tmb_mesh_decode.py, Strip.shade): normal and
    // colour as their bytes, pos the vertex in the object's space.
    inline uint32_t shade(Shading mode, const Parameters &parameters, const uint32_t normal[3], const uint32_t rgba[4], const float *pos)
    {
        float c[4];
        // The unlit strips (0x10a8) scale their colour by VU 5 too, as the scaled ones (0xf60): colour * vf31 +
        // vf11. VU 5 is the object's tint (the health bar's green, orange, red).
        if (mode == Shading::Unlit)
            for (int k = 0; k < 4; ++k)
                c[k] = static_cast<float>(rgba[k]) * parameters.tint[k] + parameters.colourOffset[k];
        else if (mode == Shading::Directional)
        {
            float n[3], d[3];
            for (int k = 0; k < 3; ++k)
                n[k] = 1.0f - static_cast<float>(normal[k] & 0xffu) / 128.0f;
            for (int i = 0; i < 3; ++i)
                d[i] = std::max(0.0f, parameters.directional[i].direction[0] * n[0] + parameters.directional[i].direction[1] * n[1] + parameters.directional[i].direction[2] * n[2]);
            for (int k = 0; k < 3; ++k)
            {
                const float light = (parameters.directional[0].colour[k] * d[0] + parameters.directional[1].colour[k] * d[1] + parameters.directional[2].colour[k] * d[2]) * parameters.tint[k];
                c[k] = static_cast<float>(rgba[k]) * light + parameters.colourOffset[k];
            }
            c[3] = static_cast<float>(rgba[3]) * parameters.tint[3] + parameters.colourOffset[3];
        }
        else // point lights
        {
            float n[3];
            for (int k = 0; k < 3; ++k)
                n[k] = static_cast<float>(normal[k] & 0xffu) / 127.0f - 128.0f / 127.0f;
            for (int k = 0; k < 4; ++k)
                c[k] = static_cast<float>(rgba[k]) * parameters.tint[k] + parameters.colourOffset[k];
            for (int i = 0; i < 3; ++i)
            {
                const float d[3] = {-(parameters.point.lights[i].position[0] + pos[0]), -(parameters.point.lights[i].position[1] + pos[1]), -(parameters.point.lights[i].position[2] + pos[2])};
                const float d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                if (d2 <= 0.0f)
                    continue;
                const float inv = 1.0f / std::sqrt(d2);
                const float facing = std::max(0.0f, (d[0] * n[0] + d[1] * n[1] + d[2] * n[2]) * inv);
                const float att = std::min(0.5f, facing * inv * inv * parameters.point.lights[i].attenuation);
                float cone = std::max(0.0f, (d[0] * parameters.point.lights[i].direction[0] + d[1] * parameters.point.lights[i].direction[1] + d[2] * parameters.point.lights[i].direction[2]) * inv + parameters.point.lights[i].coneOffset);
                cone *= cone;
                cone *= cone;
                for (int k = 0; k < 3; ++k)
                    c[k] += parameters.point.lights[i].colour[k] * att * cone;
            }
        }
        return clampByte(c[0]) | (clampByte(c[1]) << 8) | (clampByte(c[2]) << 16) | (clampByte(c[3]) << 24);
    }

    // One sprite of the 0x3000 program (its loop at 0x3030, re-entered at 0x3260): a centre p (the size in p[3]), a
    // colour (its bytes, scaled like the scaled mode) and four corners' texture coordinates (4.12). The centre goes to
    // the screen as (M * p) / w, M the camera times the object (no depth factor: z is scaled by VU 4.z here); the quad
    // around it is 608 x 304 pixels per unit of size and of 1/w (square on TMB's 640 x 256 field), corners (+x -y),
    // (-x -y), (+x +y), (-x +y), in screen space. False when it is behind the camera (VU1 culls it by its clip test).
    template <typename Vertex>
    inline bool spriteQuad(const Parameters &parameters, const float m[16], const float p[4], const uint32_t rgba[4], const int32_t st[4][2], Vertex quad[4])
    {
        float c[4];
        for (int k = 0; k < 4; ++k)
            c[k] = m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2] + m[12 + k];
        if (c[3] <= 0.0f)
            return false;
        const float q = 1.0f / c[3];
        const float x = c[0] * q, y = c[1] * q, z = c[2] * q * parameters.depthScale;
        const float hx = 608.0f * p[3] * q, hy = 304.0f * p[3] * q;
        float colour[4];
        for (int k = 0; k < 4; ++k)
            colour[k] = static_cast<float>(rgba[k]) * parameters.tint[k] + parameters.colourOffset[k];
        const uint32_t packed = clampByte(colour[0]) | (clampByte(colour[1]) << 8) | (clampByte(colour[2]) << 16) | (clampByte(colour[3]) << 24);
        const float corner[4][2] = {{x + hx, y - hy}, {x - hx, y - hy}, {x + hx, y + hy}, {x - hx, y + hy}};
        for (int i = 0; i < 4; ++i)
        {
            quad[i].x = corner[i][0];
            quad[i].y = corner[i][1];
            quad[i].z = z;
            quad[i].u = static_cast<float>(st[i][0]) / 4096.0f;
            quad[i].v = static_cast<float>(st[i][1]) / 4096.0f;
            quad[i].rgba = packed;
            quad[i].fog = fogFactor(parameters, q);
        }
        return true;
    }

    // A beam or trail (0x3270, its MSCNTs from 0x3290): N points from TOP (N in the tag at TOP). read(r, words) gives
    // the strip's quadword TOP + r as VU memory holds it (the points at 1 + 3j, colours at 2 + 3j, texture coordinates
    // at 3 + 3j: STCYCL 1, 3). Point k is projected through M (camera times object, no depth factor: screen = M p,
    // q = 1 / w) and widened on screen to two vertices, x + r and x - r with r = 608 p.w q, x and y held to 0..4095;
    // their texture coordinates (4.12, plus VU 4.xy, over Q) and colours (times VU 5, plus VU 20, at most 255) come in
    // pairs. Fog as the strips (VU 4.w + VU 21.x + VU 21.w q, 0 to VU 21.y). Both vertices of a point take ADC (no
    // triangle) when the point is outside the clip volume (VU 13-16 x VU 0-3, the setup's last vf13-16, on p with
    // y - 2: CLIP) or every other point (the count left, N - 1 - k, odd): the strip is drawn as separate segments. The
    // loop is software-pipelined: the reads below follow it (point k + 1 read while k is written, 3 or 6 quadwords on
    // by the parity). The strip's triangles are appended to triangles, in screen space.
    template <typename Vertex, typename Read, typename Triangles>
    inline void ribbonStrip(const Parameters &parameters, const float m[16], Read read, Triangles &triangles)
    {
        uint32_t words[4];
        read(0u, words);
        const uint32_t count = words[0] & 0x7fffu;
        float clip[16];
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                clip[j * 4 + k] = parameters.clipCamera[0][k] * parameters.object[j][0] + parameters.clipCamera[1][k] * parameters.object[j][1] + parameters.clipCamera[2][k] * parameters.object[j][2] + parameters.clipCamera[3][k] * parameters.object[j][3];
        const auto colourAt = [&](uint32_t r)
        {
            uint32_t w[4];
            read(r, w);
            float c[4];
            for (int k = 0; k < 4; ++k)
                c[k] = std::min(static_cast<float>(static_cast<int32_t>(w[k])) * parameters.tint[k] + parameters.colourOffset[k], 255.0f);
            return clampByte(c[0]) | (clampByte(c[1]) << 8) | (clampByte(c[2]) << 16) | (clampByte(c[3]) << 24);
        };
        const auto uvAt = [&](uint32_t r, Vertex &out)
        {
            uint32_t w[4];
            read(r, w);
            const float q = static_cast<float>(static_cast<int32_t>(w[2])) / 4096.0f;
            const float sOff = static_cast<float>(static_cast<int32_t>(w[0])) / 4096.0f + parameters.textureOffset[0];
            const float tOff = static_cast<float>(static_cast<int32_t>(w[1])) / 4096.0f + parameters.textureOffset[1];
            out.u = q != 0.0f ? sOff / q : sOff;
            out.v = q != 0.0f ? tOff / q : tOff;
        };
        Vertex previous[2]; // the last point's pair
        uint32_t position = 1u, next = 4u;
        for (uint32_t k = 0; k < count; ++k)
        {
            const uint32_t parity = (count - 1u - k) & 1u;
            uint32_t pw[4];
            read(position, pw);
            float p[4];
            std::memcpy(p, pw, sizeof(p));
            position = next;
            next += parity ? 3u : 6u;
            float c[4], cc[4];
            for (int i = 0; i < 4; ++i)
            {
                c[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i];
                cc[i] = clip[i] * p[0] + clip[4 + i] * (p[1] - 2.0f) + clip[8 + i] * p[2] + clip[12 + i];
            }
            const float q = 1.0f / c[3];
            const float aw = std::fabs(cc[3]);
            const bool clipped = std::fabs(cc[0]) > aw || std::fabs(cc[1]) > aw || std::fabs(cc[2]) > aw;
            const float x = c[0] * q, y = std::min(std::max(c[1] * q, 0.0f), 4095.0f), z = c[2] * q * parameters.depthScale;
            const float r = p[3] * q * 608.0f;
            const float fog = std::min(std::max(parameters.fogOffset + parameters.fog.base + parameters.fog.inverseDepth * q, 0.0f), parameters.fog.limit) / 255.0f;
            Vertex a, b;
            a.x = std::min(std::max(x + r, 0.0f), 4095.0f);
            b.x = std::min(std::max(x - r, 0.0f), 4095.0f);
            a.y = b.y = y;
            a.z = b.z = z;
            a.fog = b.fog = fog;
            uvAt(3u + 6u * k, a);
            uvAt(6u + 6u * k, b);
            a.rgba = colourAt(k == 0u ? 2u : 6u * k - 1u);
            b.rgba = colourAt(6u * k + (k == 0u ? 5u : 2u));
            // Vertex 2k closes (2k - 2, 2k - 1, 2k) and 2k + 1 closes (2k - 1, 2k, 2k + 1), unless their point is ADC.
            if (k > 0u && !(clipped || parity != 0u))
            {
                triangles.push_back(previous[0]);
                triangles.push_back(previous[1]);
                triangles.push_back(a);
                triangles.push_back(previous[1]);
                triangles.push_back(a);
                triangles.push_back(b);
            }
            previous[0] = a;
            previous[1] = b;
        }
    }

    // The skinned vertex passes (0x20 / 0x30 / 0x40 -> 0xd0 / 0x2f8 / 0x4d0): a pool vertex's position and normal
    // (n = 1 - b / 128) blended by its bones' matrices (each 16 floats, column c at [4c]: as VU1 reads a bone at TOP
    // + its offset), weights as the pass's integers times its 1/65536 (one bone: unweighted). Checked on 3171
    // vertices of a recorded two-bone pass (VU1's blended position and normal, within 1e-5).
    inline float skinWeight(int32_t raw)
    {
        float scale;
        const uint32_t bits = 0x377ffff6u; // the program's 1/65536
        std::memcpy(&scale, &bits, 4);
        return static_cast<float>(raw) * scale;
    }

    inline void skinVertex(int bones, const float *const matrix[3], const float weight[3], const float p[3], const float n[3], float outP[3],
                           float outN[3])
    {
        for (int k = 0; k < 3; ++k)
            outP[k] = outN[k] = 0.0f;
        for (int bone = 0; bone < bones; ++bone)
        {
            const float *m = matrix[bone];
            for (int k = 0; k < 3; ++k)
            {
                outP[k] += (m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2] + m[12 + k]) * weight[bone];
                outN[k] += (m[k] * n[0] + m[4 + k] * n[1] + m[8 + k] * n[2]) * weight[bone];
            }
        }
    }

    // A skinned strip vertex's colour (0x640: directional light on the blended normal, colour * (VU 17-19 . max(0,
    // VU 6-8 . n)) * VU 5 + VU 20; unlit, 0x878 / 0x9e8: colour * VU 5 + VU 20). All 4005 checked vertices of the
    // recorded match matched VU1.
    inline uint32_t skinShade(const Parameters &parameters, bool lit, const float n[3], const uint32_t rgba[4])
    {
        float d[3], c[4];
        for (int i = 0; i < 3; ++i)
            d[i] = std::max(0.0f, parameters.directional[i].direction[0] * n[0] + parameters.directional[i].direction[1] * n[1] + parameters.directional[i].direction[2] * n[2]);
        for (int k = 0; k < 3; ++k)
            c[k] = static_cast<float>(rgba[k]) * (lit ? parameters.directional[0].colour[k] * d[0] + parameters.directional[1].colour[k] * d[1] + parameters.directional[2].colour[k] * d[2] : 1.0f) * parameters.tint[k] + parameters.colourOffset[k];
        c[3] = static_cast<float>(rgba[3]) * parameters.tint[3] + parameters.colourOffset[3];
        return clampByte(c[0]) | (clampByte(c[1]) << 8) | (clampByte(c[2]) << 16) | (clampByte(c[3]) << 24);
    }

    // The environment-mapped strips (0x1db8, its MSCNTs from 0x1ec8, the water's from 0x2248). Its parameter step
    // (0x1de8, the MSCNT after each vertex pass) takes the alpha terms from TOP .yzw (base, slope, floor), all three 0
    // for the water (FSAND 0x001 after their sum, 0x1e58: then 1, 2, 0.1875).
    struct Reflection
    {
        float base = 0.0f, slope = 0.0f, floor = 0.0f;
        bool water = false;
    };

    inline Reflection reflection(const float terms[3])
    {
        Reflection r;
        r.water = terms[0] + terms[1] + terms[2] == 0.0f;
        r.base = r.water ? 1.0f : terms[0];
        r.slope = r.water ? 2.0f : terms[1];
        r.floor = r.water ? 0.1875f : terms[2];
        return r;
    }

    // One vertex: the eye vector e = (p + R^T t) / w (VU 0-2 the object's rotation, VU 3 its translation, w the
    // screen matrix m's, which the vertex pass leaves in the pool's w as 1 / w) is reflected about the normal
    // n = 1.00787 - b / 127 (the water's first bent by a wave: n.xy += 0.125 (s - s^3) - 0.03125, s = 2 frac(VU 21.z +
    // p.xy (0.019, 0.021)) - 1): r = e - 2 (e.n) n. The texture coordinates are 0.5 + 0.414 r taken to the world (VU
    // 0-2) along x and z (the water's x and y), Q 1. Alpha times max(base + slope (e.n), floor), then the colour times
    // VU 5 plus VU 20 (at most 255). Fog from VU 21.z (the setup put it where the vertex pass's fog takes VU 21.x:
    // ADDz.x vf18 at 0x1dc8). VU1 also cuts triangles that cross the clip volume (0x26e0); the GPU clips them.
    template <typename Vertex>
    inline void reflectVertex(const Parameters &parameters, const float m[16], const Reflection &reflect, const float pos[3], const int32_t normal[3],
                              const int32_t rgba[4], Vertex &out)
    {
        float eyeOffset[3];
        for (int j = 0; j < 3; ++j)
            eyeOffset[j] = parameters.object[j][0] * parameters.object[3][0] + parameters.object[j][1] * parameters.object[3][1] + parameters.object[j][2] * parameters.object[3][2];
        const int vAxis = reflect.water ? 1 : 2;
        out.x = pos[0];
        out.y = pos[1];
        out.z = pos[2];
        const float w = m[3] * pos[0] + m[7] * pos[1] + m[11] * pos[2] + m[15];
        const float depthQ = w != 0.0f ? 1.0f / w : 0.0f;
        float n[3], e[3];
        for (int k = 0; k < 3; ++k)
        {
            n[k] = 1.00787f - static_cast<float>(normal[k]) * 0.00787402f;
            e[k] = (pos[k] + eyeOffset[k]) * depthQ;
        }
        if (reflect.water)
        {
            const float scale[2] = {0.019f, 0.021f};
            for (int k = 0; k < 2; ++k)
            {
                const float x = parameters.fog.wavePhase + pos[k] * scale[k];
                const float wave = 2.0f * (x - 0.5f) - 2.0f * std::trunc(x);
                n[k] += 0.125f * (wave - wave * wave * wave) - 0.03125f;
            }
        }
        const float d = e[0] * n[0] + e[1] * n[1] + e[2] * n[2];
        float r[3];
        for (int k = 0; k < 3; ++k)
            r[k] = e[k] - 2.0f * d * n[k];
        out.u = 0.5f + 0.414062f * (parameters.object[0][0] * r[0] + parameters.object[1][0] * r[1] + parameters.object[2][0] * r[2]);
        out.v = 0.5f + 0.414062f * (parameters.object[0][vAxis] * r[0] + parameters.object[1][vAxis] * r[1] + parameters.object[2][vAxis] * r[2]);
        const float fade = std::max(reflect.base + reflect.slope * d, reflect.floor);
        float c[4];
        for (int k = 0; k < 4; ++k)
        {
            float value = static_cast<float>(rgba[k]);
            if (k == 3)
                value *= fade;
            c[k] = std::min(value * parameters.tint[k] + parameters.colourOffset[k], 255.0f);
        }
        out.rgba = clampByte(c[0]) | (clampByte(c[1]) << 8) | (clampByte(c[2]) << 16) | (clampByte(c[3]) << 24);
        out.fog = std::min(std::max(parameters.fogOffset + parameters.fog.wavePhase + parameters.fog.inverseDepth * (w > 0.0f ? 1.0f / w : 1.0e30f), 0.0f), parameters.fog.limit) / 255.0f; // (behind the camera: fogAt)
    }

    // The rigid strip setups' object-to-screen matrix (the setup's subroutine at 0x1c08: vf1-4): camera (VU 9-12) x
    // object (VU 0-3), column j of the result is the camera applied to column j of the object's; its z row scaled by
    // VU 4.z, the object's depth factor (MULz.z vf24, vf19z at 0x1018, 0x1188, 0x1460, 0x16c8). Columns: m[4c..4c+3].
    inline void objectToScreen(const Parameters &parameters, float m[16], bool depthScaled)
    {
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                m[j * 4 + k] = parameters.camera[0][k] * parameters.object[j][0] + parameters.camera[1][k] * parameters.object[j][1] + parameters.camera[2][k] * parameters.object[j][2] + parameters.camera[3][k] * parameters.object[j][3];
        if (depthScaled)
            for (int j = 0; j < 4; ++j)
                m[j * 4 + 2] *= parameters.depthScale;
    }
}
