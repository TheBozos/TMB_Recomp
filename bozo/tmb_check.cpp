// tmb-check: BozoEngine's call-log checker (bozo-run: BozoEngine's tools/run/README.md) with TMB's overrides running
// natively (BozoEngine's kernel.md section 3b), each call checked against the reference's log:
//   tmb-check <log.clg> --pinned --ref <run.ref> [--iso <image>] [--pad-script <file>] [--overrides log] ...
// The ELF defaults to the one this build was generated from.
//
// --window (builds with TMB_WINDOW): the same pinned run, its frames shown in a window as they are made (BozoEngine's
// window, the renderer's present), to capture the pictures of chosen frames (milestone M5's pixel comparison):
//   tmb-check <log.clg> ... --window --capture-frames n,n,... [--capture-dir dir] [--width w --height h]
//             [--render-scale s] [--aspect a]
// --capture-frames n: the picture once n frames have ended, i.e. the reference record n - 1's frame (frame_<n>.png). The
// run is held at that frame's end until the window has read it back, so the picture is exactly that frame's. The window
// is 640x448 at render scale 1 by default; --aspect is the picture's shape inside it (4:3 by default, letterboxed).
#include "run_main.h"

#if TMB_CHECK_WINDOW
#include "bozo/app/determinism.h"
#include "bozo/app/window.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <sstream>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef BOZO_RUN_DEFAULT_ELF
#define BOZO_RUN_DEFAULT_ELF ""
#endif

#if TMB_CHECK_WINDOW
namespace
{
    // The run held at a capture frame's end until the window read that frame's picture back.
    struct CaptureGate
    {
        std::mutex mutex;
        std::condition_variable wake;
        std::set<uint64_t> frames;
        uint64_t captured = 0; // the last frame count the window captured at
        bool closing = false;
    };

    CaptureGate &gate()
    {
        static CaptureGate s_gate;
        return s_gate;
    }

    int runWindowed(const std::vector<char *> &args)
    {
        bozo::app::WindowOptions window;
        window.title = "Twisted Metal: Black (BozoEngine, pinned)";
        window.width = 640;
        window.height = 448;
        window.renderScale = 1.0f;
        window.aspect = 4.0f / 3.0f; // the picture's shape, letterboxed in the window (as the fork's window and tmb)
        window.panel = false;
        window.stats = false;
        static std::vector<char *> rest;
        for (size_t k = 0; k < args.size(); ++k)
        {
            const std::string a = args[k];
            auto value = [&]() -> std::string { return k + 1 < args.size() ? args[++k] : ""; };
            if (a == "--window")
                continue;
            if (a == "--capture-frames")
            {
                std::stringstream list(value());
                for (std::string item; std::getline(list, item, ',');)
                    if (!item.empty())
                        window.captureFrames.push_back(std::stoull(item));
            }
            else if (a == "--capture-dir")
                window.captureDir = value();
            else if (a == "--width")
                window.width = std::stoi(value());
            else if (a == "--height")
                window.height = std::stoi(value());
            else if (a == "--render-scale")
                window.renderScale = std::stof(value());
            else if (a == "--aspect")
                window.aspect = std::stof(value());
            else
                rest.push_back(args[k]);
        }

        gate().frames.insert(window.captureFrames.begin(), window.captureFrames.end());
        bozo::run::setFrameHook([](uint64_t framesEnded) {
            CaptureGate &g = gate();
            std::unique_lock<std::mutex> lock(g.mutex);
            if (!g.frames.count(framesEnded))
                return;
            // Held until the window has shown and read back this frame (30 s at most: a closed window lets it go).
            if (!g.wake.wait_for(lock, std::chrono::seconds(30), [&] { return g.closing || g.captured >= framesEnded; }))
                std::fprintf(stderr, "tmb-check: no capture of frame %llu within 30 s\n", static_cast<unsigned long long>(framesEnded));
        });
        window.onCaptured = [](uint64_t frames) {
            CaptureGate &g = gate();
            {
                std::lock_guard<std::mutex> lock(g.mutex);
                g.captured = std::max(g.captured, frames);
            }
            g.wake.notify_all();
        };

        bozo::app::Game game;
        game.run = [] { return bozo::run::main(static_cast<int>(rest.size()), rest.data()); };
        game.stop = [] {
            CaptureGate &g = gate();
            {
                std::lock_guard<std::mutex> lock(g.mutex);
                g.closing = true;
            }
            g.wake.notify_all();
        };
        game.framesEnded = [] { return bozo::det::frame(); };
        return bozo::app::runWindow(window, game, nullptr);
    }
}
#endif

int main(int argc, char **argv)
{
    std::vector<char *> args(argv, argv + argc);
    bool hasElf = false, windowed = false;
    for (int i = 1; i < argc; ++i)
    {
        hasElf = hasElf || std::strcmp(argv[i], "--elf") == 0;
        windowed = windowed || std::strcmp(argv[i], "--window") == 0;
    }
    static char elfFlag[] = "--elf";
    static std::string elf = BOZO_RUN_DEFAULT_ELF;
    if (!hasElf && !elf.empty() && argc >= 2)
    {
        args.push_back(elfFlag);
        args.push_back(elf.data());
    }
    if (windowed)
    {
#if TMB_CHECK_WINDOW
        return runWindowed(args);
#else
        std::fprintf(stderr, "tmb-check: --window needs a build with TMB_WINDOW\n");
        return 2;
#endif
    }
    return bozo::run::main(static_cast<int>(args.size()), args.data());
}
