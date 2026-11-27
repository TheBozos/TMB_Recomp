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
                           kDisplayDialog = 0x00163500u;
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

    void install(tmb::Registry &runtime) { runtime.registerFunction(kDisplayDialog, &displayDialogOverride); }
}
