// TMB's 2D drawn natively, its builders rewritten from the game's code: each draws from the game's own variables
// (Renderer::drawOverlay, published with the frame), and none of them sends GS packets. TMB_NATIVE_2D=0: the game's
// own builders (their GS packets) instead.
//
// The font (fontBuildPrim, fontDmaFontData): fontSpritePrint* call fontBuildPrim for each printable character, which
// adds a sprite at the font's cursor (records of 0x20 bytes at 0x504540 USA, 0x38 at 0x50caf0 PAL, by font size: +0
// cell width, +2 cell height, +4 / +6 the glyph's size in the texture, +8 / +0xa on screen, +0xe the advance, +0x10 /
// +0x12 the cursor, +0x18 the colour; 12.4 fixed point, the cursor from the current view's top-left corner) to the
// font's packet, which fontDmaFontData sends to the GS. Here the same sprite is drawn natively, and fontDmaFontData
// only resets what it would after sending (the font's texture from the texture pack: no upload).

#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "tmb_region.h"
#include "runtime/native/native_renderer.h"
#include "runtime/native/native_texture_cache.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace tmb::native
{
    bool enabled();
}

namespace tmb::textures
{
    std::shared_ptr<const ps2x::native::TextureCache::Texture> font();
}

namespace tmb::twod
{
    namespace
    {
        constexpr uint32_t kFontBuildPrim = TMB_ADDR(0x001C5B28u);
        constexpr uint32_t kFontDmaFontData = TMB_ADDR(0x001C4FE0u);
#if TMB_REGION_US
        constexpr uint32_t kFontRecords = 0x504540u, kFontRecordBytes = 0x20u;
        constexpr uint32_t kViews = 0x4dfcd0u;
        constexpr int32_t kGpNumViews = -0x322c;
        constexpr int32_t kGpCurView = -0x3228, kGpFontMode = -0x3034, kGpFontUpload = -0x3630, kGpFontCount = -0x303c,
                          kGpFontAt = -0x3038, kGpFontTagAt = -0x3030, kGpFontStart = -0x302c;
#else
        constexpr uint32_t kFontRecords = 0x50caf0u, kFontRecordBytes = 0x38u;
        constexpr uint32_t kViews = 0x4e2cd0u;
        constexpr int32_t kGpNumViews = -0x2f5c;
        constexpr int32_t kGpCurView = -0x2f58, kGpFontMode = -0x2d28, kGpFontUpload = -0x3360, kGpFontCount = -0x2d30,
                          kGpFontAt = -0x2d2c, kGpFontTagAt = -0x2d24, kGpFontStart = -0x2d20;
#endif

        // The font's packet "sent" (fontDmaFontData) since the last publish: the frame's 2D is complete.
        std::atomic<bool> g_flushed{false};
        std::atomic<const uint8_t *> g_rdram{nullptr}; // guest memory, from the font's hooks (the publish has none)
        std::atomic<uint32_t> g_gp{0};                  // the game's gp, from the same hooks

        bool on()
        {
            static const bool s_on = []
            {
                const char *value = std::getenv("TMB_NATIVE_2D");
                return !(value && *value == '0');
            }();
            return s_on && tmb::native::enabled();
        }

        uint32_t word(const uint8_t *rdram, uint32_t address)
        {
            uint32_t value;
            std::memcpy(&value, rdram + (address & 0x1fffffcu), 4);
            return value;
        }
        uint16_t half(const uint8_t *rdram, uint32_t address)
        {
            uint16_t value;
            std::memcpy(&value, rdram + (address & 0x1fffffeu), 2);
            return value;
        }
        void setWord(uint8_t *rdram, uint32_t address, uint32_t value) { std::memcpy(rdram + (address & 0x1fffffcu), &value, 4); }
        void setHalf(uint8_t *rdram, uint32_t address, uint16_t value) { std::memcpy(rdram + (address & 0x1fffffeu), &value, 2); }

        // fontBuildPrim(size, character, packet): one glyph at the font's cursor, which then advances.
        void fontBuildPrimOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (!on())
            {
                fontBuildPrim__FicP6QwData_0x1c5b28(rdram, ctx, runtime);
                return;
            }
            const uint32_t gp = getRegU32(ctx, 28);
            const uint32_t record = kFontRecords + getRegU32(ctx, 4) * kFontRecordBytes;
            int32_t c = static_cast<int8_t>(getRegU32(ctx, 5) & 0xffu);
            // The current view's top-left corner, as viewGetCenter / viewGetWH give it.
            const uint32_t view = word(rdram, gp + kGpCurView);
            const uint32_t viewport = word(rdram, kViews + view * 16u + 4u);
            const int32_t centreX = static_cast<int32_t>(word(rdram, viewport + 0x18u)), centreY = static_cast<int32_t>(word(rdram, viewport + 0x1cu));
            const int32_t viewW = half(rdram, viewport + 4u), viewH = half(rdram, viewport + 6u);
            const int32_t left = centreX * 16 - viewW * 8, top = centreY * 16 - viewH * 8;
            if (c >= 'a' && c <= 'z')
                c -= 0x20;
            const int32_t glyph = c - 0x21, row = glyph >> 3, column = glyph - (row << 3);
            const int32_t u0 = half(rdram, record + 0u) * column + 8, v0 = half(rdram, record + 2u) * row + 8;
            const int32_t u1 = half(rdram, record + 4u) + u0, v1 = half(rdram, record + 6u) + v0;
            const int32_t x0 = static_cast<uint16_t>(half(rdram, record + 0x10u) + left), y0 = static_cast<uint16_t>(half(rdram, record + 0x12u) + top);
            const int32_t x1 = static_cast<uint16_t>(half(rdram, record + 8u) + x0), y1 = static_cast<uint16_t>(half(rdram, record + 0xau) + y0);
            uint32_t rgba;
            std::memcpy(&rgba, rdram + record + 0x18u, 4);

