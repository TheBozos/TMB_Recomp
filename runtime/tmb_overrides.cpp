// Game-specific runtime setup for Twisted Metal: Black (USA, SCUS-97101: the addresses are its executable's), on
// BozoEngine (bozo/port.h). The fork's scheduler experiments, GS lag, VIF1 thread and IOP defaults stayed behind with
// the emulation (BozoEngine's phase4-inventory.md section 3).

#include "tmb_port.h"

#include "bozo/app/determinism.h"
#include "bozo/app/frame_pacer.h"
#include "bozo/ee/kernel.h"
#include "bozo/libs/libraries.h"
#include "tmb_host.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace tmb::native
{
    void install();
    void onFrameEnd();
    void onMenuFrameEnd();
    void patchFlareDepth(uint8_t *rdram, uint32_t image, uint32_t dest);
    bool flareDepthNative();
    void setGuestMemory(const uint8_t *rdram);
    bool enabled();
}

namespace tmb::twod
{
    void install(tmb::Registry &runtime);
}

namespace tmb::cheats
{
    void install(tmb::Registry &runtime);
    void setTheGame(uint32_t game);
    void onFrame(uint8_t *rdram);
    void logProgress(uint8_t *rdram, uint32_t gp);
}

namespace tmb::meshes
{
    void install(tmb::Registry &runtime);
}

namespace tmb::particles
{
    void install(tmb::Registry &runtime);
}

namespace tmb::textures
{
    void install(tmb::Registry &runtime);
}

namespace
{
    constexpr uint32_t kTmbPalEntry = 0x00100008u;

    void timelineEvent(char type, uint32_t arg = 0); // TMB_TRACE_TIMELINE

