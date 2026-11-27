// Widescreen (the Video settings' widescreen, video.ini): in a match whose views are each the screen's full width (one
// player, or two players one above the other), the game is made to draw a 16:9 picture. Each view's width goes from
// 640 to 853 pixels about the same centre, then the game's own viewTweakSetFov (0x1FDEA0: viewSetFov for every view,
// with its horizontal FOV and the view's definition) rebuilds the frustum planes it culls with and the projection it
// clips with from that width. The game then projects, culls and clips a scene 4/3 as wide with the same focal length:
// more at the sides, nothing stretched. The 3D screen (tmb_native.cpp) and the 2D overlay (the views' union,
// tmb_2d.cpp) follow the views' width; the HUD, drawn about the screen's centre in its 640 pixels, stays in the middle;
// the window shows a 16:9 picture (bozo::app::setPictureAspect, through the menu's hooks) while a view is wide.
//
// The views, from the game's code: viewNumViews (0x4FBC44) views, each a 16-byte record at 0x4DFCD0 whose word +4 is
// its viewport (viewGetWH: width u16 +4, height +6; viewSetFov's planes and projection from them). viewCreate makes
// them again at each screen's start (the menus get theirs back at 640), so only a match frame's end widens them, and
// one ends them narrow again when widescreen is switched off or the layout is split in four.
//
// The dialogs over a match (displayDialog 0x163500: the pause, the results, won and lost) lay their text and darkening
// out for a 640-wide view (centred at x 320 from the view's left edge, the band 640 wide), and with one player the
// pause keeps the match's view: so the views go back to 640 before the dialog starts (its picture 4:3, centred), and the
// match's next frame end widens them again.
//
// The HUD (a match's, one or two players over and under): its two clusters are hier meshes on coordinate systems the
// game hangs in front of the camera each frame (Hud::updateHUDPosition 0x13CC90: the view's matrix, its translation
// plus the cluster's camera-space offset, Hud + 0x810 the radar's, + 0x820 the weapon's, x right, y the depth). Seen
// through the same focal length D (viewport + 0) they keep their 4:3 places; for the 16:9 picture each goes out by
// the width gained on its side, (w - 640) / 2 pixels, that is x - / + that * y / D for the call (the offsets put back
// after it, so a narrowed view finds them as the game set them). The radar's centre and its blips follow (computed in
// the same call); the markers and bars over the cars are the world's. (The game reaches both Hud functions by a jump
// from TheGame's HudUpdateHUDPosition 0x15BD20 and HudUpdate 0x15BD40, which are the ones overridden.) The HUD's text
// drawn from a 640 layout (Hud::Update 0x13D908: the ammo count and KILLS on the right, DEMO in the middle) goes with its cluster: x past 400
// by the width gained, x past 240 by half of it, from the view's left edge (fontBuildPrim's).
//
// Off (the default) or in the headless program (no hooks), nothing changes: replays and the reference check are the
// game's own picture.

#include "tmb_port.h"
#include "tmb_menu.h"

#include <algorithm>
#include <atomic>
#include <vector>
#include <cstring>

namespace tmb::widescreen
{
    namespace
    {
        constexpr uint32_t kViews = 0x004DFCD0u, kNumViews = 0x004FBC44u, kViewTweakSetFov = 0x001FDEA0u,
                           kDisplayDialog = 0x00163500u, kHudPositionCall = 0x0015BD20u, kHudUpdateCall = 0x0015BD40u,
                           kFontPrintXY = 0x001C44A8u, kFontPrintCenteredXY = 0x001C44D8u;
        constexpr uint32_t kViewFovH = 0x004FBC04u, kHuds = 0x001ABE50u, kHudStride = 0xA00u, kRadarOffset = 0x810u,
                           kWeaponOffset = 0x820u;
        constexpr uint16_t kNarrow = 640, kWide = 853; // 640 * 4 / 3
        constexpr float kWideAspect = 16.0f / 9.0f;

        std::atomic<bool> g_wide{false};
        // Every viewport widened (the views' records point at a few fixed ones, which viewCreate hands out again: the
        // pause's full-screen view may be one widened earlier in the match and not shown since), narrowed together.
        std::vector<uint32_t> g_widened;
        float g_aspectSet = -1.0f;