            // The sprite as two triangles: positions in GS pixels (12.4 / 16), texture coordinates over the 128 x 128 font.
            ps2x::native::Vertex corners[4];
            const float xs[2] = {x0 / 16.0f, x1 / 16.0f}, ys[2] = {y0 / 16.0f, y1 / 16.0f};
            const float us[2] = {u0 / 16.0f / 128.0f, u1 / 16.0f / 128.0f}, vs[2] = {v0 / 16.0f / 128.0f, v1 / 16.0f / 128.0f};
            for (int i = 0; i < 4; ++i)
            {
                corners[i].x = xs[i & 1];
                corners[i].y = ys[i >> 1];
                corners[i].u = us[i & 1];
                corners[i].v = vs[i >> 1];
                corners[i].rgba = rgba;
                corners[i].fog = 1.0f;
            }
            const ps2x::native::Vertex triangles[6] = {corners[0], corners[1], corners[2], corners[1], corners[3], corners[2]};
            // fontInitPacket's state: ALPHA (Cs - Cd) As + Cd, TEST alpha greater than 8, TEX1 bilinear.
            ps2x::native::Material material;
            material.textured = true;
            material.tex0 = 0xf0f7f0f7u; // the font's own GPU copy
            material.blend = ps2x::native::Material::Blend::Alpha;
            material.alphaTest = true;
            material.alphaRef = 8;
            material.clampU = material.clampV = true;
            // TMB_NATIVE_LOG_2D=1: the first glyphs, where they land.
            static const bool s_log = std::getenv("TMB_NATIVE_LOG_2D") != nullptr;
            static uint32_t s_logged = 0;
            static std::unordered_map<uint64_t, bool> s_seen; // each glyph at each place once
            const uint64_t place = (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 40) ^ (static_cast<uint64_t>(x0) << 20) ^ static_cast<uint64_t>(y0);
            if (s_log && s_logged < 6000u && !s_seen[place] && (s_seen[place] = true) && ++s_logged)
                std::fprintf(stderr, "[tmb:2d] glyph %c: (%.1f %.1f)-(%.1f %.1f) uv (%.3f %.3f)-(%.3f %.3f) rgba %08x view %u centre %d %d size %dx%d\n",
                             static_cast<char>(c), xs[0], ys[0], xs[1], ys[1], us[0], vs[0], us[1], vs[1], rgba, view, centreX, centreY, viewW, viewH);
            ps2x::native::Renderer::instance().drawOverlay(material, triangles, 6u, tmb::textures::font());

            // What fontBuildPrim leaves: its sprites' tag started (mode 2, where it would be), the cursor advanced, the
            // glyph counted.
            if (word(rdram, gp + kGpFontMode) != 2u)
            {
                setWord(rdram, gp + kGpFontMode, 2u);
                setWord(rdram, gp + kGpFontTagAt, word(rdram, gp + kGpFontAt));
            }
            setHalf(rdram, record + 0x10u, static_cast<uint16_t>(half(rdram, record + 0x10u) + half(rdram, record + 0xeu)));
            setWord(rdram, gp + kGpFontCount, word(rdram, gp + kGpFontCount) + 1u);
            ctx->pc = getRegU32(ctx, 31); // jr $ra
        }

        // fontDmaFontData: what it leaves after sending the font's packet (nothing sent: the glyphs are drawn natively).
        void fontDmaFontDataOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (!on())
            {
                fontDmaFontData__Fv_0x1c4fe0(rdram, ctx, runtime);
                return;
            }
            const uint32_t gp = getRegU32(ctx, 28);
            g_rdram.store(rdram, std::memory_order_relaxed);
            g_gp.store(gp, std::memory_order_relaxed);
            ctx->pc = getRegU32(ctx, 31); // jr $ra
            if (word(rdram, gp + kGpFontMode) == 0u)
                return;
            g_flushed.store(true, std::memory_order_relaxed);
            setWord(rdram, gp + kGpFontUpload, 0u);
            setWord(rdram, gp + kGpFontMode, 1u);
            setWord(rdram, gp + kGpFontCount, 0u);
            const uint32_t start = word(rdram, gp + kGpFontStart);
            setWord(rdram, gp + kGpFontAt, start);
            setWord(rdram, gp + kGpFontTagAt, start - 2u);
        }
        // ---- the HUD (Hud::Update's builders): flat-coloured sprites, lines and triangles, drawn natively ----
        //
        // Each builder override runs the game's own builder (its game logic: the closest enemy, the hit slots, the
        // radar's state), then draws what that builder would have sent from the same variables. The packet the game
        // builders fill is never sent: each override leaves the Hud's qword count (Hud + 4) at 1, under the 2 that
        // Update__3Hudi needs to kick its DMA (whichever builder runs last on each path is overridden).

        constexpr uint32_t kRadar = TMB_ADDR(0x0013EA28u), kHealthBar = TMB_ADDR(0x001419D8u), kSeparator = TMB_ADDR(0x0013E028u),
                           kRearViewEdge = TMB_ADDR(0x0013E2C8u), kLaser = TMB_ADDR(0x001424C8u), k4pSeparator = TMB_ADDR(0x0013E720u);
        constexpr uint32_t kTurbo4p = TMB_ADDR(0x00141220u), kEnergy4p = TMB_ADDR(0x00141790u), kHealth4p = TMB_ADDR(0x00141488u);
