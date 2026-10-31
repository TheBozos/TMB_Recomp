// The maths of TMB's hier strip microprograms (setups 0xcc8 unlit, 0xd08 scaled, 0xce8 directional, 0x1700 point
// lights), shared by the adapter that follows the VIF1 stream (runtime/tmb_native.cpp) and the mesh pack path that
// draws the objects from EE state at their hook (runtime/tmb_meshes.cpp). Both hand these functions a VU1 data
// memory image holding what the microprograms read: the object's matrix (VU 0-3), texture offset and depth factor
// (VU 4), colour scale (VU 5), light directions (VU 6-8), the frame's camera (VU 9-12), light colours (VU 17-19),
// colour offset (VU 20), fog terms (VU 21) and the point lights (VU 165-175). tools/tmb_mesh_decode.py checks the
// formulas against VU1.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tmb::vu1
{
    // VU1 data memory: 1024 quadwords, read as words or floats.
    struct VuImage
    {
        uint32_t words[1024 * 4] = {};
        float f(uint32_t qw, int lane) const
        {
            float v;
            std::memcpy(&v, &words[(qw & 0x3ffu) * 4 + lane], 4);
            return v;
        }
        uint32_t w(uint32_t qw, int lane) const { return words[(qw & 0x3ffu) * 4 + lane]; }
        void setQw(uint32_t qw, const void *sixteenBytes) { std::memcpy(&words[(qw & 0x3ffu) * 4], sixteenBytes, 16); }
    };

    enum class Shading
    {
        Unlit,       // 0xcc8 and 0xd08: colour * VU 5 + VU 20
        Directional, // 0xce8: three directional lights
        Point,       // 0x1700: three point lights
    };

    // The fog factor a microprogram writes in XYZF2's F, over 255: clamp(VU 4.w + VU 21.x + VU 21.w / w, 0, VU 21.y)
    // (the same in the strip and sprite programs; checked on 3000 strip vertices of the match trace).
    inline float fogFactor(const VuImage &vu, float q)
    {
        return std::min(std::max(vu.f(4, 3) + vu.f(21, 0) + vu.f(21, 3) * q, 0.0f), vu.f(21, 1)) / 255.0f;
    }

    inline uint32_t clampByte(float v)
    {
        if (!(v > 0.0f))
            return 0u;
        return v >= 255.0f ? 255u : static_cast<uint32_t>(v);
    }

    // The colour a strip's microprogram writes for one vertex (tools/tmb_mesh_decode.py, Strip.shade): normal and
    // colour as their bytes, pos the vertex in the object's space.
    inline uint32_t shade(Shading mode, const VuImage &vu, const uint32_t normal[3], const uint32_t rgba[4], const float *pos)
    {
        float c[4];
        // The unlit strips (0x10a8) scale their colour by VU 5 too, as the scaled ones (0xf60): colour * vf31 +
        // vf11. VU 5 is the object's tint (the health bar's green, orange, red).
        if (mode == Shading::Unlit)
            for (int k = 0; k < 4; ++k)
                c[k] = static_cast<float>(rgba[k]) * vu.f(5, k) + vu.f(20, k);
        else if (mode == Shading::Directional)
        {
            float n[3], d[3];
            for (int k = 0; k < 3; ++k)
                n[k] = 1.0f - static_cast<float>(normal[k] & 0xffu) / 128.0f;
            for (int i = 0; i < 3; ++i)
                d[i] = std::max(0.0f, vu.f(6, i) * n[0] + vu.f(7, i) * n[1] + vu.f(8, i) * n[2]);
            for (int k = 0; k < 3; ++k)
            {
                const float light = (vu.f(17, k) * d[0] + vu.f(18, k) * d[1] + vu.f(19, k) * d[2]) * vu.f(5, k);
                c[k] = static_cast<float>(rgba[k]) * light + vu.f(20, k);
            }
            c[3] = static_cast<float>(rgba[3]) * vu.f(5, 3) + vu.f(20, 3);
        }
        else // point lights
        {
            float n[3];
            for (int k = 0; k < 3; ++k)
                n[k] = static_cast<float>(normal[k] & 0xffu) / 127.0f - 128.0f / 127.0f;
            for (int k = 0; k < 4; ++k)
                c[k] = static_cast<float>(rgba[k]) * vu.f(5, k) + vu.f(20, k);
            for (int i = 0; i < 3; ++i)
            {
                const float d[3] = {-(vu.f(169, i) + pos[0]), -(vu.f(170, i) + pos[1]), -(vu.f(171, i) + pos[2])};
                const float d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                if (d2 <= 0.0f)
                    continue;
                const float inv = 1.0f / std::sqrt(d2);
                const float facing = std::max(0.0f, (d[0] * n[0] + d[1] * n[1] + d[2] * n[2]) * inv);
                const float att = std::min(0.5f, facing * inv * inv * vu.f(175, i));
                float cone = std::max(0.0f, (d[0] * vu.f(165, i) + d[1] * vu.f(166, i) + d[2] * vu.f(167, i)) * inv + vu.f(168, i));
                cone *= cone;
                cone *= cone;
                for (int k = 0; k < 3; ++k)
                    c[k] += vu.f(172 + i, k) * att * cone;
            }
        }
        return clampByte(c[0]) | (clampByte(c[1]) << 8) | (clampByte(c[2]) << 16) | (clampByte(c[3]) << 24);
    }

    // The rigid strip setups' object-to-screen matrix (the setup's subroutine at 0x1c08: vf1-4): camera (VU 9-12) x
    // object (VU 0-3), column j of the result is the camera applied to column j of the object's; its z row scaled by
    // VU 4.z, the object's depth factor (MULz.z vf24, vf19z at 0x1018, 0x1188, 0x1460, 0x16c8). Columns: m[4c..4c+3].
    inline void objectToScreen(const VuImage &vu, float m[16], bool depthScaled)
    {
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                m[j * 4 + k] = vu.f(9, k) * vu.f(j, 0) + vu.f(10, k) * vu.f(j, 1) + vu.f(11, k) * vu.f(j, 2) + vu.f(12, k) * vu.f(j, 3);
        if (depthScaled)
            for (int j = 0; j < 4; ++j)
                m[j * 4 + 2] *= vu.f(4, 2);
    }
}
