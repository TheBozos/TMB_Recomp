// tmb-headless: Twisted Metal: Black on BozoEngine with no window (phase 4, milestone M0). Every library native, the
// disc image read, BozoEngine's own timing (free mode), TMB's overrides native; the native renderer records its frames.
//
//   tmb-headless [--frames n] [--elf SCUS_971.01] [--iso image] [--pad-script [n=]file]... [--card-dir dir]
//                [--out run.ref] [--report n] [--quiet] [--wav file [--wav-frames a-b]] [--no-sound]
//
// --pad-script n=file: player n's frame-keyed script (1 when omitted; players 2-4 are the multitap's slots, as the
// reference's PS2X_PAD2_SCRIPT..PS2X_PAD4_SCRIPT).
//
// --wav: the sound the mixer made (48 kHz stereo, 16-bit), from game frame a to b (all of it by default); --no-sound:
// the mixer is not brought up at the frames' ends (as before milestone M3).
// Every --report frames (500 by default) and at the end: the frame, the EE clock, the function that waited for the
// frame's end (the game's loop: uiTitle_Main__Fv at the title), the Shell (the menus' object, gp - 0x4148) and the
// overrides that ran.
#include "tmb_host.h"
#include "tmb_port.h"

#include "bozo/app/determinism.h"
#include "bozo/audio/wav.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/host/free_host.h"
#include "tmb_scene.h"
#include "bozo/render/renderer.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

#ifndef BOZO_RUN_DEFAULT_ELF
#define BOZO_RUN_DEFAULT_ELF ""
#endif

