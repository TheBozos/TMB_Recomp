// TMB's particles drawn natively from the game's particle systems (on with TMB_NATIVE=1, TMB_NATIVE_PARTICLES=0 turns it off): no VIF
// packet is read and no GS packet is sent for the types done here.
//
// particleDraw(view) walks the active systems (_ParticleType, 0x2e80 bytes each, list nodes of 12 bytes, next at +8),
// culls each against the view's frustum (its emitter at +0x2c80 in camera space, a sphere of radius +0x2c8c), sorts the
// survivors far to near (camera depth less the system's bias at +0x2e54), and for each one appends its texture uploads
// (texmParticleTexture) and a DMA CALL of the packet its builder made in particlePrepDraw; that packet runs one of the
// effects image's VU1 programs (sprite 0x390, textured sprite 0x798, ...). Then the lens flares (the lights on screen
// and their Z read back from the GS, their visibility, the flare packet's CALL: VU1 0x2338), and the chain goes to
// VIF1 from its head (0x741f00: the effects microcode, the GS context, the effects constants' MSCALF 0x0).
//
// The override does the walk, the cull, the sort and the lens flares' list itself (the same arithmetic), then:
// - a system of a type done here is drawn now, from its struct: the vertices the builder plus its VU1 program would
//   give (the maths of runtime/tmb_native.cpp's translations, checked there against VU1, fed from the struct instead
//   of VU memory), with the register state from the game's data (section "State");
// - the others are left to the game: the function continues in its own code (its chain loop, entered at its label
//   with the remaining systems in its sorted array), so their textures, their CALLs, the lens flares and the kick are
//   the game's. The chain head is kicked whenever anything was drawn, natively or not.
//
// Draw order: the native draws take the group of the chain head's MSCALF 0x0 (the adapter numbers it as the next
// natively drawn program, tmb::native::nextStreamGroup), so they are drawn where the particle chain sits in the GS
// stream, after the 3D and before what follows; inside it, all the native systems come first (far to near), then the
// game's own (far to near), then the lens flares.
//
// The lens flares (section "The lens flares"): particleDraw's tail still calls particleLensFlareTestZbuffer and
// particleLensFlareRender, replaced while the override's particleDraw runs: the game's arithmetic on its flare list
// (the lights' screen points, the cull, the random numbers, the visibility, the debounce), the Z samples from the native
// frame's depth (Renderer::depthAt, no GS readback), the groups from lensFlarePacketInit's tables and the textures by
// id; the sprites drawn there, in the chain's group after the native systems (the game's own systems, if any, follow
// them now). No flare packet is built or called.
//
// State: ALPHA_1 from the system (+0x2e58); TEX0 from the type's texture slot (the table at 0x6879c0, texture id at
// 0x687dc0, the texture itself from the native texture pack by that id); TEST_1 0x7000d (alpha greater than 0, depth
// greater or equal), ZBUF_1 masked (particles never write depth), TEX1 bilinear and TEXA 0x80: defaultContextInit's
// constants; the effects constants (VU 0-7: the view's screen matrix, then the size, guard band and fog terms) from the
// block particleUpdateRainViewport fills for the view (0x230010), which the chain head uploads.
//
// Types done here: 1 lines (0x118), 2 sprites (0x390), 3 textured sprites (0x798), 4 stripes (0xcf8), 6 electric arcs
// (0x1198, and 0x10e0 for the arcs on a sphere), 7 sprines (0x1830), 8 kites / heat haze (0xa08), 9 the rain (0x1d40)
// or snow (0x1f30). What a builder keeps only in its packet is kept here by hooking the builder (the texture choice of
// type 3, the stripes' texture scroll, the arcs' colours and sphere state); everything else is computed from the
// struct and the game's tables as the builders compute it. The bit that attaches a system's matrix is bit 15 of the
// count word (VU1 reads it with ILW, 16 bits, and branches on its sign; loadMatrixIfNecessary sets it exactly when
// +0x2d6c is not null), so the matrix is applied whenever the system has one. Left to the game: the other types and,
// with TMB_NATIVE_PARTICLES_FLARES=0, the lens flares (particleLensFlareRender's packet, drawn by
// the adapter's 0x2338 translation).
//
// TMB_NATIVE_PARTICLES_CHECK=1 (a diagnostic): nothing drawn here; types 1, 4, 6 (not on a sphere), 7 and 8 computed and
// compared with what runtime/tmb_native.cpp's translations draw from the same systems' packets.

#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "ps2_runtime_macros.h"
#include "tmb_region.h"
#include "runtime/native/native_renderer.h"
#include "runtime/native/native_texture_cache.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tmb::native
{
    bool enabled();
    bool effectsInStream();
    uint64_t nextStreamGroup();
    bool flareDepthNative();
    bool flareDepthBlock(uint32_t x0, uint32_t y0, uint32_t z[16]);
}

namespace tmb::textures
{
    std::shared_ptr<const ps2x::native::TextureCache::Texture> lookupId(uint32_t id, uint64_t tex0, uint64_t texa);
}