#if TMB_REGION_US
        constexpr int32_t kGpGame = -0x4154, kGpSettings = -0x4148, kGpRearLarge = -0x3224;
        constexpr int32_t kGpThreat[4] = {-0x38bc, -0x38f8, -0x37a4, -0x3760};
        constexpr uint32_t kSetTeams = 0x1a30c, kSetTeam = 0x1a320, kSetFade = 0x1a360, kAlphaConstants = 0x4e6e70u;
#else
        constexpr int32_t kGpGame = -0x4174, kGpSettings = -0x4168, kGpRearLarge = -0x2f54;
        constexpr int32_t kGpThreat[4] = {-0x35e8, -0x3624, -0x34d0, -0x348c};
        constexpr uint32_t kSetTeams = 0x1a274, kSetTeam = 0x1a288, kSetFade = 0x1a2cc, kAlphaConstants = 0x4edec0u;
#endif
        constexpr uint32_t kVehicleBytes = 0x2d00;

        float flt(const uint8_t *rdram, uint32_t address)
        {
            float value;
            std::memcpy(&value, rdram + (address & 0x1fffffcu), 4);
            return value;
        }
        double dbl(const uint8_t *rdram, uint32_t address)
        {
            double value;
            std::memcpy(&value, rdram + (address & 0x1fffff8u), 8);
            return value;
        }
        int8_t byte(const uint8_t *rdram, uint32_t address) { return static_cast<int8_t>(rdram[address & 0x1ffffffu]); }

        // A view's centre and size (viewGetCenter / viewGetWH), in GS pixels.
        struct View
        {
            int32_t cx = 2048, cy = 2048, w = 640, h = 448;
        };
        View viewOf(const uint8_t *rdram, uint32_t view)
        {
            View v;
            const uint32_t def = word(rdram, kViews + view * 16u + 4u);
            v.cx = static_cast<int32_t>(word(rdram, def + 0x18u));
            v.cy = static_cast<int32_t>(word(rdram, def + 0x1cu));
            v.w = half(rdram, def + 4u);
            v.h = half(rdram, def + 6u);
            return v;
        }
        // viewGetScreenDisplay: 0 one view, 1 split side by side, 2 split one over the other (view 0's type).
        int screenDisplay(const uint8_t *rdram)
        {
            switch (word(rdram, kViews + 8u))
            {
            case 1: case 2: case 5: case 6: case 15: case 16: return 2;
            case 3: case 4: case 7: case 8: return 1;
            default: return 0;
            }
        }

        // The HUD's primitives on the overlay: GS pixel coordinates, colours as the GS has them (0x80 is 1.0); ABE: the
        // packet's ALPHA_1 (Cs - Cd) As + Cd, else opaque.
        void hudTriangles(const ps2x::native::Vertex *v, uint32_t count, bool blend)
        {
            ps2x::native::Material material;
            material.textured = false;
            material.blend = blend ? ps2x::native::Material::Blend::Alpha : ps2x::native::Material::Blend::Opaque;
            ps2x::native::Renderer::instance().drawOverlay(material, v, count, nullptr);
        }
        uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return (r & 0xffu) | ((g & 0xffu) << 8) | ((b & 0xffu) << 16) | ((a & 0xffu) << 24); }
        ps2x::native::Vertex vertex(float x, float y, uint32_t colour)
        {
            ps2x::native::Vertex v;
            v.x = x;
            v.y = y;
            v.rgba = colour;
            v.fog = 1.0f;
            return v;
        }
        // A SPRITE from (x0, y0) to (x1, y1), as the GS fills it (corners in either order).
        void hudSprite(float x0, float y0, float x1, float y1, uint32_t colour, bool blend)
        {
            const float l = std::min(x0, x1), r = std::max(x0, x1), t = std::min(y0, y1), b = std::max(y0, y1);
            const ps2x::native::Vertex v[6] = {vertex(l, t, colour), vertex(r, t, colour), vertex(l, b, colour),
                                               vertex(r, t, colour), vertex(r, b, colour), vertex(l, b, colour)};
            hudTriangles(v, 6u, blend);
        }
        // A LINE, one pixel wide.
        void hudLine(float x0, float y0, float x1, float y1, uint32_t colour, bool blend)
        {
            const float dx = x1 - x0, dy = y1 - y0, length = std::sqrt(dx * dx + dy * dy);
            if (!(length > 0.0f))
                return;
            const float nx = -dy / length * 0.5f, ny = dx / length * 0.5f;
            const ps2x::native::Vertex v[6] = {vertex(x0 + nx, y0 + ny, colour), vertex(x1 + nx, y1 + ny, colour), vertex(x0 - nx, y0 - ny, colour),
                                               vertex(x1 + nx, y1 + ny, colour), vertex(x1 - nx, y1 - ny, colour), vertex(x0 - nx, y0 - ny, colour)};
            hudTriangles(v, 6u, blend);
        }

        // mathfTransform3dTo2d(view, p, out): p plus the view's translation (viewGetWeTrans) through its world-to-screen
        // matrix (viewGetWorldToScreenMat; mathfMulVec4x4: each row's xyz dot the point, plus its w), divided by w:
        // out = (x / w, y / w, w, z / w).
#if TMB_REGION_US
        constexpr uint32_t kViewTranslations = 0x4a1240u, kViewMatrices = 0x4e0230u;