    // True when an ISO 9660 image boots this executable: its SYSTEM.CNF (in the root directory) names it on its BOOT2
    // line. Both releases sit side by side (game/ and game_us/ under the repository root, with both images there).
    bool discBootsElf(const std::filesystem::path &image, const std::string &elfName)
    {
        std::ifstream file(image, std::ios::binary);
        if (!file)
            return false;
        auto sector = [&file](uint32_t lbn, uint32_t bytes)
        {
            std::string data(bytes, '\0');
            file.seekg(static_cast<std::streamoff>(lbn) * 2048);
            file.read(data.data(), static_cast<std::streamsize>(bytes));
            data.resize(static_cast<size_t>(file.gcount()));
            return data;
        };
        const std::string pvd = sector(16, 2048);
        if (pvd.size() < 190 || pvd.compare(1, 5, "CD001") != 0)
            return false;
        auto u32 = [](const std::string &data, size_t at)
        {
            return static_cast<uint32_t>(static_cast<uint8_t>(data[at])) | (static_cast<uint32_t>(static_cast<uint8_t>(data[at + 1])) << 8) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(data[at + 2])) << 16) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(data[at + 3])) << 24);
        };
        const std::string root = sector(u32(pvd, 156 + 2), std::min<uint32_t>(u32(pvd, 156 + 10), 64 * 1024));
        for (size_t at = 0; at + 33 < root.size();)
        {
            const uint8_t length = static_cast<uint8_t>(root[at]);
            if (length == 0)
            {
                at = (at / 2048 + 1) * 2048; // records do not cross sectors
                continue;
            }
            const uint8_t nameLength = static_cast<uint8_t>(root[at + 32]);
            if (root.compare(at + 33, std::min<size_t>(nameLength, 10), "SYSTEM.CNF") == 0)
            {
                const std::string cnf = sector(u32(root, at + 2), std::min<uint32_t>(u32(root, at + 10), 2048));
                const size_t boot = cnf.find("BOOT2");
                return boot != std::string::npos && cnf.find(elfName, boot) != std::string::npos;
            }
            at += length;
        }
        return false;
    }

    // The game reads raw sectors with sceCdRead (LBN 0x10, the primary volume descriptor,
    // right after boot), which the file-backed cdrom0: cannot serve. Give the CD device the
    // disc image itself: TMB_ISO, else the .iso found walking up from the ELF folder (game_us/ or game/ -> the
    // repository root) that boots this executable.
    std::filesystem::path findDiscImage(const std::filesystem::path &elfPath)
    {
        if (const char *configured = std::getenv("TMB_ISO"); configured && configured[0] != '\0')
        {
            return configured;
        }
        const std::string elfName = elfPath.filename().string();
        std::filesystem::path dir = elfPath.parent_path();
        for (int depth = 0; depth < 4 && !dir.empty(); ++depth, dir = dir.parent_path())
        {
            std::error_code ec;
            for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (!entry.is_regular_file(ec))
                {
                    continue;
                }
                std::string ext = entry.path().extension().string();
                for (char &c : ext)
                {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (ext == ".iso" && discBootsElf(entry.path(), elfName))
                {
                    return entry.path();
                }
            }
            if (dir == dir.root_path())
            {
                break;
            }
        }
        return {};
    }

    // Movie skip. The player (Sony's ezmpeg sample) aborts a movie with videoDecAbort, then videoDecFlush appends a
    // sequence_end_code to the IPU ring and waits until the decode thread reaches it. BozoEngine's libmpeg never reads
    // that ring (it decodes from the demux), so it is told here that the playback was aborted, as the fork's was.
    constexpr uint32_t kVideoDecAbort = 0x001FE710u;

    void videoDecAbortOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        videoDecAbort__FP8VideoDec_0x1fe710(rdram, ctx, runtime);
        // The skip ends the playback for libmpeg too (sceMpegIsEnd 1 from now, as the fork's abortMpegPlayback made it):
        // the stream's own end never comes, the game stopped feeding it (milestone M5: story's skipped stage movie).
        bozo::port::libraries(runtime).mpeg.aborted = true;
    }

    // The movie player's switchThread is RotateThreadReadyQueue(1): Sony's ezmpeg sample runs its main loop at
    // priority 1, so the call lets the decode threads (priority 1) run. TMB's main thread is at priority 0, where
    // the rotation does nothing. The player's waits mostly block on something else, but after a skip
    // (videoDecAbort) updateMpegMovieDecoding loops on switchThread alone until the decode thread acknowledges
    // the abort, which it never gets to do: the game hung on a skipped copyright movie. A priority-0 caller drops
    // to priority 1 for the rotation, so the priority-1 threads run once, and gets its priority back when it is
    // scheduled again, through kSwitchThreadReturn (padding after readMpeg, never executed).
    constexpr uint32_t kSwitchThread = 0x001DF4C0u;
    constexpr uint32_t kSwitchThreadReturn = 0x001DF4BCu;
    constexpr uint32_t kMoviePriority = 1;
    std::unordered_map<int, std::pair<uint32_t, uint32_t>> g_switchThreadSaved; // thread -> (priority, return address)

    void switchThreadOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const int32_t id = bozo::port::currentThread(runtime);
        if (id <= 0 || bozo::port::currentPriority(runtime) >= kMoviePriority)
        {
            switchThread__Fv_0x1df4c0(rdram, ctx, runtime);
            return;
        }
        uint32_t oldPriority = 0;
        if (bozo::port::changePriority(runtime, id, kMoviePriority, oldPriority) != 0) // KE_OK
        {
            switchThread__Fv_0x1df4c0(rdram, ctx, runtime);
            return;
        }
        g_switchThreadSaved[id] = {oldPriority, getRegU32(ctx, 31)};
        ctx->pc = kSwitchThreadReturn;
        SET_GPR_U32(ctx, 2, static_cast<uint32_t>(bozo::port::rotateReadyQueue(runtime, kMoviePriority)));
        bozo::port::transfer(runtime, ctx);
    }

    void switchThreadReturn(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        const auto saved = g_switchThreadSaved.find(bozo::port::currentThread(runtime));
        if (saved == g_switchThreadSaved.end())
            return;
        const auto [priority, ret] = saved->second;
        g_switchThreadSaved.erase(saved);
        ctx->pc = ret;
        uint32_t oldPriority = 0;
        bozo::port::changePriority(runtime, 0, priority, oldPriority);
        bozo::port::transfer(runtime, ctx);
    }

    // TMB_TRACE_MOVIE=1: log the movie player's thread teardown (temporary bring-up aid).
    constexpr uint32_t kTermAll = 0x001DF738u;

    void termAllTrace(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t gp = getRegU32(ctx, 28);
        const auto readGuest = [&](uint32_t addr) { return *reinterpret_cast<const int32_t *>(rdram + (addr & 0x1FFFFFFu)); };
        const int ids[2] = {readGuest(gp + -0x34EC), readGuest(gp + -0x34E8)};
        termAll__Fv_0x1df738(rdram, ctx, runtime);
        for (int id : ids)
        {
            const bozo::ee::Thread *t = bozo::port::kernel(runtime).thread(id);
            std::cerr << "[tmb:termAll] thread " << id << " after terminate: "
                      << (t ? std::to_string(static_cast<int>(t->state)) : std::string("gone"))
                      << (t ? " entry=0x" : "") << std::hex << (t ? t->entry : 0u) << std::dec << std::endl;
        }
    }

    // Movie audio. The movie player (Sony's ezmpeg sample) gets each PSS audio packet in pcmCallback and copies
    // its PCM into the audioDec ring (48 KiB), which audioDecSendToIOP drains into 989snd's IOP buffer as it
    // plays. With libmpeg the demuxer stops when the ring is full; the HLE's stream callbacks have no back-pressure,
    // and pcmCallback copied what fit and dropped the rest of the packet. A drop shifted the 16-bit PCM by an odd
    // byte count from then on: full-scale noise through the logo and intro movies, then a buzz when the IOP looped
    // its last buffers. Here the packets go through a host queue that feeds the ring with the game's own
    // bookkeeping (audioDecBeginPut/EndPut) as space frees, before every audioDecSendToIOP: no byte is lost.
    constexpr uint32_t kPcmCallback = 0x001EC3E8u;
    constexpr uint32_t kAudioDecSendToIop = 0x00200E80u;
    constexpr uint32_t kAudioDecCreate = 0x00200C08u;
    constexpr uint32_t kAudioDecReset = 0x00200C80u;
    constexpr uint32_t kAudioDec = 0x004C4B48u;       // the player's AudioDec
    constexpr uint32_t kAudioDecHeaderBytes = 0x28u;  // 'SShd' + 'SSbd' headers, kept apart from the ring
    constexpr uint32_t kReadBufSizeOffset = 0x50008u; // ReadBuf: data[0x50000], then its size
    std::deque<uint8_t> g_moviePcm;
    uint32_t g_pcmCalls = 0;
    uint32_t g_sendCalls = 0;
    bool g_traceMovie = false;

    uint32_t guestU32(const uint8_t *rdram, uint32_t address)
    {
        uint32_t value = 0;
        std::memcpy(&value, rdram + (address & 0x1FFFFFFu), sizeof(value));
        return value;
    }

    void setGuestU32(uint8_t *rdram, uint32_t address, uint32_t value)
    {
        std::memcpy(rdram + (address & 0x1FFFFFFu), &value, sizeof(value));
    }

    // audioDecBeginPut / audioDecEndPut (0x201f00 / 0x201fa0), with the copy pcmCallback makes in between.
    void feedMoviePcm(uint8_t *rdram)
    {
        const uint32_t ad = kAudioDec;
        while (!g_moviePcm.empty())
        {
            const uint32_t state = guestU32(rdram, ad + 0x00u);
            const uint32_t headerFill = guestU32(rdram, ad + 0x2Cu);
            const uint32_t base = guestU32(rdram, ad + 0x30u);
            const uint32_t writePos = guestU32(rdram, ad + 0x34u);
            const uint32_t count = guestU32(rdram, ad + 0x38u);
            const uint32_t size = guestU32(rdram, ad + 0x3Cu);
            if (size == 0u)
                return;
            uint32_t area0 = 0, length0 = 0, area1 = 0, length1 = 0;
            if (state == 0u)
            {
                area0 = ad + 4u + headerFill;
                length0 = kAudioDecHeaderBytes - headerFill;
                area1 = base;
                length1 = size;
            }
            else
            {
                const uint32_t free = size - count;
                area0 = base + writePos;
                length0 = std::min(free, size - writePos);
                area1 = base;
                length1 = free - length0;
            }
            const uint32_t take =
                static_cast<uint32_t>(std::min<size_t>(g_moviePcm.size(), static_cast<size_t>(length0) + length1));
            if (take == 0u)
                return;
            for (uint32_t i = 0; i < take; ++i)
                rdram[(i < length0 ? area0 + i : area1 + (i - length0)) & 0x1FFFFFFu] = g_moviePcm[i];
            g_moviePcm.erase(g_moviePcm.begin(), g_moviePcm.begin() + take);

            uint32_t ringBytes = take;
            if (state == 0u)
            {
                const uint32_t header = std::min(kAudioDecHeaderBytes - headerFill, take);
                setGuestU32(rdram, ad + 0x2Cu, headerFill + header);
                if (headerFill + header >= kAudioDecHeaderBytes)
                    setGuestU32(rdram, ad + 0x00u, 1u);
                ringBytes -= header;
            }
            setGuestU32(rdram, ad + 0x34u, (writePos + ringBytes) % size);
            setGuestU32(rdram, ad + 0x38u, count + ringBytes);
            setGuestU32(rdram, ad + 0x40u, guestU32(rdram, ad + 0x40u) + ringBytes);
        }
    }

    // pcmCallback(mpeg, cbData, readBuf): the packet minus its 4-byte sub-header (ff a0 00 00), which may wrap
    // around the read buffer, goes to the queue.
    void pcmCallbackOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        (void)runtime;
        const uint32_t cbData = getRegU32(ctx, 5);
        const uint32_t readBuf = getRegU32(ctx, 6);
        const uint32_t data = guestU32(rdram, cbData + 0x08u);
        const uint32_t length = guestU32(rdram, cbData + 0x0Cu);
        const uint32_t ringSize = guestU32(rdram, readBuf + kReadBufSizeOffset);
        if (length > 4u && ringSize != 0u)
        {
            const uint32_t ringEnd = readBuf + ringSize;
            uint32_t source = data + 4u;
            if (source >= ringEnd)
                source -= ringSize;
            const uint32_t bytes = length - 4u;
            const uint32_t first = std::min(bytes, ringEnd - source);
            for (uint32_t i = 0; i < bytes; ++i)
                g_moviePcm.push_back(rdram[(i < first ? source + i : readBuf + (i - first)) & 0x1FFFFFFu]);
            feedMoviePcm(rdram);
        }
        if (g_traceMovie && g_pcmCalls < 8u)
            std::cerr << "[tmb:pcm] call " << g_pcmCalls + 1u << " cbData=0x" << std::hex << cbData << std::dec
                      << " bytes " << length << " queued " << g_moviePcm.size() << std::endl;
        ++g_pcmCalls;
        setReturnS32(ctx, 1);
        ctx->pc = getRegU32(ctx, 31);
    }

    void audioDecSendToIopOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        feedMoviePcm(rdram);
        const uint32_t ad = getRegU32(ctx, 4);
        const auto rd = [&](uint32_t off) { return static_cast<int32_t>(guestU32(rdram, ad + off)); };
        const int32_t stateBefore = rd(0x0);
        audioDecSendToIOP__FP8AudioDec_0x200e80(rdram, ctx, runtime);
        if (g_traceMovie && (g_sendCalls++ % 50u) == 0u)
        {
            std::cerr << "[tmb:audio] send#" << g_sendCalls << " state " << stateBefore << "->" << rd(0x0)
                      << " +0x30=" << rd(0x30) << " +0x34=" << rd(0x34) << " +0x38=" << rd(0x38)
                      << " +0x3c=" << rd(0x3c) << " +0x44=0x" << std::hex << rd(0x44) << std::dec
                      << " +0x48=" << rd(0x48) << " +0x54=" << rd(0x54) << " pcmCalls=" << g_pcmCalls
                      << " queued=" << g_moviePcm.size() << std::endl;
        }
    }

    // A new movie (or a reset one) starts from an empty queue.
    void audioDecCreateOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_moviePcm.clear();
        audioDecCreate__FP8AudioDecPUcii_0x200c08(rdram, ctx, runtime);
    }

    void audioDecResetOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_moviePcm.clear();
        audioDecReset__FP8AudioDec_0x200c80(rdram, ctx, runtime);
    }

    constexpr uint32_t kVblankHandler = 0x001C1890u;
    constexpr uint32_t kHier = 0x001D2EA0u;

    double g_hierMs = 0, g_updateMs = 0;
    uint32_t g_theGame = 0;              // `this` of TheGame::Update
    std::atomic<uint64_t> g_vblanks{0};  // the vblanks at the last TheGame::Update (the timeline's marks)
    std::chrono::steady_clock::time_point g_frameUpdate;
    bool g_traceFrames = false;

    void hierOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        tmb::native::setGuestMemory(rdram); // the native frame reads the game's fog packet
        const auto start = std::chrono::steady_clock::now();
        timelineEvent('H');
        hier__Fi_0x1d2ea0(rdram, ctx, runtime);
        timelineEvent('h');
        if (g_traceFrames)
            g_hierMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    // The vblank handler (it only runs while movies play): the game's own, marked on the timeline.
    void vblankHandlerOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        timelineEvent('B');
        vblankHandler__Fi_0x1c1890(rdram, ctx, runtime);
    }

    // TMB_TRACE_FONT=1: dump the font packet state on each fontDmaFontData (bring-up aid).
    constexpr uint32_t kFontDmaFontData = 0x001C3FB8u;
    uint32_t g_fontCalls = 0;

    void fontDmaFontDataTrace(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t gp = getRegU32(ctx, 28);
        const auto rd32 = [&](uint32_t addr) { return *reinterpret_cast<const int32_t *>(rdram + (addr & 0x1FFFFFFu)); };
        const auto rd64 = [&](uint32_t addr) { return *reinterpret_cast<const uint64_t *>(rdram + (addr & 0x1FFFFFFu)); };
        const int32_t state = rd32(gp + -0x3034), count = rd32(gp + -0x3038), base = rd32(gp + -0x302C);
        const int32_t tagIdx = rd32(gp + -0x3030), prims = static_cast<int32_t>(rd32(gp + -0x303C) & 0xFFFF);
        fontDmaFontData__Fv_0x1c3fb8(rdram, ctx, runtime);
        const uint32_t n = g_fontCalls++;
        if (n < 6u || (n % 200u) == 0u || (state == 2 && n < 400u && (n % 20u) == 0u))
        {
            constexpr uint32_t kBuf = 0x00504580u;
            std::cerr << std::hex << "[tmb:font] call#" << std::dec << n << " state=" << state << " count=" << count
                      << " base=" << base << " tagIdx=" << tagIdx << " prims=" << prims << std::hex
                      << " | tag@idx=0x" << rd64(kBuf + 16u * static_cast<uint32_t>(tagIdx)) << "_" << rd64(kBuf + 16u * static_cast<uint32_t>(tagIdx) + 8u)
                      << " q0=0x" << rd64(kBuf) << "_" << rd64(kBuf + 8u) << " after: count=" << std::dec << rd32(gp + -0x3038)
                      << " tagIdx=" << rd32(gp + -0x3030) << " state=" << rd32(gp + -0x3034) << std::endl;
        }
    }

    // End of a movie. Every PSS ends with 4 bytes past its last sector (the program end code); the player treats the
    // stream as complete with < 5 bytes left and never reads them. It then calls videoDecFlush, which appends a
    // sequence_end_code to the IPU ring so the decoder finishes. The fork's libmpeg HLE had to be told the input was
    // complete; BozoEngine's libmpeg sees the end code in the stream.
    constexpr uint32_t kVideoDecFlush = 0x001FE7D0u;

    void videoDecFlushOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        videoDecFlush__FP8VideoDec_0x1fe7d0(rdram, ctx, runtime);
    }

    // TMB_TRACE_SND=1: log every 989snd command sent with snd_SendIOPCommandAndWait (bring-up aid).
    constexpr uint32_t kSndSendIopCommandAndWait = 0x001BBFC0u;

    void sndSendIopCommandAndWaitTrace(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static uint32_t s_calls = 0;
        const uint32_t command = getRegU32(ctx, 4);
        const int32_t count = static_cast<int32_t>(getRegU32(ctx, 5));
        const uint32_t args = getRegU32(ctx, 6);
        const uint32_t n = s_calls++;
        std::cerr << std::hex << "[tmb:snd] #" << std::dec << n << " cmd=0x" << std::hex << command << " argc=" << std::dec << count;
        for (int32_t i = 0; i < count && i < 6; ++i)
        {
            std::cerr << std::hex << " 0x" << *reinterpret_cast<const uint32_t *>(rdram + ((args + 4u * static_cast<uint32_t>(i)) & 0x1FFFFFFu));
        }
        std::cerr << std::dec << std::endl;
        snd_SendIOPCommandAndWait_0x1bbfc0(rdram, ctx, runtime);
        std::cerr << "[tmb:snd] #" << n << " -> 0x" << std::hex << getRegU32(ctx, 2) << std::dec << std::endl;
    }

    // TMB_TRACE_HIER=1: count VIF1 DMA completion handler calls of the hier renderer (bring-up aid).
    constexpr uint32_t kHierDmaHandler = 0x001D5190u;
    std::atomic<uint32_t> g_hierDmaCalls{0};

    void hierDmaHandlerTrace(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t channel = getRegU32(ctx, 4);
        const auto rd = [&](uint32_t addr) { return *reinterpret_cast<const uint32_t *>(rdram + (addr & 0x1FFFFFFu)); };
        const uint32_t before0 = rd(0x004FB8A8u), before1 = rd(0x004FB8ACu);
        hierDmaHandler__Fi_0x1d5190(rdram, ctx, runtime);
        const uint32_t n = g_hierDmaCalls.fetch_add(1);
        if (n < 20u || (n % 500u) == 0u)
            std::cerr << "[tmb:hier] dma handler #" << n << " ch=" << channel << " flags " << before0 << "," << before1
                      << " -> " << rd(0x004FB8A8u) << "," << rd(0x004FB8ACu) << std::endl;
    }

    // The game measures each frame with T0 (BUSCLK/256, 576 ticks per ms) into a global that timerGetFrameTime turns
    // into milliseconds, and moves weapons and debris over it without a limit. Built for 50 fps (20 ms), it blows
    // up at the 80-100 ms frames of a slow host. Capped at 40 ms (TMB_MAX_FRAME_MS), they move in slow motion
    // instead when the host cannot keep up. (The cars use the vblank fields: theGameUpdateOverride below.)
    constexpr uint32_t kTimerEndOfRealTimeProcessing = 0x001F67F0u;
    constexpr uint32_t kSceGsExecStoreImage = 0x002043D8u;
    constexpr uint32_t kLensFlareTestZbuffer = 0x001DFE00u, kLensFlareTestZbufferEnd = 0x001E01C8u;

    // GS readbacks: the lens flares' Z samples get the native frame's depth (TMB_NATIVE, tmb_native.cpp).
    void sceGsExecStoreImageOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t image = getRegU32(ctx, 4), dest = getRegU32(ctx, 5), ra = getRegU32(ctx, 31);
        // The lens flares' Z samples (particleLensFlareTestZbuffer) from the native renderer's depth, not read back from
        // the GS: each GS readback made the EE wait for the GS thread and the GS pull its targets' pages off the GPU
        // (about 11 fps in a match). Samples start far (the flare shows), then take the native depth around the light,
        // read after the frame before (Renderer::depthAt). TMB_NATIVE_FLARE_DEPTH=0: the GS readback, as before.
        if (ra >= kLensFlareTestZbuffer && ra < kLensFlareTestZbufferEnd && tmb::native::flareDepthNative())
        {
            uint64_t trxreg = 0;
            for (uint32_t q = 0; q < 7u; ++q)
                if (rdram[(image + q * 16u + 8u) & 0x1FFFFFFu] == 0x52u)
                    std::memcpy(&trxreg, rdram + ((image + q * 16u) & 0x1FFFFFFu), 8);
            const uint32_t bytes = static_cast<uint32_t>((trxreg & 0xFFFu) * ((trxreg >> 32) & 0xFFFu) * 4u);
            std::memset(rdram + (dest & 0x1FFFFFFu), 0, std::min<uint32_t>(bytes, 0x10000u));
            tmb::native::patchFlareDepth(rdram, image, dest);
            setReturnS32(ctx, 0);
            ctx->pc = ra;
            return;
        }
        bozo::port::callLibrary(runtime, ctx, kSceGsExecStoreImage); // the library (BozoEngine's), not the game's copy
        // TMB_LOG_STOREIMAGE=1: each GS readback's caller (ra) and first words, every 100th per caller.
        static const bool s_log = std::getenv("TMB_LOG_STOREIMAGE") != nullptr;
        if (s_log)
        {
            static std::unordered_map<uint32_t, uint32_t> s_calls;
            if (++s_calls[ra] % 100u == 1u)
            {
                uint64_t bitbltbuf = 0, trxreg = 0;
                for (uint32_t q = 0; q < 7u; ++q)
                {
                    uint64_t value;
                    std::memcpy(&value, rdram + ((image + q * 16u) & 0x1FFFFFFu), 8);
                    const uint8_t address = rdram[(image + q * 16u + 8u) & 0x1FFFFFFu];
                    if (address == 0x50u)
                        bitbltbuf = value;
                    else if (address == 0x52u)
                        trxreg = value;
                }
                uint32_t first[2];
                std::memcpy(first, rdram + (dest & 0x1FFFFFFu), sizeof(first));
                std::fprintf(stderr, "[tmb:storeimage] ra %#x (call %u) sbp %#llx psm %#llx %llux%llu: %08x %08x\n", ra, s_calls[ra],
                             static_cast<unsigned long long>(bitbltbuf & 0x3FFFu), static_cast<unsigned long long>((bitbltbuf >> 24) & 0x3Fu),
                             static_cast<unsigned long long>(trxreg & 0xFFFu), static_cast<unsigned long long>((trxreg >> 32) & 0xFFFu), first[0], first[1]);
            }
        }
        if (ra >= kLensFlareTestZbuffer && ra < kLensFlareTestZbufferEnd)
            tmb::native::patchFlareDepth(rdram, image, dest);
    }
    constexpr int32_t kFrameTicksGpOffset = -0x2EE4; // timerGetFrameTime's tick count (gp-relative)
    int32_t g_maxFrameTicks = 40 * 576;

    double hitchClock();
    std::atomic<double> g_lastMatchFrameNoted{-1.0}; // the last match frame (hitchClock)

    void timerEndOfRealTimeProcessingOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        timelineEvent('E');
        g_lastMatchFrameNoted.store(hitchClock(), std::memory_order_relaxed);
        tmb::cheats::onFrame(rdram); // the Cheats tab (runtime/tmb_cheats.cpp)
        const uint32_t gp = getRegU32(ctx, 28);
        tmb::native::onFrameEnd(); // TMB_NATIVE: publish the frame's native draws
        timerEndOfRealTimeProcessing__Fv_0x1f67f0(rdram, ctx, runtime);
        int32_t *frameTicks = reinterpret_cast<int32_t *>(rdram + ((gp + kFrameTicksGpOffset) & 0x1FFFFFFu));
        if (*frameTicks > g_maxFrameTicks)
            *frameTicks = g_maxFrameTicks;
    }

    // Native frames: the game's frames on the PC clock.
    // Each frame of the match and the menus ends in timer1Handler: it adds the timer ticks of the frame (T0, 576000 a
    // second) to the game's tick total, waits a vblank (sceGsSyncV), then adds the fields the frame took (the ticks
    // rounded up to 1/50 s) to the game's field count, which TheGame::Update moves the cars by. The loading screens,
    // dialogs and timerWaitForMinUpdateRate wait sceGsSyncV directly. Here both end the frame on the FramePacer
    // instead: it holds the frame to the field rate (a longer frame is shown when it ends, not at the next vblank),
    // the EE is charged the real time its code did not account (the timers, the vblank interrupt and the IOP follow the
    // EE clock), and the picture is shown. The frame's fields come from the real time it took, the remainder carried
    // to the next frame (the tick count rounds a 21 ms frame up to two fields: a game running 1.9 times too fast).
    constexpr uint32_t kTimer1Handler = 0x001F6888u;
    constexpr uint32_t kSceGsSyncV = 0x00203B80u;
    constexpr int32_t kTicsGpOffset = -0x2EE8;       // the ticks before the T0 count
    constexpr int32_t kFieldCountGpOffset = -0x2EF0; // fields since the start
    constexpr uint32_t kT0Count = 0x10000000u;
    uint32_t g_syncVParity = 0;
    uint64_t g_frameEndCycle = 0;
    double g_fieldCarry = 0.0;
    uint64_t g_clockDebt = 0; // EE cycles the EE clock is ahead of the PC's (paced runs: nativeFrameEnd)
    bozo::app::FramePacer g_pacer;

    int32_t *gpWord(uint8_t *rdram, uint32_t gp, int32_t offset)
    {
        return reinterpret_cast<int32_t *>(rdram + ((gp + offset) & 0x1FFFFFFu));
    }

    // The game's fields per second (50 PAL, 60 NTSC; set by timerInit).
    int32_t fieldRate(uint8_t *rdram, uint32_t gp)
    {
        const int32_t rate = *gpWord(rdram, gp, -0x2ED8);
        return rate > 0 ? rate : 60;
    }

    const auto g_hitchClockStart = std::chrono::steady_clock::now();

    double hitchClock() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_hitchClockStart).count(); }

    // Holds the frame to the field rate, charges the EE the real time it did not account, ends the game's frame (the
    // host takes it: bozo::port::frameEnd); returns the frame's duration. Deterministic mode: exactly one field.
    double nativeFrameEnd(uint8_t *rdram, uint32_t gp, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_pacer.setCap(static_cast<uint32_t>(fieldRate(rdram, gp)));
        g_pacer.setPaced(bozo::port::realTime(runtime)); // a headless or checked run is not held to real time
        tmb::native::onMenuFrameEnd(); // TMB_NATIVE: publishes a menu frame's native draws (not in a match)
        const double step = g_pacer.frameEnd();
        tmb::cheats::logProgress(rdram, gp); // TMB_LOG_PROGRESS
        // TMB_TRACE_HITCH=<ms>: each game frame longer than that (from one frame end to the next), with when it ended.
        static const double s_hitchSeconds = []
        {
            const char *value = std::getenv("TMB_TRACE_HITCH");
            return value && *value ? std::atof(value) / 1000.0 : 0.0;
        }();
        if (s_hitchSeconds > 0.0)
        {
            static double s_last = -1.0;
            const double now = hitchClock();
            if (s_last >= 0.0 && now - s_last > s_hitchSeconds)
                std::fprintf(stderr, "[tmb:hitch] game frame %.0f ms, until t=%.2f\n", (now - s_last) * 1000.0, now);
            s_last = now;
        }
        // Deterministic mode: exactly one field's EE cycles (the step is one field too).
        const uint64_t real = bozo::det::enabled()
                                  ? bozo::port::kEeClockHz / static_cast<uint64_t>(fieldRate(rdram, gp))
                                  : static_cast<uint64_t>(step * static_cast<double>(bozo::port::kEeClockHz));
        const uint64_t accounted = bozo::port::now(runtime) - g_frameEndCycle;
        // Paced (a window, not deterministic): the EE clock follows the PC's over the long run. A frame whose guest
        // code accounted more than the real time it took (it waited past a 59.94 Hz vblank while frames are held to
        // 60.0) leaves the EE that much ahead: kept as a debt, paid back from the real time later frames would charge.
        // Without it the EE ran about 1 % fast (TMB_TRACE_CLOCK), so the sound, made on the EE clock, was made 1 %
        // faster than the device played it. Headless and checked runs are not paced and charge as before.
        const bool paced = bozo::port::realTime(runtime) && !bozo::det::enabled();
        if (paced && accounted > real)
            g_clockDebt += accounted - real;
        if (real > accounted)
        {
            uint64_t charge = real - accounted;
            if (paced)
            {
                const uint64_t pay = std::min(charge, g_clockDebt);
                g_clockDebt -= pay;
                charge -= pay;
            }
            if (charge)
                bozo::port::accountCycles(runtime, ctx, charge);
        }
        bozo::port::frameEnd(runtime, ctx);
        // The frame's end as the host has it (a checked run moves the clock to the reference's frame end).
        g_frameEndCycle = bozo::port::now(runtime);
        // TMB_TRACE_CLOCK=1: every 300 frames, the EE clock against the PC's since the first frame end (what the sound,
        // made on the EE clock, is ahead of the window's real time).
        static const bool s_traceClock = std::getenv("TMB_TRACE_CLOCK") != nullptr;
        if (s_traceClock)
        {
            static uint64_t s_frames = 0, s_firstCycle = 0;
            static double s_firstTime = 0.0;
            const double wall = hitchClock();
            if (s_frames == 0)
                s_firstCycle = g_frameEndCycle, s_firstTime = wall;
            if (++s_frames % 300u == 0u)
            {
                const double ee = static_cast<double>(g_frameEndCycle - s_firstCycle) / static_cast<double>(bozo::port::kEeClockHz);
                static double s_lastEe = 0.0, s_lastPc = 0.0;
                const double pc = wall - s_firstTime;
                std::fprintf(stderr,
                             "[tmb:clock] frame %llu: EE %.3f s, PC %.3f s, EE ahead %.3f s (%.2f%%); last 300 frames EE/PC %.4f; debt %.1f ms; ticks %d fields %d\n",
                             static_cast<unsigned long long>(s_frames), ee, pc, ee - pc, pc > 0.0 ? 100.0 * (ee / pc - 1.0) : 0.0,
                             pc > s_lastPc ? (ee - s_lastEe) / (pc - s_lastPc) : 0.0,
                             1000.0 * static_cast<double>(g_clockDebt) / static_cast<double>(bozo::port::kEeClockHz),
                             *gpWord(rdram, gp, kTicsGpOffset), *gpWord(rdram, gp, kFieldCountGpOffset));
                s_lastEe = ee;
                s_lastPc = pc;
            }
        }
        return step;
    }

    void sceGsSyncVNative(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        nativeFrameEnd(rdram, getRegU32(ctx, 28), ctx, runtime);
        g_syncVParity ^= 1u;
        setReturnS32(ctx, static_cast<int32_t>(g_syncVParity));
        ctx->pc = getRegU32(ctx, 31);
    }

    void timer1HandlerNative(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t gp = getRegU32(ctx, 28);
        const uint32_t parityOut = getRegU32(ctx, 4);
        *gpWord(rdram, gp, kTicsGpOffset) += static_cast<int32_t>(bozo::port::ioRead32(runtime, ctx, kT0Count));
        bozo::port::ioWrite32(runtime, ctx, kT0Count, 0u);
        const double step = nativeFrameEnd(rdram, gp, ctx, runtime);
        g_syncVParity ^= 1u;
        if (parityOut != 0u)
            *reinterpret_cast<uint32_t *>(rdram + (parityOut & 0x1FFFFFFu)) = g_syncVParity == 0u ? 1u : 0u;
        g_fieldCarry += step * fieldRate(rdram, gp);
        const int32_t fields = std::clamp(static_cast<int32_t>(std::floor(g_fieldCarry)), 1, 15);
        g_fieldCarry = std::clamp(g_fieldCarry - fields, -1.0, 1.0);
        *gpWord(rdram, gp, kFieldCountGpOffset) += fields;
        // TMB_TRACE_FRAMES=1: the game's fields per real second (the field rate when the game runs at its speed).
        if (g_traceFrames)
        {
            static uint32_t s_frames = 0;
            static int64_t s_fields = 0;
            static double s_seconds = 0.0;
            s_fields += fields;
            s_seconds += step;
            if (++s_frames == 1000u)
            {
                std::fprintf(stderr, "[tmb:native-frames] 1000 frames in %.2f s, %lld fields: %.2f fields/s (field rate %d)\n",
                             s_seconds, static_cast<long long>(s_fields), s_fields / s_seconds, fieldRate(rdram, gp));
                s_frames = 0, s_fields = 0, s_seconds = 0.0;
            }
        }
        setReturnS32(ctx, 1);
        ctx->pc = getRegU32(ctx, 31);
    }

    // TheGame::Update moves each car by its velocity times the vblank fields since the previous frame
    // (timerGetFieldsLastFrame, copied to vehicle+0x20C8) and tests the collisions once, after the move. Built for
    // 50 fps (one field per frame, two when the PS2 is overloaded), at the two to four fields of a slow host each
    // step goes two to four times as far, and fast cars landing or hitting a slope go through the ground (bots fall
    // under the arena until the game teleports them back). Run it once per field instead, the fields, update rate
    // and frame time (weapons, debris) split over the steps, at most TMB_SUBSTEPS (default 4, 1 disables) steps.
    // Update ends in a tail call and may be preempted, so each step returns through the dispatcher to
    // kSubstepReturn (padding between two functions, never executed), which starts the next one.
    constexpr uint32_t kTheGameUpdate = 0x0012FE28u;
    constexpr uint32_t kSubstepReturn = 0x0012FE24u;
    constexpr int32_t kFieldsLastFrameGpOffset = -0x2EF8;
    constexpr int32_t kUpdateRateGpOffset = -0x2EF4;
    constexpr int32_t kFieldRateGpOffset = -0x2ED8; // fields per second (50)
    int32_t g_maxSubsteps = 4;

    struct Substeps
    {
        int32_t step = 0, steps = 0;
        int32_t totalFields = 0, totalRate = 0, totalTicks = 0, ticksDone = 0;
        uint32_t self = 0, ret = 0, gp = 0;
    } g_substeps;

    int32_t *gpGlobal(uint8_t *rdram, int32_t offset)
    {
        return reinterpret_cast<int32_t *>(rdram + ((g_substeps.gp + offset) & 0x1FFFFFFu));
    }

    void runSubstep(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        Substeps &s = g_substeps;
        const int32_t fields = s.step + 1 < s.steps ? 1 : s.totalFields - (s.steps - 1);
        *gpGlobal(rdram, kFieldsLastFrameGpOffset) = fields;
        *gpGlobal(rdram, kUpdateRateGpOffset) = std::max(1, *gpGlobal(rdram, kFieldRateGpOffset) / fields);
        const int32_t ticks = static_cast<int32_t>(static_cast<int64_t>(s.totalTicks) * (s.step + 1) / s.steps) - s.ticksDone;
        s.ticksDone += ticks;
        *gpGlobal(rdram, kFrameTicksGpOffset) = ticks;
        SET_GPR_U32(ctx, 4, s.self);
        SET_GPR_U32(ctx, 31, kSubstepReturn);
        ctx->pc = kTheGameUpdate;
        Update__7TheGame_0x12fe28(rdram, ctx, runtime);
    }

    void substepReturn(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        Substeps &s = g_substeps;
        if (++s.step < s.steps)
        {
            runSubstep(rdram, ctx, runtime);
            return;
        }
        *gpGlobal(rdram, kFieldsLastFrameGpOffset) = s.totalFields;
        *gpGlobal(rdram, kUpdateRateGpOffset) = s.totalRate;
        *gpGlobal(rdram, kFrameTicksGpOffset) = s.totalTicks;
        s.steps = 0;
        if (g_traceFrames)
            g_updateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_frameUpdate).count();
        SET_GPR_U32(ctx, 31, s.ret);
        ctx->pc = s.ret;
    }

    // TMB_TRACE_TIMELINE=<file>: 4 s of the match (from its 500th TheGame::Update; TMB_TRACE_TIMELINE_START and
    // TMB_TRACE_TIMELINE_SECONDS move and stretch the window) as a timeline, for
    // tools/timeline.py: the game's events (vblank handler, Update, hier, sceGsSyncPath / sceGsSyncV enter and exit,
    // end of the real-time processing) and, every 0.2 ms, the CPU time of the game, vif1 and gs threads.
    struct Timeline
    {
        struct Event
        {
            int64_t ns;
            char type;
            uint32_t arg;
        };
        struct Sample
        {
            int64_t ns;
            int64_t cpu[3];
        };
        std::string path;
        std::atomic<int> state{0}; // 0 waiting, 1 recording, 2 done
        uint32_t updates = 0;
        uint32_t startUpdate = 500u;
        int64_t durationNs = 4000000000ll;
        int64_t startNs = 0;
        std::mutex mutex;
        std::vector<Event> events;
        std::vector<Sample> samples;
        std::thread sampler;
    } g_timeline;

    int64_t timelineNow()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void timelineEvent(char type, uint32_t arg)
    {
        if (g_timeline.state.load(std::memory_order_relaxed) != 1)
            return;
        std::lock_guard<std::mutex> lock(g_timeline.mutex);
        g_timeline.events.push_back({timelineNow(), type, arg});
    }

    void timelineDump()
    {
        FILE *out = std::fopen(g_timeline.path.c_str(), "w");
        if (!out)
            return;
        std::lock_guard<std::mutex> lock(g_timeline.mutex);
        for (const auto &e : g_timeline.events)
            std::fprintf(out, "e %lld %c %x\n", static_cast<long long>(e.ns - g_timeline.startNs), e.type, e.arg);
        for (const auto &s : g_timeline.samples)
            std::fprintf(out, "s %lld %lld %lld %lld\n", static_cast<long long>(s.ns - g_timeline.startNs),
                         static_cast<long long>(s.cpu[0]), static_cast<long long>(s.cpu[1]), static_cast<long long>(s.cpu[2]));
        std::fclose(out);
        std::fprintf(stderr, "[tmb:timeline] %zu events, %zu samples written to %s\n", g_timeline.events.size(),
                     g_timeline.samples.size(), g_timeline.path.c_str());
    }

    void timelineStart()
    {
        // The threads' CPU clocks.
        int64_t clocks[3];
        const char *names[3] = {"GameThread", "vif1", "GSFrontend"};
        for (int i = 0; i < 3; ++i)
            clocks[i] = tmb::host::findThreadCpuClock(names[i]);
        g_timeline.startNs = timelineNow();
        g_timeline.state.store(1);
        g_timeline.sampler = std::thread([clocks]()
                                         {
            // The vblank handler only runs during movies: mark the vblanks from the scheduler's tick instead.
            uint64_t lastVSyncTick = g_vblanks.load(std::memory_order_relaxed);
            while (g_timeline.state.load() == 1)
            {
                {
                    const uint64_t tick = g_vblanks.load(std::memory_order_relaxed);
                    if (tick != lastVSyncTick)
                    {
                        lastVSyncTick = tick;
                        std::lock_guard<std::mutex> lock(g_timeline.mutex);
                        g_timeline.events.push_back({timelineNow(), 'B', static_cast<uint32_t>(tick)});
                    }
                }
                Timeline::Sample sample{timelineNow(), {0, 0, 0}};
                for (int i = 0; i < 3; ++i)
                    sample.cpu[i] = tmb::host::readThreadCpuNs(clocks[i]);
                {
                    std::lock_guard<std::mutex> lock(g_timeline.mutex);
                    g_timeline.samples.push_back(sample);
                }
                if (sample.ns - g_timeline.startNs > g_timeline.durationNs)
                {
                    g_timeline.state.store(2);
                    timelineDump();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            } });
        g_timeline.sampler.detach();
    }

    void sceGsSyncPathTimeline(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = getRegU32(ctx, 31);
        timelineEvent('P', ra);
        bozo::port::callLibrary(runtime, ctx, 0x00203C18u);
        timelineEvent('p', ra);
    }

    void sceGsSyncVTimeline(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t ra = getRegU32(ctx, 31);
        timelineEvent('V', ra);
        bozo::port::callLibrary(runtime, ctx, 0x00203B80u);
        timelineEvent('v', ra);
    }

    // TMB_KILL_ENEMIES=<s> (testing): every <s> seconds of TheGame::Update, every opponent still alive takes a 10000
    // point hit through the game's own Vehicle::vehicleTakeHit (0x1ae070: a0 vehicle, a1 hit point, a2 damage, a3
    // type), so it dies as if shot and the kill counts ("kill 7 enemies to advance"). A scripted run then gets through
    // story levels without aiming. Called on a copy of the context at the top of the update, where the stack below sp
    // is free. Guest code on a copy never leaves the override (BozoEngine kernel.md section 3b, "Guest code on an
    // override's own context": no event, no stop, its inner and recursive returns plain returns); before that rule a
    // hit leaving at bspVecTestIn's inner return 0x2014f0 faulted the free story run at frame 5458.
    void killEnemies(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        static const uint32_t s_interval = []
        {
            const char *value = std::getenv("TMB_KILL_ENEMIES");
            return value && *value ? static_cast<uint32_t>(std::atof(value) * 50.0) : 0u;
        }();
        static uint32_t s_updates = 0;
        if (s_interval == 0u || g_theGame == 0u || ++s_updates % s_interval != 0u)
            return;
        int32_t count = 0;
        std::memcpy(&count, rdram + ((g_theGame + 0x001B3DB4u) & 0x1FFFFFFu), 4);
        int killed = 0;
        for (int32_t index = 0; index < std::min(count, 64); ++index)
        {
            const uint32_t vehicle = g_theGame + static_cast<uint32_t>(index) * 0x2D00u;
            float health = 0.0f;
            std::memcpy(&health, rdram + ((vehicle + 1012u) & 0x1FFFFFFu), 4);
            if (vehicle == 0x00257B50u || !(health > 0.0f))
                continue;
            R5900Context call = *ctx;
            SET_GPR_U32(&call, 4, vehicle);
            SET_GPR_U32(&call, 5, vehicle); // the hit point: only placement for the effects
            SET_GPR_U32(&call, 6, 10000u);
            SET_GPR_U32(&call, 7, 0u);
            vehicleTakeHit__7VehicleP8_fvectorii_0x1ad208(rdram, &call, runtime);
            ++killed;
        }
        if (killed)
            std::fprintf(stderr, "[tmb:kill] %d of %d vehicles hit\n", killed, count);
    }

    // TMB_DUMP_RDRAM=<s> (native port work): after <s> seconds of TheGame::Update (50 updates a second), the 32 MB of
    // EE memory to build/rdram.bin, once, for offline decoding of the game's own data (texture table at 0x4D4930,
    // meshes): tools/tmb_textures.py.
    void dumpRdramOnce(uint8_t *rdram)
    {
        static const uint32_t s_at = []
        {
            const char *value = std::getenv("TMB_DUMP_RDRAM");
            return value && *value ? static_cast<uint32_t>(std::atof(value) * 50.0) : 0u;
        }();
        static uint32_t s_updates = 0;
        if (s_at == 0u || ++s_updates != s_at)
            return;
        if (FILE *out = std::fopen("build/rdram.bin", "wb"))
        {
            std::fwrite(rdram, 1, 0x2000000u, out);
            std::fclose(out);
            std::fprintf(stderr, "[tmb:dump] EE memory written to build/rdram.bin\n");
        }
    }

    void theGameUpdateOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_theGame = getRegU32(ctx, 4); // TMB_KILL_ENEMIES, the cheats
        tmb::cheats::setTheGame(g_theGame);
        if (g_substeps.steps == 0)
        {
            killEnemies(rdram, ctx, runtime);
            dumpRdramOnce(rdram);
        }
        Substeps &s = g_substeps;
        if (!g_timeline.path.empty() && s.steps == 0)
        {
            if (g_timeline.state.load() == 0 && ++g_timeline.updates == g_timeline.startUpdate)
                timelineStart();
            timelineEvent('U', static_cast<uint32_t>(*gpGlobal(rdram, kFieldsLastFrameGpOffset)));
        }
        if (g_traceFrames && s.steps == 0)
            g_frameUpdate = std::chrono::steady_clock::now();
        if (s.steps == 0)
            g_vblanks.store(bozo::port::vblankCount(runtime), std::memory_order_relaxed);
        s.gp = getRegU32(ctx, 28);
        const int32_t fields = *gpGlobal(rdram, kFieldsLastFrameGpOffset);
        const int32_t steps = std::min(fields, g_maxSubsteps);
        if (steps <= 1 || s.steps != 0)
        {
            Update__7TheGame_0x12fe28(rdram, ctx, runtime);
            return;
        }
        s.step = 0;
        s.steps = steps;
        s.totalFields = fields;
        s.totalRate = *gpGlobal(rdram, kUpdateRateGpOffset);
        s.totalTicks = *gpGlobal(rdram, kFrameTicksGpOffset);
        s.ticksDone = 0;
        s.self = getRegU32(ctx, 4);
        s.ret = getRegU32(ctx, 31);
        runSubstep(rdram, ctx, runtime);
    }

    // TMB_TRACE_BLACKBOX=<file>: the player vehicle (0x259c70, 0x2200 bytes) at every collisCheckHdResults; when its
    // position goes past 50000 the last 64 frames are written to <file> (bring-up aid for the physics blow-ups).
    constexpr uint32_t kPlayerVehicle = 0x00257B50u;
    constexpr uint32_t kVehicleBytes = 0x2200u;
    constexpr uint32_t kBlackboxFrames = 64u;
    std::vector<uint8_t> g_blackbox;
    uint32_t g_blackboxNext = 0;
    bool g_blackboxDumped = false;
    const char *g_blackboxPath = nullptr;

    // TMB_TRACE_POS=<file>: every vehicle's position (+0x260) at each collisCheckHdResults, one line per call (vsync
    // tick, vehicle, x, y, z): falls through the ground show as the height dropping under the terrain's.
    FILE *g_posTrace = nullptr;

    void collisCheckHdResultsBlackbox(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (g_posTrace)
        {
            const uint32_t vehicle = getRegU32(ctx, 4);
            float p[3];
            std::memcpy(p, rdram + ((vehicle + 0x260u) & 0x1FFFFFFu), sizeof(p));
            std::fprintf(g_posTrace, "%llu %x %.2f %.2f %.2f\n",
                         static_cast<unsigned long long>(bozo::port::vblankCount(runtime)), vehicle, p[0], p[1],
                         p[2]);
        }
        if (g_blackboxPath && getRegU32(ctx, 4) == kPlayerVehicle && !g_blackboxDumped)
        {
            if (g_blackbox.empty())
                g_blackbox.resize(kBlackboxFrames * kVehicleBytes);
            std::memcpy(g_blackbox.data() + (g_blackboxNext % kBlackboxFrames) * kVehicleBytes, rdram + kPlayerVehicle,
                        kVehicleBytes);
            ++g_blackboxNext;
            float pos[3];
            std::memcpy(pos, rdram + kPlayerVehicle + 0x260u, sizeof(pos));
            if (!(std::fabs(pos[0]) < 50000.0f && std::fabs(pos[1]) < 50000.0f && std::fabs(pos[2]) < 50000.0f))
            {
                g_blackboxDumped = true;
                if (FILE *out = std::fopen(g_blackboxPath, "wb"))
                {
                    for (uint32_t i = 0; i < kBlackboxFrames; ++i)
                        std::fwrite(g_blackbox.data() + ((g_blackboxNext + i) % kBlackboxFrames) * kVehicleBytes, 1,
                                    kVehicleBytes, out);
                    std::fclose(out);
                }
                std::fprintf(stderr, "[tmb:blackbox] position %g %g %g at frame %u: dumped\n", pos[0], pos[1], pos[2],
                             g_blackboxNext);
            }
        }
        collisCheckHdResults__7Vehicle_0x117d48(rdram, ctx, runtime);
    }

    // A multitap in pad port 0, as the game's own sceMtap library would find one (its IOP module is not run): four
    // players on that port's slots, which the game needs for its 3 and 4 player modes. TMB_MULTITAP=0: none.
    // BozoEngine's libpad then answers players 2-4 on slots 1-3 (a gamepad each, or a script), port 2's pad idle.
    constexpr uint32_t kMtapInit = 0x00206250u, kMtapPortOpen = 0x002064D0u,
                       kMtapPortClose = 0x00206540u, kMtapGetConnection = 0x002065B0u;
    bool multitap()
    {
        static const bool s_on = []
        {
            const char *value = std::getenv("TMB_MULTITAP");
            return !(value && *value == '0');
        }();
        return s_on;
    }
    void mtapInit(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        setReturnS32(ctx, 1);
        ctx->pc = getRegU32(ctx, 31);
    }
    void mtapPortOpen(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        const bool open = multitap() && getRegU32(ctx, 4) == 0u;
        if (open)
            bozo::port::libraries(runtime).pad.multitap = true;
        setReturnS32(ctx, open ? 1 : 0);
        ctx->pc = getRegU32(ctx, 31);
    }
    void mtapPortClose(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (getRegU32(ctx, 4) == 0u)
            bozo::port::libraries(runtime).pad.multitap = false;
        setReturnS32(ctx, 1);
        ctx->pc = getRegU32(ctx, 31);
    }
    void mtapGetConnection(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        setReturnS32(ctx, getRegU32(ctx, 4) == 0u && bozo::port::libraries(runtime).pad.multitap ? 1 : 0);
        ctx->pc = getRegU32(ctx, 31);
    }

    constexpr uint32_t kParticleMove = 0x001E5158u;
    constexpr uint32_t kParticleCreateFx = 0x001E6610u;
    constexpr uint32_t kFxTestReturn = 0x00100264u;
    constexpr uint32_t kVehiclePosition = 0x260u; // as TMB_TRACE_POS reads it
    constexpr int kFxTypes = 87;                  // particleInit's table: 0x57 entries
    const auto g_processStart = std::chrono::steady_clock::now();

    struct FxTest
    {
        double period = 0.0, next = 0.0;
        int type = 0;
        uint32_t ra = 0, sp = 0;
    } g_fxTest;

    double processSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_processStart).count();
    }

    void particleMoveFxTest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        FxTest &t = g_fxTest;
        const double now = processSeconds();
        if (t.next == 0.0)
            t.next = now + 5.0; // the match's first seconds without
        if (now < t.next || t.type >= kFxTypes)
        {
            particleMove__Fv_0x1e5158(rdram, ctx, runtime);
            return;
        }
        t.next = now + t.period;
        const int type = t.type++;
        float target[4];
        std::memcpy(target, rdram + kPlayerVehicle + kVehiclePosition, sizeof(target));
        target[3] = 1.0f;
        t.ra = getRegU32(ctx, 31);
        t.sp = getRegU32(ctx, 29);
        const uint32_t sp = (t.sp - 0x40u) & ~0xFu; // the vector above the callee's frame
        std::memcpy(rdram + ((sp + 0x10u) & 0x1FFFFFFu), target, sizeof(target));
        SET_GPR_U32(ctx, 29, sp);
        SET_GPR_U32(ctx, 4, sp + 0x10u);
        SET_GPR_U32(ctx, 5, static_cast<uint32_t>(type));
        SET_GPR_U32(ctx, 6, 0u); // no orientation
        ctx->f[12] = 1.0f;       // scale
        ctx->f[13] = 0.0f;
        SET_GPR_U32(ctx, 31, kFxTestReturn);
        std::fprintf(stderr, "[tmb:fx] t=%.2f type %d at %.1f %.1f %.1f\n", now, type, target[0], target[1], target[2]);
        ctx->pc = kParticleCreateFx;
        particleCreateFx__FP8_fvectorifPA3_ff_0x1e6610(rdram, ctx, runtime);
    }

    void fxTestReturn(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        std::fprintf(stderr, "[tmb:fx]   -> fx %d\n", static_cast<int32_t>(getRegU32(ctx, 2)));
        SET_GPR_U32(ctx, 29, g_fxTest.sp);
        SET_GPR_U32(ctx, 31, g_fxTest.ra);
        ctx->pc = kParticleMove;
        particleMove__Fv_0x1e5158(rdram, ctx, runtime);
    }

    // The Logitech wheel's library (lgDevInit, the input module's force-feedback calls): no wheel is ever attached, so
    // each returns 0 at once (the PS2Recomp build's TOML "ret0" stubs; they stay stubs in the generated code:
    // recomp/tmb_stubs.txt).
    constexpr uint32_t kLgStubs[] = {0x0020F8F8u, 0x001D78D0u, 0x001D9370u, 0x001D93D0u, 0x001D9568u,
                                     0x001D96A8u, 0x001D9968u, 0x001D9B20u, 0x001D9B98u};

    void returnZero(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        setReturnS32(ctx, 0);
        ctx->pc = getRegU32(ctx, 31);
    }

    void applyTmb(tmb::Registry &runtime)
    {
        for (uint32_t address : kLgStubs)
            runtime.registerFunction(address, &returnZero);
        runtime.registerFunction(kVblankHandler, &vblankHandlerOverride);
        runtime.registerFunction(kHier, &hierOverride);
        tmb::native::install();          // TMB_NATIVE
        tmb::textures::install(runtime); // the textures from the offline texture pack for the native renderer
        tmb::twod::install(runtime);     // the 2D builders rewritten natively (runtime/tmb_2d.cpp)
        tmb::meshes::install(runtime);   // rigid objects from the offline mesh pack (runtime/tmb_meshes.cpp)
        tmb::particles::install(runtime); // particles drawn from their structs (runtime/tmb_particles.cpp)
        if (const char *trace = std::getenv("TMB_TRACE_HIER"); trace && *trace == '1')
            runtime.registerFunction(kHierDmaHandler, &hierDmaHandlerTrace);
        if (const char *trace = std::getenv("TMB_TRACE_SND"); trace && *trace == '1')
            runtime.registerFunction(kSndSendIopCommandAndWait, &sndSendIopCommandAndWaitTrace);
        runtime.registerFunction(kVideoDecFlush, &videoDecFlushOverride);
        if (const char *maxMs = std::getenv("TMB_MAX_FRAME_MS"); maxMs && *maxMs)
            g_maxFrameTicks = std::max(1, std::atoi(maxMs)) * 576;
        runtime.registerFunction(kTimerEndOfRealTimeProcessing, &timerEndOfRealTimeProcessingOverride);
        if (tmb::native::enabled())
            runtime.registerFunction(kSceGsExecStoreImage, &sceGsExecStoreImageOverride);
        g_traceFrames = std::getenv("TMB_TRACE_FRAMES") != nullptr;
        if (const char *substeps = std::getenv("TMB_SUBSTEPS"); substeps && *substeps)
            g_maxSubsteps = std::max(1, std::atoi(substeps));
        runtime.registerFunction(kTheGameUpdate, &theGameUpdateOverride);
        if (const char *timeline = std::getenv("TMB_TRACE_TIMELINE"); timeline && *timeline)
        {
            g_timeline.path = timeline;
            if (const char *start = std::getenv("TMB_TRACE_TIMELINE_START"); start && *start)
                g_timeline.startUpdate = static_cast<uint32_t>(std::max(1, std::atoi(start)));
            if (const char *seconds = std::getenv("TMB_TRACE_TIMELINE_SECONDS"); seconds && *seconds)
                g_timeline.durationNs = static_cast<int64_t>(std::max(1.0, std::atof(seconds)) * 1e9);
            runtime.registerFunction(0x00203C18u, &sceGsSyncPathTimeline);
        }
        runtime.registerFunction(kSubstepReturn, &substepReturn);
        runtime.registerFunction(kMtapInit, &mtapInit);
        runtime.registerFunction(kMtapPortOpen, &mtapPortOpen);
        runtime.registerFunction(kMtapPortClose, &mtapPortClose);
        runtime.registerFunction(kMtapGetConnection, &mtapGetConnection);
        tmb::cheats::install(runtime); // TMB_NO_AI, infinite health / ammo / energy, weak enemies (runtime/tmb_cheats.cpp)
        if (const char *fx = std::getenv("TMB_FX_TEST"); fx && *fx)
        {
            g_fxTest.period = std::max(0.5, std::atof(fx));
            runtime.registerFunction(kParticleMove, &particleMoveFxTest);
            runtime.registerFunction(kFxTestReturn, &fxTestReturn);
        }
        // Native frames: the frames on the PC clock (timer1HandlerNative).
        runtime.registerFunction(kTimer1Handler, &timer1HandlerNative);
        runtime.registerFunction(kSceGsSyncV, &sceGsSyncVNative);
        if (const char *path = std::getenv("TMB_TRACE_BLACKBOX"); path && *path)
            g_blackboxPath = path;
        if (const char *path = std::getenv("TMB_TRACE_POS"); path && *path)
            if ((g_posTrace = std::fopen(path, "w")))
                std::setvbuf(g_posTrace, nullptr, _IOLBF, 0);
        if (g_blackboxPath || g_posTrace)
            runtime.registerFunction(0x00117D48u, &collisCheckHdResultsBlackbox);
        runtime.registerFunction(kVideoDecAbort, &videoDecAbortOverride);
        runtime.registerFunction(kPcmCallback, &pcmCallbackOverride);
        runtime.registerFunction(kAudioDecSendToIop, &audioDecSendToIopOverride);
        runtime.registerFunction(kAudioDecCreate, &audioDecCreateOverride);
        runtime.registerFunction(kAudioDecReset, &audioDecResetOverride);
        runtime.registerFunction(kSwitchThread, &switchThreadOverride);
        runtime.registerFunction(kSwitchThreadReturn, &switchThreadReturn);
        if (const char *trace = std::getenv("TMB_TRACE_FONT"); trace && *trace == '1')
            runtime.registerFunction(kFontDmaFontData, &fontDmaFontDataTrace);
        if (const char *trace = std::getenv("TMB_TRACE_MOVIE"); trace && *trace == '1')
        {
            runtime.registerFunction(kTermAll, &termAllTrace);
            g_traceMovie = true;
        }
    }
}

namespace tmb
{
    std::string findDiscImage(const std::string &fromFolder, const std::string &elfName)
    {
        return ::findDiscImage(std::filesystem::path(fromFolder) / elfName).string();
    }
}

BOZO_PORT("Twisted Metal - Black (USA)", "SCUS_971.01", kTmbPalEntry, &applyTmb)