namespace tmb::particles
{
    namespace
    {
        using ps2x::native::Material;
        using ps2x::native::Vertex;
        using TexturePtr = std::shared_ptr<const ps2x::native::TextureCache::Texture>;

#if TMB_REGION_US
        constexpr bool kRegionSupported = true;
        constexpr float kScreenTop = 2048.0f - 112.0f; // the field's top line (640 x 224)
        constexpr uint32_t kParticleDraw = 0x1e9540u, kTexSpriteBuild = 0x1e12d8u;
        // particleDraw's labels: its chain loop (s3 the index into the sorted array at fp, s7 their count, s2 the chain)
        // and its tail (the DMA wait, the lens flares, the END tag and the kick when s7 is not 0).
        constexpr uint32_t kResumeChainLoop = 0x1e98e0u, kResumeTail = 0x1e99f0u;
        constexpr uint32_t kSystems = 0x687f00u, kNodes = 0x742f80u;
        constexpr uint32_t kTypeTextures = 0x687280u; // per texture type: s16 textures, u16 slot, u16 width, u16 height (x16)
        constexpr uint32_t kSlotTex0 = 0x6879c0u, kSlotIds = 0x687dc0u;
        constexpr uint32_t kViews = 0x4dfcd0u, kViewTable2 = 0x4dfd30u;
        constexpr uint32_t kEffectsVu = 0x230010u; // VU 0-7 of the effects image (particleUpdateRainViewport)
        constexpr uint32_t kFogPacket = 0x49f5d0u;  // fogUpdate's A+D packet (FOGCOL)
        constexpr int32_t kGpColours = -0x2f40, kGpChain = -0x2f3c, kGpHead = -0x2f38, kGpLensCounts = -0x2f28,
                          kGpLensLists = -0x2f20, kGpLensScale = -0x47dc, kGpSeed = -0x34c0;
        // The builders whose per-system state only goes into their packet: kept here too (the stripes' texture scroll,
        // the arcs' colours, shape and matrix).
        // stripePacketInit is entered by particlePostInitFx's tail jump (not a call the runtime dispatches): the
        // post-init is what is hooked for it.
        constexpr uint32_t kPostInit = 0x1e64a0u, kStripeBuild = 0x1e2180u, kArcBuild = 0x1e22f0u, kArcUpdate = 0x1e2670u;
        // The precipitation (particlePrecipitationCreate, particleUpdateRain, particleUpdateRainViewport): its system, its
        // rain flag, its drop count's word, and the pointers the game keeps to its state (the second matrix, the head
        // offset, the box, the tail vector, the texture matrix); the constants it is made from.
        constexpr int32_t kGpPrecipIndex = -0x34b8, kGpPrecipCount = -0x34b4, kGpRainFlag = -0x2f44, kGpRainMatrix = -0x2f58,
                          kGpRainHead = -0x2f54, kGpRainBox = -0x2f50, kGpRainTail = -0x2f4c, kGpRainTexMatrix = -0x2f48,
                          kGpRainStep = -0x474c, kGpRainMul = -0x4740;
        constexpr int32_t kGpKite = -0x47d0;  // kitePacketInit's three constants (alpha factor, origin y, factor step)
        constexpr int32_t kGpSprine = -0x47c4; // sprinePacketInit's
        void gameParticleDraw(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { particleDraw__Fi_0x1e9540(rdram, ctx, runtime); }
        void gameTexSpriteBuild(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            texSpritePacketBuild__FP13_ParticleType_0x1e12d8(rdram, ctx, runtime);
        }
        void gamePostInit(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            particlePostInitFx__FP8_fvectorP13_ParticleTypef_0x1e64a0(rdram, ctx, runtime);
        }
        void gameStripeBuild(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { stripePacketBuild__FP13_ParticleType_0x1e2180(rdram, ctx, runtime); }
        constexpr uint32_t kPrecipCreate = 0x1e8238u;
        constexpr int32_t kGpPrecipLevel = -0x34b0;
        void gamePrecipCreate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { particlePrecipitationCreate__Fb_0x1e8238(rdram, ctx, runtime); }
        void gameArcBuild(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            electricArcPacketBuild__FP13_ParticleTypebPUi_0x1e22f0(rdram, ctx, runtime);
        }
        void gameArcUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            electricArcPacketUpdate__FiPUiP8_fvectorPA3_fT2_0x1e2670(rdram, ctx, runtime);
        }
        // The lens flares: particleDraw's tail calls particleLensFlareTestZbuffer(view) (the lights on screen, their Z
        // samples read back) and particleLensFlareRender(chain, view) (their visibility, lensFlarePacketBuild, the
        // groups' texture uploads, the CALL of the flare packet). The views' tables they read (viewGetWeTrans,
        // viewGetWorldToScreenMat), the Z samples' buffer (4 x 4 words per light) and the lights' debounce table
        // (32 bytes per light id, 8 per view); lensFlarePacketInit's group tables (18 groups: R, G, B bytes, the
        // texture types, the steps along the line, the sizes, the size factors, the UV extents' indexes).
        constexpr uint32_t kLensTest = 0x1dfe00u, kLensRender = 0x1e01c8u;
        constexpr uint32_t kWeTrans = 0x4a1240u, kWorldToScreen = 0x4e0230u, kLensSamples = 0x7435c0u, kLensDebounce = 0x743460u;
        constexpr uint32_t kFlareColours = 0x4f2ff0u, kFlareTypes = 0x4f3028u, kFlareSteps = 0x4f3040u, kFlareSizes = 0x4f3088u,
                           kFlareWeights = 0x4f30d0u, kFlareUvs = 0x4f3118u;
        void gameLensTest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { particleLensFlareTestZbuffer__Fi_0x1dfe00(rdram, ctx, runtime); }
        void gameLensRender(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { particleLensFlareRender__FP6QwDatai_0x1e01c8(rdram, ctx, runtime); }
#else
        // PAL (SCES-50360): dropped (US only); the addresses would be particleDraw 0x1ea750, systems 0x690500, nodes
        // 0x74b580, type textures 0x68f880, slots 0x68ffc0 / 0x6903c0, gp offsets 0x308 higher. Not installed.
        constexpr bool kRegionSupported = false;
        constexpr float kScreenTop = 2048.0f - 128.0f;
        constexpr uint32_t kParticleDraw = 0u, kTexSpriteBuild = 0u, kResumeChainLoop = 0u, kResumeTail = 0u;
        constexpr uint32_t kSystems = 0u, kNodes = 0u, kTypeTextures = 0u, kSlotTex0 = 0u, kSlotIds = 0u, kViews = 0u,
                           kViewTable2 = 0u, kEffectsVu = 0u, kFogPacket = 0u;
        constexpr int32_t kGpColours = 0, kGpChain = 0, kGpHead = 0, kGpLensCounts = 0, kGpLensLists = 0, kGpLensScale = 0,
                          kGpSeed = 0;
        constexpr uint32_t kPostInit = 0u, kStripeBuild = 0u, kArcBuild = 0u, kArcUpdate = 0u;
        constexpr int32_t kGpPrecipIndex = 0, kGpPrecipCount = 0, kGpRainFlag = 0, kGpRainMatrix = 0, kGpRainHead = 0, kGpRainBox = 0,
                          kGpRainTail = 0, kGpRainTexMatrix = 0, kGpRainStep = 0, kGpRainMul = 0, kGpKite = 0, kGpSprine = 0;
        void gameParticleDraw(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameTexSpriteBuild(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gamePostInit(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameStripeBuild(uint8_t *, R5900Context *, PS2Runtime *) {}
        constexpr uint32_t kPrecipCreate = 0u;
        constexpr int32_t kGpPrecipLevel = 0;
        void gamePrecipCreate(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameArcBuild(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameArcUpdate(uint8_t *, R5900Context *, PS2Runtime *) {}
        constexpr uint32_t kLensTest = 0u, kLensRender = 0u;
        constexpr uint32_t kWeTrans = 0u, kWorldToScreen = 0u, kLensSamples = 0u, kLensDebounce = 0u;
        constexpr uint32_t kFlareColours = 0u, kFlareTypes = 0u, kFlareSteps = 0u, kFlareSizes = 0u, kFlareWeights = 0u, kFlareUvs = 0u;
        void gameLensTest(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameLensRender(uint8_t *, R5900Context *, PS2Runtime *) {}
#endif

        // _ParticleType
        constexpr uint32_t kStride = 0x2e80u;
        constexpr uint32_t kPositions = 0x10u, kAges = 0x2940u, kBytes = 0x2b40u, kTextureIds = 0x2c40u, kEmitter = 0x2c80u,
                           kRadius = 0x2c8cu, kTranslation = 0x2d68u, kRotation = 0x2d6cu, kTexFlags = 0x2e10u,
                           kDepthTerm = 0x2e2cu, kTexScale = 0x2e34u, kColourScale = 0x2e38u, kLight = 0x2e3cu, kType = 0x2e40u,
                           kTexMask = 0x2e44u, kTexType = 0x2e48u, kColourMode = 0x2e4cu, kColourStart = 0x2e50u,
                           kSortBias = 0x2e54u, kAlpha = 0x2e58u, kFlags = 0x2e5cu, kCount = 0x2e70u;
        // The types' own parameters (what each builder makes its program's constants from).
        constexpr uint32_t kMirror = 0x2cc0u, kSpan = 0x2d88u, kArcScale = 0x2d8cu, kWidth = 0x2d98u, kWidthEnd = 0x2d9cu,
                           kSpread = 0x2da4u, kWidthStep = 0x2e24u, kLifetime = 0x2e2au, kHeads = 0x2e60u;

        // The register state every particle draws with (defaultContextInit's DIRECT, the context the chain head sets).
        constexpr uint64_t kTest = 0x7000du;  // ATE, ATST GREATER, AREF 0, ZTE, ZTST GREATER
        constexpr uint64_t kTexa = 0x80u;
        constexpr float kScreenSpace[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

        struct Settings
        {
            bool on = false;
            uint32_t types = 0; // bit per particle type drawn natively
            bool log = false;
            bool check = false;
            int precip = -1;          // TMB_NATIVE_PARTICLES_PRECIP: the precipitation forced (0 snow, 1 rain), its level
            float precipLevel = 0.0f;
            double toggle = 0.0;
            bool flares = true;       // TMB_NATIVE_PARTICLES_FLARES=0: the lens flares left to the game's packet
            bool toggleFlares = false; // TMB_NATIVE_PARTICLES_TOGGLE_FLARES=1: the toggle alternates the flares alone
        };

        const Settings &settings();

        bool toggledOff()
        {
            const double period = settings().toggle;
            if (!(period > 0.0))
                return false;
            static const auto s_start = std::chrono::steady_clock::now();
            const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_start).count();
            return (static_cast<uint64_t>(now / period) & 1u) != 0u;
        }

        const Settings &settings()
        {
            static const Settings s_settings = []
            {
                Settings s;
                const char *on = std::getenv("TMB_NATIVE_PARTICLES");
                s.on = kRegionSupported && !(on && *on == '0'); // on with TMB_NATIVE=1 unless =0
                // TMB_NATIVE_PARTICLE_TYPES=<type>[,<type>...]: the types drawn natively (default: all done here).
                s.types = (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9);
                if (const char *types = std::getenv("TMB_NATIVE_PARTICLE_TYPES"); types && *types)
                {
                    uint32_t mask = 0;
                    for (char *end = nullptr; *types; types = *end ? end + 1 : end)
                    {
                        const unsigned long type = std::strtoul(types, &end, 10);
                        if (end == types)
                            break;
                        if (type < 32u)
                            mask |= 1u << type;
                    }
                    s.types &= mask;
                }
                // TMB_NATIVE_PARTICLES_CHECK=1: nothing drawn here; the types the adapter still translates from their
                // packets (1, 4, 6, 7, 8) computed and compared with those translations (a diagnostic of the struct path).
                const char *check = std::getenv("TMB_NATIVE_PARTICLES_CHECK");
                s.check = check && *check == '1';
                // TMB_NATIVE_PARTICLES_PRECIP=<0 snow | 1 rain>,<level>: a diagnostic: the arena's precipitation made
                // snow or rain (particlePrecipitationCreate's flag) at that level (the factor of its drop count),
                // to see either where the arena has the other or too little.
                if (const char *precip = std::getenv("TMB_NATIVE_PARTICLES_PRECIP"); precip && *precip)
                {
                    s.precip = *precip == '1' ? 1 : 0;
                    if (const char *comma = std::strchr(precip, ','))
                        s.precipLevel = static_cast<float>(std::strtod(comma + 1, nullptr));
                }
                const char *log = std::getenv("TMB_NATIVE_PARTICLES_LOG");
                s.log = log && *log == '1';
                // TMB_NATIVE_PARTICLES_TOGGLE=<seconds>: native and the game's own draws in turn, that long each (an A/B
                // in one run; the native periods mark the screen's top-left corner with a small magenta square).
                if (const char *toggle = std::getenv("TMB_NATIVE_PARTICLES_TOGGLE"); toggle && *toggle)
                    s.toggle = std::strtod(toggle, nullptr);
                // TMB_NATIVE_PARTICLES_FLARES=0: the lens flares through the game's own path (particleLensFlareTestZbuffer,
                // particleLensFlareRender, its packet drawn by the adapter's 0x2338 translation); with
                // TMB_NATIVE_PARTICLES_TOGGLE_FLARES=1 the toggle above alternates only them (the other types stay native).
                const char *flares = std::getenv("TMB_NATIVE_PARTICLES_FLARES");
                s.flares = !(flares && *flares == '0');
                const char *toggleFlares = std::getenv("TMB_NATIVE_PARTICLES_TOGGLE_FLARES");
                s.toggleFlares = toggleFlares && *toggleFlares == '1';
                return s;
            }();
            return s_settings;
        }

        // Guest memory (ordinary RAM: the structs, tables, stack).
        uint32_t u32(const uint8_t *rdram, uint32_t address)
        {
            uint32_t value;
            std::memcpy(&value, rdram + (address & 0x1fffffcu), 4);
            return value;
        }
        uint64_t u64(const uint8_t *rdram, uint32_t address)
        {
            uint64_t value;
            std::memcpy(&value, rdram + (address & 0x1fffff8u), 8);
            return value;
        }
        uint16_t u16(const uint8_t *rdram, uint32_t address)
        {
            uint16_t value;
            std::memcpy(&value, rdram + (address & 0x1fffffeu), 2);
            return value;
        }
        uint8_t u8(const uint8_t *rdram, uint32_t address) { return rdram[address & 0x1ffffffu]; }
        float f32(const uint8_t *rdram, uint32_t address)
        {
            float value;
            std::memcpy(&value, rdram + (address & 0x1fffffcu), 4);
            return value;
        }
        void put32(uint8_t *rdram, uint32_t address, uint32_t value) { std::memcpy(rdram + (address & 0x1fffffcu), &value, 4); }
        void put64(uint8_t *rdram, uint32_t address, uint64_t value) { std::memcpy(rdram + (address & 0x1fffff8u), &value, 8); }
        void putF(uint8_t *rdram, uint32_t address, float value) { std::memcpy(rdram + (address & 0x1fffffcu), &value, 4); }
        void put8(uint8_t *rdram, uint32_t address, uint8_t value) { rdram[address & 0x1ffffffu] = value; }

        uint64_t reg64(const R5900Context *ctx, int reg) { return reg == 0 ? 0u : static_cast<uint64_t>(_mm_cvtsi128_si64(ctx->r[reg])); }
        void setReg(R5900Context *ctx, int reg, uint32_t value)
        {
            ctx->r[reg] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(value)));
        }

        bool typeNative(uint32_t type) { return type < 32u && ((settings().types >> type) & 1u) != 0u; }

        // texSpritePacketBuild's texture choice per particle (its first loop), as the builder will make it: kept per
        // system for the native draw (the builder's own result only goes into the packet).
        std::unordered_map<uint32_t, std::vector<uint8_t>> g_texIndex;

        void chooseTextures(const uint8_t *rdram, uint32_t gp, uint32_t ps)
        {
            const uint32_t count = u32(rdram, ps + kCount), flags = u32(rdram, ps + kTexFlags);
            const uint32_t maskWord = u32(rdram, ps + kTexMask), scale = u32(rdram, ps + kTexScale);
            const uint8_t maskByte = u8(rdram, ps + kTexMask);
            uint64_t seed = u64(rdram, gp + static_cast<uint32_t>(kGpSeed)); // the game's LCG: the builder draws the same numbers
            std::vector<uint8_t> &out = g_texIndex[ps];
            out.assign(std::min<uint32_t>(count, 0x200u), 0u);
            for (uint32_t i = 0; i < out.size(); ++i)
            {
                uint32_t index;
                if (flags & 1u)
                {
                    seed = seed * 0x41c64e6dull + 0x3039u;
                    index = static_cast<uint32_t>(seed >> 16) & 0xffu & maskByte;
                }
                else if (flags & 4u)
                    index = maskByte & (u8(rdram, ps + kBytes + i) >> 2);
                else
                {
                    const uint32_t t = (static_cast<uint32_t>(u16(rdram, ps + kAges + 2u * i)) * scale) >> 12;
                    index = (flags & 2u) ? (maskWord < t ? maskWord : t) : (maskByte & t);
                }
                out[i] = static_cast<uint8_t>(index);
            }
        }

        void texSpriteBuildOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (settings().on && typeNative(3u))
                chooseTextures(rdram, getRegU32(ctx, 28), getRegU32(ctx, 4));
            gameTexSpriteBuild(rdram, ctx, runtime); // the packet still, for the draws left to the game (menus)
        }

        // loadColors for one particle: the system's palette (the colour table + start * 4) by age.
        uint32_t particleColour(const uint8_t *rdram, uint32_t gp, uint32_t ps, uint32_t index)
        {
            const uint32_t mode = u32(rdram, ps + kColourMode);
            const uint32_t table = u32(rdram, gp + static_cast<uint32_t>(kGpColours)) + u32(rdram, ps + kColourStart) * 4u;
            if (mode < 2u)
                return mode == 0u ? 0x80808080u : u32(rdram, table);
            const uint32_t t = static_cast<uint32_t>(u16(rdram, ps + kAges + 2u * index)) * u32(rdram, ps + kColourScale);
            const uint32_t at = table + (t >> 12) * 4u, step = t & 0xfffu;
            uint32_t colour = 0;
            for (uint32_t k = 0; k < 4u; ++k)
            {
                const uint32_t lo = u8(rdram, at + k), hi = u8(rdram, at + 4u + k);
                const uint32_t mixed = lo + (static_cast<uint32_t>(static_cast<int32_t>(hi - lo) * static_cast<int32_t>(step)) >> 12);
                colour |= (mixed & 0xffu) << (8u * k);
            }
            return colour;
        }

        // The GS blend of ALPHA_1, as the adapter maps it (runtime/tmb_native.cpp blendFor).
        Material::Blend blendFor(uint64_t alpha)
        {
            const uint32_t b = (alpha >> 2) & 3u, d = (alpha >> 6) & 3u;
            if (b == 2u && d == 1u)
                return Material::Blend::Additive;
            return Material::Blend::Alpha;
        }

        uint32_t fogColour(const uint8_t *rdram)
        {
            if (u8(rdram, kFogPacket + 8u) != 0x3du)
                return 0u;
            return u32(rdram, kFogPacket) & 0xffffffu;
        }

        struct Stats
        {
            uint64_t frames = 0, nativeSystems = 0, nativeSprites = 0, gameSystems = 0, culled = 0, missingTextures = 0, sphereArcs = 0;
            uint64_t flareLights = 0, flareSprites = 0;
            uint64_t byType[16] = {};
        } g_stats;

        // One draw batch: a material and its screen-space triangles.
        struct Batch
        {
            Material material;
            TexturePtr texture;
            std::vector<Vertex> triangles;
            bool open = false;
        };

        std::vector<Vertex> *g_capture = nullptr; // TMB_NATIVE_PARTICLES_CHECK: the draws go here instead

        void flush(Batch &batch)
        {
            if (g_capture)
                g_capture->insert(g_capture->end(), batch.triangles.begin(), batch.triangles.end());
            else if (batch.open && !batch.triangles.empty())
                ps2x::native::Renderer::instance().draw(batch.material, kScreenSpace, batch.triangles.data(),
                                                        static_cast<uint32_t>(batch.triangles.size()), batch.texture);
            batch.triangles.clear();
            batch.open = false;
        }

        // A batch for this texture (a new one when it differs from the open one).
        void useTexture(Batch &batch, const Material &base, const uint8_t *rdram, uint32_t id, uint64_t tex0)
        {
            const uint64_t key = (tex0 & ~uint64_t{0x3fff}) | (id & 0x3fffu); // TBP is the game's VRAM choice: keyed by id
            if (batch.open && batch.material.tex0 == key)
                return;
            flush(batch);
            batch.material = base;
            batch.material.tex0 = key;
            batch.texture = tmb::textures::lookupId(id, tex0, kTexa);
            batch.material.textured = batch.texture != nullptr;
            if (!batch.texture)
                ++g_stats.missingTextures;
            batch.open = true;
            (void)rdram;
        }

        void pushQuad(Batch &batch, const Vertex corner[4])
        {
            const int order[6] = {0, 1, 2, 1, 2, 3}; // as the adapter splits a sprite
            for (int i : order)
                batch.triangles.push_back(corner[i]);
        }

        // The view's data the draws use: the effects constants (VU 0-3 the screen matrix's columns, VU 4-7 vf29, vf27,
        // vf28, vf31 of the programs) and the fog colour.
        struct ViewConstants
        {
            float vu[8][4];
            uint32_t fog = 0;
        };

        // A sprite's state: textured, blended by the system's ALPHA, alpha tested, depth tested and never written.
        Material baseMaterial(const uint8_t *rdram, uint32_t ps, bool fogged, uint32_t fog)
        {
            Material m;
            m.texa = kTexa;
            m.blend = blendFor(u32(rdram, ps + kAlpha));
            const uint32_t atst = (kTest >> 1) & 7u, aref = (kTest >> 4) & 0xffu;
            m.alphaTest = atst == 6u || aref > 0u;
            m.alphaRef = static_cast<uint8_t>(atst == 6u ? aref : aref - 1u);
            m.depthTest = true;
            m.depthWrite = false;
            m.fog = fogged;
            m.fogColour = fog;
            return m;
        }

        // The type's four UV variants (VU 296-299 from spritePacketBuild / texSpritePacketBuild): U1 V1 U2 V2 in texels x 16,
        // half a texel in from the edges.
        void uvRows(const uint8_t *rdram, uint32_t typeTexture, uint32_t rows[4][4])
        {
            const uint32_t w = static_cast<uint16_t>(u16(rdram, typeTexture + 4u) - 8u), h = static_cast<uint16_t>(u16(rdram, typeTexture + 6u) - 8u);
            const uint32_t values[4][4] = {{w, h, 8u, 8u}, {w, 8u, 8u, h}, {8u, h, w, 8u}, {8u, 8u, w, h}};
            std::memcpy(rows, values, sizeof(values));
        }

        // The object-to-screen matrix: VU 0-3 times the system's own (its attach matrix: rows at *+0x2d6c, the
        // translation at *+0x2d68), as loadMatrixIfNecessary loads it and the programs multiply it in.
        void screenMatrix(const uint8_t *rdram, uint32_t ps, const ViewConstants &view, bool attach, float m[16])
        {
            float object[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
            const uint32_t rotation = u32(rdram, ps + kRotation);
            if (attach && rotation != 0u)
            {
                const uint32_t translation = u32(rdram, ps + kTranslation);
                for (int j = 0; j < 3; ++j)
                {
                    for (int k = 0; k < 3; ++k)
                        object[j][k] = f32(rdram, rotation + 16u * j + 4u * k);
                    object[j][3] = 0.0f;
                }
                for (int k = 0; k < 3; ++k)
                    object[3][k] = f32(rdram, translation + 4u * k);
                object[3][3] = 1.0f;
            }
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    m[j * 4 + k] = view.vu[0][k] * object[j][0] + view.vu[1][k] * object[j][1] + view.vu[2][k] * object[j][2] +
                                   view.vu[3][k] * object[j][3];
        }

        // Type 2 (spritePacketBuild, VU1 0x390): runtime/tmb_native.cpp particles(). The particles go in from the last:
        // the builder unpacks the positions at VU 8 on, the program reads them downwards, and the colours, UV indexes
        // (the low two bits of +0x2b40) and ages are read downwards too.
        void drawSprites(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            const uint32_t count = u32(rdram, ps + kCount);
            const uint32_t typeTexture = kTypeTextures + u32(rdram, ps + kTexType) * 8u;
            const uint32_t slot = u16(rdram, typeTexture + 2u);
            const uint64_t tex0 = u64(rdram, kSlotTex0 + slot * 8u);
            const uint32_t id = u16(rdram, kSlotIds + slot * 2u);
            const uint32_t alpha = u32(rdram, ps + kAlpha);
            // ALPHA 0x44 (over): fogged (PRIM.FGE), the first variant; otherwise the second (TOP.z & 0x8000): fog as
            // the alpha's factor, no size limit.
            const bool fogged = alpha == 0x44u, fade = !fogged;
            Batch batch;
            useTexture(batch, baseMaterial(rdram, ps, fogged, view.fog), rdram, id, tex0);
            uint32_t rows[4][4];
            uvRows(rdram, typeTexture, rows);
            float m[16];
            screenMatrix(rdram, ps, view, true, m);
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float(&c7)[4] = view.vu[7];
            const uint32_t width = 1u << ((tex0 >> 26) & 0xfu), height = 1u << ((tex0 >> 30) & 0xfu);
            const float guard = std::fabs(c6[3]);
            for (uint32_t k = 0; k < count; ++k)
            {
                const uint32_t index = count - 1u - k;
                const uint32_t at = ps + kPositions + 16u * index;
                const float p[4] = {f32(rdram, at), f32(rdram, at + 4u), f32(rdram, at + 8u), f32(rdram, at + 12u)};
                float c[4];
                for (int i = 0; i < 4; ++i)
                    c[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i];
                const float q = 1.0f / c[3];
                float half[3];
                for (int i = 0; i < 3; ++i)
                    half[i] = c5[i] * p[3] * q;
                float fog, fadeFactor = 1.0f;
                if (!fade)
                {
                    half[0] = std::min(half[0], c5[0] * 0.5f);
                    half[1] = std::min(half[1], c5[1] * 0.5f);
                    fog = std::max(std::min(c4[3] + c5[3] * q, c7[3]), 0.0f);
                }
                else
                    fog = fadeFactor = std::max(std::min((c4[3] + c5[3] * q) / c7[3], 1.0f), 0.0f);
                const float x0 = c[0] * q - half[0], y0 = c[1] * q - half[1];
                const float x1 = c[0] * q + half[0], y1 = c[1] * q + half[1], z = c[2] * q + half[2];
                if (std::fabs(x0 + c6[0]) > guard || std::fabs(y0 + c6[1]) > guard || std::fabs(x1 + c6[0]) > guard ||
                    std::fabs(y1 + c6[1]) > guard || c6[2] + c[3] < -guard)
                    continue;
                const uint32_t colour = particleColour(rdram, gp, ps, index);
                uint32_t rgba = 0;
                for (int i = 0; i < 3; ++i)
                {
                    const float value = static_cast<float>((colour >> (8 * i)) & 0xffu) + c4[i];
                    const uint32_t byte = fade ? static_cast<uint32_t>(static_cast<int32_t>(value)) & 0xffu
                                               : (value > 0.0f ? std::min(static_cast<uint32_t>(value), 255u) : 0u);
                    rgba |= byte << (8 * i);
                }
                const float a = static_cast<float>(colour >> 24) * fadeFactor;
                const uint32_t alphaByte = fade ? static_cast<uint32_t>(static_cast<int32_t>(a)) & 0xffu
                                                : (a > 0.0f ? std::min(static_cast<uint32_t>(a), 255u) : 0u);
                rgba |= alphaByte << 24;
                const uint32_t *row = rows[u8(rdram, ps + kBytes + index) & 3u];
                const float u0 = static_cast<float>(row[0]) / 16.0f / width, v0 = static_cast<float>(row[1]) / 16.0f / height;
                const float u1 = static_cast<float>(row[2]) / 16.0f / width, v1 = static_cast<float>(row[3]) / 16.0f / height;
                Vertex corner[4];
                const float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1}, us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
                for (int i = 0; i < 4; ++i)
                {
                    corner[i].x = xs[i];
                    corner[i].y = ys[i];
                    corner[i].z = z;
                    corner[i].u = us[i];
                    corner[i].v = vs[i];
                    corner[i].rgba = rgba;
                    corner[i].fog = std::floor(fog) / 255.0f;
                }
                pushQuad(batch, corner);
                ++g_stats.nativeSprites;
            }
            flush(batch);
        }

        // Type 3 (texSpritePacketBuild, VU1 0x798): runtime/tmb_native.cpp texParticles(). A texture per particle (one of the
        // type's textures, chosen by the builder: chooseTextures), never attached to a matrix, always fogged, Z from
        // the distance alone: -1024 + 16778240 / (w - the system's depth term at +0x2e2c).
        void drawTexSprites(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            (void)gp;
            const uint32_t count = u32(rdram, ps + kCount);
            const uint32_t typeTexture = kTypeTextures + u32(rdram, ps + kTexType) * 8u;
            const uint32_t slot = u16(rdram, typeTexture + 2u);
            const auto found = g_texIndex.find(ps);
            const std::vector<uint8_t> *choice = found != g_texIndex.end() ? &found->second : nullptr;
            const Material base = baseMaterial(rdram, ps, true, view.fog);
            Batch batch;
            uint32_t rows[4][4];
            uvRows(rdram, typeTexture, rows);
            float m[16];
            screenMatrix(rdram, ps, view, false, m);
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float(&c7)[4] = view.vu[7];
            const float bias = f32(rdram, ps + kDepthTerm);
            const float guard = std::fabs(c6[3]);
            for (uint32_t k = 0; k < count; ++k)
            {
                const uint32_t index = count - 1u - k;
                const uint32_t at = ps + kPositions + 16u * index;
                const float p[4] = {f32(rdram, at), f32(rdram, at + 4u), f32(rdram, at + 8u), f32(rdram, at + 12u)};
                float c[4];
                for (int i = 0; i < 4; ++i)
                    c[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i];
                const float q = 1.0f / c[3];
                const float hx = std::min(c5[0] * p[3] * q, c5[0] * 0.5f), hy = std::min(c5[1] * p[3] * q, c5[1] * 0.5f);
                const float fog = std::min(c4[3] + c5[3] * q, c7[3]);
                const float z = -1024.0f + 16778240.0f / (c[3] - bias);
                const float x0 = c[0] * q - hx, y0 = c[1] * q - hy, x1 = c[0] * q + hx, y1 = c[1] * q + hy;
                if (std::fabs(x0 + c6[0]) > guard || std::fabs(y0 + c6[1]) > guard || c6[2] + c[3] < -guard ||
                    std::fabs(x1 + c6[0]) > guard || std::fabs(y1 + c6[1]) > guard || c6[2] + fog < -guard)
                    continue;
                const uint32_t texture = choice && index < choice->size() ? (*choice)[index] : 0u;
                const uint64_t tex0 = u64(rdram, kSlotTex0 + (slot + texture) * 8u);
                const uint32_t id = u16(rdram, kSlotIds + (slot + texture) * 2u);
                useTexture(batch, base, rdram, id, tex0);
                const uint32_t width = 1u << ((tex0 >> 26) & 0xfu), height = 1u << ((tex0 >> 30) & 0xfu);
                const uint32_t colour = particleColour(rdram, gp, ps, index);
                uint32_t rgba = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const float value = static_cast<float>((colour >> (8 * i)) & 0xffu) + (i < 3 ? c4[i] : 0.0f);
                    rgba |= std::min(static_cast<uint32_t>(std::max(value, 0.0f)), 255u) << (8 * i);
                }
                const uint32_t *row = rows[u8(rdram, ps + kBytes + index) & 3u];
                const float u0 = static_cast<float>(row[0]) / 16.0f / width, v0 = static_cast<float>(row[1]) / 16.0f / height;
                const float u1 = static_cast<float>(row[2]) / 16.0f / width, v1 = static_cast<float>(row[3]) / 16.0f / height;
                Vertex corner[4];
                const float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1}, us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
                for (int i = 0; i < 4; ++i)
                {
                    corner[i].x = xs[i];
                    corner[i].y = ys[i];
                    corner[i].z = z;
                    corner[i].u = us[i];
                    corner[i].v = vs[i];
                    corner[i].rgba = rgba;
                    corner[i].fog = std::floor(std::max(fog, 0.0f)) / 255.0f;
                }
                pushQuad(batch, corner);
                ++g_stats.nativeSprites;
            }
            flush(batch);
        }

        // Type 1 (linePacketInit / linePacketBuild, VU1 0x118): runtime/tmb_native.cpp streaks(). Line k runs from its head
        // (+0x2e60, VU 8 + k) back by twice its tail vector (+0x2e64, VU 264 + k), coloured from the palette the init
        // converts to floats (min(mode + 1, 16) entries from the colour table at start - 1 + 1; the entries past the mode
        // keep the last colour with alpha 0) at the tail's w times (mode - 1) / lifetime, its alpha times the fog terms
        // and the head's w; a Gouraud line (no texture, no fog) fading to alpha 0, drawn one pixel wide.
        void drawLines(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            const uint32_t count = u32(rdram, ps + kCount), mode = u32(rdram, ps + kColourMode);
            const uint32_t heads = u32(rdram, ps + 0x2e60u), tails = u32(rdram, ps + 0x2e64u);
            float palette[16][4] = {};
            const uint32_t entries = mode + 1u < 17u ? mode + 1u : 16u;
            uint32_t at = u32(rdram, gp + static_cast<uint32_t>(kGpColours)) + u32(rdram, ps + kColourStart) * 4u - 4u;
            for (uint32_t j = 0; j < entries; ++j)
            {
                if (j < mode)
                {
                    at += 4u;
                    palette[j][3] = static_cast<float>(u8(rdram, at + 3u));
                }
                for (uint32_t k = 0; k < 3u; ++k)
                    palette[j][k] = static_cast<float>(u8(rdram, at + k));
            }
            const int16_t lifetime = static_cast<int16_t>(u16(rdram, ps + 0x2e2au));
            const float scale = static_cast<float>(static_cast<int32_t>(mode - 1u)) / static_cast<float>(lifetime);
            float m[16];
            screenMatrix(rdram, ps, view, true, m);
            for (int j = 0; j < 4; ++j)
                m[j * 4 + 2] *= 0.0625f; // the program's z row over 16 (XYZ2 takes FTOI4)
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float guard = std::fabs(c6[3]);
            Batch batch;
            batch.material = baseMaterial(rdram, ps, false, view.fog);
            batch.material.textured = false;
            batch.open = true;
            const auto paletteAt = [&](int32_t i, int k) { return i >= 0 && i < 16 ? palette[i][k] : 0.0f; };
            for (uint32_t k = 0; k < count; ++k)
            {
                const uint32_t h = heads + 16u * k, t = tails + 16u * k;
                const float start[4] = {f32(rdram, h), f32(rdram, h + 4u), f32(rdram, h + 8u), f32(rdram, h + 12u)};
                const float end[3] = {start[0] - 2.0f * f32(rdram, t), start[1] - 2.0f * f32(rdram, t + 4u), start[2] - 2.0f * f32(rdram, t + 8u)};
                float a[4], b[4];
                for (int i = 0; i < 4; ++i)
                {
                    a[i] = m[i] * start[0] + m[4 + i] * start[1] + m[8 + i] * start[2] + m[12 + i];
                    b[i] = m[i] * end[0] + m[4 + i] * end[1] + m[8 + i] * end[2] + m[12 + i];
                }
                const float qa = 1.0f / a[3], qb = 1.0f / b[3];
                const float pos = f32(rdram, t + 12u) * scale;
                const int32_t index = static_cast<int32_t>(pos);
                const float step = pos - static_cast<float>(index);
                float colour[4];
                for (int i = 0; i < 4; ++i)
                    colour[i] = paletteAt(index, i) - paletteAt(index, i) * step + paletteAt(index + 1, i) * step;
                colour[3] = std::min(colour[3] * (c4[3] / 255.0f * start[3] + c5[3] / 255.0f * start[3] * qa), 255.0f);
                const float ax = a[0] * qa, ay = a[1] * qa, bx = b[0] * qb, by = b[1] * qb;
                if (std::fabs(ax + c6[0]) > guard || std::fabs(ay + c6[1]) > guard || c6[2] + a[3] < -guard ||
                    std::fabs(bx + c6[0]) > guard || std::fabs(by + c6[1]) > guard || c6[2] + b[3] < -guard || colour[3] < 1.0f)
                    continue;
                uint32_t nearColour = 0;
                for (int i = 0; i < 4; ++i)
                    nearColour |= (static_cast<uint32_t>(static_cast<int32_t>(colour[i])) & 0xffu) << (8 * i);
                const uint32_t farColour = nearColour & 0x00ffffffu;
                const float dx = bx - ax, dy = by - ay, length = std::sqrt(dx * dx + dy * dy);
                const float nx = length > 0.0f ? -dy / length * 0.5f : 0.5f, ny = length > 0.0f ? dx / length * 0.5f : 0.0f;
                Vertex corner[4];
                const float xs[4] = {ax + nx, ax - nx, bx + nx, bx - nx}, ys[4] = {ay + ny, ay - ny, by + ny, by - ny};
                for (int i = 0; i < 4; ++i)
                {
                    corner[i].x = xs[i];
                    corner[i].y = ys[i];
                    corner[i].z = (i < 2 ? a[2] * qa : b[2] * qb) * 16.0f;
                    corner[i].rgba = i < 2 ? nearColour : farColour;
                }
                pushQuad(batch, corner);
                ++g_stats.nativeSprites;
            }
            flush(batch);
        }

        // ---- Types 4 and up: the builders' constants, computed here from the struct as each builder computes them ----

        // VU1's random numbers (RINIT, RNEXT): a 23-bit shift register under the exponent of 1.0 (tmb_native.cpp's).
        uint32_t vuRandInit(float seed)
        {
            uint32_t value;
            std::memcpy(&value, &seed, 4);
            return 0x3f800000u | (value & 0x7fffffu);
        }
        uint32_t vuRandNext(uint32_t r)
        {
            const uint32_t x = (r >> 4) & 1u, y = (r >> 22) & 1u;
            return (((r << 1) ^ x ^ y) & 0x7fffffu) | 0x3f800000u;
        }
        float vuRandFloat(uint32_t r)
        {
            float value;
            std::memcpy(&value, &r, 4);
            return value;
        }

        // The builders' (float) of an unsigned count: mode - 1 is converted as unsigned (0 gives 2^32 - 1).
        float unsignedFloat(uint32_t value) { return static_cast<float>(value); }

        // The type's texture (its slot's TEX0, its texture id at +0x2c40, as the init stores it).
        void slotTexture(const uint8_t *rdram, uint32_t ps, uint64_t &tex0, uint32_t &id)
        {
            const uint32_t typeTexture = kTypeTextures + u32(rdram, ps + kTexType) * 8u;
            tex0 = u64(rdram, kSlotTex0 + u16(rdram, typeTexture + 2u) * 8u);
            id = u16(rdram, ps + kTextureIds);
        }

        // The palette the stripe and sprine inits unpack (V4-8, unsigned) at VU 264: under two colours, the system's colour
        // (0x80808080 for mode 0) and that colour with alpha 0; else the mode's entries of the colour table from the
        // system's start. The rows past them are 0 here (the sprine's init clears its 32; the stripe's VU rows past its
        // palette only ever weigh 0).
        void paletteRows(const uint8_t *rdram, uint32_t gp, uint32_t ps, int32_t rows[33][4])
        {
            std::memset(rows, 0, sizeof(int32_t) * 33 * 4);
            const uint32_t mode = u32(rdram, ps + kColourMode);
            const uint32_t table = u32(rdram, gp + static_cast<uint32_t>(kGpColours)) + u32(rdram, ps + kColourStart) * 4u;
            const auto put = [&](uint32_t row, uint32_t colour)
            {
                for (int k = 0; k < 4; ++k)
                    rows[row][k] = static_cast<int32_t>((colour >> (8 * k)) & 0xffu);
            };
            if (mode < 2u)
            {
                const uint32_t colour = mode == 0u ? 0x80808080u : u32(rdram, table);
                put(0, colour);
                put(1, colour & 0x00ffffffu);
            }
            else
                for (uint32_t i = 0; i < mode && i < 32u; ++i)
                    put(i, u32(rdram, table + 4u * i));
        }

        // Colour between two palette rows as the programs mix it (vf30's 2^23 bias: whole, truncated, low byte).
        uint32_t mixRows(const int32_t lo[4], const int32_t hi[4], float frac, const float *bias = nullptr, bool clampHigh = false)
        {
            uint32_t rgba = 0;
            for (int k = 0; k < 4; ++k)
            {
                const float l = static_cast<float>(lo[k]), h = static_cast<float>(hi[k]);
                float value = (l + 8388608.0f + (bias && k < 3 ? bias[k] : 0.0f)) - l * frac + h * frac;
                if (clampHigh)
                    value = std::min(value, 8388863.0f);
                rgba |= (static_cast<uint32_t>(static_cast<int32_t>(value - 8388608.0f)) & 0xffu) << (8 * k);
            }
            return rgba;
        }

        // A point of a type whose builder unpacks its positions at VU 8 (base: the first): VU 0-7 below that are the
        // effects constants (the programs read one past the first, a curve's end or a heat haze's last seed).
        void vuPoint(const uint8_t *rdram, uint32_t base, const ViewConstants &view, int32_t at, float out[4])
        {
            for (int k = 0; k < 4; ++k)
                out[k] = at >= 8 ? f32(rdram, base + 16u * static_cast<uint32_t>(at - 8) + 4u * k) : at >= 0 ? view.vu[at][k] : 0.0f;
        }

        // A strip as the effects image's strip builder (0xf38) kicks it: two vertices per point, a point whose corners
        // (or the previous point's) left the guard band taking ADC (no triangle ends there).
        void pushStrip(Batch &batch, const std::vector<Vertex> &strip, const std::vector<bool> &adc)
        {
            for (size_t i = 2; i < strip.size(); ++i)
            {
                if (adc[i])
                    continue;
                batch.triangles.push_back(strip[i - 2]);
                batch.triangles.push_back(strip[i - 1]);
                batch.triangles.push_back(strip[i]);
                ++g_stats.nativeSprites;
            }
        }

        float fogOf(float fog) { return std::floor(std::max(fog, 0.0f)) / 255.0f; }

        // Stripes (type 4): the texture's S scroll lives only in the packet (VU 298.x: -8 at the init, less 16 / lifetime
        // each time a new particle is born, +1 under -8), kept here by the same rule.
        struct StripeState
        {
            float scroll = -8.0f, step = 0.0f;
        };
        std::unordered_map<uint32_t, StripeState> g_stripes;

        // particlePostInitFx(position, system, ...): for a stripe it ends in stripePacketInit (VU 298.x = -8).
        void postInitOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ps = getRegU32(ctx, 5);
            gamePostInit(rdram, ctx, runtime);
            if (u32(rdram, ps + kType) != 4u)
                return;
            StripeState &state = g_stripes[ps];
            state.scroll = -8.0f;
            const double step = 16.0 / static_cast<double>(static_cast<int16_t>(u16(rdram, ps + kLifetime)));
            state.step = static_cast<float>(step);
            if (std::fabs(static_cast<double>(state.step)) > std::fabs(step)) // the EE's division rounds toward zero
                state.step = std::nextafter(state.step, 0.0f);
        }

        void stripeBuildOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ps = getRegU32(ctx, 4);
            const auto found = g_stripes.find(ps);
            if (found != g_stripes.end())
            {
                const uint32_t count = u32(rdram, ps + kCount);
                if (u16(rdram, ps + kAges + 2u * (count - 1u)) == 0u)
                {
                    // The EE's FPU rounds toward zero: so here (else the scroll drifts by an ulp, and its wrap with it).
                    const auto towardZero = [](double exact)
                    {
                        float value = static_cast<float>(exact);
                        if (std::fabs(static_cast<double>(value)) > std::fabs(exact))
                            value = std::nextafter(value, 0.0f);
                        return value;
                    };
                    float scroll = towardZero(static_cast<double>(found->second.scroll) - found->second.step);
                    if (scroll < -8.0f)
                        scroll = towardZero(static_cast<double>(scroll) + 1.0);
                    found->second.scroll = scroll;
                }
            }
            gameStripeBuild(rdram, ctx, runtime);
        }

        // Electric arcs (type 6): electricArcPacketBuild(system, onSphere, colours) copies the colours and, on a sphere,
        // the emitter, the matrix's rows and a scale into the packet; electricArcPacketUpdate(index, colours, emitter,
        // rows, scale) rewrites them there. Both kept here from their arguments, as VU 292 on, 301 and 302-306 have them.
        struct ArcState
        {
            bool sphere = false;
            uint32_t colours[32] = {};
            float params[4] = {};  // VU 301: texture scale, displacement, subdivision, width
            float emitter[3] = {}; // VU 302 (on a sphere: the translation)
            float rows[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            float scale[3] = {1, 1, 1}; // VU 306 (the rows' factors)
        };
        std::unordered_map<uint32_t, ArcState> g_arcs;

        void arcBuildOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ps = getRegU32(ctx, 4), colours = getRegU32(ctx, 6);
            ArcState state;
            state.sphere = reg64(ctx, 5) != 0u;
            const uint32_t mode = u32(rdram, ps + kColourMode);
            for (uint32_t i = 0; i < mode && i < 32u; ++i)
                state.colours[i] = u32(rdram, colours + 4u * i);
            const float scale = f32(rdram, ps + kArcScale), width = f32(rdram, ps + kWidth);
            if (state.sphere)
            {
                const float radius = f32(rdram, ps + kRadius);
                state.params[0] = radius * 0.375f * scale / width;
                state.params[2] = radius / width;
                for (uint32_t k = 0; k < 3u; ++k)
                    state.emitter[k] = f32(rdram, ps + kEmitter + 4u * k);
                const uint32_t rows = u32(rdram, ps + kRotation), factors = u32(rdram, ps + kTranslation);
                for (uint32_t j = 0; j < 3u; ++j)
                    for (uint32_t k = 0; k < 3u; ++k)
                        state.rows[j][k] = f32(rdram, rows + 16u * j + 4u * k);
                for (uint32_t k = 0; k < 3u; ++k)
                    state.scale[k] = f32(rdram, factors + 4u * k);
            }
            else
            {
                state.params[0] = scale / width;
                state.params[2] = 1.0f / width;
            }
            state.params[1] = scale;
            state.params[3] = width;
            g_arcs[ps] = state;
            gameArcBuild(rdram, ctx, runtime);
        }

        void arcUpdateOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t index = getRegU32(ctx, 4), colours = getRegU32(ctx, 5), emitter = getRegU32(ctx, 6),
                           rows = getRegU32(ctx, 7), factors = getRegU32(ctx, 8);
            const uint32_t ps = kSystems + index * kStride;
            const auto found = index - 1u < 63u && u32(rdram, ps + kType) == 6u && u32(rdram, kNodes + index * 12u) != 0u ? g_arcs.find(ps)
                                                                                                                          : g_arcs.end();
            if (found != g_arcs.end())
            {
                ArcState &state = found->second;
                const uint32_t mode = u32(rdram, ps + kColourMode);
                if (colours != 0u)
                    for (uint32_t i = 0; i < mode && i < 32u; ++i)
                        state.colours[i] = u32(rdram, colours + 4u * i);
                if (emitter != 0u)
                    for (uint32_t k = 0; k < 3u; ++k)
                        state.emitter[k] = f32(rdram, emitter + 4u * k);
                if (rows != 0u)
                    for (uint32_t j = 0; j < 3u; ++j)
                        for (uint32_t k = 0; k < 3u; ++k)
                            state.rows[j][k] = f32(rdram, rows + 16u * j + 4u * k);
                if (factors != 0u)
                    for (uint32_t k = 0; k < 3u; ++k)
                        state.scale[k] = f32(rdram, factors + 4u * k);
            }
            gameArcUpdate(rdram, ctx, runtime);
        }

