// tmb: Twisted Metal: Black on BozoEngine in a window, in real time (phase 4, milestones M1 and M2). The free host as
// tmb-headless runs it (every library native, the disc image read, TMB's overrides native), with the host's clock: the
// game's frames held to the field rate (TMB's native frame end, bozo::app::FramePacer), each one presented in an SDL3
// window through OpenGL 3.3 (bozo::app::runWindow). The players' pads: the keyboard, mouse and up to four gamepads
// through input.ini's bindings (bozo::app::DevicePad; with the game's multitap, players 2-4 are its slots), or the
// frame-keyed scripts given.
//
//   tmb [--iso image] [--elf SCUS_971.01] [--card-dir dir] [--width w --height h] [--fullscreen] [--no-vsync]
//       [--render-scale s] [--capture-dir dir] [--capture-frames n,n,...] [--quit-after n] [--pad-script [n=]file]...
//       [--no-sound] [--audio-latency ms] [--audio-stats] [--wav file]
//
// The user folder (%APPDATA%\TMB_Recomp, as the fork's TMB; PS2X_USER_DIR overrides it) holds input.ini, the memory
// cards (mc0, mc1; --card-dir overrides) and the logs (logs/TMB_Recomp.log). Without --elf the game data is looked for
// as the fork's TMB did (<exe>/game/SCUS_971.01, then the same in the user folder).
//
// F8 saves the window as a PNG (--capture-dir, the current folder by default); --capture-frames saves it at the first
// present after each of those game frames. F11 / Alt+Enter: fullscreen. --quit-after: close after that many frames.
// PS2X_FRAME_STATS=1: the frames' work times every 2 s (the FramePacer).
//
// Sound (milestone M3): the mixer's samples, made on the EE clock at each frame's end, go to the default playback device
// (bozo::app::AudioDevice: a queue of --audio-latency ms, 32 by default, plus the device's buffer). --audio-stats: the
// queue, latency and underruns every 2 s. --wav (or PS2X_AUDIO_RECORD): what the mixer made, also to a WAV file.
#include "tmb_host.h"
#include "tmb_port.h"

