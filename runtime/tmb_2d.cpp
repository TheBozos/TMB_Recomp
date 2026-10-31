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
        constexpr int32_t kGpCurView = -0x3228, kGpFontMode = -0x3034, kGpFontUpload = -0x3630, kGpFontCount = -0x303c,
                          kGpFontAt = -0x3038, kGpFontTagAt = -0x3030, kGpFontStart = -0x302c;
#else
        constexpr uint32_t kFontRecords = 0x50caf0u, kFontRecordBytes = 0x38u;
        constexpr uint32_t kViews = 0x4e2cd0u;
        constexpr int32_t kGpCurView = -0x2f58, kGpFontMode = -0x2d28, kGpFontUpload = -0x3360, kGpFontCount = -0x2d30,
                          kGpFontAt = -0x2d2c, kGpFontTagAt = -0x2d24, kGpFontStart = -0x2d20;
#endif

        // The font's packet "sent" (fontDmaFontData) since the last publish: the frame's 2D is complete.
        std::atomic<bool> g_flushed{false};

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
            if (s_log && s_logged < 400u && !s_seen[place] && (s_seen[place] = true) && ++s_logged)
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
        ps2x::native::Renderer::instance().publishOverlay();
    }

    void install(PS2Runtime &runtime)
    {
        runtime.registerFunction(kFontBuildPrim, &fontBuildPrimOverride);
        runtime.registerFunction(kFontDmaFontData, &fontDmaFontDataOverride);
    }
}