        // A textured strip type's state: its slot's texture, blended by the system's ALPHA, fogged (PRIM 0x7c / 0x176:
        // FGE), alpha and depth tested, depth never written.
        Batch texturedBatch(const uint8_t *rdram, uint32_t ps, const ViewConstants &view, bool fogged, uint64_t &tex0)
        {
            uint32_t id;
            slotTexture(rdram, ps, tex0, id);
            Batch batch;
            useTexture(batch, baseMaterial(rdram, ps, fogged, view.fog), rdram, id, tex0);
            return batch;
        }

        // Type 4 (stripePacketInit / stripePacketBuild, VU1 0xcf8): runtime/tmb_native.cpp beams(). The points are the
        // positions (VU 8 on), drawn from the last (VU 7 + n) down, or from the first up when the last is the nearer
        // (the steps then run from the far end back); the system's attach matrix (VU TOP + 3). The init's constants:
        // VU 297 the steps (S 16 / (lifetime +0x2d88), palette (mode - 1) / (lifetime +0x2d88), width +0x2e24), VU 298 the
        // starts (S the scroll, palette the last particle's age (mode - 1) / lifetime (the build), width +0x2d98).
        void drawStripe(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            const int32_t count = static_cast<int32_t>(u32(rdram, ps + kCount) & 0x7fffu);
            if (count < 2)
                return;
            const uint32_t mode = u32(rdram, ps + kColourMode);
            const float lifetime = static_cast<float>(static_cast<int16_t>(u16(rdram, ps + kLifetime)));
            const float span = lifetime * f32(rdram, ps + kSpan), modeLess = unsignedFloat(mode - 1u);
            const auto stripe = g_stripes.find(ps);
            float sCoord = stripe != g_stripes.end() ? stripe->second.scroll : -8.0f;
            float index = static_cast<float>(u16(rdram, ps + kAges + 2u * static_cast<uint32_t>(count - 1))) * (modeLess / lifetime);
            float width = f32(rdram, ps + kWidth);
            float sStep = 16.0f / span, indexStep = modeLess / span, widthStep = f32(rdram, ps + kWidthStep);
            int32_t palette[33][4];
            paletteRows(rdram, gp, ps, palette);
            float m[16];
            screenMatrix(rdram, ps, view, true, m);
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float(&c7)[4] = view.vu[7];
            const float guard = std::fabs(c6[3]);
            const uint32_t positions = ps + kPositions;
            const auto project = [&](int32_t i, float out[4])
            {
                const uint32_t at = positions + 16u * static_cast<uint32_t>(i);
                const float p[3] = {f32(rdram, at), f32(rdram, at + 4u), f32(rdram, at + 8u)};
                for (int k = 0; k < 4; ++k)
                    out[k] = m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2] + m[12 + k];
            };
            float first[4], last[4];
            project(count - 1, first);
            project(0, last);
            const bool firstBehind = first[3] < 1.0f, lastBehind = last[3] < 1.0f;
            if (firstBehind && lastBehind)
                return;
            float a[4], b[4];
            std::memcpy(a, first, sizeof(a));
            std::memcpy(b, last, sizeof(b));
            if (firstBehind || lastBehind)
            {
                float *p = firstBehind ? a : b;
                const float *o = firstBehind ? b : a;
                const float t = (1.0f - o[3]) / (p[3] - o[3]);
                for (int i : {0, 1, 3})
                    p[i] = o[i] + (p[i] - o[i]) * t;
            }
            const float qa = 1.0f / a[3], qb = 1.0f / b[3];
            const float dx = a[0] * qa - b[0] * qb, dy = 2.0f * (a[1] * qa - b[1] * qb);
            const float length = std::sqrt(dx * dx + dy * dy);
            if (!(length > 0.0f))
                return;
            const float perp[2] = {c5[0] * dy / length, -c5[1] * dx / length};
            int32_t at = count - 1, direction = -1;
            if (first[3] - last[3] < 0.0f)
            {
                at = 0;
                direction = 1;
                sCoord += sStep * static_cast<float>(count - 1);
                index += indexStep * static_cast<float>(count - 1);
                width += widthStep * static_cast<float>(count - 1);
                sStep = -sStep;
                indexStep = -indexStep;
                widthStep = -widthStep;
            }
            uint64_t tex0;
            Batch batch = texturedBatch(rdram, ps, view, true, tex0);
            std::vector<Vertex> strip;
            std::vector<bool> adc;
            bool outsideBefore = false;
            for (int32_t k = 0; k < count; ++k, at += direction)
            {
                float c[4];
                project(at, c);
                const float q = 1.0f / c[3];
                const float fogA = c4[3] + c5[3] * q, fog = std::min(fogA, c7[3]);
                const int32_t i = static_cast<int32_t>(index);
                const float frac = index - static_cast<float>(i);
                static const int32_t kZero[4] = {0, 0, 0, 0};
                const int32_t *lo = i >= 0 && i < 33 ? palette[i] : kZero, *hi = i + 1 >= 0 && i + 1 < 33 ? palette[i + 1] : kZero;
                const uint32_t rgba = mixRows(lo, hi, frac);
                Vertex va, vb;
                va.x = c[0] * q - perp[0] * width * q;
                va.y = c[1] * q - perp[1] * width * q;
                vb.x = c[0] * q + perp[0] * width * q;
                vb.y = c[1] * q + perp[1] * width * q;
                va.z = vb.z = c[2] * q;
                va.u = vb.u = sCoord;
                va.v = 1.0f;
                vb.v = 0.0f;
                va.rgba = vb.rgba = rgba;
                va.fog = vb.fog = fogOf(fog);
                const bool outside = std::fabs(va.x + c6[0]) > guard || std::fabs(va.y + c6[1]) > guard || c6[2] + fogA < -guard ||
                                     std::fabs(vb.x + c6[0]) > guard || std::fabs(vb.y + c6[1]) > guard || c6[2] + c[3] < -guard;
                const bool kick = !(outside || outsideBefore);
                outsideBefore = outside;
                strip.push_back(va);
                strip.push_back(vb);
                adc.push_back(!kick);
                adc.push_back(!kick);
                sCoord += sStep;
                index += indexStep;
                width += widthStep;
            }
            pushStrip(batch, strip, adc);
            flush(batch);
        }