#else
        constexpr uint32_t kViewTranslations = 0x4a4240u, kViewMatrices = 0x4e3230u;
#endif
        void project(const uint8_t *rdram, uint32_t view, const float p[4], float out[4])
        {
            const uint32_t translation = kViewTranslations + view * 0xf0u, matrix = kViewMatrices + view * 64u;
            float q[3], r[4];
            for (int k = 0; k < 3; ++k)
                q[k] = p[k] + flt(rdram, translation + 4u * k);
            for (int row = 0; row < 4; ++row)
                r[row] = flt(rdram, matrix + 16u * row) * q[0] + flt(rdram, matrix + 16u * row + 4u) * q[1] + flt(rdram, matrix + 16u * row + 8u) * q[2] +
                         flt(rdram, matrix + 16u * row + 12u);
            static const bool s_log = std::getenv("TMB_NATIVE_LOG_HUD") != nullptr;
            static uint32_t s_logs = 0;
            if (s_log && s_logs++ % 400u == 0u && s_logs < 4000u)
            {
                std::fprintf(stderr, "[tmb:2d] view %u translation (%.1f %.1f %.1f %.1f) at %#x, matrix at %#x:", view, flt(rdram, translation), flt(rdram, translation + 4u),
                             flt(rdram, translation + 8u), flt(rdram, translation + 12u), translation, matrix);
                for (int k = 0; k < 16; ++k)
                    std::fprintf(stderr, " %.3f", flt(rdram, matrix + 4u * k));
                std::fprintf(stderr, "%s", "\n");
            }
            const float inverse = 1.0f / r[3];
            out[0] = r[0] * inverse;
            out[1] = r[1] * inverse;
            out[2] = r[3];
            out[3] = r[2] * inverse;
        }

        // GetIdColor__3HudiPUcT2T2: a car's colour by its id.
        uint32_t idColour(uint32_t id)
        {
            switch (id)
            {
            case 0x20: return rgba(0x31, 0x5b, 0x01, 0);
            case 0x40: return rgba(0xdd, 0xbf, 0x28, 0);
            case 0x60: return rgba(0x34, 0x42, 0xff, 0);
            case 0x80: return rgba(0x5f, 0xaa, 0xba, 0);
            case 0xa0: return rgba(0x84, 0x47, 0x1b, 0);
            case 0xc0: return rgba(0xd0, 0x8f, 0xcd, 0);
            case 0xe0: return rgba(0xcc, 0xb9, 0xe1, 0);
            case 0x100: return rgba(0x7a, 0x64, 0x3d, 0);
            case 0x120: return rgba(0x75, 0x3d, 0xb4, 0);
            case 0x140: return rgba(0xa4, 0x1f, 0x26, 0);
            case 0x160: return rgba(0x4e, 0x44, 0x99, 0);
            case 0x180: return rgba(0x7f, 0xa1, 0x5c, 0);
            case 0x1a0: return rgba(0x4b, 0xe9, 0x47, 0);
            case 0x1c0: return rgba(0x86, 0x5c, 0x00, 0);
            case 0x1e0: return rgba(0xff, 0x60, 0x00, 0);
            case 0x200: return rgba(0xa4, 0x1f, 0x26, 0);
            case 0x220: return rgba(0xff, 0x60, 0x00, 0);
            case 0x13e2: return rgba(0x64, 0x64, 0x64, 0);
            default: return rgba(0xff, 0xff, 0xff, 0);
            }
        }

        // mathfManhatDist: the largest of |dx|, |dy|, |dz|, plus 0.34375 the middle one and 0.25 the smallest.
        float manhattan(const uint8_t *rdram, uint32_t a, uint32_t b)
        {
            float d[3];
            for (int k = 0; k < 3; ++k)
                d[k] = std::fabs(flt(rdram, b + 4u * k) - flt(rdram, a + 4u * k));
            std::sort(d, d + 3);
            return d[2] + d[1] * 0.34375f + d[0] * 0.25f;
        }

        // BuildRadarStuff's own primitives (its health bars come through BuildHealthBar): the blips, then a triangle
        // over each enemy on screen. Drawn before the game's builder runs, which clears the enemies' hit flags.
        void radar(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t hud, uint32_t viewIndex)
        {
            const uint32_t gp = getRegU32(ctx, 28);
            // Part A: the blips (Hud + 0x1c of them), around the radar's centre (Hud + 0x800 / 0x804, pixels).
            const int32_t centreX = static_cast<int32_t>(word(rdram, hud + 0x800u)) << 4, centreY = static_cast<int32_t>(word(rdram, hud + 0x804u)) << 4;
            const uint32_t blips = std::min<uint32_t>(word(rdram, hud + 0x1cu), 100u);
            for (uint32_t i = 0; i < blips; ++i)
            {
                if (rdram[(hud + 0x534u + i) & 0x1ffffffu] == 0u)
                    continue;
                const float x = static_cast<float>(centreX + static_cast<int32_t>(word(rdram, hud + 0x20u + 4u * i))) / 16.0f;
                const float y = static_cast<float>(centreY - static_cast<int32_t>(word(rdram, hud + 0x1b0u + 4u * i))) / 16.0f;
                const uint32_t r = rdram[(hud + 0x340u + i) & 0x1ffffffu], g = rdram[(hud + 0x3a4u + i) & 0x1ffffffu], b = rdram[(hud + 0x408u + i) & 0x1ffffffu];
                switch (word(rdram, hud + 0x5e4u + 4u * i))
                {
                case 0:
                    hudSprite(x - 15.0f / 16.0f, y + 15.0f / 16.0f, x + 15.0f / 16.0f, y - 15.0f / 16.0f, rgba(r, g, b, 0xff), true);
                    break;
                case 1:
                    hudLine(x - 5.0f, y, x + 6.0f, y, rgba(0xff, 0, 0, 0xff), false);
                    hudLine(x - 0.5f, y + 3.0f, x - 0.5f, y - 3.0f, rgba(0xff, 0, 0, 0xff), false);
                    hudLine(x + 0.5f, y + 3.0f, x + 0.5f, y - 3.0f, rgba(0xff, 0, 0, 0xff), false);
                    break;
                case 2:
                    hudLine(x - 4.5f, y, x + 4.5f, y, rgba(r, g, b, 0xff), false);
                    hudLine(x - 0.5f, y + 2.25f, x - 0.5f, y - 2.25f, rgba(r, g, b, 0xff), false);
                    hudLine(x + 0.5f, y + 2.25f, x + 0.5f, y - 2.25f, rgba(r, g, b, 0xff), false);
                    break;
                default:
                    break;
                }
            }

            // Part B: a triangle over each enemy on screen, its car's colour (red when a threat is near it).
            const uint32_t game = word(rdram, gp + kGpGame), settings = word(rdram, gp + kGpSettings);
            const uint32_t player = word(rdram, hud + 0x5e0u);
            const uint32_t cars = word(rdram, game + 0x1b3db4u);
            const View clipView = viewOf(rdram, viewIndex);
            const uint32_t current = word(rdram, gp + kGpCurView);
            for (uint32_t i = 0; i < cars && i < 64u; ++i)
            {
                const uint32_t car = game + i * kVehicleBytes;
                if (word(rdram, car + 0x20b0u) == word(rdram, player + 0x20b0u) || byte(rdram, car + 0x487u) || byte(rdram, car + 0x475u))
                    continue;
                const uint32_t id = word(rdram, car + 0x20a8u);
                if (word(rdram, car + 0x580u) - 1u >= 2u && !(id == 0x200u && word(rdram, game + 0x1ab430u) == 3u))
                    continue;
                uint32_t colour = idColour(id);
                if (static_cast<int32_t>(word(rdram, game + 0x1b3db8u)) >= 3 && word(rdram, settings + kSetTeams) == 1u)
                    colour = word(rdram, settings + kSetTeam + 4u * i) == 0u ? rgba(0x1a, 0x8e, 0x28, 0) : rgba(0xff, 0xff, 0xff, 0);
                const uint32_t threats[3] = {0x19bbc8u, 0x19bc0cu, 0x19bc30u};
                for (uint32_t k = 0; k < 4u; ++k)
                    for (int t = 0; t < 3; ++t)
                    {
                        const uint32_t object = word(rdram, game + threats[t] + 4u * k);
                        if (object && word(rdram, object + 0xcu) && manhattan(rdram, car + 0x1d0u, object + 0x10u) < flt(rdram, gp + kGpThreat[t]))
                            colour = rgba(0xff, 0, 0, 0);
                    }
                if (word(rdram, player + 0x20a8u) == 0x120u && word(rdram, player + 0x24d0u))
                {
                    const uint32_t target = word(rdram, word(rdram, player + 0x24d0u) + 0x130u) + 0x10u;
                    if (manhattan(rdram, car + 0x1d0u, target) < flt(rdram, gp + kGpThreat[3]))
                        colour = rgba(0xff, 0, 0, 0);
                }
                if (word(rdram, car + 0x24bcu))
                    colour = rgba(0xff, 0, 0, 0);
                // The point over the car, projected as the game does (mathfTransform3dTo2d).
                const uint32_t at = id == 0x200u ? word(rdram, game + 0x1ab500u) + 0x10u : car + 0x1d0u;
                float p[4] = {flt(rdram, at), flt(rdram, at + 4u), flt(rdram, at + 8u), flt(rdram, at + 12u)};
                p[2] += (id == 0xc0u && word(rdram, car + 0x243cu)) ? 20.0f : id == 0x200u ? -75.0f : 10.0f;
                float s[4];
                project(rdram, current, p, s);
                const float sx = std::round(s[0]), sy = std::round(s[1]);
                // TMB_NATIVE_LOG_HUD=1: now and then, each car's projected marker and whether it shows.
                static const bool s_logHud = std::getenv("TMB_NATIVE_LOG_HUD") != nullptr;
                static uint32_t s_hudLogs = 0;
                if (s_logHud && s_hudLogs++ % 97u == 0u && s_hudLogs < 4000u)
                    std::fprintf(stderr, "[tmb:2d] radar car %u id %#x: p (%.1f %.1f %.1f) -> (%.1f %.1f) w %.2f, view %d %d %dx%d, hidden %d%s", i, id, p[0],
                                 p[1], p[2], s[0], s[1], s[2], clipView.cx, clipView.cy, clipView.w, clipView.h, byte(rdram, car + 0x480u), "\n");
                if (s[2] <= 1.0f || s[0] < clipView.cx - clipView.w * 0.5f || s[0] > clipView.cx + clipView.w * 0.5f ||
                    s[1] < clipView.cy - clipView.h * 0.5f || s[1] > clipView.cy + clipView.h * 0.5f || byte(rdram, car + 0x480u))
                    continue;
                uint32_t alpha = 0x80;
                if (id != 0x200u && word(rdram, settings + kSetFade) == 1u)
                {
                    // the distance in the player's frame, in its plane (mathfMulVec by the player's rotation)
                    float d[3], local[3] = {0, 0, 0};
                    for (int k = 0; k < 3; ++k)
                        d[k] = flt(rdram, player + 0x1d0u + 4u * k) - flt(rdram, car + 0x1d0u + 4u * k);
                    for (int row = 0; row < 3; ++row)
                        for (int k = 0; k < 3; ++k)
                            local[row] += flt(rdram, player + 0xd0u + 16u * row + 4u * k) * d[k];
                    const double distance = std::sqrt(local[0] * local[0] + local[1] * local[1]);
                    if (distance > 600.0)
                        alpha = distance > 2000.0 ? 0x1eu
                                                  : static_cast<uint32_t>(static_cast<int32_t>((dbl(rdram, kAlphaConstants) - distance) / dbl(rdram, kAlphaConstants + 8u) *
                                                                                              dbl(rdram, kAlphaConstants + 16u)) + 0x1e);
                }
                colour = (colour & 0xffffffu) | (std::min<uint32_t>(alpha, 0xffu) << 24);
                const ps2x::native::Vertex v[3] = {vertex(sx - 12.0f, sy - 6.0f, colour), vertex(sx, sy, colour), vertex(sx + 12.0f, sy - 6.0f, colour)};
                hudTriangles(v, 3u, true);
            }
        }

        void radarOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4);
            if (on())
                radar(rdram, ctx, runtime, hud, getRegU32(ctx, 6));
            BuildRadarStuff__3HudP6QwDatai_0x13ea28(rdram, ctx, runtime);
            if (on())
                setWord(rdram, hud + 4u, 1u);
        }

        // BuildHealthBar(hud, packet, x, y, w, h, car): a bar measured from the current view's bottom-left corner, filling
        // from its right end, green / yellow / red by the health left.
        void healthBarOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), car = getRegU32(ctx, 10);
            const int32_t x = static_cast<int32_t>(getRegU32(ctx, 6)), y = static_cast<int32_t>(getRegU32(ctx, 7));
            const int32_t w = static_cast<int32_t>(getRegU32(ctx, 8)), h = static_cast<int32_t>(getRegU32(ctx, 9));
            const uint32_t gp = getRegU32(ctx, 28);
            BuildHealthBar__3HudP6QwDataiiiiP7Vehicle_0x1419d8(rdram, ctx, runtime);
            if (!on())
                return;
            const uint32_t game = word(rdram, gp + kGpGame);
            int32_t fill = 0;
            if (word(rdram, game + 0x1b3db0u) == 20u && word(rdram, game + 0x1ab500u) && word(rdram, car + 0x20a8u) == 0x200u)
            {
                const int32_t current = static_cast<int32_t>(word(rdram, game + 0x1ab43cu)), most = static_cast<int32_t>(word(rdram, game + 0x1ab438u));
                fill = most ? std::min((w - 4) * current / most + 1, w - 4) : 0;
                if (current <= 0)
                    fill = 0;
            }
            else
            {
                const float hp = flt(rdram, car + 0x3f4u), most = flt(rdram, car + 0x3f0u);
                fill = most != 0.0f ? std::min(static_cast<int32_t>(static_cast<float>(w - 4) * hp / most + 1.0f), w - 4) : 0;
                if (hp <= 0.0f)
                    fill = 0;
            }
            const View v = viewOf(rdram, word(rdram, gp + kGpCurView));
            const float left = static_cast<float>(v.cx - v.w / 2 + x), top = static_cast<float>(v.cy + v.h / 2 - y);
            hudSprite(left, top, left + w, top + h, rgba(0, 0, 0, 0x40), true);
            const uint32_t colour = word(rdram, hud + 0x5ccu) ? rgba(0, 0, 0, 0x40)
                                                              : rgba(word(rdram, hud + 0x5c0u), word(rdram, hud + 0x5c4u), 0, 0x40);
            const float right = left - 2.0f + w;
            hudSprite(right - fill, top + 1.0f, right, top + h - 1.0f, colour, true);
        }

        // BuildSeparator: the black line between the two views of a split screen (drawn by view 0's HUD).
        void separatorOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            BuildSeparator__3HudP6QwData_0x13e028(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const int display = screenDisplay(rdram);
            if (display == 0 || word(rdram, gp + kGpCurView) != 0u)
                return;
            const View v = viewOf(rdram, 0);
            const bool rearLarge = word(rdram, gp + kGpRearLarge) != 0u;
            float x0 = 0, y0 = 0;
            if (display == 2)
            {
                x0 = static_cast<float>(v.cx - v.w / 2);
                y0 = static_cast<float>(rearLarge ? v.cy - v.h / 2 : v.cy + v.h / 2 - 2);
            }
            else
            {
                x0 = static_cast<float>(v.cx + v.w / 2 - 2);
                y0 = static_cast<float>(v.cy - v.h / 2);
            }
            const float x1 = static_cast<float>(v.cx + v.w / 2), y1 = static_cast<float>(rearLarge ? v.cy - v.h / 2 + 2 : v.cy + v.h / 2);
            hudSprite(x0, y0, x1, y1, rgba(0, 0, 0, 0x80), false);
        }

        // BuildRearViewEdge(hud, packet, view): the black frame around the small rear view.
        void rearViewEdgeOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), viewIndex = getRegU32(ctx, 6);
            BuildRearViewEdge__3HudP6QwDatai_0x13e2c8(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const View v = viewOf(rdram, viewIndex);
            const float l = static_cast<float>(v.cx - v.w / 2 - 3), r = l + v.w + 1, t = static_cast<float>(v.cy - v.h / 2), b = t + v.h;
            const uint32_t black = rgba(0, 0, 0, 0x80);
            hudSprite(l, t, r, t + 1, black, false);
            hudSprite(r, t, r + 1, b, black, false);
            hudSprite(r, b, l, b - 1, black, false);
            hudSprite(l + 3, b - 1, l + 4, t + 1, black, false);
        }

        // boundFAngle: an angle into [-pi, pi).
        float boundAngle(float a)
        {
            constexpr float kPi = 3.14159265f, kTwoPi = 6.28318531f;
            return a - kTwoPi * std::floor((a + kPi) / kTwoPi);
        }
        // mathfRotMatrixRPH(m, angles): the rotation of angles (x, y, z) (sines and cosines as the game combines them),
        // rows of three.
        void rotationRph(const float angles[3], float m[3][3])
        {
            const float sx = std::sin(angles[0]), cx = std::cos(angles[0]), sy = std::sin(angles[1]), cy = std::cos(angles[1]);
            const float sz = std::sin(angles[2]), cz = std::cos(angles[2]);
            m[0][0] = cy * cz + sy * sx * sz;
            m[0][1] = -cy * sz + sy * sx * cz;
            m[0][2] = -sy * cx;
            m[1][0] = cx * sz;
            m[1][1] = cx * cz;
            m[1][2] = sx;
            m[2][0] = sy * cz - cy * sx * sz;
            m[2][1] = -sy * sz - cy * sx * cz;
            m[2][2] = cy * cx;
        }
