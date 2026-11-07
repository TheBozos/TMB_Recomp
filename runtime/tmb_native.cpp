// Native PC rendering, TMB's frame (docs/RECOMP_NOTES.md "Native PC port"): TMB's objects, particles, 2D and movies are
// drawn natively at their hooks (runtime/tmb_meshes.cpp, tmb_particles.cpp, tmb_2d.cpp) and submitted to BozoEngine's
// renderer (bozo::render::Renderer); this file publishes each frame and keeps what the hooks share: whether a match or
// the menus are under way, the screen, the view's clip, the fog colour, the lens flares' depth.
//
// On BozoEngine nothing reads the game's VIF1 or GS packets: the adapter that translated the VU1 programs from the VIF1
// stream, the GS front end and its diagnostic modes stayed on the fork (BozoEngine's phase4-inventory.md section 3).
//
// TMB_NATIVE=1 (the default) turns it on.

#include "tmb_port.h"

#include "bozo/app/determinism.h"
#include "bozo/render/renderer.h"
#include "tmb_host.h"
#include "tmb_meshes.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tmb::twod
{
    void publish();
    bool viewClip(const uint8_t *rdram, float (&clip)[4]);
}

namespace tmb::native
{
    namespace
    {
        // A match is under way while its frames end (the end of real-time processing, onFrameEnd) less than 100 ms
        // apart on the host's clock; the menus (the shell overlay's loop) end none. Deterministic mode: the clock is
        // the game frames ended (a sixtieth of a second each), so which frames count as a match's or a menu's is the
        // kinds of the frames before, never the host's timing.
        double hostSeconds()
        {
            if (bozo::det::enabled())
                return static_cast<double>(bozo::det::frame()) / 60.0;
            static const auto s_start = std::chrono::steady_clock::now();
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - s_start).count();
        }
        double g_lastMatchFrame = -1.0, g_lastMenuFrame = -1.0;
        bool inMatch() { return g_lastMatchFrame >= 0.0 && hostSeconds() - g_lastMatchFrame < 0.1; }

        // The menus natively too (TMB_NATIVE_MENUS=0: not drawn). Their frames end at sceGsSyncV (onMenuFrameEnd).
        bool menusNative()
        {
            static const bool s_on = []
            {
                const char *value = std::getenv("TMB_NATIVE_MENUS");
                return !(value && *value == '0');
            }();
            return s_on && !inMatch() && g_lastMenuFrame >= 0.0 && hostSeconds() - g_lastMenuFrame < 0.1;
        }

        const uint8_t *g_rdram = nullptr; // guest memory, from the first hier (setGuestMemory)

        // The fog colour the GS fogs with: fogUpdate's A+D packet (FOGCOL, then register 0x3d), which the game builds
        // for the GS. Until the game's memory is known, `fallback`.
        uint32_t gameFogColour(uint32_t fallback)
        {
            if (!g_rdram)
                return fallback;
            constexpr uint32_t kFogPacket = 0x0049F5D0u;
            if (g_rdram[(kFogPacket + 8u) & 0x1ffffffu] != 0x3du)
                return fallback;
            uint32_t value;
            std::memcpy(&value, g_rdram + (kFogPacket & 0x1ffffffu), sizeof(value));
            return value & 0xffffffu;
        }

        // The draws that follow clipped to the view the game draws now (a split screen's: the GS scissor's cut).
        void viewClip()
        {
            float clip[4];
            if (g_rdram && tmb::twod::viewClip(g_rdram, clip))
                bozo::render::Renderer::instance().setClip(clip[0], clip[1], clip[2], clip[3]);
            else
                bozo::render::Renderer::instance().clearClip();
        }

        // What the native frame starts from, as the GS frame was cleared before the views drew: the fog colour in a
        // match (where nothing is drawn, a sky left to the fog, it shows; viewGetBgColor's until fogUpdate's is known),
        // black in the menus.
        constexpr uint32_t kBgColour = 0x004E04F0u;

        uint32_t frameClearColour()
        {
            if (!g_rdram || menusNative())
                return 0u;
            uint32_t background;
            std::memcpy(&background, g_rdram + kBgColour, 4);
            return gameFogColour(background & 0xffffffu);
        }

        // TMB draws a 640 x 256 field in PAL, 640 x 224 in NTSC (it shows each one twice as tall): the screen is that
        // many lines below 2048 - 128 (2048 - 112).
        bozo::render::Screen screen()
        {
            bozo::render::Screen screen;
            // A match draws fields (the frame's every other line), the shell's menus whole frames.
            const float lines = menusNative() ? 2.0f : 1.0f;
            screen.originY = 2048.0f - 112.0f * lines; // NTSC: a 640 x 224 field
            screen.height = 224.0f * lines;
            // GS Z is z / w itself (the microprograms write FTOI4 of z / w into XYZF2's 24-bit Z, that word over 16).
            screen.gsZScale = 1.0f;
            return screen;
        }

