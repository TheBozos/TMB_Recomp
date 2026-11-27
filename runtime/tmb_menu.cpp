// PC SETTINGS in the game's own Options screen (a ninth line under the game's eight) and in its pause menu (a line under
// its own, below), and behind it pages drawn with the
// game's font and steered with the pad, for what the settings window (F2) offers besides the controls: Video (the
// window's settings, applied and saved to video.ini; widescreen: runtime/tmb_widescreen.cpp), Cheats
// (runtime/tmb_cheats.cpp's switches, at once) and Save states (the slot, load, save), and QUIT GAME (a second Cross
// ends the game). The controls stay in F2.
//
// The Options screen (uiOptions, 0x1911F0, screen 10 of uiMain's table; one call a frame), from its code:
// - Its cursor is options_main_line (0x4FB2F4), 0 to 7 (DIFFICULTY ... SAVE SETTINGS TO MEMORY CARD); options_subScreen
//   (0x4FB2F0) is 0 on that page (1 the controls, 2 the brightness). Up and Down wrap the cursor (0x1915AC, 0x1915E8:
//   0 - 1 gives 7, 7 + 1 gives 0) from the button flags uiGetInput (0x199880, called at 0x191284) sets that frame, one
//   press one flag (up 0x4FB368, down 0x4FB36C, left 0x4FB360, right 0x4FB364, square 0x4FB370, triangle 0x4FB374,
//   circle 0x4FB378, cross 0x4FB37C). Every other use of the cursor compares it with a line, so 8 matches none: no line
//   is highlighted and no line's handler runs. The game puts it back to 0 when the screen ends (0x191338).
// - Cross on a line without a handler of its own accepts the settings and leaves (0x1923D4); Triangle restores them and
//   leaves, Square sets the defaults.
// - The lines: x 0x320, y 0x550 + 0x210 a line in 12.4 sub-pixels from the view's corner, through fontSetColor,
//   fontSetCharSizesInSubPixels, fontSetCursorAtSubPixel and fontSpritePrint on font 0; the selected line in
//   (0x60, 0x4C, 0x36) at sizes (307, 332, -48, 48), the others in (0x30, 0x26, 0x1C) at (288, 312, -48, 48); the title
//   centred at (0x1400, 0x100) in (0x29, 0x1E, 0x0E). The sounds: playShellSFXSound(shell + 0x1A144, n), 0 down, 1 up,
//   2 accept, 3 back, 5 a value changed.
//
// Here: uiGetInput's override, when the Options page called it, lets the cursor reach 8 (Down on 7, Up on 0) and takes
// Cross on 8; uiOptions' override draws the line after the game's page, and while the pages are open draws them
// instead of the game's (the 3D shell behind them stays the game's; its sliders hidden by the game's blankSliders, as
// its controls page does). The pages read the pad through the game's uiGetInput and draw through the game's font
// functions, so they look and answer as the game's, on a copy of the context (Guest, below). Its legend (X apply all,
// Triangle cancel, Square default) stays the game's: Cross acts, Triangle backs out. The lines' text uses the font's
// glyphs (capitals, digits, space, ( ) and -; no apostrophe). A save state's load closes the pages (stateLoaded).
//
// Only where the game's graphics are native (every free run): the reference checker runs the game's own code.

#include "tmb_port.h"
#include "tmb_menu.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <string>
#include <vector>

namespace tmb::cheats
{
    struct Values
    {
        bool unlockAll = false, noAi = false;
        uint32_t health = 0u, ammo = 0u, energy = 0u;
        float enemyCap = -1.0f;
    };
    Values values();
    void setValues(const Values &v);
}

namespace tmb::menu
{
    namespace
    {
        constexpr uint32_t kUiOptions = 0x001911F0u, kUiGetInput = 0x00199880u, kOptionsInputReturn = 0x0019128Cu;
        constexpr uint32_t kFontSetColor = 0x001C4258u, kFontSetSizes = 0x001C4700u, kFontSetCursor = 0x001C47B8u,
                           kFontPrint = 0x001C47D8u, kFontPrintCentered = 0x001C4568u, kPlaySound = 0x00179720u,
                           kBlankSliders = 0x0019DE38u;
        // The pause menu (pauseLevelOverlay 0x166960, in displayDialog's own loop; types 0 and 16 the pause).
        constexpr uint32_t kPauseOverlay = 0x00166960u, kMenuSelection = 0x00167AF8u, kPauseExit = 0x00167670u,
                           kPauseSelectionReturn = 0x00166AE4u, kPauseExitReturn = 0x00166D64u, kPauseReturn0 = 0x001637E4u,
                           kPauseReturn16 = 0x001637F4u;
        constexpr uint32_t kSetMenuFont = 0x00167810u, kSetTitleFont = 0x00167AC8u, kFontSetSize = 0x001C4330u,
                           kFontPrintXY = 0x001C44A8u, kFontPrintCenteredXY = 0x001C44D8u, kPlayRtSound = 0x00179768u,
                           kInputGetInput = 0x001D8770u, kInputGetPlayerPad = 0x001D8658u;
        constexpr uint32_t kCurrentMenuSelection = 0x004FAD30u, kPlayerThatPaused = 0x004FAE64u;
        constexpr uint32_t kScreen = 0x004FB144u, kTransitioning = 0x004FB1ACu, kSubScreen = 0x004FB2F0u, kLine = 0x004FB2F4u;
        constexpr uint32_t kShellPointer = 0x004FAD28u, kShellSound = 0x1A144u;
        constexpr uint32_t kLeft = 0x004FB360u, kRight = 0x004FB364u, kUp = 0x004FB368u, kDown = 0x004FB36Cu,
                           kTriangle = 0x004FB374u, kCircle = 0x004FB378u, kCross = 0x004FB37Cu;
        // The flags a leaving page sets (still_processing_triangle, _x, _x_main_options, _x_controls, _x_brightness).
        constexpr uint32_t kBusy[] = {0x004FB2FCu, 0x004FB300u, 0x004FB304u, 0x004FB308u, 0x004FB30Cu};
        constexpr uint32_t kOptionsScreen = 10u, kLastLine = 7u, kPcLine = 8u;
        constexpr int32_t kLineX = 0x320, kPcLineY = 0x17C0;
        constexpr int kSoundDown = 0, kSoundUp = 1, kSoundAccept = 2, kSoundBack = 3, kSoundValue = 5;