        uint32_t word(const uint8_t *rdram, uint32_t address)
        {
            uint32_t value;
            std::memcpy(&value, rdram + (address & 0x1FFFFFCu), 4);
            return value;
        }
        uint16_t half(const uint8_t *rdram, uint32_t address)
        {
            uint16_t value;
            std::memcpy(&value, rdram + (address & 0x1FFFFFEu), 2);
            return value;
        }
        void setHalf(uint8_t *rdram, uint32_t address, uint16_t value) { std::memcpy(rdram + (address & 0x1FFFFFEu), &value, 2); }
        float flt(const uint8_t *rdram, uint32_t address)
        {
            float value;
            std::memcpy(&value, rdram + (address & 0x1FFFFFCu), 4);
            return value;
        }
        void setFlt(uint8_t *rdram, uint32_t address, float value) { std::memcpy(rdram + (address & 0x1FFFFFCu), &value, 4); }

        // The view's viewport (0 none).
        uint32_t viewport(const uint8_t *rdram, uint32_t view)
        {
            const uint32_t v = word(rdram, kViews + view * 16u + 4u);
            return v >= 0x100000u && v < 0x2000000u ? v : 0u;
        }
    }

    namespace
    {
        // The game's own rebuild of every view's frustum and projection, on a copy of the context (guest code there
        // never leaves the override: BozoEngine kernel.md section 3b), its stack below the thread's.
        void rebuildViews(uint8_t *rdram, const R5900Context *ctx, PS2Runtime *runtime)
        {
            R5900Context copy = *ctx;
            SET_GPR_U32(&copy, 29, (getRegU32(ctx, 29) - 0x200u) & ~0xFu);
            copy.leaving = 0;
            copy.pc = kViewTweakSetFov;
            viewTweakSetFov__Fv_0x1fdea0(rdram, &copy, runtime);
        }
    }

    namespace
    {
        // Every widened viewport back at 640; whether one was.
        bool narrowAll(uint8_t *rdram)
        {
            bool changed = false;
            for (const uint32_t vp : g_widened)
                if (half(rdram, vp + 4u) == kWide)
                    setHalf(rdram, vp + 4u, kNarrow), changed = true;
            g_widened.clear();
            return changed;
        }
    }

    bool wide() { return g_wide.load(std::memory_order_relaxed); }
    float screenWidth() { return wide() ? kNarrow * 4.0f / 3.0f : static_cast<float>(kNarrow); }