#if TMB_REGION_US
        constexpr uint32_t kLaserOffsets = 0x4e6ef0u; // {0.006, -7.05, 4.08}, then {0.016, 9.204, 1.649}
#else
        constexpr uint32_t kLaserOffsets = 0x4edf40u;
#endif

        // BuildLaserStuff: the laser sight, a line from the gun (the car's frame by one offset, then its turret's angles
        // by another) to its target (car + 0x2438), both projected as mathfTransform3dTo2d; alpha 0x1a red.
        void laserOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            BuildLaserStuff__3HudP6QwData_0x1424c8(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const uint32_t car = word(rdram, hud + 0x5e0u), target = word(rdram, car + 0x2438u);
            if (!car || !target)
                return;
            const uint32_t body = word(rdram, car + 0x2018u);
            float p[4] = {flt(rdram, car + 0x1d0u), flt(rdram, car + 0x1d4u), flt(rdram, car + 0x1d8u), 1.0f};
            const float c1[3] = {flt(rdram, kLaserOffsets), flt(rdram, kLaserOffsets + 4u), flt(rdram, kLaserOffsets + 8u)};
            const float c2[3] = {flt(rdram, kLaserOffsets + 16u), flt(rdram, kLaserOffsets + 20u), flt(rdram, kLaserOffsets + 24u)};
            for (int k = 0; k < 3; ++k) // mathfMulTransVec(body + 0x20, c1): rows weighted by c1
                p[k] += flt(rdram, body + 0x20u + 4u * k) * c1[0] + flt(rdram, body + 0x30u + 4u * k) * c1[1] + flt(rdram, body + 0x40u + 4u * k) * c1[2];
            float angles[3], r[3][3];
            for (int k = 0; k < 3; ++k)
                angles[k] = boundAngle(flt(rdram, car + 0x23d0u + 4u * k) + flt(rdram, body + 0x60u + 4u * k));
            rotationRph(angles, r);
            for (int k = 0; k < 3; ++k)
                p[k] += r[0][k] * c2[0] + r[1][k] * c2[1] + r[2][k] * c2[2];
            const uint32_t view = word(rdram, gp + kGpCurView);
            float a[4], b[4];
            const float q[4] = {flt(rdram, target + 0x10u), flt(rdram, target + 0x14u), flt(rdram, target + 0x18u), 1.0f};
            project(rdram, view, p, a);
            project(rdram, view, q, b);
            hudLine(std::trunc(a[0]), std::trunc(a[1]), std::trunc(b[0]), std::trunc(b[1]), rgba(0xff, 0x28, 0x28, 0x1a), true);
        }
        // build4PTurboBar / build4PEnergyBar(hud, packet, x, y, w, h): a horizontal bar of the current view's bottom-left
        // corner (y up from its bottom), filling from its right end; opaque.
        void horizontalBar4p(uint8_t *rdram, uint32_t gp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t fill, uint32_t colour)
        {
            const View v = viewOf(rdram, word(rdram, gp + kGpCurView));
            const float x0 = static_cast<float>(v.cx - v.w / 2 - 2 + x + w - fill), y0 = static_cast<float>(v.cy + v.h / 2 + 1 - y);
            hudSprite(x0, y0, x0 + fill, y0 - 2 + h, colour, false);
        }
        void turbo4pOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            const int32_t x = static_cast<int32_t>(getRegU32(ctx, 6)), y = static_cast<int32_t>(getRegU32(ctx, 7));
            const int32_t w = static_cast<int32_t>(getRegU32(ctx, 8)), h = static_cast<int32_t>(getRegU32(ctx, 9));
            build4PTurboBar__3HudP6QwDataiiii_0x141220(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const float turbo = flt(rdram, word(rdram, hud + 0x5e0u) + 0x410u);
            const int32_t fill = turbo <= 0.0f ? 0 : std::min(static_cast<int32_t>(static_cast<float>(w - 4) * turbo / 100.0f + 1.0f), w - 4);
            horizontalBar4p(rdram, gp, x, y, w, h, fill, rgba(0x14, 0x58, 0x80, 0x20));
        }
        void energy4pOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            const int32_t x = static_cast<int32_t>(getRegU32(ctx, 6)), y = static_cast<int32_t>(getRegU32(ctx, 7));
            const int32_t w = static_cast<int32_t>(getRegU32(ctx, 8)), h = static_cast<int32_t>(getRegU32(ctx, 9));
            build4PEnergyBar__3HudP6QwDataiiii_0x141790(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const uint32_t car = word(rdram, hud + 0x5e0u);
            const int32_t current = static_cast<int32_t>(word(rdram, car + 0x2214u)), most = static_cast<int32_t>(word(rdram, car + 0x2218u));
            const int32_t fill = current <= 0 || most == 0 ? 0 : std::min((w - 4) * current / most + 1, w - 4);
            horizontalBar4p(rdram, gp, x, y, w, h, fill, rgba(0x89, 0x44, 0x20, 0x20));
        }
        // build4PHealthBar(hud, packet, x, y, w, h): a vertical bar rising from the current view's bottom edge (y up), red /
        // yellow / green by the health left; blended.
        void health4pOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            const int32_t x = static_cast<int32_t>(getRegU32(ctx, 6)), y = static_cast<int32_t>(getRegU32(ctx, 7));
            const int32_t w = static_cast<int32_t>(getRegU32(ctx, 8)), h = static_cast<int32_t>(getRegU32(ctx, 9));
            build4PHealthBar__3HudP6QwDataiiii_0x141488(rdram, ctx, runtime);
            if (!on())
                return;
            setWord(rdram, hud + 4u, 1u);
            const uint32_t car = word(rdram, hud + 0x5e0u);
            const float hp = flt(rdram, car + 0x3f4u), most = flt(rdram, car + 0x3f0u);
            const int32_t fill = hp <= 0.0f || most == 0.0f ? 0 : std::min(static_cast<int32_t>(static_cast<float>(h - 4) * hp / most + 1.0f), h - 4);
            const uint32_t colour = word(rdram, hud + 0x5ccu) ? rgba(0, 0, 0, 0x3f)
                                                              : rgba(word(rdram, hud + 0x5c0u), word(rdram, hud + 0x5c4u), word(rdram, hud + 0x5c8u), 0x40);
            const View v = viewOf(rdram, word(rdram, gp + kGpCurView));
            const float x0 = static_cast<float>(v.cx - v.w / 2 + x), y0 = static_cast<float>(v.cy + v.h / 2 - (y + fill));
            hudSprite(x0, y0, x0 + w, y0 + fill, colour, true);
        }

        void separator4pOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t hud = getRegU32(ctx, 4);
            Build4pSeparater__3HudP6QwDatai_0x13e720(rdram, ctx, runtime);
            if (on())
                setWord(rdram, hud + 4u, 1u);
        }
    }

    // At the end of a game frame (the adapter's publish): the 2D drawn since the last one goes on screen once the game
    // sent it (fontDmaFontData); a frame end with nothing sent since leaves what shows (the menus end frames twice).
    void publish()
    {
        if (!on())
            return;
        static uint32_t s_idle = 0;
        if (g_flushed.exchange(false, std::memory_order_relaxed))
            s_idle = 0;
        else if (++s_idle < 8u)
            return;
        // Mapped to the window by the game's own view 0 (its centre and size): a menu's 640 x 448, a match's 640 x 224.
        const uint8_t *rdram = g_rdram.load(std::memory_order_relaxed);
        if (!rdram)
        {
            ps2x::native::Renderer::instance().publishOverlay();
            return;
        }
        // The whole display: every view the game shows (viewGetNumViews), e.g. a rear view over the main one.
        const uint32_t views = std::clamp<uint32_t>(word(rdram, g_gp.load(std::memory_order_relaxed) + kGpNumViews), 1u, 4u);
        float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
        for (uint32_t i = 0; i < views; ++i)
        {
            const View v = viewOf(rdram, i);
            if (v.w <= 0 || v.h <= 0)
                continue;
            left = std::min(left, v.cx - v.w * 0.5f);
            top = std::min(top, v.cy - v.h * 0.5f);
            right = std::max(right, v.cx + v.w * 0.5f);
            bottom = std::max(bottom, v.cy + v.h * 0.5f);
        }
        if (!(right > left && bottom > top))
        {
            ps2x::native::Renderer::instance().publishOverlay();
            return;
        }
        ps2x::native::Screen screen;
        screen.originX = left;
        screen.originY = top;
        screen.width = right - left;
        screen.height = bottom - top;
        ps2x::native::Renderer::instance().publishOverlay(screen);
    }

    void install(PS2Runtime &runtime)
    {
        runtime.registerFunction(kFontBuildPrim, &fontBuildPrimOverride);
        runtime.registerFunction(kFontDmaFontData, &fontDmaFontDataOverride);
        runtime.registerFunction(kRadar, &radarOverride);
        runtime.registerFunction(kHealthBar, &healthBarOverride);
        runtime.registerFunction(kSeparator, &separatorOverride);
        runtime.registerFunction(kRearViewEdge, &rearViewEdgeOverride);
        runtime.registerFunction(kLaser, &laserOverride);
        runtime.registerFunction(k4pSeparator, &separator4pOverride);
        runtime.registerFunction(kTurbo4p, &turbo4pOverride);
        runtime.registerFunction(kEnergy4p, &energy4pOverride);
        runtime.registerFunction(kHealth4p, &health4pOverride);
    }
}