        // Type 6 (electricArcPacketBuild, VU1 0x1198, or 0x10e0 on a sphere): runtime/tmb_native.cpp lightning(). Bolt b
        // between the positions 2b and 2b + 1, the parameters, colours and sphere from the builder (ArcState). 0x10e0 is
        // 0x1198 with its own matrix (the view's times the rows scaled by VU 306, translated to the emitter, VU 302) and
        // its midpoints put back on the unit sphere (each over its length, 0x1570's loop); that loop draws three random
        // numbers per midpoint and runs two midpoints ahead, so it takes six more per level where 0x1198 takes two.
        void drawArc(const uint8_t *rdram, uint32_t ps, const ViewConstants &view)
        {
            const auto found = g_arcs.find(ps);
            if (found == g_arcs.end())
                return;
            const ArcState &arc = found->second;
            g_stats.sphereArcs += arc.sphere ? 1u : 0u;
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float(&c7)[4] = view.vu[7];
            const float guard = std::fabs(c6[3]);
            float m[16];
            float object[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
            if (arc.sphere)
            {
                for (int j = 0; j < 3; ++j)
                {
                    for (int k = 0; k < 3; ++k)
                        object[j][k] = arc.rows[j][k] * arc.scale[j];
                    object[j][3] = 0.0f;
                    object[3][j] = arc.emitter[j];
                }
                object[3][3] = 1.0f;
            }
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    m[j * 4 + k] = view.vu[0][k] * object[j][0] + view.vu[1][k] * object[j][1] + view.vu[2][k] * object[j][2] +
                                   view.vu[3][k] * object[j][3];
            const auto project = [&](const float p[3], float out[4])
            {
                for (int i = 0; i < 4; ++i)
                    out[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i];
            };
            int32_t palette[4][4] = {}; // VU 292-295: the colours (rows 264 + 28 on)
            for (int r = 0; r < 4; ++r)
                for (int k = 0; k < 4; ++k)
                    palette[r][k] = static_cast<int32_t>((arc.colours[r] >> (8 * k)) & 0xffu);
            const float x301 = arc.params[0], y301 = arc.params[1], z301 = arc.params[2], w301 = arc.params[3];
            const uint32_t bolts = u32(rdram, ps + kCount) & 0xffffu;
            uint64_t tex0;
            Batch batch = texturedBatch(rdram, ps, view, true, tex0);
            for (uint32_t bolt = 0; bolt < bolts; ++bolt)
            {
                float start[4], end[4];
                for (uint32_t i = 0; i < 4u; ++i)
                {
                    start[i] = f32(rdram, ps + kPositions + 32u * bolt + 4u * i);
                    end[i] = f32(rdram, ps + kPositions + 32u * bolt + 16u + 4u * i);
                }
                const float d3[3] = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};
                const float length = std::sqrt(d3[0] * d3[0] + d3[1] * d3[1] + d3[2] * d3[2]);
                float sc[4], ec[4];
                project(start, sc);
                project(end, ec);
                const bool startBehind = sc[3] < 1.0f, endBehind = ec[3] < 1.0f;
                if (startBehind && endBehind)
                    continue;
                const bool fromEnd = !(ec[3] - sc[3] < 0.0f);
                float first[4], second[4];
                std::memcpy(first, fromEnd ? ec : sc, sizeof(first));
                std::memcpy(second, fromEnd ? sc : ec, sizeof(second));
                if (startBehind || endBehind)
                {
                    const bool firstBehind = fromEnd ? endBehind : startBehind;
                    float *p = firstBehind ? first : second;
                    const float *o = firstBehind ? second : first;
                    const float t = (1.0f - o[3]) / (p[3] - o[3]);
                    for (int i : {0, 1, 3})
                        p[i] = o[i] + (p[i] - o[i]) * t;
                }
                const float qa = first[3] != 0.0f ? 1.0f / first[3] : 0.0f, qb = second[3] != 0.0f ? 1.0f / second[3] : 0.0f;
                const float dx = first[0] * qa - second[0] * qb, dy = 2.0f * (first[1] * qa - second[1] * qb);
                const float span = std::sqrt(dx * dx + dy * dy);
                if (!(span > 0.0f))
                    continue;
                const float perp[2] = {c5[0] * dy / span * w301, -c5[1] * dx / span * w301};
                std::vector<std::array<float, 3>> points;
                points.push_back(fromEnd ? std::array<float, 3>{end[0], end[1], end[2]} : std::array<float, 3>{start[0], start[1], start[2]});
                points.push_back(fromEnd ? std::array<float, 3>{start[0], start[1], start[2]} : std::array<float, 3>{end[0], end[1], end[2]});
                uint32_t random = vuRandInit(start[3]);
                float scale = y301 * length, level = std::min(z301 * length, 65.0f);
                uint32_t levels = 0;
                while (level >= 1.5f && points.size() < 513u)
                {
                    std::vector<std::array<float, 3>> next;
                    next.reserve(points.size() * 2u);
                    next.push_back(points[0]);
                    for (size_t i = 0; i + 1 < points.size(); ++i)
                    {
                        float r[3];
                        for (float &value : r)
                        {
                            random = vuRandNext(random);
                            value = vuRandFloat(random);
                        }
                        std::array<float, 3> mid;
                        for (int k = 0; k < 3; ++k)
                            mid[k] = points[i][k] * 0.5f + (r[k] - 1.5f) * scale + points[i + 1][k] * 0.5f;
                        if (arc.sphere)
                        {
                            const float norm = std::sqrt(mid[0] * mid[0] + mid[1] * mid[1] + mid[2] * mid[2]);
                            if (norm > 0.0f)
                                for (float &value : mid)
                                    value /= norm;
                        }
                        next.push_back(mid);
                        next.push_back(points[i + 1]);
                    }
                    for (int extra = arc.sphere ? 6 : 2; extra > 0; --extra)
                        random = vuRandNext(random);
                    points.swap(next);
                    scale *= 0.5f;
                    level *= 0.5f;
                    ++levels;
                }
                const float step = x301 * length / static_cast<float>(1u << levels);
                float sCoord = -0.5f * x301 * length + (start[3] - 1.5f);
                float index = fromEnd ? 29.0f : 28.0f;
                const float indexStep = (fromEnd ? -1.0f : 1.0f) / static_cast<float>(1u << levels);
                const bool flip = start[3] - 1.5f >= 0.0f;
                std::vector<Vertex> strip;
                std::vector<bool> adc;
                bool outsideBefore = false;
                for (const auto &p : points)
                {
                    float c[4];
                    project(p.data(), c);
                    const float q = 1.0f / c[3];
                    const float fogA = c4[3] + c5[3] * q - length * q, fog = std::min(fogA, c7[3]);
                    const int32_t i = static_cast<int32_t>(index) - 28;
                    const float frac = index - static_cast<float>(i + 28);
                    static const int32_t kZero[4] = {0, 0, 0, 0};
                    const uint32_t rgba = mixRows(i >= 0 && i < 4 ? palette[i] : kZero, i + 1 >= 0 && i + 1 < 4 ? palette[i + 1] : kZero, frac);
                    Vertex a, v;
                    a.x = c[0] * q - perp[0] * q;
                    a.y = c[1] * q - perp[1] * q;
                    a.z = c[2] * q - d3[2] * q;
                    v.x = c[0] * q + perp[0] * q;
                    v.y = c[1] * q + perp[1] * q;
                    v.z = c[2] * q + d3[2] * q;
                    a.u = v.u = sCoord;
                    a.v = flip ? 1.0f : 0.0f;
                    v.v = flip ? 0.0f : 1.0f;
                    a.rgba = v.rgba = rgba;
                    a.fog = v.fog = fogOf(fog);
                    const bool outside = std::fabs(a.x + c6[0]) > guard || std::fabs(a.y + c6[1]) > guard || c6[2] + fogA < -guard ||
                                         std::fabs(v.x + c6[0]) > guard || std::fabs(v.y + c6[1]) > guard || c6[2] + c[3] < -guard;
                    const bool kick = !(outside || outsideBefore);
                    outsideBefore = outside;
                    strip.push_back(a);
                    strip.push_back(v);
                    adc.push_back(!kick);
                    adc.push_back(!kick);
                    sCoord += step;
                    index += indexStep;
                }
                pushStrip(batch, strip, adc);
            }
            flush(batch);
        }