        std::mutex g_hooksMutex;
        Hooks g_hooks; // under g_hooksMutex
        Hooks hooks()
        {
            std::lock_guard lock(g_hooksMutex);
            return g_hooks;
        }

        uint32_t word(const uint8_t *rdram, uint32_t address)
        {
            uint32_t value;
            std::memcpy(&value, rdram + (address & 0x1FFFFFCu), 4);
            return value;
        }
        void setWord(uint8_t *rdram, uint32_t address, uint32_t value) { std::memcpy(rdram + (address & 0x1FFFFFCu), &value, 4); }

        // The game's functions called from an override, with their arguments in a0..a3, t0, on a copy of the thread's
        // context (BozoEngine kernel.md section 3b, "Guest code on an override's own context"): guest code there never
        // leaves the override (an event due waits for the thread's own code), so the override's own code after each
        // call runs on the game's state as that call left it, and the thread's registers are never touched. The copy's
        // stack starts 0x200 bytes below the thread's (the callees' frames go below that); the strings printed go in
        // the free space between the two.
        struct Guest
        {
            uint8_t *rdram;
            PS2Runtime *rt;
            R5900Context copy;
            uint32_t text = 0u;
            bool pause = false; // the pause menu's pages: its pad, sounds and fonts (pixels, not sub-pixels)

            Guest(uint8_t *memory, const R5900Context *ctx, PS2Runtime *runtime) : rdram(memory), rt(runtime), copy(*ctx)
            {
                const uint32_t frame = (getRegU32(ctx, 29) - 0x200u) & ~0xFu;
                SET_GPR_U32(&copy, 29, frame);
                copy.leaving = 0;
                text = (frame + 0x20u) & 0x1FFFFFFu;
            }

