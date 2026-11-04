// tmb: Twisted Metal: Black on BozoEngine in a window, in real time (phase 4, milestone M1). The free host as
// tmb-headless runs it (every library native, the disc image read, TMB's overrides native), with the host's clock: the
// game's frames held to the field rate (TMB's native frame end, bozo::app::FramePacer), each one presented in an SDL3
// window through OpenGL 3.3 (bozo::app::runWindow), the keyboard and the first gamepad as port 1's pad.
//
//   tmb [--iso image] [--elf SCUS_971.01] [--card-dir dir] [--width w --height h] [--fullscreen] [--no-vsync]
//       [--render-scale s] [--capture-dir dir] [--capture-frames n,n,...] [--quit-after n] [--pad-script file]
//
// F8 saves the window as a PNG (--capture-dir, the current folder by default); --capture-frames saves it at the first
// present after each of those game frames. F11 / Alt+Enter: fullscreen. --quit-after: close after that many frames.
// PS2X_FRAME_STATS=1: the frames' work times every 2 s (the FramePacer).
#include "tmb_host.h"
#include "tmb_port.h"

#include "bozo/app/determinism.h"
#include "bozo/app/device_pad.h"
#include "bozo/app/window.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/host/free_host.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

#ifndef BOZO_RUN_DEFAULT_ELF
#define BOZO_RUN_DEFAULT_ELF ""
#endif

int main(int argc, char **argv)
{
    bozo::host::FreeOptions opt;
    opt.elf = BOZO_RUN_DEFAULT_ELF;
    opt.realTime = true;
    opt.quiet = true;
    bozo::app::WindowOptions window;
    window.title = "Twisted Metal: Black (BozoEngine)";
    for (int k = 1; k < argc; ++k)
    {
        const std::string a = argv[k];
        auto value = [&]() -> std::string { return k + 1 < argc ? argv[++k] : ""; };
        if (a == "--elf")
            opt.elf = value();
        else if (a == "--iso")
            opt.iso = value();
        else if (a == "--card-dir")
            opt.cardDir = value();
        else if (a == "--pad-script")
            opt.padScript = value();
        else if (a == "--width")
            window.width = std::stoi(value());
        else if (a == "--height")
            window.height = std::stoi(value());
        else if (a == "--fullscreen")
            window.fullscreen = true;
        else if (a == "--no-vsync")
            window.vsync = false;
        else if (a == "--render-scale")
            window.renderScale = std::stof(value());
        else if (a == "--capture-dir")
            window.captureDir = value();
        else if (a == "--capture-frames")
        {
            std::stringstream list(value());
            for (std::string item; std::getline(list, item, ',');)
                if (!item.empty())
                    window.captureFrames.push_back(std::stoull(item));
        }
        else if (a == "--quit-after")
            window.quitAfterFrames = std::stoull(value());
        else if (a == "--print")
            opt.printGameOutput = true;
        else
        {
            std::fprintf(stderr, "usage: tmb [--iso image] [--elf path] [--card-dir dir] [--width w] [--height h] [--fullscreen] "
                                 "[--no-vsync] [--render-scale s] [--capture-dir dir] [--capture-frames n,n,...] [--quit-after n] "
                                 "[--pad-script file] [--print]\n");
            return 2;
        }
    }
    if (opt.elf.empty())
    {
        std::fprintf(stderr, "tmb: no ELF (--elf)\n");
        return 2;
    }
    tmb::host::setElfDirectory(std::filesystem::path(opt.elf).parent_path().string());
    if (opt.iso.empty())
        opt.iso = tmb::findDiscImage(std::filesystem::path(opt.elf).parent_path().string(), "SCUS_971.01");
    if (opt.iso.empty())
    {
        std::fprintf(stderr, "tmb: no disc image (--iso, TMB_ISO, or an .iso beside the ELF)\n");
        return 2;
    }
    // Real time: the host's clock reaches the game (frame steps, the EE charged the real time a frame took).
    bozo::det::setEnabled(false);

    const bozo::port::Port *port = bozo::port::port();
    bozo::port::Registry registry;
    port->install(registry);
    const std::vector<uint32_t> missing = bozo::port::undeclared(registry, tmb_external_table, tmb_external_count);
    if (!missing.empty())
    {
        std::fprintf(stderr, "%zu registered overrides are not externals of the generated code (regenerate)\n", missing.size());
        return 2;
    }

    bozo::host::FreeHost host(opt);
    bozo::ee::Dispatcher d(host);
    d.setFunctions(tmb_function_table, tmb_function_count, tmb_resume_table, tmb_resume_count);
    std::string error;
    if (!host.setUp(d, tmb_external_table, tmb_external_count, &registry, error))
    {
        std::fprintf(stderr, "tmb: %s\n", error.c_str());
        return 2;
    }
    bozo::app::DevicePad pad;
    if (opt.padScript.empty())
        host.libraries().pad.input = &pad;
    std::fprintf(stderr, "tmb: %s, %zu overrides, ELF %s, disc %s\n", port->title, registry.functions().size(), opt.elf.c_str(),
                 opt.iso.c_str());

    bozo::app::Game game;
    game.run = [&]
    {
        const uint64_t frames = host.run();
        std::fprintf(stderr, "tmb: the game ended after %llu frames: %s\n", static_cast<unsigned long long>(frames),
                     host.endReason().empty() ? "every thread ended" : host.endReason().c_str());
        return host.endReason().rfind("FAULT", 0) == 0 ? 1 : 0;
    };
    game.stop = [&] { host.requestStop(); };
    game.framesEnded = [&] { return host.framesEnded(); };
    return bozo::app::runWindow(window, game, &pad);
}