#include "bozo/app/app.h"
#include "bozo/app/audio_device.h"
#include "bozo/app/determinism.h"
#include "bozo/audio/wav.h"
#include "bozo/app/device_pad.h"
#include "bozo/app/input_bindings.h"
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
    // The fork's TMB identity (its PS2X_APP_* CMake values): the same user folder, so the cards and input.ini carry over.
    bozo::app::Identity identity;
    identity.title = "Twisted Metal: Black";
    identity.folder = "TMB_Recomp";
    identity.xdgFolder = "tmb_recomp";
    identity.bootElf = "SCUS_971.01";
    identity.dataFolder = "game";
    bozo::app::setIdentity(identity);
    bozo::app::startLogging();
    bozo::app::installCrashHandler();

    bozo::host::FreeOptions opt;
    opt.elf = BOZO_RUN_DEFAULT_ELF;
    opt.realTime = true;
    opt.quiet = true;
    bozo::app::WindowOptions window;
    window.title = "Twisted Metal: Black (BozoEngine)";
    bozo::app::AudioOptions audio;
    bool sound = true;
    std::string wavPath;
    if (const char *record = std::getenv("PS2X_AUDIO_RECORD"); record && *record)
        wavPath = record;
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
        {
            // [n=]file: player n's script (1 when omitted; players 2-4: the multitap's slots)
            std::string v = value();
            int player = 1;
            if (v.size() > 2 && v[1] == '=' && v[0] >= '1' && v[0] <= '4')
            {
                player = v[0] - '0';
                v = v.substr(2);
            }
            (player == 1 ? opt.padScript : opt.playerScripts[static_cast<size_t>(player - 2)]) = v;
        }
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
        else if (a == "--no-sound")
            sound = false;
        else if (a == "--audio-latency")
            audio.latencyMs = std::stod(value());
        else if (a == "--audio-stats")
            audio.stats = true;
        else if (a == "--wav")
            wavPath = value();
        else
        {
            std::fprintf(stderr, "usage: tmb [--iso image] [--elf path] [--card-dir dir] [--width w] [--height h] [--fullscreen] "
                                 "[--no-vsync] [--render-scale s] [--capture-dir dir] [--capture-frames n,n,...] [--quit-after n] "
                                 "[--pad-script [n=]file]... [--print] [--no-sound] [--audio-latency ms] [--audio-stats] [--wav file]\n");
            return 2;
        }
    }
    std::error_code ec;
    if (opt.elf.empty() || !std::filesystem::is_regular_file(opt.elf, ec))
    {
        std::string searched;
        const std::filesystem::path found = bozo::app::findGameData(&searched);
        if (found.empty())
        {
            std::fprintf(stderr, "tmb: no ELF (--elf); looked for the game data at\n%s", searched.c_str());
            return 2;
        }
        opt.elf = found.string();
    }
    // The memory cards in the user folder (<user>/mc0, mc1: the fork's layout, its saves read as they are).
    if (opt.cardDir.empty() && !bozo::app::userDirectory().empty())
        opt.cardDir = bozo::app::userDirectory().string();
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
    bool scripted = !opt.padScript.empty();
    for (const std::string &script : opt.playerScripts)
        scripted = scripted || !script.empty();
    if (!scripted)
        host.libraries().pad.input = &pad;
    const std::string pads = scripted ? std::string("the scripts") : "the PC's devices (" + bozo::app::input::configPath() + ")";
    std::fprintf(stderr, "tmb: user folder %s, memory cards in %s, pads from %s\n", bozo::app::userDirectory().string().c_str(),
                 opt.cardDir.empty() ? "memory" : opt.cardDir.c_str(), pads.c_str());
    std::fprintf(stderr, "tmb: %s, %zu overrides, ELF %s, disc %s\n", port->title, registry.functions().size(), opt.elf.c_str(),
                 opt.iso.c_str());

    // The sound: what the mixer makes at each frame's end, to the device (and the WAV file).
    bozo::app::AudioDevice device(audio);
    bozo::audio::WavWriter wav;
    if (!wavPath.empty() && !wav.open(wavPath))
        std::fprintf(stderr, "tmb: cannot write %s\n", wavPath.c_str());
    if (sound)
        device.open();
    if (device.isOpen() || wav.isOpen())
        host.onAudio = [&](uint64_t, const int16_t *samples, size_t count) {
            device.push(samples, count);
            wav.write(samples, count);
        };

    bozo::app::Game game;
    game.run = [&]
    {
        const uint64_t frames = host.run();
        if (device.isOpen())
        {
            const bozo::app::AudioDevice::Stats a = device.stats();
            std::fprintf(stderr, "tmb: sound: %.1f s pushed, %llu frames dropped, %llu underruns (%.1f ms of silence); queue %.1f ms "
                                 "(%.1f-%.1f) + device %.1f ms; ratio %.4f\n",
                         a.pushed / 48000.0, static_cast<unsigned long long>(a.dropped), static_cast<unsigned long long>(a.underruns),
                         a.missing / 48.0, a.queueMs, a.minQueueMs, a.maxQueueMs, a.deviceMs, a.ratio);
            device.close();
        }
        wav.close();
        std::fprintf(stderr, "tmb: the game ended after %llu frames: %s\n", static_cast<unsigned long long>(frames),
                     host.endReason().empty() ? "every thread ended" : host.endReason().c_str());
        return host.endReason().rfind("FAULT", 0) == 0 ? 1 : 0;
    };
    game.stop = [&] { host.requestStop(); };
    game.framesEnded = [&] { return host.framesEnded(); };
    return bozo::app::runWindow(window, game, &pad);
}