            void call(void (*fn)(uint8_t *, R5900Context *, Runtime *), uint32_t address, std::initializer_list<uint32_t> args)
            {
                static constexpr int kArgs[] = {4, 5, 6, 7, 8};
                int i = 0;
                for (const uint32_t a : args)
                    SET_GPR_U32(&copy, kArgs[i++], a);
                copy.pc = address;
                fn(rdram, &copy, rt);
            }
            uint32_t result() const { return getRegU32(&copy, 2); }
            uint32_t string(const std::string &s)
            {
                const size_t n = std::min<size_t>(s.size(), 255u);
                std::memcpy(rdram + text, s.data(), n);
                rdram[text + n] = 0;
                return text;
            }
            void sound(int which)
            {
                if (pause)
                {
                    // The pause's own sounds (getCurrentMenuSelection, checkInputForPauseExit): 0x2C down, 0x2B up,
                    // 0x2A accept.
                    const uint32_t code = which == kSoundDown || which == kSoundValue ? 0x2Cu : which == kSoundAccept ? 0x2Au : 0x2Bu;
                    call(&playRTShellSound__Fi_0x179768, kPlayRtSound, {code});
                    return;
                }
                const uint32_t shell = word(rdram, kShellPointer);
                if (shell != 0u)
                    call(&playShellSFXSound__10ShellSoundi_0x179720, kPlaySound, {shell + kShellSound, static_cast<uint32_t>(which)});
            }
            void color(uint32_t r, uint32_t g, uint32_t b) { call(&fontSetColor__Fiiii_0x1c4258, kFontSetColor, {0u, r, g, b}); }
            // The game's two sizes (the selected line 1.6 times the font's 12 x 13, the others 1.5), times `scale`:
            // fontSetCharSizesInSubPixels(font, width, height, -48, 48), as uiOptions sets them.
            void sizes(bool large, float scale = 1.0f)
            {
                const float k = (large ? 1.6f : 1.5f) * scale;
                call(&fontSetCharSizesInSubPixels__Fiiiii_0x1c4700, kFontSetSizes,
                     {0u, static_cast<uint32_t>(std::lround(12.0f * k * 16.0f)), static_cast<uint32_t>(std::lround(13.0f * k * 16.0f)),
                      static_cast<uint32_t>(-48), 48u});
            }
            void print(int32_t x, int32_t y, const std::string &s)
            {
                call(&fontSetCursorAtSubPixel__Fiii_0x1c47b8, kFontSetCursor, {0u, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                call(&fontSpritePrint__FiPc_0x1c47d8, kFontPrint, {0u, string(s)});
            }
            // Centred on x, as the game centres its values: the cursor set, then fontSpritePrintCentered.
            void printCentered(int32_t x, int32_t y, const std::string &s)
            {
                call(&fontSetCursorAtSubPixel__Fiii_0x1c47b8, kFontSetCursor, {0u, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                call(&fontSpritePrintCentered__FiPc_0x1c4568, kFontPrintCentered, {0u, string(s)});
            }
            void title(const std::string &s)
            {
                color(0x29u, 0x1Eu, 0x0Eu);
                sizes(true);
                call(&fontSetCursorAtSubPixel__Fiii_0x1c47b8, kFontSetCursor, {0u, 0x1400u, 0x100u});
                call(&fontSpritePrintCentered__FiPc_0x1c4568, kFontPrintCentered, {0u, string(s)});
            }
        };

        void setSizes(Guest &g, bool large, float scale) { g.sizes(large, scale); }

        bool active(PS2Runtime *rt) { return bozo::port::nativeGraphics(rt); }

        bool optionsPageIdle(const uint8_t *rdram)
        {
            if (word(rdram, kScreen) != kOptionsScreen || word(rdram, kSubScreen) != 0u || word(rdram, kTransitioning) != 0u)
                return false;
            for (const uint32_t flag : kBusy)
                if (word(rdram, flag) != 0u)
                    return false;
            return true;
        }

        // ---- The pages ----

        enum class Page
        {
            Closed,
            Top,
            Video,
            Cheats,
            States
        };

        struct Row
        {
            std::string label;
            std::string value;   // "" a line Cross acts on
            bool info = false;   // a line the cursor skips
        };

        struct Menu
        {
            Page page = Page::Closed;
            int line = 0;
            Page lastTop = Page::Video;
            bozo::app::video::Settings video; // the Video page's edits (Apply and save sends them)
            std::vector<std::pair<int, int>> resolutions;
            int monitorRefresh = 0;
            std::string status; // a line under the page: what Cross did
            bool quitArmed = false; // QUIT GAME pressed once: a second Cross quits
            bool inPause = false;   // opened from the pause menu (its style and pad), not the Options screen
        } g_menu;

        std::string upper(std::string s)
        {
            for (char &c : s)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        template <typename T> int indexOf(const std::vector<T> &list, const T &value, int fallback = 0)
        {
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i] == value)
                    return static_cast<int>(i);
            return fallback;
        }
        template <typename T> T step(const std::vector<T> &list, const T &value, int delta)
        {
            const int n = static_cast<int>(list.size());
            const int i = indexOf(list, value, delta > 0 ? -1 : 0);
            return list[static_cast<size_t>(((i + delta) % n + n) % n)];
        }

        // Video.
        const std::vector<int> kModes = {0, 1, 2};
        const std::vector<float> kScales = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
        const std::vector<int> kUpscales = {1, 2, 4};
        const std::vector<int> kSmoothings = {0, 1, 2, 4, 8, 16};
        const std::vector<int> kAntiAliasings = {0, 2, 4, 8};
        const std::vector<int> kFastForwards = {200, 300, 400, 0};

        std::string modeText(bozo::app::video::WindowMode m)
        {
            switch (m)
            {
            case bozo::app::video::WindowMode::Fullscreen: return "FULLSCREEN";
            case bozo::app::video::WindowMode::Borderless: return "BORDERLESS";
            default: return "WINDOWED";
            }
        }
        std::string scaleText(float s)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%d", static_cast<int>(std::lround(s * 100.0f)));
            return std::string(text) + " PERCENT";
        }
        std::string timesText(int n) { return std::to_string(n) + "X"; }
        std::string onOff(bool on) { return on ? "ON" : "OFF"; }
        bool packOn(const std::string &pack) { return pack != "off" && pack != "0"; }

        std::vector<int> frameRates(const Menu &m)
        {
            std::vector<int> rates = {0, -1};
            if (m.video.frameRate > 0)
                rates.push_back(m.video.frameRate);
            return rates;
        }
        std::string frameRateText(int rate, int refresh)
        {
            if (rate == 0)
                return "60 (GAME)";
            if (rate < 0)
                return refresh > 0 ? "MONITOR (" + std::to_string(refresh) + ")" : "MONITOR";
            return std::to_string(rate);
        }

        enum VideoLine
        {
            VMode,
            VResolution,
            VScale,
            VFrameRate,
            VAntiAliasing,
            VSmoothing,
            VUpscale,
            VFilter,
            VPack,
            VWide,
            VPause,
            VFast,
            VApply,
            VCount
        };

        std::vector<Row> videoRows(const Menu &m, const Hooks &h)
        {
            const auto &v = m.video;
            std::vector<Row> rows(VCount);
            rows[VMode] = {"DISPLAY", modeText(v.mode)};
            rows[VResolution] = {"RESOLUTION", std::to_string(v.width) + " X " + std::to_string(v.height)};
            rows[VScale] = {"RENDER SCALE", scaleText(v.renderScale)};
            rows[VFrameRate] = {"FRAME RATE", frameRateText(v.frameRate, m.monitorRefresh)};
            rows[VAntiAliasing] = {"ANTIALIASING", v.antiAliasing ? timesText(v.antiAliasing) : "OFF"};
            rows[VSmoothing] = {"TEXTURE SMOOTHING", v.textureSmoothing == 0 ? "OFF" : v.textureSmoothing == 1 ? "TRILINEAR"
                                                                                                                : timesText(v.textureSmoothing)};
            rows[VUpscale] = {"TEXTURE UPSCALING", v.textureUpscale.factor > 1 ? timesText(v.textureUpscale.factor) : "OFF"};
            rows[VFilter] = {"UPSCALING FILTER", v.textureUpscale.filter == bozo::render::UpscaleFilter::Smooth ? "SMOOTH" : "EDGE"};
            const bool pack = h.texturePackFolder && !h.texturePackFolder(packOn(v.texturePack) ? v.texturePack : "").empty();
            rows[VPack] = {"TEXTURE PACK", pack ? onOff(packOn(v.texturePack)) : "NONE"};
            rows[VWide] = {"WIDESCREEN", onOff(v.widescreen)};
            rows[VPause] = {"PAUSE IN BACKGROUND", onOff(v.pauseInBackground)};
            rows[VFast] = {"FAST FORWARD", v.fastForward == 0 ? "UNLIMITED" : timesText(v.fastForward / 100)};
            rows[VApply] = {"APPLY AND SAVE", ""};
            return rows;
        }

        bool videoChange(Menu &m, int line, int delta)
        {
            auto &v = m.video;
            switch (line)
            {
            case VMode:
                v.mode = static_cast<bozo::app::video::WindowMode>(step(kModes, static_cast<int>(v.mode), delta));
                return true;
            case VResolution:
            {
                if (m.resolutions.empty())
                    return false;
                const auto next = step(m.resolutions, std::make_pair(v.width, v.height), delta);
                v.width = next.first, v.height = next.second;
                return true;
            }
            case VScale:
            {
                int i = 2;
                for (size_t k = 0; k < kScales.size(); ++k)
                    if (std::fabs(v.renderScale - kScales[k]) < 0.01f)
                        i = static_cast<int>(k);
                const int n = static_cast<int>(kScales.size());
                v.renderScale = kScales[static_cast<size_t>(((i + delta) % n + n) % n)];
                return true;
            }
            case VFrameRate: v.frameRate = step(frameRates(m), v.frameRate, delta); return true;
            case VAntiAliasing: v.antiAliasing = step(kAntiAliasings, v.antiAliasing, delta); return true;
            case VSmoothing: v.textureSmoothing = step(kSmoothings, v.textureSmoothing, delta); return true;
            case VUpscale:
                v.textureUpscale.factor = static_cast<uint8_t>(step(kUpscales, static_cast<int>(v.textureUpscale.factor), delta));
                return true;
            case VFilter:
                v.textureUpscale.filter = v.textureUpscale.filter == bozo::render::UpscaleFilter::Smooth ? bozo::render::UpscaleFilter::Edge
                                                                                                           : bozo::render::UpscaleFilter::Smooth;
                return true;
            case VPack:
            {
                static std::string s_lastOn; // the folder a set texture_pack named, given back when turned on again
                if (packOn(v.texturePack) && !v.texturePack.empty())
                    s_lastOn = v.texturePack;
                const Hooks h = hooks();
                if (!h.texturePackFolder || h.texturePackFolder(packOn(v.texturePack) ? v.texturePack : s_lastOn).empty())
                    return false;
                v.texturePack = packOn(v.texturePack) ? "off" : s_lastOn;
                return true;
            }
            case VWide: v.widescreen = !v.widescreen; return true;
            case VPause: v.pauseInBackground = !v.pauseInBackground; return true;
            case VFast: v.fastForward = step(kFastForwards, v.fastForward, delta); return true;
            default: return false;
            }
        }

        // Cheats.
        enum CheatLine
        {
            CUnlock,
            CHealth,
            CAmmo,
            CEnergy,
            CNoAi,
            CWeak,
            CCount
        };
        const std::vector<uint32_t> kMasks = {0u, 1u, 0xFu};
        const std::vector<float> kCaps = {-1.0f, 10.0f, 25.0f, 50.0f, 100.0f};
        std::string maskText(uint32_t mask)
        {
            if (mask == 0u)
                return "OFF";
            if (mask == 1u)
                return "PLAYER 1";
            if ((mask & 0xFu) == 0xFu)
                return "ALL PLAYERS";
            std::string players = "PLAYERS";
            for (int p = 0; p < 4; ++p)
                if (mask >> p & 1u)
                    players += " " + std::to_string(p + 1);
            return players;
        }
        std::vector<Row> cheatRows()
        {
            const cheats::Values c = cheats::values();
            std::vector<Row> rows(CCount);
            rows[CUnlock] = {"UNLOCK EVERYTHING", onOff(c.unlockAll)};
            rows[CHealth] = {"INFINITE HEALTH", maskText(c.health)};
            rows[CAmmo] = {"INFINITE AMMO", maskText(c.ammo)};
            rows[CEnergy] = {"INFINITE ENERGY", maskText(c.energy)};
            rows[CNoAi] = {"NO AI", onOff(c.noAi)};
            rows[CWeak] = {"WEAK ENEMIES", c.enemyCap < 0.0f ? "OFF" : "HEALTH " + std::to_string(static_cast<int>(std::lround(c.enemyCap)))};
            return rows;
        }
        bool cheatChange(int line, int delta)
        {
            cheats::Values c = cheats::values();
            const auto mask = [&](uint32_t &m) { m = step(kMasks, m, delta); };
            switch (line)
            {
            case CUnlock: c.unlockAll = !c.unlockAll; break;
            case CHealth: mask(c.health); break;
            case CAmmo: mask(c.ammo); break;
            case CEnergy: mask(c.energy); break;
            case CNoAi: c.noAi = !c.noAi; break;
            case CWeak:
            {
                int i = c.enemyCap < 0.0f ? 0 : 1;
                for (size_t k = 1; k < kCaps.size(); ++k)
                    if (std::fabs(c.enemyCap - kCaps[k]) < 0.5f)
                        i = static_cast<int>(k);
                const int n = static_cast<int>(kCaps.size());
                c.enemyCap = kCaps[static_cast<size_t>(((i + delta) % n + n) % n)];
                break;
            }
            default: return false;
            }
            cheats::setValues(c);
            return true;
        }

        // Save states.
        enum StateLine
        {
            SSlot,
            SSummary,
            SLoad,
            SSave,
            SCount
        };
        std::vector<Row> stateRows(const Hooks &h)
        {
            const int slot = h.stateSlot ? h.stateSlot() : 0;
            std::string summary = h.slotSummary ? upper(h.slotSummary(slot)) : "";
            if (const size_t colon = summary.find(": "); colon != std::string::npos)
                summary = summary.substr(colon + 2);
            std::vector<Row> rows(SCount);
            rows[SSlot] = {"SLOT", std::to_string(slot)};
            rows[SSummary] = {"   " + summary, "", true};
            rows[SLoad] = {"LOAD", ""};
            rows[SSave] = {"SAVE", ""};
            return rows;
        }

        // The top page: the pages this program can offer.
        std::vector<Page> topPages(const Hooks &h)
        {
            std::vector<Page> pages;
            if (h.videoShared && h.videoApply && h.videoShared().open)
                pages.push_back(Page::Video);
            pages.push_back(Page::Cheats);
            if (h.stateSlot && h.saveState && h.loadState)
                pages.push_back(Page::States);
            return pages;
        }
        const char *pageName(Page p)
        {
            switch (p)
            {
            case Page::Video: return "VIDEO";
            case Page::Cheats: return "CHEATS";
            case Page::States: return "SAVE STATES";
            default: return "";
            }
        }

        std::vector<Row> rowsOf(const Menu &m, const Hooks &h)
        {
            switch (m.page)
            {
            case Page::Top:
            {
                std::vector<Row> rows;
                for (const Page p : topPages(h))
                    rows.push_back({pageName(p), ""});
                if (h.quit)
                    rows.push_back({m.quitArmed ? "QUIT GAME: PRESS X AGAIN" : "QUIT GAME", ""});
                return rows;
            }
            case Page::Video: return videoRows(m, h);
            case Page::Cheats: return cheatRows();
            case Page::States: return stateRows(h);
            default: return {};
            }
        }

        int nextLine(const std::vector<Row> &rows, int line, int delta)
        {
            const int n = static_cast<int>(rows.size());
            for (int k = 0; k < n; ++k)
            {
                line = ((line + delta) % n + n) % n;
                if (!rows[static_cast<size_t>(line)].info)
                    return line;
            }
            return line;
        }

        void open(Menu &m, Page page, const Hooks &h)
        {
            m.page = page;
            m.line = 0;
            m.status.clear();
            m.quitArmed = false;
            if (page == Page::Video && h.videoShared)
            {
                const bozo::app::video::Shared s = h.videoShared();
                m.video = s.applied;
                m.resolutions = s.resolutions;
                m.monitorRefresh = s.monitorRefresh;
            }
        }

        // The pages in the pause's own style: its fonts (setTitleFont's title; the selected line at size 0x12 in
        // (0x78, 0x6E, 0x00), the others at 0x10 in (0x46, 0x46, 0x46), as setMenuFont), in pixels of the paused view.
        void drawPause(Guest &g, const Menu &m, const std::vector<Row> &rows)
        {
            g.call(&setTitleFont__Fv_0x167ac8, kSetTitleFont, {});
            g.call(&fontSpritePrintCenteredXY__FiiiPc_0x1c44d8, kFontPrintCenteredXY,
                   {0u, 0x140u, 0x12u, g.string(m.page == Page::Top ? "PC SETTINGS" : pageName(m.page))});
            const int32_t top = m.page == Page::Top ? 0x40 : 0x26, spacing = 0xB;
            for (size_t i = 0; i < rows.size(); ++i)
            {
                const Row &r = rows[i];
                const bool selected = static_cast<int>(i) == m.line && !r.info;
                const uint32_t y = static_cast<uint32_t>(top + static_cast<int32_t>(i) * spacing);
                g.call(&fontSetSize__Fii_0x1c4330, kFontSetSize, {0u, selected ? 0x12u : 0x10u});
                if (selected)
                    g.color(0x78u, 0x6Eu, 0x00u);
                else
                    g.color(0x46u, 0x46u, 0x46u);
                if (m.page == Page::Top)
                    g.call(&fontSpritePrintCenteredXY__FiiiPc_0x1c44d8, kFontPrintCenteredXY, {0u, 0x140u, y, g.string(r.label)});
                else
                {
                    g.call(&fontSpritePrintXY__FiiiPc_0x1c44a8, kFontPrintXY, {0u, 0x3Cu, y, g.string(r.label)});
                    if (!r.value.empty())
                        g.call(&fontSpritePrintCenteredXY__FiiiPc_0x1c44d8, kFontPrintCenteredXY,
                               {0u, 0x1C0u, y, g.string(selected ? "\\ " + r.value + " ]" : r.value)});
                }
            }
            if (!m.status.empty())
            {
                g.call(&fontSetSize__Fii_0x1c4330, kFontSetSize, {0u, 0x10u});
                g.color(0x78u, 0x6Eu, 0x00u);
                g.call(&fontSpritePrintCenteredXY__FiiiPc_0x1c44d8, kFontPrintCenteredXY,
                       {0u, 0x140u, static_cast<uint32_t>(top + static_cast<int32_t>(rows.size()) * spacing + 4), g.string(m.status)});
            }
        }

        // A frame of the open pages: the pad's presses (uiGetInput's flags), then the page drawn.
        void runPage(Guest &g)
        {
            Menu &m = g_menu;
            const Hooks h = hooks();
            // This frame's press: the Options page's from uiGetInput's flags; the pause's from the pausing player's pad
            // (inputGetInput, one press a frame in uiGetInput's order), as the pause reads it.
            uint32_t press = 0u;
            if (g.pause)
            {
                g.call(&inputGetPlayerPad__Fi_0x1d8658, kInputGetPlayerPad, {word(g.rdram, kPlayerThatPaused)});
                const uint32_t pad = g.result();
                static constexpr std::pair<uint32_t, uint32_t> kMasks[] = {{0x40u, kDown}, {0x10u, kUp}, {0x4000u, kCross}, {0x80u, kLeft},
                                                                          {0x20u, kRight}, {0x1000u, kTriangle}, {0x2000u, kCircle}};
                for (const auto &[mask, flag] : kMasks)
                {
                    g.call(&inputGetInput__Fii_0x1d8770, kInputGetInput, {mask, pad});
                    if (g.result() != 0u)
                    {
                        press = flag;
                        break;
                    }
                }
            }
            else
                g.call(&uiGetInput__Fv_0x199880, kUiGetInput, {});
            const auto pressed = [&](uint32_t flag) { return g.pause ? press == flag : word(g.rdram, flag) != 0u; };
            std::vector<Row> rows = rowsOf(m, h);
            if (m.page != Page::Closed && rows.empty())
                m.page = Page::Top, rows = rowsOf(m, h);
            m.line = std::clamp(m.line, 0, std::max(0, static_cast<int>(rows.size()) - 1));

            if (pressed(kTriangle) || pressed(kCircle))
            {
                if (m.quitArmed)
                {
                    m.quitArmed = false; // Back only disarms QUIT GAME
                    g.sound(kSoundBack);
                    rows = rowsOf(m, h);
                    goto draw;
                }
                g.sound(kSoundBack);
                if (m.page == Page::Top)
                {
                    m.page = Page::Closed;
                    return; // the game's page again from the next frame, its cursor on PC SETTINGS
                }
                const Page from = m.page;
                open(m, Page::Top, h);
                const std::vector<Page> pages = topPages(h);
                m.line = std::max(0, indexOf(pages, from));
                rows = rowsOf(m, h);
            }
            else if (pressed(kUp) || pressed(kDown))
            {
                m.line = nextLine(rows, m.line, pressed(kUp) ? -1 : 1);
                m.quitArmed = false;
                g.sound(pressed(kUp) ? kSoundUp : kSoundDown);
                m.status.clear();
            }
            else if (pressed(kLeft) || pressed(kRight))
            {
                const int delta = pressed(kLeft) ? -1 : 1;
                bool changed = false;
                if (m.page == Page::Video)
                    changed = videoChange(m, m.line, delta);
                else if (m.page == Page::Cheats)
                    changed = cheatChange(m.line, delta);
                else if (m.page == Page::States && m.line == SSlot && h.stateSlot && h.setStateSlot)
                {
                    h.setStateSlot(h.stateSlot() + delta);
                    changed = true;
                }
                if (changed)
                {
                    g.sound(kSoundValue);
                    m.status.clear();
                    rows = rowsOf(m, h);
                }
            }
            else if (pressed(kCross))
            {
                if (m.page == Page::Top)
                {
                    const std::vector<Page> pages = topPages(h);
                    if (m.line < static_cast<int>(pages.size()))
                    {
                        g.sound(kSoundAccept);
                        open(m, pages[static_cast<size_t>(m.line)], h);
                        rows = rowsOf(m, h);
                    }
                    else if (h.quit && m.line == static_cast<int>(pages.size()))
                    {
                        // QUIT GAME: a second Cross ends the game (the window closes, as closing it does).
                        g.sound(kSoundAccept);
                        if (m.quitArmed)
                            h.quit();
                        m.quitArmed = !m.quitArmed;
                        rows = rowsOf(m, h);
                    }
                }
                else if (m.page == Page::Video && m.line == VApply && h.videoApply)
                {
                    h.videoApply(m.video);
                    g.sound(kSoundAccept);
                    m.status = "APPLIED AND SAVED";
                }
                else if (m.page == Page::States && (m.line == SLoad || m.line == SSave) && h.stateSlot)
                {
                    const int slot = h.stateSlot();
                    const bool empty = h.slotSummary && upper(h.slotSummary(slot)).find("EMPTY") != std::string::npos;
                    if (m.line == SLoad && empty)
                    {
                        g.sound(kSoundBack);
                        m.status = "SLOT " + std::to_string(slot) + " IS EMPTY";
                    }
                    else
                    {
                        const bool ok = m.line == SLoad ? (h.loadState && h.loadState(slot)) : (h.saveState && h.saveState(slot));
                        g.sound(ok ? kSoundAccept : kSoundBack);
                        m.status = !ok ? "NOT AVAILABLE" : m.line == SLoad ? "LOADING SLOT " + std::to_string(slot) : "SAVING IN SLOT " + std::to_string(slot);
                    }
                }
            }

        draw:
            if (g.pause)
            {
                drawPause(g, m, rows);
                return;
            }
            // The page's sliders hidden, as the game's controls page hides them (uiOptions shows them again on its page).
            g.call(&blankSliders__Fi_0x19de38, kBlankSliders, {0u});
            // Drawn: the title, then each line (the selected one larger and brighter, as the game's), its value to the right.
            g.title(m.page == Page::Top ? "PC SETTINGS" : pageName(m.page));
            const bool wide = m.page == Page::Top;
            const int32_t top = wide ? 0x550 : 0x4A0, spacing = wide ? 0x210 : 0x1A0;
            const float scale = wide ? 1.0f : 0.8f;
            for (size_t i = 0; i < rows.size(); ++i)
            {
                const Row &r = rows[i];
                const bool selected = static_cast<int>(i) == m.line && !r.info;
                const int32_t y = top + static_cast<int32_t>(i) * spacing;
                if (selected)
                    g.color(0x60u, 0x4Cu, 0x36u);
                else
                    g.color(0x30u, 0x26u, 0x1Cu);
                setSizes(g, selected, scale);
                g.print(kLineX, y, r.label);
                if (!r.value.empty())
                    g.printCentered(0x1D00, y, selected ? "\\ " + r.value + " ]" : r.value);
            }
            if (!m.status.empty())
            {
                g.color(0x60u, 0x4Cu, 0x36u);
                setSizes(g, false, scale);
                g.printCentered(0x1400, top + static_cast<int32_t>(rows.size()) * spacing + spacing / 2, m.status);
            }
        }

        // ---- The overrides ----

        // uiGetInput: the game's, then, for the Options page's call, the cursor let onto line 8 and Cross taken there.
        void uiGetInputOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ra = getRegU32(ctx, 31);
            uiGetInput__Fv_0x199880(rdram, ctx, runtime);
            // A call that left (an event due) goes on from the dispatcher: its flags are not all set yet.
            if (ctx->leaving || ra != kOptionsInputReturn || !active(runtime) || !optionsPageIdle(rdram))
                return;
            const uint32_t line = word(rdram, kLine);
            int sound = -1;
            if (line == kLastLine && word(rdram, kDown))
            {
                setWord(rdram, kDown, 0u); // the game would wrap to 0
                setWord(rdram, kLine, kPcLine);
                sound = kSoundDown;
            }
            else if (line == 0u && word(rdram, kUp))
                setWord(rdram, kLine, kPcLine + 1u); // the game's Up then gives 8
            else if (line == kPcLine)
            {
                if (word(rdram, kDown))
                    setWord(rdram, kLine, kLastLine); // the game's Down then wraps to 0
                else if (word(rdram, kCross))
                {
                    setWord(rdram, kCross, 0u); // not the game's accept-and-leave
                    sound = kSoundAccept;
                    open(g_menu, Page::Top, hooks());
                    g_menu.inPause = false;
                }
                setWord(rdram, kLeft, 0u);
                setWord(rdram, kRight, 0u);
            }
            if (sound >= 0)
            {
                Guest g(rdram, ctx, runtime);
                g.sound(sound);
            }
        }

