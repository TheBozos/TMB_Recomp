// TMB's particles drawn natively from the game's particle systems (TMB_NATIVE_PARTICLES=1, with TMB_NATIVE=1): no VIF
// packet is read and no GS packet is sent for the types done here.
//
// particleDraw(view) walks the active systems (_ParticleType, 0x2e80 bytes each, list nodes of 12 bytes, next at +8),
// culls each against the view's frustum (its emitter at +0x2c80 in camera space, a sphere of radius +0x2c8c), sorts the
// survivors far to near (camera depth less the system's bias at +0x2e54), and for each one appends its texture uploads
// (texmParticleTexture) and a DMA CALL of the packet its builder made in particlePrepDraw; that packet runs one of the
// effects image's VU1 programs (sprite 0x390, textured sprite 0x798, ...). Then the lens flares, and the chain goes to
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
// State: ALPHA_1 from the system (+0x2e58); TEX0 from the type's texture slot (the table at 0x6879c0, texture id at
// 0x687dc0, the texture itself from the native texture pack by that id); TEST_1 0x7000d (alpha greater than 0, depth
// greater or equal), ZBUF_1 masked (particles never write depth), TEX1 bilinear and TEXA 0x80: defaultContextInit's
// constants; the effects constants (VU 0-7: the view's screen matrix, then the size, guard band and fog terms) from the
// block particleUpdateRainViewport fills for the view (0x230010), which the chain head uploads.

#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "tmb_region.h"
#include "runtime/native/native_renderer.h"
#include "runtime/native/native_texture_cache.h"

#include <algorithm>
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
        void gameParticleDraw(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { particleDraw__Fi_0x1e9540(rdram, ctx, runtime); }
        void gameTexSpriteBuild(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            texSpritePacketBuild__FP13_ParticleType_0x1e12d8(rdram, ctx, runtime);
        }
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
        void gameParticleDraw(uint8_t *, R5900Context *, PS2Runtime *) {}
        void gameTexSpriteBuild(uint8_t *, R5900Context *, PS2Runtime *) {}
#endif

        // _ParticleType
        constexpr uint32_t kStride = 0x2e80u;
        constexpr uint32_t kPositions = 0x10u, kAges = 0x2940u, kBytes = 0x2b40u, kTextureIds = 0x2c40u, kEmitter = 0x2c80u,
                           kRadius = 0x2c8cu, kTranslation = 0x2d68u, kRotation = 0x2d6cu, kTexFlags = 0x2e10u,
                           kDepthTerm = 0x2e2cu, kTexScale = 0x2e34u, kColourScale = 0x2e38u, kLight = 0x2e3cu, kType = 0x2e40u,
                           kTexMask = 0x2e44u, kTexType = 0x2e48u, kColourMode = 0x2e4cu, kColourStart = 0x2e50u,
                           kSortBias = 0x2e54u, kAlpha = 0x2e58u, kFlags = 0x2e5cu, kCount = 0x2e70u;

        // The register state every particle draws with (defaultContextInit's DIRECT, the context the chain head sets).
        constexpr uint64_t kTest = 0x7000du;  // ATE, ATST GREATER, AREF 0, ZTE, ZTST GREATER
        constexpr uint64_t kTexa = 0x80u;
        constexpr float kScreenSpace[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

        struct Settings
        {
            bool on = false;
            uint32_t types = 0; // bit per particle type drawn natively
            bool log = false;
            double toggle = 0.0;
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
                s.on = kRegionSupported && on && *on == '1';
                // TMB_NATIVE_PARTICLE_TYPES=<type>[,<type>...]: the types drawn natively (default: all done here).
                s.types = (1u << 1) | (1u << 2) | (1u << 3);
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
                const char *log = std::getenv("TMB_NATIVE_PARTICLES_LOG");
                s.log = log && *log == '1';
                // TMB_NATIVE_PARTICLES_TOGGLE=<seconds>: native and the game's own draws in turn, that long each (an A/B
                // in one run; the native periods mark the screen's top-left corner with a small magenta square).
                if (const char *toggle = std::getenv("TMB_NATIVE_PARTICLES_TOGGLE"); toggle && *toggle)
                    s.toggle = std::strtod(toggle, nullptr);
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
            uint64_t frames = 0, nativeSystems = 0, nativeSprites = 0, gameSystems = 0, culled = 0, missingTextures = 0;
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

        void flush(Batch &batch)
        {
            if (batch.open && !batch.triangles.empty())
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

        bool drawNatively(const uint8_t *rdram, uint32_t gp, uint32_t ps, uint32_t type, const ViewConstants &view)
        {
            switch (type)
            {
            case 1: drawLines(rdram, gp, ps, view); return true;
            case 2: drawSprites(rdram, gp, ps, view); return true;
            case 3: drawTexSprites(rdram, gp, ps, view); return true;
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

        struct Sorted
        {
            float key;
            uint32_t index;
        };

        void particleDrawOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t view = getRegU32(ctx, 4), gp = getRegU32(ctx, 28);
            if (!settings().on || !tmb::native::effectsInStream() || u32(rdram, gp + static_cast<uint32_t>(kGpColours)) == 0u ||
                toggledOff())
            {
                ctx->pc = kParticleDraw;
                gameParticleDraw(rdram, ctx, runtime);
                return;
            }

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
            for (const Sorted &entry : sorted)
            {
                const uint32_t ps = kSystems + entry.index * kStride;
                const uint32_t type = u32(rdram, ps + kType);
                ++g_stats.byType[type & 15u];
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

            if (settings().toggle > 0.0) // the A/B's mark of a native period: a magenta square at the top-left corner
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
                std::fprintf(stderr, "[tmb:particles] per 300 draws: %llu systems native (%llu sprites), %llu left to the game, %llu culled, %llu missing textures; by type:",
                             static_cast<unsigned long long>(g_stats.nativeSystems), static_cast<unsigned long long>(g_stats.nativeSprites),
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
            gameParticleDraw(rdram, ctx, runtime);
        }
    }

    void install(PS2Runtime &runtime)
    {
        if (!settings().on || !tmb::native::enabled())
            return;
        runtime.registerFunction(kParticleDraw, &particleDrawOverride);
        runtime.registerFunction(kTexSpriteBuild, &texSpriteBuildOverride);
        std::fprintf(stderr, "[tmb:particles] native particles on (types mask %#x)\n", settings().types);
    }
}