    // At a frame's end: in a match (`match`), the views widened or narrowed as the setting and the layout want; at
    // every frame end, whether a view is wide now and the window's picture shaped to it.
    void update(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, bool match)
    {
        if (!bozo::port::nativeGraphics(runtime))
            return;
        const tmb::menu::Hooks h = tmb::menu::currentHooks();
        if (!h.setPictureAspect)
            return; // the headless program: no window, no widescreen
        const uint32_t count = std::min<uint32_t>(word(rdram, kNumViews), 4u);
        if (match)
        {
            bool fullWidth = count > 0u;
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t vp = viewport(rdram, i);
                const uint16_t w = vp ? half(rdram, vp + 4u) : 0;
                fullWidth = fullWidth && (w == kNarrow || w == kWide);
            }
            const bool want = fullWidth && h.widescreen && h.widescreen();
            bool changed = false;
            for (uint32_t i = 0; i < count; ++i)
                if (const uint32_t vp = viewport(rdram, i))
                {
                    const uint16_t w = half(rdram, vp + 4u);
                    if (want && w == kNarrow)
                    {
                        setHalf(rdram, vp + 4u, kWide), changed = true;
                        if (std::find(g_widened.begin(), g_widened.end(), vp) == g_widened.end())
                            g_widened.push_back(vp);
                    }
                }
            if (!want)
                changed = narrowAll(rdram) || changed;
            if (changed)
                rebuildViews(rdram, ctx, runtime);
        }
        bool anyWide = false;
        for (uint32_t i = 0; i < count; ++i)
            if (const uint32_t vp = viewport(rdram, i))
                anyWide = anyWide || half(rdram, vp + 4u) == kWide;
        g_wide.store(anyWide, std::memory_order_relaxed);
        const float aspect = anyWide ? kWideAspect : 0.0f;
        if (aspect != g_aspectSet)
        {
            g_aspectSet = aspect;
            h.setPictureAspect(aspect);
        }
    }

    namespace
    {
        // displayDialog(type): the views narrow again first (above), then the game's dialog.
        void displayDialogOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (bozo::port::nativeGraphics(runtime))
            {
                if (narrowAll(rdram))
                {
                    rebuildViews(rdram, ctx, runtime);
                    g_wide.store(false, std::memory_order_relaxed);
                    const tmb::menu::Hooks h = tmb::menu::currentHooks();
                    if (h.setPictureAspect)
                        h.setPictureAspect(0.0f), g_aspectSet = 0.0f;
                }
            }
            displayDialog__F13_shellOverlay_0x163500(rdram, ctx, runtime);
        }
    }


    namespace
    {
        // TheGame::HudUpdateHUDPosition(this, i) (0x15BD20, which jumps on to Hud::updateHUDPosition of the Hud at
        // TheGame + 0x1ABE50 + i * 0xA00): the clusters' offsets out by the width gained, for the call (above).
        void updateHudPositionOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            // The focal length every view is given (viewTweakSetFov's viewFovH), while the match's views are wide.
            const uint32_t hud = getRegU32(ctx, 4) + kHuds + getRegU32(ctx, 5) * kHudStride;
            const float d = flt(rdram, kViewFovH);
            if (!bozo::port::nativeGraphics(runtime) || !wide() || !(d > 1.0f))
            {
                HudUpdateHUDPosition__7TheGamei_0x15bd20(rdram, ctx, runtime);
                return;
            }
            const float gained = (kWide - kNarrow) * 0.5f;
            const float radar = flt(rdram, hud + kRadarOffset), weapon = flt(rdram, hud + kWeaponOffset);
            setFlt(rdram, hud + kRadarOffset, radar - gained * flt(rdram, hud + kRadarOffset + 4u) / d);
            setFlt(rdram, hud + kWeaponOffset, weapon + gained * flt(rdram, hud + kWeaponOffset + 4u) / d);
            // On a copy of the context: the call ends here (it never leaves), so the offsets go back after all of it.
            R5900Context copy = *ctx;
            copy.leaving = 0;
            HudUpdateHUDPosition__7TheGamei_0x15bd20(rdram, &copy, runtime);
            setFlt(rdram, hud + kRadarOffset, radar);
            setFlt(rdram, hud + kWeaponOffset, weapon);
            ctx->pc = getRegU32(ctx, 31);
        }

        bool g_inHudUpdate = false;
        void hudUpdateOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            g_inHudUpdate = true;
            HudUpdate__7TheGamei_0x15bd40(rdram, ctx, runtime); // on to Hud::Update
            g_inHudUpdate = false;
        }
        // fontSpritePrintXY / CenteredXY (font, x, y, text) from Hud::Update: x from its 640 layout, moved with its cluster.
        void shiftHudText(R5900Context *ctx)
        {
            if (!g_inHudUpdate || !wide())
                return;
            const int32_t x = static_cast<int32_t>(getRegU32(ctx, 5));
            const int32_t gained = kWide - kNarrow;
            SET_GPR_U32(ctx, 5, static_cast<uint32_t>(x > 400 ? x + gained : x > 240 ? x + gained / 2 : x));
        }
        void fontPrintXYOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            shiftHudText(ctx);
            fontSpritePrintXY__FiiiPc_0x1c44a8(rdram, ctx, runtime);
        }
        void fontPrintCenteredXYOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            shiftHudText(ctx);
            fontSpritePrintCenteredXY__FiiiPc_0x1c44d8(rdram, ctx, runtime);
        }
    }

    void install(tmb::Registry &runtime)
    {
        runtime.registerFunction(kDisplayDialog, &displayDialogOverride);
        runtime.registerFunction(kHudPositionCall, &updateHudPositionOverride);
        runtime.registerFunction(kHudUpdateCall, &hudUpdateOverride);
        runtime.registerFunction(kFontPrintXY, &fontPrintXYOverride);
        runtime.registerFunction(kFontPrintCenteredXY, &fontPrintCenteredXYOverride);
    }
}