        // Type 7 (sprinePacketInit / sprinePacketBuild, VU1 0x1830): runtime/tmb_native.cpp trails(). A curve through the
        // control points (*(+0x2e60), VU 8 on) from the last down, sprites along it. The init's constants, n the sprites
        // per segment (+0x2d88 + 0.5, at least 2):
        //   TOP     x +0x2da4 (the random factor of the palette index)
        //   TOP + 1 (mode - 1) / lifetime / n, +0x2e24 / n (size step), -, the size's random spread ((+0x2d9c - +0x2d98) /
        //           +0x2d98, or 1)
        //   TOP + 2 +0x2d98 (size), n, (n + 1) / 2 (where the curve's weights switch), the sprites per batch
        //   TOP + 3, TOP + 4: the forward differences' weights for n; TOP + 5 the mirror of the first point (+0x2cc0)
        //   TOP + 6 (the build) the sprites in all, the start of t (the last point's age), both times the build's n
        //   (+0x2d88 + 0.5, not raised to 2), and the points' count - 1
        // The palette (VU 264, 32 rows) and UVs (VU 296) as the init unpacks them; the system's attach matrix (TOP + 7).
        void drawSprine(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            const uint32_t count = u32(rdram, ps + kCount);
            if (count < 2u)
                return;
            const uint32_t mode = u32(rdram, ps + kColourMode);
            const float lifetime = static_cast<float>(static_cast<int16_t>(u16(rdram, ps + kLifetime)));
            int32_t n = static_cast<int32_t>(f32(rdram, ps + kSpan) + 0.5f);
            const int32_t buildN = n;
            if (!(1 < n))
                n = 2;
            const float nf = static_cast<float>(n), inverse = 1.0f / nf, inverse2 = inverse * inverse;
            const float sizeBase = f32(rdram, ps + kWidth);
            float t1[4] = {inverse * unsignedFloat(mode - 1u) / lifetime, inverse * f32(rdram, ps + kWidthStep),
                           inverse * f32(rdram, gp + static_cast<uint32_t>(kGpSprine)), 1.0f};
            if (0.0f < sizeBase)
                t1[3] = (f32(rdram, ps + kWidthEnd) - sizeBase) / sizeBase;
            const float t3[4] = {(nf * -0.5f + 0.75f) * inverse2, inverse2 * -1.75f, (nf * 0.5f + 1.25f) * inverse2, inverse2 * -0.25f};
            const float t4[4] = {inverse2 * 1.5f, inverse2 * -3.5f, inverse2 * 2.5f, inverse2 * -0.5f};
            const float t5[3] = {f32(rdram, ps + kMirror), f32(rdram, ps + kMirror + 4u), f32(rdram, ps + kMirror + 8u)};
            const float a = f32(rdram, ps + kSpread), size0 = sizeBase;
            const uint32_t perSegment = static_cast<uint32_t>(n) & 0xffffu, switchAt = static_cast<uint32_t>((n + 1) >> 1) & 0xffffu;
            const uint32_t perBatch = static_cast<uint32_t>((361 / (5 * n)) * n) & 0xffffu;
            int32_t total = static_cast<int32_t>(static_cast<uint32_t>((static_cast<int32_t>(count) - 1) * buildN) & 0xffffu);
            float t = static_cast<float>(static_cast<int32_t>(u16(rdram, ps + kAges + 2u * (count - 1u))) * buildN);
            uint32_t at = (count - 1u) & 0x7fffu;
            int32_t palette[33][4];
            paletteRows(rdram, gp, ps, palette);
            const uint32_t typeTexture = kTypeTextures + u32(rdram, ps + kTexType) * 8u;
            uint32_t rows[4][4];
            uvRows(rdram, typeTexture, rows);
            for (int k = 0; k < 4; ++k)
                palette[32][k] = static_cast<int32_t>(rows[0][k]); // row 296 past the palette's 32
            float m[16];
            screenMatrix(rdram, ps, view, true, m);
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float(&c7)[4] = view.vu[7];
            const uint32_t base = u32(rdram, ps + kHeads);
            const auto row = [&](uint32_t vuIndex, float out[4]) { vuPoint(rdram, base, view, static_cast<int32_t>(vuIndex & 0x3ffu), out); };
            uint64_t tex0;
            Batch batch = texturedBatch(rdram, ps, view, true, tex0);
            const uint32_t width = 1u << ((tex0 >> 26) & 0xfu), height = 1u << ((tex0 >> 30) & 0xfu);
            const float h = (perSegment & 1u) ? 0.125f : 0.5f, y13 = 1.0f + 2.0f * a;
            float p5[4], p6[4], p7[4], p8[4];
            row(8u + at, p6);
            row(7u + at, p7);
            row(6u + at, p8);
            --at;
            const float d = 2.0f * (t5[0] * (p7[0] - p6[0]) + t5[1] * (p7[1] - p6[1]) + t5[2] * (p7[2] - p6[2]));
            for (int i = 0; i < 3; ++i)
                p5[i] = p7[i] - t5[i] * d;
            p5[3] = 0.0f;
            uint32_t random = vuRandNext(vuRandInit(p6[3]));
            float rnd = vuRandFloat(random);
            if (at == 0u)
                for (int i = 0; i < 3; ++i)
                    p8[i] = 2.0f * p7[i] - p6[i];
            const float guard = std::fabs(c6[3]);
            while (total > 0 && perBatch > 0u)
            {
                int32_t left = static_cast<int32_t>(std::min<uint32_t>(static_cast<uint32_t>(total), perBatch));
                float pos[3] = {p6[0], p6[1], p6[2]};
                bool firstSegment = true;
                while (true)
                {
                    float vel[3], acc[3], late[3];
                    for (int i = 0; i < 3; ++i)
                    {
                        vel[i] = p5[i] * t3[0] + p6[i] * t3[1] + p7[i] * t3[2] + p8[i] * t3[3];
                        acc[i] = p5[i] * t4[0] + p6[i] * t4[1] + p7[i] * t4[2] + p8[i] * t4[3];
                        late[i] = p5[i] * t4[3] + p6[i] * t4[2] + p7[i] * t4[1] + p8[i] * t4[0];
                    }
                    random = vuRandInit(p6[3]);
                    --at;
                    const bool switches = perSegment != 2u;
                    const uint32_t mid = switches ? switchAt : 2u * perSegment;
                    std::memcpy(p5, p6, sizeof(p5));
                    std::memcpy(p6, p7, sizeof(p6));
                    std::memcpy(p7, p8, sizeof(p7));
                    if (at != 0u)
                        row(7u + at, p8);
                    else
                        for (int i = 0; i < 3; ++i)
                            p8[i] = 2.0f * p7[i] - p6[i];
                    for (uint32_t k = perSegment; k > 0; --k)
                    {
                        const float sizeFactor = 1.0f - t1[3] + rnd * t1[3];
                        float pal = std::min(t1[0] * t * (y13 - a * rnd), 31.0f);
                        if (k == perSegment && !firstSegment)
                            pal = std::min(pal * (y13 - a * rnd), 31.0f);
                        const int32_t index = static_cast<int32_t>(pal);
                        const float step = pal - static_cast<float>(index);
                        const float size = (t1[1] * t + size0) * sizeFactor;
                        t += 1.0f;
                        random = vuRandNext(random);
                        rnd = vuRandFloat(random);
                        float c[4];
                        for (int i = 0; i < 4; ++i)
                            c[i] = m[i] * pos[0] + m[4 + i] * pos[1] + m[8 + i] * pos[2] + m[12 + i];
                        const float q = 1.0f / c[3];
                        for (int i = 0; i < 3; ++i)
                        {
                            pos[i] += vel[i];
                            vel[i] += acc[i];
                        }
                        if (switches && k - 1u == mid)
                            for (int i = 0; i < 3; ++i)
                            {
                                vel[i] = vel[i] - acc[i] * h + late[i] * h;
                                acc[i] = late[i];
                            }
                        const float x0 = c[0] * q - c5[0] * size * q, y0 = c[1] * q - c5[1] * size * q;
                        const float x1 = c[0] * q + c5[0] * size * q, y1 = c[1] * q + c5[1] * size * q;
                        const float z1 = c[2] * q + c5[2] * size * q;
                        const float fog = std::min(c4[3] + c5[3] * q, c7[3]);
                        --left;
                        --total;
                        if (std::fabs(x0 + c6[0]) > guard || std::fabs(y0 + c6[1]) > guard || c6[2] + c[3] < -guard ||
                            std::fabs(x1 + c6[0]) > guard || std::fabs(y1 + c6[1]) > guard || c6[2] + fog < -guard)
                            continue;
                        static const int32_t kZero[4] = {0, 0, 0, 0};
                        const uint32_t rgba = mixRows(index >= 0 && index < 33 ? palette[index] : kZero,
                                                      index + 1 >= 0 && index + 1 < 33 ? palette[index + 1] : kZero, step, c4, true);
                        const uint32_t *uv = rows[random & 3u];
                        const float u0 = static_cast<float>(uv[0]) / 16.0f / width, v0 = static_cast<float>(uv[1]) / 16.0f / height;
                        const float u1 = static_cast<float>(uv[2]) / 16.0f / width, v1 = static_cast<float>(uv[3]) / 16.0f / height;
                        Vertex corner[4];
                        const float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1}, us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
                        for (int i = 0; i < 4; ++i)
                        {
                            corner[i].x = xs[i];
                            corner[i].y = ys[i];
                            corner[i].z = z1;
                            corner[i].u = us[i];
                            corner[i].v = vs[i];
                            corner[i].rgba = rgba;
                            corner[i].fog = fogOf(fog);
                        }
                        pushQuad(batch, corner);
                        ++g_stats.nativeSprites;
                    }
                    std::memcpy(pos, p6, sizeof(pos));
                    firstSegment = false;
                    if (left <= 0 || total <= 0)
                        break;
                }
            }
            flush(batch);
        }

        // Type 8 (kitePacketInit / kitePacketBuild, VU1 0xa08): runtime/tmb_native.cpp heatHaze(). Each position (VU 8 on,
        // from the last down) a fan of four triangles textured with the frame drawn so far (the frame grab: sampled from
        // the target, its UVs the vertices' own pixels), the centre's UV moved by a random step, which bends what is
        // behind. The init's constants: VU 266 the centre's colour (0x82, 0x82, 0x82, 0x80), VU 269 the corners' (0x90,
        // 0x90, 0x90, 0); VU 267 y the size (+0x2d98), z -1.5, w the alpha's factor (kitePacketInit's first constant);
        // VU 268 y the frame's origin term, z the factor's step (the third constant / (lifetime +0x2d88)), w the factor's
        // start (the build: 1 - the last particle's age / lifetime). The packet's CLAMP_1 (the region u 1-638, v 1-222) is
        // left to the target's own edges, as the adapter's translation does.
        void drawKite(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            const int32_t count = static_cast<int32_t>(u32(rdram, ps + kCount) & 0x7fffu);
            if (count <= 0)
                return;
            const float lifetime = static_cast<float>(static_cast<int16_t>(u16(rdram, ps + kLifetime)));
            const float alphaFactor = f32(rdram, gp + static_cast<uint32_t>(kGpKite));
            const float originTerm = f32(rdram, gp + static_cast<uint32_t>(kGpKite) + 4u);
            const float factorStep = f32(rdram, gp + static_cast<uint32_t>(kGpKite) + 8u) / (lifetime * f32(rdram, ps + kSpan));
            float factor = 1.0f - static_cast<float>(u16(rdram, ps + kAges + 2u * static_cast<uint32_t>(count - 1))) / lifetime;
            const float size = f32(rdram, ps + kWidth);
            float m[16];
            screenMatrix(rdram, ps, view, true, m);
            const float(&c4)[4] = view.vu[4];
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float sizeX = c5[0] * size, sizeY = c5[1] * size;
            const float originX = 524288.0f - 522560.0f, originY = 524288.0f - originTerm;
            const uint32_t base = ps + kPositions;
            const auto seed = [&](int32_t vuIndex)
            {
                float p[4];
                vuPoint(rdram, base, view, vuIndex & 0x3ff, p);
                return p[3];
            };
            const auto jitter = [&](float value, float out[2])
            {
                uint32_t r = vuRandNext(vuRandInit(value));
                const float rx = vuRandFloat(r);
                r = vuRandNext(r);
                const float ry = vuRandFloat(r);
                out[0] = sizeX * -1.5f + sizeX * rx;
                out[1] = sizeY * -1.5f + sizeY * ry;
            };
            Batch batch;
            batch.material = baseMaterial(rdram, ps, false, view.fog);
            batch.material.textured = false;
            batch.material.sampleTarget = true;
            batch.open = true;
            const uint32_t centreColour = 0x80828282u & 0x00ffffffu, cornerColour = 0x00909090u;
            int32_t at = count + 7;
            float step[2];
            jitter(seed(at), step);
            const float guard = std::fabs(c6[3]);
            const auto clampU = [](float u) { return std::max(u, 0.0f); };
            const auto clampV = [](float v) { return std::max(v, 0.0f); };
            for (int32_t k = 0; k < count; ++k)
            {
                float p[4];
                vuPoint(rdram, base, view, at-- & 0x3ff, p);
                float c[4];
                for (int i = 0; i < 4; ++i)
                    c[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i];
                const float q = 1.0f / c[3];
                const float cx = c[0] * q, cy = c[1] * q, z = std::trunc(c[2] * q);
                const float fog = c4[3] + c5[3] * q;
                const float x0 = cx - sizeX * q, y0 = cy - sizeY * q, x1 = cx + sizeX * q, y1 = cy + sizeY * q;
                const float alpha = std::min(fog * alphaFactor, 128.0f) * factor;
                const float du = step[0] * q * factor, dv = step[1] * q * factor;
                factor += factorStep;
                jitter(seed(at), step);
                if (std::fabs(x0 + c6[0]) > guard || std::fabs(y0 + c6[1]) > guard || c6[2] + c[3] < -guard ||
                    std::fabs(x1 + c6[0]) > guard || std::fabs(y1 + c6[1]) > guard || c6[2] + fog < -guard)
                    continue;
                Vertex v[5];
                const float xs[5] = {cx, x0, x1, x1, x0}, ys[5] = {cy, y0, y0, y1, y1};
                for (int i = 0; i < 5; ++i)
                {
                    v[i].x = xs[i];
                    v[i].y = ys[i];
                    v[i].z = z;
                    v[i].u = clampU(xs[i] - originX);
                    v[i].v = clampV(ys[i] - originY);
                    v[i].rgba = cornerColour;
                }
                v[0].u = clampU(cx - originX + du);
                v[0].v = clampV(cy - originY + dv);
                v[0].rgba = centreColour | (std::min(static_cast<uint32_t>(std::max(alpha, 0.0f)), 255u) << 24);
                const int fan[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1};
                for (int i : fan)
                    batch.triangles.push_back(v[i]);
                ++g_stats.nativeSprites;
            }
            flush(batch);
        }

        // Type 9 (particlePrecipitationCreate, VU1 0x1d40 rain / 0x1f30 snow). Its VU data (VU 8-21) is the game's rain
        // state: VU 8-11 the second matrix, 12 the head offset (w: the frame counter's low 6 bits / 64 + 1), 13 the box
        // around the camera, 14 the tail vector, 16-19 the texture matrix, each where the game's pointer to it says
        // (particleUpdateRainViewport and particleUpdateRain move them); VU 20.w the drops (its own word, the count
        // particleUpdateRainViewport sets); VU 15 and VU 21 the constants the create puts there (the recurrence's
        // factors, the tile 80, the seed and the guard band 480). Both programs share the setup (0x1c38): A = the view's
        // matrix (VU 0-3) moved to the head offset, B = VU 8-11 moved to the tail, both with their z rows over 16; the
        // drops' positions follow runtime/tmb_native.cpp rain()'s recurrence (the same in both loops).
        struct Precip
        {
            float a[4][4], b[4][4], c[4][4]; // A and B as the setup makes them; C the texture matrix (VU 16-19)
            float box[3], step[4], mul[4], head[4];
            uint32_t drops;
        };

        void precipState(const uint8_t *rdram, uint32_t gp, const ViewConstants &view, Precip &out)
        {
            const auto vec = [&](int32_t gpOffset, uint32_t index, float v[4])
            {
                const uint32_t at = u32(rdram, gp + static_cast<uint32_t>(gpOffset)) + 16u * index;
                for (uint32_t k = 0; k < 4u; ++k)
                    v[k] = f32(rdram, at + 4u * k);
            };
            float head[4], tail[4], b[4][4];
            vec(kGpRainHead, 0, head);
            vec(kGpRainTail, 0, tail);
            for (uint32_t j = 0; j < 4u; ++j)
            {
                vec(kGpRainMatrix, j, b[j]);
                vec(kGpRainTexMatrix, j, out.c[j]);
            }
            float box[4];
            vec(kGpRainBox, 0, box);
            std::memcpy(out.box, box, sizeof(out.box));
            std::memcpy(out.head, head, sizeof(out.head));
            for (uint32_t k = 0; k < 3u; ++k)
            {
                out.step[k] = f32(rdram, gp + static_cast<uint32_t>(kGpRainStep) + 4u * k);
                tail[k] = head[k] - tail[k];
            }
            out.step[3] = 80.0f;
            out.mul[0] = f32(rdram, gp + static_cast<uint32_t>(kGpRainMul));
            out.mul[1] = f32(rdram, gp + static_cast<uint32_t>(kGpRainMul) + 4u);
            out.mul[2] = 1.0f;
            out.mul[3] = 480.0f;
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                {
                    out.a[j][k] = view.vu[j][k];
                    out.b[j][k] = b[j][k];
                }
            for (int k = 0; k < 4; ++k)
            {
                out.a[3][k] = view.vu[0][k] * head[0] + view.vu[1][k] * head[1] + view.vu[2][k] * head[2] + view.vu[3][k];
                out.b[3][k] = b[0][k] * tail[0] + b[1][k] * tail[1] + b[2][k] * tail[2] + b[3][k];
            }
            for (int j = 0; j < 4; ++j)
            {
                out.a[j][2] *= 0.0625f;
                out.b[j][2] *= 0.0625f;
            }
            out.drops = std::min(u32(rdram, u32(rdram, gp + static_cast<uint32_t>(kGpPrecipCount))) & 0xffffu, 4096u);
        }

        // The drops' positions in the box (rain()'s recurrence): p0 = VU 21, then the wrapped steps.
        struct DropWalk
        {
            float p[3], next[3], u[3], c[3];
            const Precip &s;
            explicit DropWalk(const Precip &state) : s(state)
            {
                for (int k = 0; k < 3; ++k)
                {
                    c[k] = s.step[k] * s.step[3];
                    p[k] = s.mul[k];
                    next[k] = c[k] < s.box[k] ? c[k] + s.step[3] : c[k];
                    u[k] = (s.step[k] + s.mul[k]) * s.step[3];
                }
            }
            void advance()
            {
                for (int k = 0; k < 3; ++k)
                {
                    p[k] = next[k];
                    const float wrapped = u[k] > s.step[3] ? u[k] - s.step[3] : u[k];
                    next[k] = wrapped < s.box[k] ? wrapped + s.step[3] : wrapped;
                    u[k] = wrapped * s.mul[k] + c[k];
                }
            }
        };

        void transform4(const float m[4][4], const float p[3], float out[4])
        {
            for (int k = 0; k < 4; ++k)
                out[k] = m[0][k] * p[0] + m[1][k] * p[1] + m[2][k] * p[2] + m[3][k];
        }

        // Rain (0x1d40): runtime/tmb_native.cpp rain(). Each drop a line from A p (head) to B p (tail); the head's alpha
        // 255 - 3.1875 w (the tail's w), the tail's 1; texture coordinates C p (ST, one for both ends). Dropped when the
        // alpha is under 1, an end leaves the guard band (VU 21.w, offset by VU 6) or lies behind (z + VU 6.z under it).
        // Z: XYZ2 takes FTOI4 of z / 16 / w, so GS Z is z / w.
        void drawRain(Batch &batch, const Precip &s, const ViewConstants &view)
        {
            const float(&c6)[4] = view.vu[6];
            const float guard = s.mul[3];
            DropWalk walk(s);
            for (uint32_t n = 0; n < s.drops; ++n, walk.advance())
            {
                float h[4], t[4], st[4];
                transform4(s.a, walk.p, h);
                transform4(s.b, walk.p, t);
                transform4(s.c, walk.p, st);
                if (h[3] <= 0.0f || t[3] <= 0.0f)
                    continue;
                const float alpha = std::trunc(255.0f - 3.1875f * t[3]);
                const float hx = h[0] / h[3], hy = h[1] / h[3], hz = h[2] / h[3];
                const float tx = t[0] / t[3], ty = t[1] / t[3], tz = t[2] / t[3];
                if (alpha < 1.0f || std::fabs(hx + c6[0]) > guard || std::fabs(hy + c6[1]) > guard || hz + c6[2] < -guard ||
                    std::fabs(tx + c6[0]) > guard || std::fabs(ty + c6[1]) > guard || tz + c6[2] < -guard)
                    continue;
                const float dx = tx - hx, dy = ty - hy, length = std::sqrt(dx * dx + dy * dy);
                if (length <= 0.0f)
                    continue;
                const float nx = -dy / length * 0.5f, ny = dx / length * 0.5f;
                const float corner[4][3] = {{hx + nx, hy + ny, hz}, {hx - nx, hy - ny, hz}, {tx + nx, ty + ny, tz}, {tx - nx, ty - ny, tz}};
                const float w = std::max(st[3], 0.0f), q = st[2] != 0.0f ? st[2] : 1.0f;
                Vertex quad[4];
                for (int i = 0; i < 4; ++i)
                {
                    quad[i].x = corner[i][0];
                    quad[i].y = corner[i][1];
                    quad[i].z = corner[i][2] * 16.0f;
                    quad[i].u = w > 0.0f ? st[0] / w / q : 0.0f;
                    quad[i].v = w > 0.0f ? st[1] / w / q : 0.0f;
                    quad[i].rgba = 0x00808080u | ((i < 2 ? static_cast<uint32_t>(alpha) : 1u) << 24);
                }
                pushQuad(batch, quad);
                ++g_stats.nativeSprites;
            }
        }

        // Snow (0x1f30, its loop at 0x2000; translated here from its microcode). Each flake a triangle textured over
        // (-0.366, 0), (1.366, 0), (0.5, 1.5) (an equilateral one around the flake's texture), stretched along its motion
        // on screen: with H = A p / w and T = B p / w (head and tail on screen), d = T - H, u = d / |d|, its side
        // (-u.y, u.x) and k = VU 5.xy / 8 (times 1 / w of the head, k' of the tail):
        //   H - k (u + sqrt3 side),  H - k (u - sqrt3 side),  T + 2 k' u      (Z: the head's, the head's, the tail's)
        // (d = 0: H + (-sqrt3 k, k), H + (sqrt3 k, k), T + (0, 2 k')). Colour 128 grey, alpha min(255 - 3.1875 w, 128)
        // (the head's w, at least 0) times (9000 / w) / (9000 / w + |d|^2): the faster on screen, the fainter. Each flake
        // sways: its homogeneous x (head and tail) plus 160 sin(a), a from the flake two before (the sine of one flake is
        // read in the next one's step, which moves the flake after it): a = (|f - 1/2| - 1/4) 2 pi, f = frac(W - VU 12.w),
        // W the flake's random number (VU1's, seeded from VU 21.x; the first flake's W is VU 15.z, the second's sway
        // sin(1) from the prologue). Dropped when a corner leaves the guard band (as the rain).
        void drawSnow(Batch &batch, const Precip &s, const ViewConstants &view)
        {
            const float(&c5)[4] = view.vu[5];
            const float(&c6)[4] = view.vu[6];
            const float guard = s.mul[3];
            const float kx = c5[0] * 0.125f, ky = c5[1] * 0.125f, root3 = 1.73205f;
            DropWalk walk(s);
            uint32_t random = vuRandNext(vuRandInit(s.mul[0])); // RINIT vf19x; the prologue's RNEXT
            float weight = s.step[2];                           // the first flake's W: VU 15.z
            float sway = 0.0f, swayNext = 160.0f * std::sin(1.0f);
            for (uint32_t n = 0; n < s.drops; ++n, walk.advance())
            {
                float h[4], t[4];
                transform4(s.a, walk.p, h);
                transform4(s.b, walk.p, t);
                h[0] += sway;
                t[0] += sway;
                float f = weight - s.head[3];
                const bool negative = f < 0.0f;
                f -= 0.5f;
                if (negative)
                    f += 1.0f;
                const float angle = (std::fabs(f) - 0.25f) * 6.28319f;
                sway = swayNext;
                swayNext = 160.0f * std::sin(angle);
                random = vuRandNext(random);
                weight = vuRandFloat(random);
                if (h[3] == 0.0f || t[3] == 0.0f)
                    continue;
                const float qa = 1.0f / h[3], qb = 1.0f / t[3];
                const float hx = h[0] * qa, hy = h[1] * qa, hz = h[2] * qa, tx = t[0] * qb, ty = t[1] * qb, tz = t[2] * qb;
                const float dx = tx - hx, dy = ty - hy, d2 = dx * dx + dy * dy;
                float alpha = std::max(std::min(255.0f - h[3] * 3.1875f, 128.0f), 0.0f);
                float corner[3][2];
                if (d2 != 0.0f)
                {
                    const float inverse = 1.0f / std::sqrt(d2), ux = dx * inverse, uy = dy * inverse;
                    const float ax = kx * qa, ay = ky * qa;
                    corner[0][0] = hx - ax * ux + root3 * ax * uy;
                    corner[0][1] = hy - ay * uy - root3 * ay * ux;
                    corner[1][0] = hx - ax * ux - root3 * ax * uy;
                    corner[1][1] = hy - ay * uy + root3 * ay * ux;
                    corner[2][0] = tx + 2.0f * kx * qb * ux;
                    corner[2][1] = ty + 2.0f * ky * qb * uy;
                    const float distance = 9000.0f * qa;
                    alpha *= distance / (distance + d2);
                }
                else
                {
                    corner[0][0] = hx - root3 * kx * qa;
                    corner[0][1] = hy + ky * qa;
                    corner[1][0] = hx + root3 * kx * qa;
                    corner[1][1] = hy + ky * qa;
                    corner[2][0] = tx;
                    corner[2][1] = ty + 2.0f * ky * qb;
                }
                const float zs[3] = {hz, hz, tz};
                bool outside = false;
                for (int i = 0; i < 3; ++i)
                    outside = outside || std::fabs(corner[i][0] + c6[0]) > guard || std::fabs(corner[i][1] + c6[1]) > guard ||
                              zs[i] + c6[2] < -guard;
                if (outside)
                    continue;
                const uint32_t rgba = 0x00808080u | (static_cast<uint32_t>(static_cast<int32_t>(alpha)) & 0xffu) << 24;
                const float us[3] = {-0.366025f, 1.36603f, 0.5f}, vs[3] = {0.0f, 0.0f, 1.5f};
                for (int i = 0; i < 3; ++i)
                {
                    Vertex v;
                    v.x = corner[i][0];
                    v.y = corner[i][1];
                    v.z = zs[i] * 16.0f;
                    v.u = us[i];
                    v.v = vs[i];
                    v.rgba = rgba;
                    batch.triangles.push_back(v);
                }
                ++g_stats.nativeSprites;
            }
        }

        // The precipitation's state: its slot's texture with CLAMP_1 5 (clamped both ways), blended by the system's
        // ALPHA, unfogged (PRIM 0x59 / 0x5b), alpha and depth tested, depth never written.
        void drawPrecip(const uint8_t *rdram, uint32_t gp, uint32_t ps, const ViewConstants &view)
        {
            if (static_cast<int32_t>(u32(rdram, gp + static_cast<uint32_t>(kGpPrecipIndex))) < 0)
                return;
            Precip state;
            precipState(rdram, gp, view, state);
            Material base = baseMaterial(rdram, ps, false, view.fog);
            base.clampU = base.clampV = true;
            uint64_t tex0;
            uint32_t id;
            slotTexture(rdram, ps, tex0, id);
            Batch batch;
            useTexture(batch, base, rdram, id, tex0);
            const bool rain = u32(rdram, gp + static_cast<uint32_t>(kGpRainFlag)) != 0u;
            const size_t before = batch.triangles.size();
            if (rain)
                drawRain(batch, state, view);
            else
                drawSnow(batch, state, view);
            static uint32_t s_calls = 0;
            if (settings().log && s_calls++ % 300u == 0u)
                std::fprintf(stderr, "[tmb:particles] %s: %u drops, %zu vertices, tex0 %#llx, alpha %#x, head (%.1f %.1f %.1f %.3f) box (%.1f %.1f %.1f) "
                                     "A.w row (%.3f %.3f %.3f %.3f) B row3 (%.1f %.1f %.1f %.1f)\n",
                             rain ? "rain" : "snow", state.drops, batch.triangles.size() - before, static_cast<unsigned long long>(tex0),
                             u32(rdram, ps + kAlpha), state.head[0], state.head[1], state.head[2], state.head[3], state.box[0], state.box[1],
                             state.box[2], state.a[0][3], state.a[1][3], state.a[2][3], state.a[3][3], state.b[3][0], state.b[3][1],
                             state.b[3][2], state.b[3][3]);
            flush(batch);
        }

        void precipCreateOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            setReg(ctx, 4, static_cast<uint32_t>(settings().precip));
            gamePrecipCreate(rdram, ctx, runtime);
        }

        bool drawNatively(const uint8_t *rdram, uint32_t gp, uint32_t ps, uint32_t type, const ViewConstants &view)
        {
            switch (type)
            {
            case 1: drawLines(rdram, gp, ps, view); return true;
            case 2: drawSprites(rdram, gp, ps, view); return true;
            case 3: drawTexSprites(rdram, gp, ps, view); return true;
            case 4: drawStripe(rdram, gp, ps, view); return true;
            case 6:
                if (g_arcs.find(ps) == g_arcs.end())
                    return false; // built before the hooks (never, from boot): the game's packet
                drawArc(rdram, ps, view);
                return true;
            case 7: drawSprine(rdram, gp, ps, view); return true;
            case 8: drawKite(rdram, gp, ps, view); return true;
            case 9: drawPrecip(rdram, gp, ps, view); return true;
            default: return false;
            }
        }

        // particleAddLensFlareToList(system, view): the system's light as a lens flare element of the view's parity
        // (48 bytes: position, 0, the depth term with 3 in its low byte, colour from the flags' low three bytes times the
        // game's scales, the light's size squared / 255), at most 0x4a.
        void addLensFlare(uint8_t *rdram, uint32_t gp, uint32_t ps, uint32_t view)
        {
            const uint32_t parity = view & 1u;
            const uint32_t countAt = gp + static_cast<uint32_t>(kGpLensCounts) + parity * 4u;
            const int32_t count = static_cast<int32_t>(u32(rdram, countAt));
            if (count >= 0x4a)
                return;
            const uint32_t element = u32(rdram, gp + static_cast<uint32_t>(kGpLensLists) + parity * 4u) + static_cast<uint32_t>(count) * 0x30u;
            for (uint32_t k = 0; k < 3u; ++k)
                put32(rdram, element + 4u * k, u32(rdram, ps + kEmitter + 4u * k));
            const uint32_t flags = u32(rdram, ps + kFlags);
            put32(rdram, element + 0xcu, 0u);
            putF(rdram, element + 0x10u, f32(rdram, ps + kDepthTerm));
            for (uint32_t k = 0; k < 3u; ++k)
                putF(rdram, element + 0x20u + 4u * k,
                     static_cast<float>((flags >> (8u * k)) & 0xffu) * f32(rdram, gp + static_cast<uint32_t>(kGpLensScale) + 4u * k));
            put8(rdram, element + 0x10u, 3u);
            const float light = f32(rdram, ps + kLight);
            putF(rdram, element + 0x2cu, light * light / 255.0f);
            put32(rdram, countAt, static_cast<uint32_t>(count + 1));
        }

        // ---- The lens flares (VU1 0x2338), drawn here from the flare list ----
        //
        // The list (gp-0x2f20 per view parity, gp-0x2f28 its count; 48 bytes per light, filled by addLensFlare and the
        // hierarchy's lights) is the game's; particleDraw's tail keeps calling the two functions below, which are
        // replaced while the particles are drawn here (g_flaresNative): the game's arithmetic on the list, the same
        // writes to it, its random numbers drawn, and in place of the GS Z readback the native frame's depth, and in
        // place of lensFlarePacketBuild, the texture uploads and the packet's CALL, the flares drawn now, as
        // runtime/tmb_native.cpp lensFlares() draws them from VU memory (checked there against VU1).

        bool g_flaresNative = false; // inside particleDrawOverride's own particleDraw, with the flares drawn here

        // fptoui (the game's soft float to unsigned): truncated, 0 under 1 (and for NaN), saturated.
        uint32_t fptoui(float value)
        {
            if (!(value >= 1.0f))
                return 0u;
            return value >= 4294967296.0f ? 0xffffffffu : static_cast<uint32_t>(value);
        }

        uint64_t nextSeed(uint64_t seed) { return seed * 0x41c64e6dull + 0x3039u; }

        // The game's float arithmetic for the flares' (the checks compare them bit for bit): the FPU's (add.s ...
        // truncated, a zero divisor giving the largest float) and VU0's (macro mode: the host's rounding), each
        // operation on its own (no fused multiply-add).
        float fpuAdd(float a, float b) { return ps2_fadd(a, b); }
        float fpuSub(float a, float b) { return ps2_fsub(a, b); }
        float fpuMul(float a, float b) { return ps2_fmul(a, b); }
        float fpuDiv(float a, float b) { return ps2_fdiv(a, b); }
        float vuAdd(float a, float b) { return _mm_cvtss_f32(ps2_vadd(_mm_set_ss(a), _mm_set_ss(b))); }
        float vuMul(float a, float b) { return _mm_cvtss_f32(ps2_vmul(_mm_set_ss(a), _mm_set_ss(b))); }

        // particleLensFlareTestZbuffer(view): each light through mathfTransform3dTo2d (its position plus the view's
        // viewGetWeTrans, times viewGetWorldToScreenMat's rows, over w); the ones inside the view (viewGetCenter -+
        // viewGetWH / 2) and not behind the camera (w >= 1) kept in place at the list's front (screen x, y, z / w, then
        // w, or 100 when the light's flags have bit 7), the others dropped from the count; two random numbers per light,
        // a third per kept one (the game's LCG, nothing else of them used). The 4 x 4 Z samples the game reads back from
        // (x - 2, y) of the field (its StoreImages' TRXPOS: x - 0x6c2, y - 0x790) go in the samples' buffer, from the
        // native frame's depth. Returns the count left (particleDraw adds it to its kick count).
        int32_t lensFlareTest(uint8_t *rdram, uint32_t gp, uint32_t view)
        {
            const uint32_t parity = view & 1u;
            const uint32_t countAt = gp + static_cast<uint32_t>(kGpLensCounts) + parity * 4u;
            const uint32_t list = u32(rdram, gp + static_cast<uint32_t>(kGpLensLists) + parity * 4u);
            const uint32_t viewport = u32(rdram, kViews + view * 16u + 4u);
            const int32_t cx = static_cast<int32_t>(u32(rdram, viewport + 0x18u)), cy = static_cast<int32_t>(u32(rdram, viewport + 0x1cu));
            const int32_t halfW = static_cast<int32_t>(u16(rdram, viewport + 4u)) >> 1, halfH = static_cast<int32_t>(u16(rdram, viewport + 6u)) >> 1;
            const float xMin = static_cast<float>(cx - halfW), xMax = static_cast<float>(cx + halfW);
            const float yMin = static_cast<float>(cy - halfH), yMax = static_cast<float>(cy + halfH);
            const uint32_t weTrans = kWeTrans + view * 0xf0u, matrix = kWorldToScreen + view * 64u;
            float m[4][4];
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    m[j][k] = f32(rdram, matrix + 16u * j + 4u * k);
            int32_t count = static_cast<int32_t>(u32(rdram, countAt));
            const int32_t lights = count;
            uint64_t seed = u64(rdram, gp + static_cast<uint32_t>(kGpSeed));
            uint32_t out = list, kept = 0;
            for (int32_t i = 0; i < lights; ++i)
            {
                const uint32_t in = list + static_cast<uint32_t>(i) * 0x30u;
                seed = nextSeed(nextSeed(seed));
                float p[3], t[4][3];
                for (uint32_t k = 0; k < 3u; ++k)
                    p[k] = fpuAdd(f32(rdram, in + 4u * k), f32(rdram, weTrans + 4u * k));
                for (int j = 0; j < 4; ++j)
                    for (int k = 0; k < 3; ++k)
                        t[j][k] = vuMul(m[j][k], p[k]);
                // mathfMulVec4x4: row j's xyz . p + its w, summed in its VU0 order.
                const float r[4] = {vuAdd(t[0][0], vuAdd(t[0][1], vuAdd(t[0][2], m[0][3]))),
                                    vuAdd(t[1][1], vuAdd(t[1][2], vuAdd(m[1][3], t[1][0]))),
                                    vuAdd(t[2][2], vuAdd(m[2][3], vuAdd(t[2][0], t[2][1]))),
                                    vuAdd(m[3][3], vuAdd(t[3][0], vuAdd(t[3][1], t[3][2])))};
                const float q = fpuDiv(1.0f, r[3]);
                const float x = fpuMul(r[0], q), y = fpuMul(r[1], q), z = fpuMul(r[2], q), w = r[3];
                if (!(xMin < x) || !(x < xMax) || !(yMin < y) || !(y < yMax) || !(1.0f <= w))
                {
                    put32(rdram, countAt, static_cast<uint32_t>(--count));
                    continue;
                }
                const bool fixed = (u8(rdram, in + 0x10u) >> 7) != 0u;
                putF(rdram, out, x);
                putF(rdram, out + 4u, y);
                putF(rdram, out + 8u, z);
                putF(rdram, out + 0x14u, fixed ? 100.0f : w);
                seed = nextSeed(seed);
                if (out != in)
                {
                    put32(rdram, out + 0xcu, u32(rdram, in + 0xcu));
                    put32(rdram, out + 0x10u, u32(rdram, in + 0x10u));
                    for (uint32_t at = 0x18u; at < 0x30u; at += 4u)
                        put32(rdram, out + at, u32(rdram, in + at));
                }
                uint32_t samples[16];
                tmb::native::flareDepthBlock((fptoui(x) - 0x6c2u) & 0x7ffu, (fptoui(y) - 0x790u) & 0x7ffu, samples);
                for (uint32_t k = 0; k < 16u; ++k)
                    put32(rdram, kLensSamples + kept * 64u + 4u * k, samples[k]);
                ++kept;
                out += 0x30u;
            }
            put64(rdram, gp + static_cast<uint32_t>(kGpSeed), seed);
            return count;
        }

        // particleLensFlareRender's loop over the kept lights: each one's visibility from its Z samples (a sample
        // shows the light when its Z is not above the light's): with bit 7 of its flags, all 16 against the light's
        // z / w times 1.03125 (plus 1), else the middle 2 x 2 against the view's Z of w less the light's depth term
        // (at least 1.5; viewGetZBuffParams: b + a w' over w'); its visibility (+0x2c) times the fraction that shows, its
        // screen point moved to their centroid; averaged with the frame before through the debounce table when it has
        // a light id (+0xc); hidden (flags' low byte 0) under 1 / 256.
        void lensFlareVisibility(uint8_t *rdram, uint32_t list, int32_t count, uint32_t view)
        {
            const float zA = f32(rdram, kViewTable2 + view * 64u + 0x28u), zB = f32(rdram, kViewTable2 + view * 64u + 0x2cu);
            for (int32_t j = 0; j < count; ++j)
            {
                const uint32_t element = list + static_cast<uint32_t>(j) * 0x30u, samples = kLensSamples + static_cast<uint32_t>(j) * 64u;
                const bool all = (u8(rdram, element + 0x10u) >> 7) != 0u;
                uint32_t limit;
                uint32_t first, last;
                if (all)
                {
                    limit = fptoui(fpuMul(f32(rdram, element + 8u), 1.03125f)) + 1u;
                    first = 0u;
                    last = 4u;
                }
                else
                {
                    const float term = f32(rdram, element + 0x10u), w = f32(rdram, element + 0x14u);
                    const float depth = 1.5f < term ? fpuSub(w, term) : fpuSub(w, 1.5f);
                    limit = fptoui(fpuDiv(fpuAdd(zB, fpuMul(zA, depth)), depth));
                    first = 1u;
                    last = 3u;
                }
                float shown = 0.0f;
                uint32_t sumX = 0, sumY = 0;
                for (uint32_t row = first; row < last; ++row)
                    for (uint32_t column = first; column < last; ++column)
                        if (!(limit < (u32(rdram, samples + (row * 4u + column) * 4u) & 0xffffffu)))
                        {
                            shown = fpuAdd(shown, 1.0f);
                            sumX += column;
                            sumY += row;
                        }
                const float share = all ? 0.0625f : 0.25f, offset = all ? 2.0f : 1.0f;
                float visibility = fpuMul(f32(rdram, element + 0x2cu), fpuMul(shown, share));
                putF(rdram, element + 0x2cu, visibility);
                putF(rdram, element, fpuAdd(f32(rdram, element), fpuSub(fpuDiv(static_cast<float>(sumX), shown), offset)));
                putF(rdram, element + 4u, fpuAdd(f32(rdram, element + 4u), fpuSub(fpuDiv(static_cast<float>(sumY), shown), offset)));
                const int32_t id = static_cast<int32_t>(u32(rdram, element + 0xcu));
                if (id != 0 && id < 0x7ff)
                {
                    const uint32_t entry = kLensDebounce + view * 8u + static_cast<uint32_t>(id) * 32u;
                    const float before = visibility;
                    visibility = fpuMul(fpuAdd(f32(rdram, entry + 4u), before), 0.5f);
                    putF(rdram, element + 0x2cu, visibility);
                    putF(rdram, entry + 4u, before);
                }
                if (static_cast<double>(visibility) < 1.0 / 256.0)
                    put8(rdram, element + 0x10u, 0u);
            }
        }

        // The flares' 18 groups, from lensFlarePacketInit's tables: the texture (its type's slot: TEX0 and id), the step
        // along the line from the view's centre to the light, the size (its float's low two bits replaced by the UV
        // extent's index: 31.5, 63.5 or 127.5 texels, the size of the packet's output buffers' w), the colour and the
        // size factor.
        struct FlareGroup
        {
            uint64_t tex0 = 0;
            uint32_t id = 0;
            float step = 0.0f, size = 0.0f, extent = 0.0f;
            float colour[4] = {};
        };

        FlareGroup flareGroup(const uint8_t *rdram, uint32_t g)
        {
            FlareGroup group;
            const uint32_t typeTexture = kTypeTextures + u8(rdram, kFlareTypes + g) * 8u;
            const uint32_t slot = u16(rdram, typeTexture + 2u);
            group.tex0 = u64(rdram, kSlotTex0 + slot * 8u);
            group.id = u16(rdram, kSlotIds + slot * 2u);
            group.step = f32(rdram, kFlareSteps + 4u * g);
            const uint32_t sizeBits = (u32(rdram, kFlareSizes + 4u * g) & ~3u) | u8(rdram, kFlareUvs + g);
            std::memcpy(&group.size, &sizeBits, 4);
            const float extents[4] = {31.5f, 63.5f, 127.5f, 0.0f};
            group.extent = extents[sizeBits & 3u];
            for (uint32_t k = 0; k < 3u; ++k)
                group.colour[k] = static_cast<float>(u8(rdram, kFlareColours + 0x12u * k + g));
            group.colour[3] = f32(rdram, kFlareWeights + 4u * g);
            return group;
        }

        // The program's sprites (runtime/tmb_native.cpp lensFlares, its inputs from the list: element j's screen point at
        // +0, its flags at +0x10 and three terms at +0x14, its colour at +0x20 and visibility at +0x2c; the view's centre
        // from viewGetCenter). Each light's scale and visibility, then per group one sprite per drawn light on the line
        // from the centre through it: the first group sized by the light's scale (alpha 192), the others at their own
        // size (alpha 128, colours times the visibility, at most 32); a light whose colour comes out under 1 left out of
        // the groups after. Added (ALPHA 0x48), not fogged, Z the farthest over everything (no depth written).
        void drawLensFlares(const uint8_t *rdram, uint32_t list, int32_t count, uint32_t view)
        {
            if (count <= 0)
                return;
            const uint32_t viewport = u32(rdram, kViews + view * 16u + 4u);
            const float centre[2] = {static_cast<float>(static_cast<int32_t>(u32(rdram, viewport + 0x18u))),
                                     static_cast<float>(static_cast<int32_t>(u32(rdram, viewport + 0x1cu)))};
            const uint32_t items = static_cast<uint32_t>(count);
            std::vector<uint8_t> drawn(items);
            std::vector<float> scale(items), seen(items);
            for (uint32_t j = 0; j < items; ++j)
            {
                const uint32_t element = list + j * 0x30u;
                const uint32_t flags = u32(rdram, element + 0x10u);
                const float a = f32(rdram, element + 0x14u), b = f32(rdram, element + 0x18u), c = f32(rdram, element + 0x1cu);
                float visibility = f32(rdram, element + 0x2cu), size = 0.0f;
                bool show = (flags & 1u) != 0u;
                if (show)
                {
                    const float root = std::sqrt(std::fabs(visibility));
                    const float z = ((flags & 16u) ? std::fabs(b) : std::max(b, 0.0f)) + (c - 1.0f);
                    visibility = visibility / a / a;
                    size = root / a;
                    if (flags & 8u)
                    {
                        size = size * z / c;
                        const float scaled = visibility * z;
                        visibility = scaled / c;
                        show = scaled > 0.0f;
                    }
                }
                drawn[j] = show ? 1u : 0u;
                scale[j] = size;
                seen[j] = visibility;
                ++g_stats.flareLights;
            }
            Material base;
            base.texa = kTexa;
            base.blend = blendFor(0x48u);
            const uint32_t atst = (kTest >> 1) & 7u, aref = (kTest >> 4) & 0xffu;
            base.alphaTest = atst == 6u || aref > 0u;
            base.alphaRef = static_cast<uint8_t>(atst == 6u ? aref : aref - 1u);
            base.depthTest = true;
            base.depthWrite = false;
            base.fog = false;
            Batch batch;
            for (uint32_t g = 0; g < 18u; ++g)
            {
                const FlareGroup group = flareGroup(rdram, g);
                useTexture(batch, base, rdram, group.id, group.tex0);
                const uint32_t width = 1u << ((group.tex0 >> 26) & 0xfu), height = 1u << ((group.tex0 >> 30) & 0xfu);
                const float sizeX = group.size, sizeY = group.size * 0.5f;
                for (uint32_t j = 0; j < items; ++j)
                {
                    if (!drawn[j])
                        continue;
                    const uint32_t element = list + j * 0x30u;
                    const float w = scale[j] * group.colour[3], weight = group.colour[3] * seen[j];
                    float rgb[3];
                    for (uint32_t i = 0; i < 3u; ++i)
                        rgb[i] = group.colour[i] * f32(rdram, element + 0x20u + 4u * i) * (g == 0u ? 1.0f : weight);
                    const float x = centre[0] + (f32(rdram, element) - centre[0]) * group.step;
                    const float y = centre[1] + (f32(rdram, element + 4u) - centre[1]) * group.step;
                    const float dx = g == 0u ? sizeX * w : sizeX, dy = g == 0u ? sizeY * w : sizeY;
                    const float limit = g == 0u ? 255.0f : 32.0f;
                    uint32_t rgba = (g == 0u ? 192u : 128u) << 24;
                    for (uint32_t i = 0; i < 3u; ++i)
                    {
                        const float value = std::min(rgb[i], limit);
                        rgba |= (value > 0.0f ? (value >= 255.0f ? 255u : static_cast<uint32_t>(value)) : 0u) << (8u * i);
                    }
                    if (rgb[0] < 1.0f && rgb[1] < 1.0f && rgb[2] < 1.0f)
                        drawn[j] = 0u; // left out of the next groups
                    const float xs[4] = {x + dx, x - dx, x + dx, x - dx}, ys[4] = {y + dy, y + dy, y - dy, y - dy};
                    const float us[4] = {0.5f, group.extent, 0.5f, group.extent}, vs[4] = {0.5f, 0.5f, group.extent, group.extent};
                    Vertex corner[4];
                    for (int i = 0; i < 4; ++i)
                    {
                        corner[i].x = xs[i];
                        corner[i].y = ys[i];
                        corner[i].z = 16777215.0f;
                        corner[i].u = us[i] / static_cast<float>(width);
                        corner[i].v = vs[i] / static_cast<float>(height);
                        corner[i].rgba = rgba;
                        corner[i].fog = 0.0f;
                    }
                    pushQuad(batch, corner);
                    ++g_stats.flareSprites;
                }
            }
            flush(batch);
        }

        // TMB_NATIVE_PARTICLES_CHECK: per program, the systems computed here in chain order, waiting for the
        // adapter's translation of their packets (the game's CALLs come in the same sorted order).
        struct Captured
        {
            std::vector<Vertex> vertices;
            uint32_t ps = 0;
            bool attached = false;
        };
        std::unordered_map<uint32_t, std::vector<Captured>> g_checkQueue;
        std::unordered_map<uint32_t, size_t> g_checkNext;
        struct CheckStats
        {
            uint64_t systems = 0, countDiffers = 0, countDiffersAttached = 0, unmatched = 0, vertices = 0, posOff = 0, colourOff = 0,
                     uvOff = 0;
            float maxPos = 0.0f, maxUv = 0.0f;
            uint32_t maxColour = 0;
        };
        std::unordered_map<uint32_t, CheckStats> g_checkStats;

        uint32_t checkProgram(uint32_t ps, uint32_t type)
        {
            switch (type)
            {
            case 1: return 0x118u;
            case 4: return 0xcf8u;
            case 6:
            {
                const auto found = g_arcs.find(ps);
                return found != g_arcs.end() && !found->second.sphere ? 0x1198u : 0u; // 0x10e0: not translated there
            }
            case 7: return 0x1830u;
            case 8: return 0xa08u;
            default: return 0u;
            }
        }

        void startCheckFrame()
        {
            for (auto &entry : g_checkQueue)
                g_checkStats[entry.first].unmatched += entry.second.size() - std::min(entry.second.size(), g_checkNext[entry.first]);
            g_checkQueue.clear();
            g_checkNext.clear();
        }

        void compareCaptured(uint32_t pc, const Vertex *theirs, size_t count)
        {
            std::vector<Captured> &queue = g_checkQueue[pc];
            size_t &next = g_checkNext[pc];
            CheckStats &st = g_checkStats[pc];
            if (next >= queue.size())
            {
                ++st.unmatched;
                return;
            }
            const Captured &mine = queue[next++];
            ++st.systems;
            if (mine.vertices.size() != count)
            {
                ++st.countDiffers;
                st.countDiffersAttached += mine.attached ? 1u : 0u;
                static uint32_t s_logged = 0;
                if (s_logged++ < 12u)
                    std::fprintf(stderr, "[tmb:particles] check %#x: system %#x%s: %zu vertices here, %zu translated\n", pc, mine.ps,
                                 mine.attached ? " (attached)" : "", mine.vertices.size(), count);
            }
            else
                for (size_t i = 0; i < count; ++i)
                {
                    const Vertex &a = mine.vertices[i], &b = theirs[i];
                    const float pos = std::max(std::fabs(a.x - b.x), std::fabs(a.y - b.y));
                    const float uv = std::max(std::fabs(a.u - b.u), std::fabs(a.v - b.v));
                    uint32_t colour = 0;
                    for (int k = 0; k < 4; ++k)
                    {
                        const int32_t d = static_cast<int32_t>((a.rgba >> (8 * k)) & 0xffu) - static_cast<int32_t>((b.rgba >> (8 * k)) & 0xffu);
                        colour = std::max<uint32_t>(colour, static_cast<uint32_t>(d < 0 ? -d : d));
                    }
                    ++st.vertices;
                    st.posOff += pos > 0.01f ? 1u : 0u;
                    st.uvOff += uv > 0.001f ? 1u : 0u;
                    st.colourOff += colour > 1u ? 1u : 0u;
                    st.maxPos = std::max(st.maxPos, pos);
                    st.maxUv = std::max(st.maxUv, uv);
                    st.maxColour = std::max(st.maxColour, colour);
                }
            if (st.systems % 100u == 0u || (st.systems & (st.systems - 1u)) == 0u)
                std::fprintf(stderr, "[tmb:particles] check %#x: %llu systems (%llu other vertex counts, %llu of them attached; %llu unmatched), "
                                     "%llu vertices: %llu off by > 0.01 px (max %.3f), %llu colours by > 1 (max %u), %llu UVs by > 0.001 (max %.4f)\n",
                             pc, static_cast<unsigned long long>(st.systems), static_cast<unsigned long long>(st.countDiffers),
                             static_cast<unsigned long long>(st.countDiffersAttached), static_cast<unsigned long long>(st.unmatched),
                             static_cast<unsigned long long>(st.vertices), static_cast<unsigned long long>(st.posOff), st.maxPos,
                             static_cast<unsigned long long>(st.colourOff), st.maxColour, static_cast<unsigned long long>(st.uvOff), st.maxUv);
        }

        // TMB_NATIVE_PARTICLES_CHECK=1 with the flares: the game's two functions run (their packet translated by the
        // adapter), and the ones here on the same input before them, their results compared byte for byte (the list,
        // its counts, the random seed, the Z samples, the debounce table), then the game's kept; the sprites here
        // from the game's list compared with the adapter's 0x2338 translation (compareCaptured).
        bool g_flaresCheck = false;
        struct FlareCheckStats
        {
            uint64_t calls = 0, differ = 0, unfinished = 0;
        };
        FlareCheckStats g_flareCheck[2]; // the test, the render

        // The guest memory the flare functions write: the list (0x4a lights), the counts and seed, the samples and the
        // debounce table (the light ids are small: their entries sit below the samples).
        struct FlareMemory
        {
            std::vector<uint8_t> list, tables;
            uint32_t counts[2] = {}, listAt = 0;
            uint64_t seed = 0;
        };
        constexpr uint32_t kFlareTablesSize = (kLensSamples - kLensDebounce) + 0x4au * 64u;

        FlareMemory saveFlares(const uint8_t *rdram, uint32_t gp, uint32_t view)
        {
            FlareMemory m;
            m.listAt = u32(rdram, gp + static_cast<uint32_t>(kGpLensLists) + (view & 1u) * 4u);
            m.list.assign(rdram + (m.listAt & 0x1ffffffu), rdram + (m.listAt & 0x1ffffffu) + 0x4au * 0x30u);
            m.tables.assign(rdram + kLensDebounce, rdram + kLensDebounce + kFlareTablesSize);
            for (uint32_t k = 0; k < 2u; ++k)
                m.counts[k] = u32(rdram, gp + static_cast<uint32_t>(kGpLensCounts) + 4u * k);
            m.seed = u64(rdram, gp + static_cast<uint32_t>(kGpSeed));
            return m;
        }

        void restoreFlares(uint8_t *rdram, uint32_t gp, const FlareMemory &m)
        {
            std::memcpy(rdram + (m.listAt & 0x1ffffffu), m.list.data(), m.list.size());
            std::memcpy(rdram + kLensDebounce, m.tables.data(), m.tables.size());
            for (uint32_t k = 0; k < 2u; ++k)
                put32(rdram, gp + static_cast<uint32_t>(kGpLensCounts) + 4u * k, m.counts[k]);
            put64(rdram, gp + static_cast<uint32_t>(kGpSeed), m.seed);
        }

        void compareFlares(int which, const FlareMemory &mine, const FlareMemory &game, int32_t mineResult, int32_t gameResult)
        {
            FlareCheckStats &st = g_flareCheck[which];
            ++st.calls;
            const char *name = which == 0 ? "test" : "render";
            size_t at = 0;
            const char *where = nullptr;
            if (mineResult != gameResult)
                where = "result";
            else if (std::memcmp(mine.counts, game.counts, sizeof(mine.counts)) != 0)
                where = "count";
            else if (mine.seed != game.seed)
                where = "seed";
            else
            {
                const size_t used = std::min(static_cast<size_t>(std::max(gameResult, 0)) * 0x30u, mine.list.size());
                while (at < used && mine.list[at] == game.list[at])
                    ++at;
                if (at < used)
                    where = "list";
                else
                {
                    at = 0;
                    while (at < mine.tables.size() && mine.tables[at] == game.tables[at])
                        ++at;
                    if (at < mine.tables.size())
                        where = "tables";
                }
            }
            if (where)
            {
                ++st.differ;
                static uint32_t s_logged = 0;
                if (s_logged++ < 20u)
                {
                    uint32_t a = 0, b = 0;
                    const bool inList = where[0] == 'l';
                    const std::vector<uint8_t> &x = inList ? mine.list : mine.tables, &y = inList ? game.list : game.tables;
                    if ((inList || where[0] == 't') && (at & ~size_t{3}) + 4u <= x.size())
                    {
                        std::memcpy(&a, &x[at & ~size_t{3}], 4);
                        std::memcpy(&b, &y[at & ~size_t{3}], 4);
                    }
                    std::fprintf(stderr, "[tmb:particles] flare check %s: differs (%s, offset %#zx: here %#x, game %#x; results %d, %d)\n", name,
                                 where, at, a, b, mineResult, gameResult);
                }
            }
            if ((st.calls & (st.calls - 1u)) == 0u || st.calls % 1000u == 0u)
                std::fprintf(stderr, "[tmb:particles] flare check %s: %llu calls, %llu differ (%llu more not compared: the game's left midway)\n",
                             name, static_cast<unsigned long long>(st.calls), static_cast<unsigned long long>(st.differ),
                             static_cast<unsigned long long>(st.unfinished));
        }

        void lensTestOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_flaresCheck)
            {
                const uint32_t gp = getRegU32(ctx, 28), view = getRegU32(ctx, 4);
                const FlareMemory before = saveFlares(rdram, gp, view);
                const int32_t mineResult = lensFlareTest(rdram, gp, view);
                const FlareMemory mine = saveFlares(rdram, gp, view);
                restoreFlares(rdram, gp, before);
                const uint32_t ra = getRegU32(ctx, 31);
                ctx->pc = kLensTest;
                gameLensTest(rdram, ctx, runtime);
                if (ctx->pc != ra) // left midway (its DMA wait, a GS readback): compared only when it ran through
                    ++g_flareCheck[0].unfinished;
                else
                    compareFlares(0, mine, saveFlares(rdram, gp, view), mineResult, static_cast<int32_t>(getRegU32(ctx, 2)));
                return;
            }
            if (!g_flaresNative)
            {
                ctx->pc = kLensTest;
                gameLensTest(rdram, ctx, runtime);
                return;
            }
            setReg(ctx, 2, static_cast<uint32_t>(lensFlareTest(rdram, getRegU32(ctx, 28), getRegU32(ctx, 4))));
            ctx->pc = getRegU32(ctx, 31);
        }

        // particleLensFlareRender(chain, view) natively: the visibility, then the sprites in the particle chain's group
        // (after the native systems); the chain comes back as it went in (no texture upload, no packet, no CALL).
        void lensRenderOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_flaresCheck)
            {
                const uint32_t view = getRegU32(ctx, 5), gp = getRegU32(ctx, 28);
                const int32_t count = static_cast<int32_t>(u32(rdram, gp + static_cast<uint32_t>(kGpLensCounts) + (view & 1u) * 4u));
                const FlareMemory before = saveFlares(rdram, gp, view);
                if (count > 0)
                    lensFlareVisibility(rdram, before.listAt, count, view);
                const FlareMemory mine = saveFlares(rdram, gp, view);
                restoreFlares(rdram, gp, before);
                const uint32_t ra = getRegU32(ctx, 31);
                ctx->pc = kLensRender;
                gameLensRender(rdram, ctx, runtime);
                if (ctx->pc != ra)
                    ++g_flareCheck[1].unfinished;
                else
                    compareFlares(1, mine, saveFlares(rdram, gp, view), count, count);
                if (count > 0 && ctx->pc == ra) // the sprites from the game's list, for the adapter's translation of its packet
                {
                    Captured captured;
                    captured.ps = before.listAt;
                    g_capture = &captured.vertices;
                    drawLensFlares(rdram, before.listAt, count, view);
                    g_capture = nullptr;
                    g_checkQueue[0x2338u].push_back(std::move(captured));
                }
                return;
            }
            if (!g_flaresNative)
            {
                ctx->pc = kLensRender;
                gameLensRender(rdram, ctx, runtime);
                return;
            }
            const uint32_t chain = getRegU32(ctx, 4), view = getRegU32(ctx, 5), gp = getRegU32(ctx, 28);
            const uint32_t parity = view & 1u;
            const int32_t count = static_cast<int32_t>(u32(rdram, gp + static_cast<uint32_t>(kGpLensCounts) + parity * 4u));
            const uint32_t list = u32(rdram, gp + static_cast<uint32_t>(kGpLensLists) + parity * 4u);
            if (count > 0)
            {
                lensFlareVisibility(rdram, list, count, view);
                ps2x::native::Renderer &renderer = ps2x::native::Renderer::instance();
                renderer.beginGroup(tmb::native::nextStreamGroup());
                renderer.setKey(0);
                drawLensFlares(rdram, list, count, view);
            }
            setReg(ctx, 2, chain);
            ctx->pc = getRegU32(ctx, 31);
        }

        struct Sorted
        {
            float key;
            uint32_t index;
        };

        void particleDrawOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t view = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            const bool toggledAll = toggledOff() && !settings().toggleFlares;
            if (!settings().on || !tmb::native::effectsInStream() || u32(rdram, gp + static_cast<uint32_t>(kGpColours)) == 0u || toggledAll)
            {
                g_flaresNative = false;
                ctx->pc = kParticleDraw;
                gameParticleDraw(rdram, ctx, runtime);
                return;
            }
            // The lens flares here too unless left to the game (the check, TMB_NATIVE_PARTICLES_FLARES=0, the flares' toggle
            // period, the native depth off: their GS readback then).
            const bool flaresNative = settings().flares && !settings().check && !(settings().toggleFlares && toggledOff()) &&
                                      tmb::native::flareDepthNative();

            // The frustum (viewGetWH, viewGetFov): the half extents' slopes and the sphere test's factors.
            const uint32_t viewport = u32(rdram, kViews + view * 16u + 4u);
            const int32_t width = u16(rdram, viewport + 4u), height2 = 2 * static_cast<int32_t>(u16(rdram, viewport + 6u));
            const float fov = f32(rdram, viewport);
            const float inverse = 1.0f / fov;
            const float slopeY = static_cast<float>(height2) * 0.5f * inverse, slopeX = static_cast<float>(width) * 0.5f * inverse;
            const float radiusY = std::sqrt(static_cast<float>(height2 * height2) * 0.25f + fov * fov) * inverse;
            const float radiusX = std::sqrt(static_cast<float>(width * width) * 0.25f + fov * fov) * inverse;
            (void)kViewTable2;
            // The camera (viewGetTrans, viewGetMat): emitter - translation, times the rotation's rows.
            const uint32_t camera = u32(rdram, kViews + view * 16u);
            float trans[3], rot[3][3];
            for (int k = 0; k < 3; ++k)
                trans[k] = f32(rdram, camera + 0x10u + 4u * k);
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    rot[j][k] = f32(rdram, camera + 0x20u + 16u * j + 4u * k);

            std::vector<Sorted> sorted;
            for (uint32_t node = u32(rdram, gp + static_cast<uint32_t>(kGpHead)); node != 0xffffffffu;
                 node = u32(rdram, kNodes + node * 12u + 8u))
            {
                const uint32_t ps = kSystems + node * kStride;
                const uint32_t type = u32(rdram, ps + kType), count = u32(rdram, ps + kCount);
                if (type == 7u ? static_cast<int32_t>(count) < 2 : count == 0u)
                    continue;
                if (type == 10u)
                    continue;
                float d[3], v[3];
                for (int k = 0; k < 3; ++k)
                    d[k] = f32(rdram, ps + kEmitter + 4u * k) - trans[k];
                for (int j = 0; j < 3; ++j)
                    v[j] = rot[j][0] * d[0] + rot[j][1] * d[1] + rot[j][2] * d[2];
                const float r = f32(rdram, ps + kRadius);
                const float x = v[0], depth = v[1], y = v[2];
                if (!(1.0f < depth + r) || !(-depth * slopeX < x + r * radiusX) || !(x - r * radiusX < depth * slopeX) ||
                    !(-depth * slopeY < y + r * radiusY) || !(y - r * radiusY < depth * slopeY))
                {
                    ++g_stats.culled;
                    continue;
                }
                const float key = depth - static_cast<float>(static_cast<int32_t>(u32(rdram, ps + kSortBias)));
                // Insertion as particleDraw's: the larger keys (farther) first, an equal key after those already in.
                size_t at = sorted.size();
                sorted.push_back({key, node});
                while (at > 0u && sorted[at - 1u].key < key)
                {
                    sorted[at] = sorted[at - 1u];
                    --at;
                }
                sorted[at] = {key, node};
                if (u32(rdram, ps + kFlags) & 0x01000000u)
                    addLensFlare(rdram, gp, ps, view);
            }

            // The native systems now, in the sorted order, in the particle chain's group; the rest stays in the array.
            ViewConstants constants;
            for (int j = 0; j < 8; ++j)
                for (int k = 0; k < 4; ++k)
                    constants.vu[j][k] = f32(rdram, kEffectsVu + 16u * j + 4u * k);
            constants.fog = fogColour(rdram);
            ps2x::native::Renderer &renderer = ps2x::native::Renderer::instance();
            bool drewNative = false;
            std::vector<Sorted> left;
            left.reserve(sorted.size());
            if (settings().check)
                startCheckFrame();
            if (settings().precip >= 0 && settings().precipLevel > 0.0f)
                putF(rdram, gp + static_cast<uint32_t>(kGpPrecipLevel), settings().precipLevel);
            for (const Sorted &entry : sorted)
            {
                const uint32_t ps = kSystems + entry.index * kStride;
                const uint32_t type = u32(rdram, ps + kType);
                ++g_stats.byType[type & 15u];
                if (settings().check)
                {
                    if (const uint32_t pc = typeNative(type) ? checkProgram(ps, type) : 0u)
                    {
                        Captured captured;
                        captured.ps = ps;
                        captured.attached = u32(rdram, ps + kRotation) != 0u;
                        g_capture = &captured.vertices;
                        drawNatively(rdram, gp, ps, type, constants);
                        g_capture = nullptr;
                        g_checkQueue[pc].push_back(std::move(captured));
                    }
                    ++g_stats.gameSystems;
                    left.push_back(entry);
                    continue;
                }
                if (typeNative(type))
                {
                    if (!drewNative)
                    {
                        renderer.beginGroup(tmb::native::nextStreamGroup());
                        renderer.setKey(0);
                    }
                    if (drawNatively(rdram, gp, ps, type, constants))
                    {
                        drewNative = true;
                        ++g_stats.nativeSystems;
                        continue;
                    }
                }
                ++g_stats.gameSystems;
                left.push_back(entry);
            }

            // The A/B's mark of a native period: a magenta square at the top-left corner (with the flares' toggle: when they
            // are drawn here).
            if (settings().toggle > 0.0 && (!settings().toggleFlares || flaresNative))
            {
                if (!drewNative)
                {
                    renderer.beginGroup(tmb::native::nextStreamGroup());
                    renderer.setKey(0);
                }
                drewNative = true;
                const float x0 = 2048.0f - 320.0f + 4.0f, y0 = kScreenTop + 4.0f;
                Vertex corner[4];
                for (int i = 0; i < 4; ++i)
                {
                    corner[i].x = x0 + ((i & 1) ? 12.0f : 0.0f);
                    corner[i].y = y0 + ((i & 2) ? 6.0f : 0.0f);
                    corner[i].z = 16777000.0f;
                    corner[i].rgba = 0x80ff00ffu;
                }
                Batch mark;
                mark.material.textured = false;
                mark.material.blend = Material::Blend::Opaque;
                mark.material.depthTest = false;
                mark.material.depthWrite = false;
                mark.open = true;
                pushQuad(mark, corner);
                flush(mark);
            }

            // The game's own code for the rest: particleDraw's frame (its prologue's stores), its sorted arrays, then its
            // chain loop (or its tail, with s7 = 1 when only native systems drew: the chain head is still kicked, for
            // its constants and the group the native draws took).
            const uint32_t sp = getRegU32(ctx, 29) - 0x310u;
            put64(rdram, sp + 0x2d0u, reg64(ctx, 31));
            put64(rdram, sp + 0x2c0u, reg64(ctx, 30));
            const int saved[8] = {23, 22, 21, 20, 19, 18, 17, 16};
            for (int i = 0; i < 8; ++i)
                put64(rdram, sp + 0x2b0u - 0x10u * static_cast<uint32_t>(i), reg64(ctx, saved[i]));
            for (int i = 0; i < 5; ++i)
                putF(rdram, sp + 0x300u - 8u * static_cast<uint32_t>(i), ctx->f[24 - i]);
            put32(rdram, sp + 0x230u, view);
            const uint32_t keys = sp, indexes = sp + 0x100u;
            for (uint32_t i = 0; i < 64u; ++i)
            {
                putF(rdram, keys + 4u * i, i < left.size() ? left[i].key : 0.0f);
                put32(rdram, indexes + 4u * i, i < left.size() ? left[i].index : 0xffffffffu);
            }
            const uint32_t kept = static_cast<uint32_t>(std::min<size_t>(left.size(), 64u)); // the game's arrays hold 64
            setReg(ctx, 29, sp);
            setReg(ctx, 30, indexes);
            setReg(ctx, 18, u32(rdram, gp + static_cast<uint32_t>(kGpChain)));
            setReg(ctx, 19, 0u);
            setReg(ctx, 23, kept != 0u ? kept : (drewNative ? 1u : 0u));
            ctx->pc = kept != 0u ? kResumeChainLoop : kResumeTail;

            if (settings().log && ++g_stats.frames % 300u == 0u)
            {
                std::fprintf(stderr, "[tmb:particles] per 300 draws: %llu systems native (%llu sprites, %llu arcs on a sphere), %llu flare lights (%llu sprites), %llu left to the game, %llu culled, %llu missing textures; by type:",
                             static_cast<unsigned long long>(g_stats.nativeSystems), static_cast<unsigned long long>(g_stats.nativeSprites),
                             static_cast<unsigned long long>(g_stats.sphereArcs), static_cast<unsigned long long>(g_stats.flareLights),
                             static_cast<unsigned long long>(g_stats.flareSprites),
                             static_cast<unsigned long long>(g_stats.gameSystems), static_cast<unsigned long long>(g_stats.culled),
                             static_cast<unsigned long long>(g_stats.missingTextures));
                for (int t = 0; t < 16; ++t)
                    if (g_stats.byType[t])
                        std::fprintf(stderr, " %d:%llu", t, static_cast<unsigned long long>(g_stats.byType[t]));
                std::fprintf(stderr, "\n");
                const uint64_t frames = g_stats.frames;
                g_stats = Stats{};
                g_stats.frames = frames;
            }
            g_flaresNative = flaresNative; // its tail's lensTestOverride and lensRenderOverride
            g_flaresCheck = settings().check && settings().flares && tmb::native::flareDepthNative();
            gameParticleDraw(rdram, ctx, runtime);
            g_flaresNative = g_flaresCheck = false;
        }
    }

    bool checking() { return settings().on && settings().check; }

    void compare(uint32_t pc, const ps2x::native::Vertex *vertices, size_t count)
    {
        if (checking())
            compareCaptured(pc, vertices, count);
    }

    void install(PS2Runtime &runtime)
    {
        if (!settings().on || !tmb::native::enabled())
            return;
        runtime.registerFunction(kParticleDraw, &particleDrawOverride);
        runtime.registerFunction(kTexSpriteBuild, &texSpriteBuildOverride);
        runtime.registerFunction(kPostInit, &postInitOverride);
        if (settings().precip >= 0)
            runtime.registerFunction(kPrecipCreate, &precipCreateOverride);
        runtime.registerFunction(kStripeBuild, &stripeBuildOverride);
        runtime.registerFunction(kArcBuild, &arcBuildOverride);
        runtime.registerFunction(kArcUpdate, &arcUpdateOverride);
        runtime.registerFunction(kLensTest, &lensTestOverride);
        runtime.registerFunction(kLensRender, &lensRenderOverride);
        std::fprintf(stderr, "[tmb:particles] native particles on (types mask %#x)\n", settings().types);
    }
}