        // uiOptions: the game's page and the PC SETTINGS line under it, or, while they are open, the PC pages instead.
        void uiOptionsOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_menu.page != Page::Closed && (g_menu.inPause || !active(runtime) || !optionsPageIdle(rdram)))
                g_menu.page = Page::Closed;
            const bool pages = g_menu.page != Page::Closed;
            if (!pages)
                uiOptions__Fv_0x1911f0(rdram, ctx, runtime); // returns with pc its return address (or where it left)
            if (active(runtime) && optionsPageIdle(rdram))
            {
                Guest g(rdram, ctx, runtime);
                if (pages)
                    runPage(g);
                else
                {
                    const bool selected = word(rdram, kLine) == kPcLine;
                    if (selected)
                        g.color(0x60u, 0x4Cu, 0x36u);
                    else
                        g.color(0x30u, 0x26u, 0x1Cu);
                    setSizes(g, selected, 1.0f);
                    g.print(kLineX, kPcLineY, "PC SETTINGS");
                }
            }
            if (pages)
                ctx->pc = getRegU32(ctx, 31); // jr $ra: the game's page not run
        }

        // ---- The pause menu (displayDialog's pauseLevelOverlay, types 0 and 16) ----
        // getCurrentMenuSelection(count, pad) moves the cursor (gCurrentMenuSelection) on the pausing player's pad,
        // wrapping at `count` (4 in one player: RESUME, QUIT, SET VIBRATION, ADJUST REAR VIEW; 3 with more players);
        // checkInputForPauseExit returns 2 for Cross; at index `count` the game keeps a hidden TWEAK action, which Cross
        // there would run. So: for the pause's call (return 0x166AE4) the count is one more (PC SETTINGS at `count`),
        // Cross there (return 0x166D64) opens the pages instead, and pauseLevelOverlay draws the line after its menu,
        // or, while the pages are open, the pages instead (displayDialog's loop goes on: 3D, font flush, frame).
        uint32_t g_pauseCount = 0u;
        bool g_pauseMenuShown = false; // the overlay drew its menu this frame (not the QUIT confirmation or the save)
        std::chrono::steady_clock::time_point g_lastPause{};