        uint64_t g_group = 0; // the draw groups so far (the stream's programs: none without the VIF1 stream)

        // Publishes what was submitted and starts the next frame.
        void publish()
        {
            bozo::render::Renderer &renderer = bozo::render::Renderer::instance();
            tmb::meshes::flushUnsent(); // a view whose chain hier__Fi did not kick itself (the menus): its objects now
            tmb::twod::publish();       // the 2D the game's rewritten builders drew (runtime/tmb_2d.cpp), on this frame's screen
            renderer.endFrame();
            renderer.beginFrame(screen());
            renderer.setClearColour(frameClearColour());
            tmb::meshes::endFrame();
        }
    }

    void setGuestMemory(const uint8_t *rdram) { g_rdram = rdram; }

    bool enabled()
    {
        static const bool on = []
        {
            tmb::host::setEnvDefault("TMB_NATIVE", "1"); // the default: the native port (TMB_NATIVE=0 opts out)
            const char *value = std::getenv("TMB_NATIVE");
            return value && *value == '1';
        }();
        return on;
    }

    // For the particles drawn natively (runtime/tmb_particles.cpp): whether the effects are drawn natively now (a match
    // or a native menu), and the group the next natively drawn program gets.
    bool effectsInStream() { return enabled() && (inMatch() || menusNative()); }

    uint64_t nextStreamGroup() { return g_group + 1u; }

    // For the movie pictures (runtime/tmb_2d.cpp): whether the shell's menus are drawn natively now.
    bool menusDrawn() { return enabled() && menusNative(); }

    // No VIF1 stream is read on BozoEngine (the fork's TMB_NATIVE_VIF comparison mode is gone).
    bool vifObserved() { return false; }

    // The draws that follow clipped to the view the game draws now (tmb_meshes.cpp, at a view's kick).
    void applyViewClip() { viewClip(); }

    // The fog colour the GS fogs with (fogUpdate's FOGCOL), for the draws made outside the stream (0 until known).
    uint32_t fogColour() { return gameFogColour(0u); }

    void install()
    {
        if (!enabled())
            return;
        bozo::render::Renderer &renderer = bozo::render::Renderer::instance();
        renderer.setEnabled(true);
        renderer.beginFrame(screen());
        renderer.setPresentsAlone(true); // the native frame is the whole picture
    }

    // Lens-flare occlusion samples the previous native frame. Availability does not expire on a host hitch.
    // Disabling this selects the legacy
    // particle diagnostic, whose unowned SDK readback is explicitly unsupported by the native engine.
    bool flareDepthNative()
    {
        static const bool s_on = []
        {
            const char *value = std::getenv("TMB_NATIVE_FLARE_DEPTH");
            return !(value && *value == '0');
        }();
        return s_on && enabled();
    }

    // Native lens flares sample a 4 x 4 block at game screen coordinates from the last presented frame.
    // Missing samples stay far (zero); no image definition or transfer packet is read.
    bool flareDepthBlock(uint32_t x0, uint32_t y0, uint32_t z[16])
    {
        std::fill(z, z + 16, 0u);
        if (!flareDepthNative())
            return false;
        if (bozo::det::enabled()) // deterministic mode: all far (the GPU's depth is host timing)
            return true;
        bozo::render::Renderer &renderer = bozo::render::Renderer::instance();
        renderer.wantDepth();
        const bozo::render::Screen view = screen();
        for (uint32_t y = 0; y < 4u; ++y)
            for (uint32_t x = 0; x < 4u; ++x)
            {
                uint32_t sample = 0;
                if (renderer.depthAt(view.originX + static_cast<float>((x0 + x) & 0x7ffu) + 0.5f,
                                     view.originY + static_cast<float>((y0 + y) & 0x7ffu) + 0.5f, sample))
                    z[y * 4u + x] = std::min<uint32_t>(sample, 0xffffffu);
            }
        return true;
    }

    // The end of a frame's real-time processing (a match frame): publish what was submitted, start the next frame.
    void onFrameEnd()
    {
        if (!enabled())
            return;
        g_lastMatchFrame = hostSeconds();
        publish();
    }

    // A native frame end (timer 1 or sceGsSyncV) outside a match: the end of a menu frame; publishes what was drawn
    // of it (empty with TMB_NATIVE_MENUS=0, so the last match frame does not stay over a menu).
    void onMenuFrameEnd()
    {
        if (!enabled() || inMatch())
            return;
        g_lastMenuFrame = hostSeconds();
        publish();
    }

    // Whether a match is under way (its frames ending less than 100 ms apart).
    bool matchUnderWay() { return inMatch(); }
}