int main(int argc, char **argv)
{
    bozo::host::FreeOptions opt;
    opt.elf = BOZO_RUN_DEFAULT_ELF;
    uint64_t report = 500;
    std::string wavPath;
    uint64_t wavFrom = 0, wavTo = UINT64_MAX;
    for (int k = 1; k < argc; ++k)
    {
        const std::string a = argv[k];
        auto value = [&]() -> std::string { return k + 1 < argc ? argv[++k] : ""; };
        if (a == "--frames")
            opt.frames = static_cast<uint32_t>(std::stoul(value()));
        else if (a == "--elf")
            opt.elf = value();
        else if (a == "--iso")
            opt.iso = value();
        else if (a == "--pad-script")
        {
            std::string v = value();
            int player = 1;
            if (v.size() > 2 && v[1] == '=' && v[0] >= '1' && v[0] <= '4')
            {
                player = v[0] - '0';
                v = v.substr(2);
            }
            (player == 1 ? opt.padScript : opt.playerScripts[static_cast<size_t>(player - 2)]) = v;
        }
        else if (a == "--card-dir")
            opt.cardDir = value();
        else if (a == "--out")
            opt.out = value();
        else if (a == "--report")
            report = std::stoull(value());
        else if (a == "--quiet")
            opt.quiet = true;
        else if (a == "--print")
            opt.printGameOutput = true;
        else if (a == "--wav")
            wavPath = value();
        else if (a == "--wav-frames")
        {
            const std::string range = value();
            const size_t dash = range.find('-');
            wavFrom = std::stoull(range.substr(0, dash));
            if (dash != std::string::npos && dash + 1 < range.size())
                wavTo = std::stoull(range.substr(dash + 1));
        }
        else if (a == "--no-sound")
            opt.sound = false;
        else
        {
            std::fprintf(stderr, "usage: tmb-headless [--frames n] [--elf path] [--iso image] [--pad-script [n=]file]... [--card-dir dir] "
                                 "[--out run.ref] [--report n] [--print] [--quiet] [--wav file [--wav-frames a-b]] [--no-sound]\n");
            return 2;
        }
    }
    if (opt.elf.empty())
    {
        std::fprintf(stderr, "tmb-headless: no ELF (--elf)\n");
        return 2;
    }
    tmb::host::setElfDirectory(std::filesystem::path(opt.elf).parent_path().string());
    if (opt.iso.empty())
        opt.iso = tmb::findDiscImage(std::filesystem::path(opt.elf).parent_path().string(), "SCUS_971.01");
    if (opt.iso.empty())
    {
        std::fprintf(stderr, "tmb-headless: no disc image (--iso, TMB_ISO, or an .iso beside the ELF)\n");
        return 2;
    }
    // Headless: no host clock reaches the game (determinism.h).
    bozo::det::setEnabled(true);

    const bozo::port::Port *port = bozo::port::port();
    bozo::port::Registry registry;
    port->install(registry);
    const std::vector<uint32_t> missing = bozo::port::undeclared(registry, tmb_external_table, tmb_external_count);
    if (!missing.empty())
    {
        std::fprintf(stderr, "%zu registered overrides are not externals of the generated code (regenerate):", missing.size());
        for (uint32_t a : missing)
            std::fprintf(stderr, " 0x%x", a);
        std::fprintf(stderr, "\n");
        return 2;
    }

    bozo::host::FreeHost host(opt);
    bozo::ee::Dispatcher d(host);
    tmb::scene::Binding scene(&d.runtime());
    d.setFunctions(tmb_function_table, tmb_function_count, tmb_resume_table, tmb_resume_count);
    std::string error;
    if (!host.setUp(d, tmb_external_table, tmb_external_count, &registry, error))
    {
        std::fprintf(stderr, "tmb-headless: %s\n", error.c_str());
        return 2;
    }
    std::printf("tmb-headless: %s, %zu overrides, ELF %s, disc %s\n", port->title, registry.functions().size(), opt.elf.c_str(),
                opt.iso.c_str());

    // What the game is doing at a frame's end.
    std::string waiter;
    uint32_t shell = 0;
    // The shell's screen: gp - 0x3d2c, which uiMain__Fv switches on through its table at 0x4eae90 (case 0, 0x18ee14,
    // calls uiTitle_Main__Fv), and the frame it last changed at; -1 outside the shell's loop.
    int32_t screen = -1;
    uint64_t screenSince = 0;
    std::map<int32_t, uint64_t> screens; // frames per screen
    const auto t0 = std::chrono::steady_clock::now();
    std::map<std::string, uint64_t> waiters; // the frames each loop function ended
    auto state = [&](uint64_t frame) {
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const bozo::render::Renderer &r = bozo::render::Renderer::instance();
        const auto published = r.published();
        const auto overlay = r.publishedOverlay();
        std::printf("frame %llu: EE cycle %llu (%llu vblanks), loop %s, Shell 0x%x, screen %d%s since frame %llu; renderer: 3D "
                    "%zu batches, 2D %zu batches, movie picture %s; %.1f s\n",
                    static_cast<unsigned long long>(frame), static_cast<unsigned long long>(d.now()),
                    static_cast<unsigned long long>(d.kernel().vblankCount()), waiter.c_str(), shell, screen,
                    screen == 0 ? " (uiTitle_Main__Fv)" : "", static_cast<unsigned long long>(screenSince),
                    published ? published->batches.size() : 0u, overlay ? overlay->batches.size() : 0u,
                    r.backdrop() ? "yes" : "no", secs);
        std::fflush(stdout);
    };
    host.onFrame = [&](uint64_t frame, const R5900Context *ctx) {
        if (ctx)
        {
            // The function that called the frame's end (timer1Handler's caller, or sceGsSyncV's): the game's loop.
            waiter = host.functionAt(ctx->r[31].uw[0]);
            const std::string fn = waiter.substr(0, waiter.find('+'));
            ++waiters[fn];
            const uint32_t gp = ctx->r[28].uw[0];
            int32_t now = -1;
            if (gp)
            {
                std::memcpy(&shell, d.runtime().memory.ram() + ((gp - 0x4148u) & 0x1FFFFFCu), 4);
                if (fn == "uiMain__Fv")
                    std::memcpy(&now, d.runtime().memory.ram() + ((gp - 0x3D2Cu) & 0x1FFFFFCu), 4);
            }
            if (now != screen)
            {
                screen = now;
                screenSince = frame;
            }
            ++screens[now];
        }
        if (report && (frame + 1) % report == 0 && !opt.quiet)
            state(frame + 1);
    };
    bozo::audio::WavWriter wav;
    if (!wavPath.empty())
    {
        if (!wav.open(wavPath))
        {
            std::fprintf(stderr, "tmb-headless: cannot write %s\n", wavPath.c_str());
            return 2;
        }
        host.onAudio = [&](uint64_t frame, const int16_t *samples, size_t count) {
            if (frame >= wavFrom && frame <= wavTo)
                wav.write(samples, count);
        };
    }
    const uint64_t frames = host.run();
    if (wav.isOpen())
    {
        std::printf("wav: %s, %.1f s of sound\n", wavPath.c_str(), wav.frames() / 48000.0);
        wav.close();
    }
    state(frames);
    const bozo::host::FreeStats &s = host.stats();
    std::printf("end: %s; %llu frames; %llu library calls, %llu kernel calls, %llu override calls, %llu VU0 programs, %llu "
                "hardware accesses (%llu to registers the I/O model does not know)\n",
                host.endReason().empty() ? "every thread ended" : host.endReason().c_str(), static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(s.libraryCalls), static_cast<unsigned long long>(s.kernelCalls),
                static_cast<unsigned long long>(s.overrideCalls), static_cast<unsigned long long>(s.vu0Calls),
                static_cast<unsigned long long>(s.ioAccesses), static_cast<unsigned long long>(s.ioUnknown));
    std::printf("frame ends by loop function:");
    for (const auto &[fn, n] : waiters)
        std::printf(" %s %llu;", fn.c_str(), static_cast<unsigned long long>(n));
    std::printf("\nframes per shell screen (-1: outside the shell's loop):");
    for (const auto &[sc, n] : screens)
        std::printf(" %d: %llu;", sc, static_cast<unsigned long long>(n));
    std::printf("\noverride calls:");
    for (const auto &[a, n] : host.overrideCalls())
        std::printf(" 0x%x:%llu", a, static_cast<unsigned long long>(n));
    std::printf("\n");
    // The pads the game found and read: each socket's reads (port.slot), and whether the multitap was open.
    const bozo::libs::PadState &pads = host.libraries().pad;
    std::printf("pad reads (port.slot: reads; multitap %s):", pads.multitap ? "open" : "closed");
    for (int p = 0; p < 2; ++p)
        for (int sl = 0; sl < 4; ++sl)
            std::printf(" %d.%d: %llu;", p + 1, sl, static_cast<unsigned long long>(pads.socketReads[p][sl]));
    std::printf("\n");
    for (const auto &[name, n] : s.unanswered)
        std::printf("no native answer: %s (%llu)\n", name.c_str(), static_cast<unsigned long long>(n));
    return host.endReason().rfind("FAULT", 0) == 0 ? 1 : 0;
}