        void menuSelectionOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (getRegU32(ctx, 31) == kPauseSelectionReturn && active(runtime))
            {
                g_pauseCount = getRegU32(ctx, 4);
                g_pauseMenuShown = true;
                SET_GPR_U32(ctx, 4, g_pauseCount + 1u);
            }
            getCurrentMenuSelection__Fii_0x167af8(rdram, ctx, runtime);
        }

        void pauseExitOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ra = getRegU32(ctx, 31);
            checkInputForPauseExit__Fi_0x167670(rdram, ctx, runtime);
            if (ctx->leaving || ra != kPauseExitReturn || !active(runtime) || !g_pauseMenuShown)
                return;
            if (getRegU32(ctx, 2) == 2u && word(rdram, kCurrentMenuSelection) == g_pauseCount)
            {
                SET_GPR_U32(ctx, 2, 0u); // the pause stays (not TWEAK)
                open(g_menu, Page::Top, hooks());
                g_menu.inPause = true;
            }
        }

        void pauseOverlayOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ra = getRegU32(ctx, 31);
            const bool pause = (ra == kPauseReturn0 || ra == kPauseReturn16) && active(runtime);
            if (pause)
            {
                // A new pause (the overlay not called for a while) starts on the game's menu.
                const auto now = std::chrono::steady_clock::now();
                if (now - g_lastPause > std::chrono::milliseconds(500) && g_menu.inPause)
                    g_menu.page = Page::Closed;
                g_lastPause = now;
                if (g_menu.inPause && g_menu.page != Page::Closed)
                {
                    Guest g(rdram, ctx, runtime);
                    g.pause = true;
                    runPage(g);
                    SET_GPR_U32(ctx, 2, 0u); // the dialog stays
                    ctx->pc = ra;
                    return;
                }
            }
            g_pauseMenuShown = false;
            pauseLevelOverlay__Fv_0x166960(rdram, ctx, runtime);
            if (!pause || ctx->leaving || !g_pauseMenuShown)
                return;
            Guest g(rdram, ctx, runtime);
            g.pause = true;
            g.call(&setMenuFont__Fi_0x167810, kSetMenuFont, {g_pauseCount}); // highlighted as the game's lines
            g.call(&fontSpritePrintCenteredXY__FiiiPc_0x1c44d8, kFontPrintCenteredXY,
                   {0u, 0x140u, 0x61u + 0xBu * g_pauseCount, g.string("PC SETTINGS")});
        }
    }

    Hooks currentHooks() { return hooks(); }
    bool pausePagesOpen() { return g_menu.inPause && g_menu.page != Page::Closed; }

    void setHooks(Hooks h)
    {
        std::lock_guard lock(g_hooksMutex);
        g_hooks = std::move(h);
    }

    void stateLoaded() { g_menu.page = Page::Closed; }

    void install(tmb::Registry &runtime)
    {
        runtime.registerFunction(kUiGetInput, &uiGetInputOverride);
        runtime.registerFunction(kUiOptions, &uiOptionsOverride);
        runtime.registerFunction(kMenuSelection, &menuSelectionOverride);
        runtime.registerFunction(kPauseExit, &pauseExitOverride);
        runtime.registerFunction(kPauseOverlay, &pauseOverlayOverride);
    }
}
