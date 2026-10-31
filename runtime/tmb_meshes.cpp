// TMB's rigid 3D objects drawn natively from the mesh pack (tools/tmb_mesh_pack.py: the disc's NGP files converted
// offline, generated_us/tmb_meshes.pack) and the EE state the game computes per object, at its per-object hook: no VIF
// or GIF packet is read for them. TMB_NATIVE_MESHES=1 (with TMB_NATIVE=1) turns it on; default off.
//
// - Which NGP file an object is in: fileOnlyNgpFile / fileAddNgpFile (address, size) after fileReadf / zipInflateAll
//   read a file there (its name, tmb::textures::nameAt); the pack's file of that name, its size and CRC checked
//   against the image as loaded. An object is keyed (file, obj - the file's address).
// - The object: pktAddVu1ObjAsm (hierTraverseAsm's per-object call; s5 the hierobject, s7 the instance, s0 the
//   scratchpad cache, a3 "matrix changed", fp the flags' upper half). What the header it builds would put in VU1
//   memory comes from where the header takes it, the EE's own state: the object-to-camera matrix from VU0 vf1, vf2,
//   vf3, vf6 (VU 0-3, when a3 says it changed, else the last one), the car's light directions vf7-9 (VU 6-8), the
//   texture offset (obj+0x20 / +0x24 or the instance's float table), depth factor and fog offset (obj+0x2c, +0x28:
//   VU 4), the colour (obj+0x40 or the float table, times the fade in f20 / f22 and the car's colour: VU 5), the
//   point lights (VU0 vf10.. after hierTraverseAsm's 0x808: VU 165-175), the fog terms (the scratchpad cache
//   pktCacheForAsm fills: VU 20-21), and the frame's camera, light colours and colour offset (VU 9-21: the constants
//   block hier__Fi sends at each view, which the game keeps up to date in EE memory). The setup program is chosen
//   from the flags as pktAddVu1ObjAsm chooses it; the strips' maths are the adapter's (runtime/tmb_vu1_shading.h).
// - Vertex positions from the pack, each pool then read where the game keeps it in the loaded file (the pack gives
//   its offset: a few meshes, a car's headlight beams among them, have their vertices moved by the game).
// - Textures by the object's texture numbers (obj+0x50[slot], tmb::textures::lookupId), with the TEX0 the game wrote
//   in the object's register block (obj+0x70 + 0x30 * slot) for the size and palette.
//
// The draws wait for the object's place in the frame: the hook marks the header's last qword (its VIF NOP, which
// carries no data: the mark sits in the NOP's unused immediate) and the adapter (runtime/tmb_native.cpp), as it
// reaches that object's setup in the stream it already follows for the other draws, submits these instead of its
// own translation, in the same group of the GS stream. Sprites, ribbons, skinned, environment-mapped and morphing
// objects and the 2P resume stay on the adapter's path.

#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "tmb_region.h"
#include "tmb_meshes.h"
#include "tmb_vu1_shading.h"
#include "runtime/native/native_renderer.h"
#include "runtime/native/native_texture_cache.h"
#include "runtime/ps2_memory.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <immintrin.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef TMB_MESH_PACK
#define TMB_MESH_PACK "tmb_meshes.pack"
#endif

namespace tmb::textures
{
    std::shared_ptr<const ps2x::native::TextureCache::Texture> lookupId(uint32_t id, uint64_t tex0, uint64_t texa);
    std::shared_ptr<const ps2x::native::TextureCache::Texture> lookup(uint64_t tex0, uint64_t texa);
    uint32_t idAtTbp(uint32_t tbp);
    std::string nameAt(uint32_t address);
}

namespace tmb::native
{
    bool enabled();
}

namespace tmb::meshes
{
    namespace
    {
        constexpr uint32_t kFileOnlyNgpFile = TMB_ADDR(0x001C3B20u);
        constexpr uint32_t kFileAddNgpFile = TMB_ADDR(0x001C38A8u);
        constexpr uint32_t kHierCacheForAsm = TMB_ADDR(0x002032F4u);
        constexpr uint32_t kPktAddVu1ObjAsm = TMB_ADDR(0x001EBCF8u);
        constexpr uint32_t kPktAddVu1Obj = TMB_ADDR(0x001EADD0u);
        // hierTraverseAsm's two calls of pktAddVu1ObjAsm: an opaque object (sent in traversal order) and an alpha one
        // (linked into the depth-sorted list, sent after: it always sends its matrix and leaves VU1's matrix as is).
        constexpr uint32_t kOpaqueReturn = TMB_ADDR(0x001D67C0u);
        // The constants block hier__Fi sends to VU1 at each view (its VIF1 chain after the hier image's MPG): VU 5-8
        // from here, then VU 9-12, 13-16, 17-20, 21, each block after its STCYCL / UNPACK pair. An EE buffer the game
        // keeps (the camera, plightUpdateParaLights' colours, fogUpdate's terms); no symbol holds it.
#if defined(TMB_REGION_US)
        constexpr uint32_t kConstants = 0x002335F0u;
#else
        constexpr uint32_t kConstants = 0x00234870u;
#endif
        constexpr uint32_t kConstVu5 = kConstants + 0x10u;  // VU 5-8
        constexpr uint32_t kConstVu9 = kConstants + 0x58u;  // VU 9-12
        constexpr uint32_t kConstVu13 = kConstants + 0xa0u; // VU 13-16
        constexpr uint32_t kConstVu17 = kConstants + 0xe8u; // VU 17-20
        constexpr uint32_t kConstVu21 = kConstants + 0x130u; // VU 21

        bool active()
        {
            static const bool s_on = []
            {
                const char *value = std::getenv("TMB_NATIVE_MESHES");
                return value && *value == '1' && tmb::native::enabled();
            }();
            return s_on;
        }

        // ---- the pack ----

        struct PackObject
        {
            uint32_t offset = 0, kind = 0;
            uint64_t flags = 0;
            uint16_t ntex = 0;
            uint16_t textures[5] = {};
            const uint8_t *body = nullptr; // its positions' count
        };
        struct PackFile
        {
            std::string name;
            uint32_t size = 0, crc = 0;
            std::vector<PackObject> objects; // by offset
        };
        struct Pack
        {
            bool loaded = false;
            std::vector<uint8_t> blob;
            std::vector<PackFile> files;
        };

        template <typename T>
        T rd(const uint8_t *&p)
        {
            T v;
            std::memcpy(&v, p, sizeof(T));
            p += sizeof(T);
            return v;
        }

        Pack &pack()
        {
            static Pack s_pack;
            if (s_pack.loaded)
                return s_pack;
            s_pack.loaded = true;
            const char *path = std::getenv("TMB_MESH_PACK");
            if (!path || !*path)
                path = TMB_MESH_PACK;
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in)
            {
                std::fprintf(stderr, "[tmb:mesh] no mesh pack at %s (tools/tmb_mesh_pack.py): objects stay on the stream path\n", path);
                return s_pack;
            }
            s_pack.blob.resize(static_cast<size_t>(in.tellg()));
            in.seekg(0);
            in.read(reinterpret_cast<char *>(s_pack.blob.data()), static_cast<std::streamsize>(s_pack.blob.size()));
            const uint8_t *p = s_pack.blob.data(), *end = p + s_pack.blob.size();
            if (s_pack.blob.size() < 12u || std::memcmp(p, "TMBMESH2", 8) != 0)
            {
                std::fprintf(stderr, "[tmb:mesh] %s: not a mesh pack\n", path);
                s_pack.blob.clear();
                return s_pack;
            }
            p += 8;
            const uint32_t fileCount = rd<uint32_t>(p);
            size_t objects = 0;
            for (uint32_t f = 0; f < fileCount && p < end; ++f)
            {
                PackFile file;
                const uint16_t length = rd<uint16_t>(p);
                file.name.assign(reinterpret_cast<const char *>(p), length);
                p += length;
                file.size = rd<uint32_t>(p);
                file.crc = rd<uint32_t>(p);
                const uint32_t count = rd<uint32_t>(p);
                file.objects.resize(count);
                for (PackObject &o : file.objects)
                {
                    o.offset = rd<uint32_t>(p);
                    o.kind = rd<uint32_t>(p);
                    o.flags = rd<uint64_t>(p);
                    p += 16; // sphere
                    o.ntex = rd<uint16_t>(p);
                    for (uint16_t &t : o.textures)
                        t = rd<uint16_t>(p);
                    rd<uint32_t>(p); // pool object
                    o.body = p;
                    const uint32_t positions = rd<uint32_t>(p), aux = rd<uint32_t>(p);
                    p += static_cast<size_t>(positions) * 16u * (aux ? 2u : 1u);
                    const uint32_t sources = rd<uint32_t>(p);
                    p += static_cast<size_t>(sources) * 12u;
                    const uint32_t params = rd<uint32_t>(p);
                    p += static_cast<size_t>(params) * 20u;
                    const uint32_t strips = rd<uint32_t>(p);
                    for (uint32_t s = 0; s < strips; ++s)
                    {
                        p += 12;
                        const uint32_t vertices = rd<uint32_t>(p);
                        p += static_cast<size_t>(vertices) * 16u;
                    }
                }
                objects += count;
                s_pack.files.push_back(std::move(file));
            }
            std::fprintf(stderr, "[tmb:mesh] mesh pack %s: %zu files, %zu objects\n", path, s_pack.files.size(), objects);
            return s_pack;
        }

        uint32_t crc32(const uint8_t *data, size_t size)
        {
            static uint32_t s_table[256];
            static const bool s_made = []
            {
                for (uint32_t i = 0; i < 256u; ++i)
                {
                    uint32_t c = i;
                    for (int k = 0; k < 8; ++k)
                        c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
                    s_table[i] = c;
                }
                return true;
            }();
            (void)s_made;
            uint32_t c = 0xffffffffu;
            for (size_t i = 0; i < size; ++i)
                c = s_table[(c ^ data[i]) & 0xffu] ^ (c >> 8);
            return c ^ 0xffffffffu;
        }

        // ---- the NGP files as the game loaded them ----

        struct Loaded
        {
            uint32_t base = 0, size = 0;
            const PackFile *file = nullptr; // null: not in the pack (or not the converted image)
            uint32_t index = 0;             // the pack file's index (keys)
        };
        std::mutex g_mutex;
        std::vector<Loaded> g_loaded;

        void noteNgp(uint8_t *rdram, R5900Context *ctx, bool only)
        {
            const uint32_t base = getRegU32(ctx, 4), size = getRegU32(ctx, 5);
            const std::string name = tmb::textures::nameAt(base);
            Pack &p = pack();
            Loaded loaded;
            loaded.base = base;
            loaded.size = size;
            for (size_t i = 0; i < p.files.size(); ++i)
                if (p.files[i].name == name)
                {
                    const PackFile &file = p.files[i];
                    const uint32_t crc = size == file.size && (base & 0x1ffffffu) + size <= 0x2000000u ? crc32(rdram + (base & 0x1ffffffu), size) : 0u;
                    if (size == file.size && crc == file.crc)
                    {
                        loaded.file = &file;
                        loaded.index = static_cast<uint32_t>(i);
                    }
                    else
                        std::fprintf(stderr, "[tmb:mesh] %s at %#x: %u bytes (crc %#x), the pack's is %u bytes (crc %#x): not used\n", name.c_str(), base,
                                     size, crc, file.size, file.crc);
                    break;
                }
            std::fprintf(stderr, "[tmb:mesh] NGP %s at %#x (%u bytes)%s\n", name.empty() ? "?" : name.c_str(), base, size,
                         loaded.file ? "" : ": no pack entry");
            std::lock_guard<std::mutex> lock(g_mutex);
            if (only)
                g_loaded.clear();
            g_loaded.push_back(loaded);
        }

        void fileOnlyNgpFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            noteNgp(rdram, ctx, true);
            fileOnlyNgpFile__FPci_0x1c3b20(rdram, ctx, runtime);
        }
        void fileAddNgpFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            noteNgp(rdram, ctx, false);
            fileAddNgpFile__FPci_0x1c38a8(rdram, ctx, runtime);
        }

        // ---- EE memory ----

        struct Ee
        {
            uint8_t *rdram;
            uint8_t *scratch;
            uint8_t *at(uint32_t address) const
            {
                return (address >> 28) == 7u ? scratch + (address & 0x3fffu) : rdram + (address & 0x1ffffffu);
            }
            uint32_t u32(uint32_t address) const
            {
                uint32_t v;
                std::memcpy(&v, at(address), 4);
                return v;
            }
            float f32(uint32_t address) const
            {
                float v;
                std::memcpy(&v, at(address), 4);
                return v;
            }
            int16_t s16(uint32_t address) const
            {
                int16_t v;
                std::memcpy(&v, at(address), 2);
                return v;
            }
            uint16_t u16(uint32_t address) const { return static_cast<uint16_t>(s16(address)); }
            uint64_t u64(uint32_t address) const
            {
                uint64_t v;
                std::memcpy(&v, at(address), 8);
                return v;
            }
        };

        // ---- per object ----

        // VU1 data memory as the headers leave it, built from the EE state they come from (only what the strip
        // programs read): the matrix and light directions stay from the last opaque object that sent them.
        tmb::vu1::VuImage g_vu;
        float g_lastMatrix[4][4] = {};
        float g_lastLights[3][4] = {};

        std::unordered_map<uint32_t, Drawn> g_drawn; // mark -> the object's draws
        uint32_t g_nextMark = 1;
        uint32_t g_frame = 0;
        std::unordered_map<uint64_t, uint32_t> g_keysThisFrame;

        struct Stats
        {
            uint64_t calls = 0, cCalls = 0, drawn = 0, taken = 0, unclaimed = 0, noFile = 0, noObject = 0, kindOther = 0,
                     program = 0, extra = 0, unmarked = 0, triangles = 0, idMismatch = 0;
        } g_stats;

        void vf(const R5900Context *ctx, int reg, float out[4]) { _mm_storeu_ps(out, ctx->vu0_vf[reg]); }

        void readConstants(const Ee &ee, uint32_t from, uint32_t firstQw, uint32_t count)
        {
            for (uint32_t q = 0; q < count; ++q)
                g_vu.setQw(firstQw + q, ee.at(from + q * 16u));
        }

        // The rigid setup pktAddVu1ObjAsm picks for these flags (its upper half, fp) and clip state (s6), or 0 for
        // the programs this path leaves to the stream (environment map, ribbon, sprite, morph, 2P resume).
        uint32_t setupFor(uint32_t fp, uint32_t s6, bool pointLights)
        {
            if (fp & 0x1000u)
                return 0; // bit 44: MSCAL 0xc78, the split screen's resume
            if ((fp >> 1) & 7u)
                return 0; // bits 33-35: 0x1db8, environment map
            const uint32_t sprite = (fp >> 5) & 3u;
            if (sprite == 1u || sprite == 2u)
                return 0; // 0x3270 ribbon, 0x3000 sprite
            if ((fp >> 10) & 1u)
                return 0; // bit 42: 0xd48 / 0xd68 / 0xd28 (morph)
            if (pointLights)
                return 0x1700u;
            if (fp & 0x100u)
                return 0x0ce8u;
            return s6 == 1u ? 0x0d08u : 0x0cc8u;
        }

        // The object's strips from the pack, shaded as the setup's strip program shades them.
        void buildStrips(const Ee &ee, uint32_t obj, uint32_t fileBase, const PackObject &object, uint32_t pc, Drawn &out)
        {
            const tmb::vu1::Shading mode = pc == 0x0ce8u ? tmb::vu1::Shading::Directional
                                         : pc == 0x1700u ? tmb::vu1::Shading::Point
                                                         : tmb::vu1::Shading::Unlit;
            tmb::vu1::objectToScreen(g_vu, out.matrix, true);
            const float *m = out.matrix;
            const uint8_t *p = object.body;
            const uint32_t positionCount = rd<uint32_t>(p), aux = rd<uint32_t>(p);
            // x, y, z, w: the pack's, then each pool's from where the game keeps it in the loaded file (the V3-32 data of
            // its packet: a few meshes have their vertices moved by the game, a car's headlight beams among them).
            static thread_local std::vector<float> s_positions;
            s_positions.assign(reinterpret_cast<const float *>(p), reinterpret_cast<const float *>(p) + static_cast<size_t>(positionCount) * 4u);
            p += static_cast<size_t>(positionCount) * 16u * (aux ? 2u : 1u);
            const uint32_t sources = rd<uint32_t>(p);
            for (uint32_t i = 0; i < sources; ++i)
            {
                const uint32_t offset = rd<uint32_t>(p), first = rd<uint32_t>(p), count = rd<uint32_t>(p);
                if (offset == 0xffffffffu || first + count > positionCount)
                    continue;
                const uint8_t *from = ee.at(fileBase + offset);
                for (uint32_t v = 0; v < count; ++v)
                    std::memcpy(&s_positions[static_cast<size_t>(first + v) * 4u], from + v * 12u, 12);
            }
            const float *positions = s_positions.data();
            const uint32_t params = rd<uint32_t>(p);
            p += static_cast<size_t>(params) * 20u;
            const uint32_t strips = rd<uint32_t>(p);
            const float uOffset = g_vu.f(4, 0), vOffset = g_vu.f(4, 1);
            // The material as the strip program and the GS keep it: a strip's header brings a texture (its slot's
            // register block) or keeps the last; the setup starts untextured (as the adapter's useMaterial).
            uint64_t tex0 = 0;
            bool textured = false;
            uint32_t texnum = ~0u;
            Batch *batch = nullptr;
            std::vector<ps2x::native::Vertex> strip;
            static const bool s_check = std::getenv("TMB_NATIVE_MESHES_CHECK") != nullptr;
            for (uint32_t si = 0; si < strips; ++si)
            {
                const uint16_t prim = rd<uint16_t>(p);
                rd<uint16_t>(p); // gif flags (PRE: TMB's strips all carry their PRIM)
                rd<uint32_t>(p); // material header
                const int32_t slot = rd<int32_t>(p);
                const uint32_t vertexCount = rd<uint32_t>(p);
                const uint8_t *vertices = p;
                p += static_cast<size_t>(vertexCount) * 16u;
                Batch writes; // the block's other GS registers
                bool otherWrites = false;
                if (slot >= 0 && slot < 5 && slot < static_cast<int32_t>(ee.u16(obj + 0x5au)))
                {
                    // The slot's register block (A+D: TEX0_1 as texmGetTextureAddrs rewrote it this frame, TEX1_1, ...).
                    const uint32_t block = obj + 0x70u + 0x30u * static_cast<uint32_t>(slot);
                    for (uint32_t q = 0; q < 3u; ++q)
                    {
                        const uint32_t address = ee.u32(block + q * 16u + 8u) & 0xffu;
                        const uint64_t value = ee.u64(block + q * 16u);
                        if (address == 0x06u) // TEX0_1
                        {
                            tex0 = value;
                            textured = true;
                            writes.setsTex0 = true;
                        }
                        else if (address == 0x42u) // ALPHA_1
                            writes.setsAlpha = otherWrites = true, writes.alpha = value;
                        else if (address == 0x47u) // TEST_1
                            writes.setsTest = otherWrites = true, writes.test = value;
                        else if (address == 0x4eu) // ZBUF_1
                            writes.setsZbuf = otherWrites = true, writes.depthMasked = ((value >> 32) & 1u) != 0u;
                    }
                    texnum = ee.u16(obj + 0x50u + 2u * static_cast<uint32_t>(slot));
                }
                const bool blended = (prim & 0x40u) != 0u, fogged = (prim & 0x20u) != 0u;
                if (!batch || otherWrites || batch->tex0 != tex0 || batch->textured != textured || batch->blended != blended ||
                    batch->fogged != fogged)
                {
                    out.batches.emplace_back(std::move(writes));
                    batch = &out.batches.back();
                    batch->tex0 = tex0;
                    batch->textured = textured;
                    batch->blended = blended;
                    batch->fogged = fogged;
                    if (textured)
                    {
                        batch->texture = tmb::textures::lookupId(texnum, tex0, 0u);
                        if (s_check && tmb::textures::idAtTbp(static_cast<uint32_t>(tex0 & 0x3fffu)) != texnum)
                        {
                            ++g_stats.idMismatch;
                            static uint32_t s_logged = 0;
                            if (s_logged++ < 20u)
                                std::fprintf(stderr, "[tmb:mesh] object %#x: texture number %u, texInfo has %u at TBP %u\n", obj, texnum,
                                             tmb::textures::idAtTbp(static_cast<uint32_t>(tex0 & 0x3fffu)), static_cast<uint32_t>(tex0 & 0x3fffu));
                        }
                    }
                }
                strip.clear();
                for (uint32_t v = 0; v < vertexCount; ++v)
                {
                    const uint8_t *vx = vertices + v * 16u;
                    uint16_t index;
                    std::memcpy(&index, vx, 2);
                    ps2x::native::Vertex outV;
                    if (index >= positionCount)
                    {
                        outV.rgba = 0xdeadu; // a break: no triangle uses it
                        strip.push_back(outV);
                        continue;
                    }
                    const float *pos = positions + static_cast<size_t>(index) * 4u;
                    outV.x = pos[0];
                    outV.y = pos[1];
                    outV.z = pos[2];
                    const uint32_t normal[3] = {vx[2], vx[3], vx[4]};
                    const uint32_t rgba[4] = {vx[6], vx[7], vx[8], vx[9]};
                    outV.rgba = tmb::vu1::shade(mode, g_vu, normal, rgba, pos);
                    int16_t st[2];
                    std::memcpy(st, vx + 10, 4);
                    outV.u = static_cast<float>(st[0]) / 4096.0f + uOffset;
                    outV.v = static_cast<float>(st[1]) / 4096.0f + vOffset;
                    const float w = m[3] * pos[0] + m[7] * pos[1] + m[11] * pos[2] + m[15];
                    outV.fog = w > 0.0f ? tmb::vu1::fogFactor(g_vu, 1.0f / w) : 0.0f;
                    strip.push_back(outV);
                }
                if (s_check)
                    out.checkStrips.push_back(strip);
                for (size_t i = 2; i < strip.size(); ++i)
                {
                    if (strip[i].rgba == 0xdeadu || strip[i - 1].rgba == 0xdeadu || strip[i - 2].rgba == 0xdeadu)
                        continue;
                    batch->vertices.push_back(strip[i - 2]);
                    batch->vertices.push_back(strip[i - 1]);
                    batch->vertices.push_back(strip[i]);
                }
            }
            for (const Batch &b : out.batches)
                g_stats.triangles += b.vertices.size() / 3u;
        }

        void pktAddVu1ObjAsmOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            // Its arguments as the caller set them (it walks s4 through the extra qwords it copies).
            const uint32_t obj = getRegU32(ctx, 21), inst = getRegU32(ctx, 23), spr = getRegU32(ctx, 16);
            const uint32_t a3 = getRegU32(ctx, 7), extra = getRegU32(ctx, 20), s6 = getRegU32(ctx, 22);
            pktAddVu1ObjAsm_0x1ebcf8(rdram, ctx, runtime);
            if (!active())
                return;
            ++g_stats.calls;
            const Ee ee{rdram, runtime->memory().getScratchpad()};
            const uint32_t fp = ee.u16(obj + 0xcu); // lh $fp, 0xC($s5): the flags' bits 32-47
            const bool opaque = getRegU32(ctx, 31) == kOpaqueReturn;

            // VU 0-3: the object-to-camera matrix (hierMulEoForObjAsm: vf1-3 the rotation, vf6 the translation),
            // sent when it changed; VU1 keeps the last opaque object's otherwise.
            float matrix[4][4];
            if (a3 != 0u)
            {
                vf(ctx, 1, matrix[0]);
                vf(ctx, 2, matrix[1]);
                vf(ctx, 3, matrix[2]);
                vf(ctx, 6, matrix[3]);
                if (opaque)
                    std::memcpy(g_lastMatrix, matrix, sizeof(matrix));
            }
            else
                std::memcpy(matrix, g_lastMatrix, sizeof(matrix));
            for (uint32_t q = 0; q < 4u; ++q)
                g_vu.setQw(q, matrix[q]);
            // VU 6-8: a car's light directions in its own frame (vf7-9), sent when scratchpad 0x38 holds a car.
            float lights[3][4];
            if (ee.u32(spr + 0x38u) != 0u)
            {
                vf(ctx, 7, lights[0]);
                vf(ctx, 8, lights[1]);
                vf(ctx, 9, lights[2]);
                if (opaque)
                    std::memcpy(g_lastLights, lights, sizeof(lights));
            }
            else
                std::memcpy(lights, g_lastLights, sizeof(lights));
            for (uint32_t q = 0; q < 3u; ++q)
                g_vu.setQw(6u + q, lights[q]);

            // VU 4: texture offset (obj+0x20, +0x24, or the float table's scroll with flag 43), depth factor
            // (obj+0x2c) and fog offset (obj+0x28).
            const uint32_t table = inst != 0u ? ee.u32(inst + 4u) : 0u; // the instance's float table
            const bool haveTable = inst != 0u;
            const auto entry = [&](uint32_t field, float otherwise)
            {
                const int16_t index = ee.s16(obj + field);
                return index >= 0 ? ee.f32(table + 4u * static_cast<uint32_t>(index)) : otherwise;
            };
            float vu4[4] = {ee.f32(obj + 0x20u), ee.f32(obj + 0x24u), ee.f32(obj + 0x2cu), ee.f32(obj + 0x28u)};
            if (haveTable && ((fp >> 11) & 1u))
            {
                vu4[0] = entry(0x30u, 0.0f);
                vu4[1] = entry(0x32u, 0.0f);
            }
            g_vu.setQw(4, vu4);
            // VU 5: the colour, times the fade (f20 or f22, by f21 < f20) and the car's colour (CsColor, unless flag 36).
            const float fade = ctx->f[21] < ctx->f[20] ? ctx->f[20] : ctx->f[22];
            float vu5[4] = {ee.f32(obj + 0x40u), ee.f32(obj + 0x44u), ee.f32(obj + 0x48u), ee.f32(obj + 0x4cu) * fade};
            if (haveTable)
            {
                vu5[0] = entry(0x34u, vu5[0]);
                vu5[1] = entry(0x36u, vu5[1]);
                vu5[2] = entry(0x38u, vu5[2]);
                if (ee.s16(obj + 0x3au) >= 0)
                    vu5[3] = entry(0x3au, 0.0f) * fade;
            }
            const uint32_t car = ee.u32(spr + 0x38u);
            if (car != 0u && ee.u32(spr + 0x8cu) != 0u && !(fp & 0x10u))
                for (int k = 0; k < 3; ++k)
                    vu5[k] *= ee.f32(car + 0x80u + 4u * static_cast<uint32_t>(k));
            g_vu.setQw(5, vu5);
            // VU 9-21: the frame's constants as the game keeps them; VU 20-21 the fog's (pktCacheForAsm's scratchpad
            // cache: flag 41 picks the first set) while the fog is on.
            readConstants(ee, kConstVu9, 9, 4);
            readConstants(ee, kConstVu13, 13, 4);
            readConstants(ee, kConstVu17, 17, 4);
            readConstants(ee, kConstVu21, 21, 1);
            if (ee.u32(spr + 0x90u) != 0u)
            {
                const bool first = (fp >> 9) & 1u;
                g_vu.setQw(20, ee.at(spr + (first ? 0x120u : 0x150u)));
                g_vu.setQw(21, ee.at(spr + (first ? 0x130u : 0x160u)));
            }
            // VU 165-175: the point lights hierTraverseAsm's 0x808 left in VU0 (vi14 set: some reach the object).
            const bool pointLights = ee.u32(spr + 0x9cu) != 0u && ctx->vi[14] != 0u;
            if (pointLights)
            {
                const int regs[11] = {10, 18, 12, 13, 14, 15, 16, 20, 21, 22, 28};
                for (uint32_t q = 0; q < 11u; ++q)
                {
                    float v[4];
                    vf(ctx, regs[q], v);
                    g_vu.setQw(165u + q, v);
                }
            }

            // What this path draws: a rigid object (no skeleton) of the pack, with one of the four strip setups and
            // no extra header qwords.
            const uint32_t pc = setupFor(fp, s6, pointLights);
            if (pc == 0u || ee.u32(obj + 0x64u) != 0u)
            {
                ++g_stats.program;
                return;
            }
            if (extra != 0u)
            {
                // TMB_NATIVE_MESHES_LOG_EXTRA=1: the extra qwords of the first headers that bring some (a diagnostic).
                static const bool s_logExtra = std::getenv("TMB_NATIVE_MESHES_LOG_EXTRA") != nullptr;
                static uint32_t s_logged = 0;
                static std::unordered_map<uint64_t, bool> s_seen; // each kind once: its first qword's upper words
                const uint64_t kind = (static_cast<uint64_t>(ee.u32(extra + 8u)) << 32) | ee.u32(extra + 12u);
                if (s_logExtra && !s_seen[kind] && s_logged++ < 60u)
                {
                    s_seen[kind] = true;
                    std::string text;
                    const uint32_t count = ee.u16(extra + 0xcu) + 1u;
                    for (uint32_t q = 0; q < std::min(count, 4u); ++q)
                    {
                        char item[48];
                        std::snprintf(item, sizeof(item), " [%08x %08x %08x %08x]", ee.u32(extra + q * 16u), ee.u32(extra + q * 16u + 4u),
                                      ee.u32(extra + q * 16u + 8u), ee.u32(extra + q * 16u + 12u));
                        text += item;
                    }
                    std::fprintf(stderr, "[tmb:mesh] object %#x (setup %#x): %u extra qwords at %#x:%s\n", obj, pc, count, extra, text.c_str());
                }
                // The extra qwords are the group's parameter block: in the arenas seen so far only a DIRECT of A+D
                // writes (ALPHA_1, TEST_1, ...), GS state the adapter follows in the stream before the setup, which
                // submitDrawn takes. Drawn here as the others.
                ++g_stats.extra;
            }
            const Loaded *file = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                for (const Loaded &l : g_loaded)
                    if (obj >= l.base && obj < l.base + l.size)
                        file = &l;
            }
            if (!file || !file->file)
            {
                ++g_stats.noFile;
                return;
            }
            const uint32_t offset = obj - file->base;
            const std::vector<PackObject> &objects = file->file->objects;
            const auto it = std::lower_bound(objects.begin(), objects.end(), offset, [](const PackObject &o, uint32_t v) { return o.offset < v; });
            if (it == objects.end() || it->offset != offset)
            {
                ++g_stats.noObject;
                return;
            }
            if (it->kind > 1u)
            {
                ++g_stats.kindOther;
                return;
            }

            // The header's last qword: NOP, BASE, OFFSET, MSCAL (pktAddVu1ObjAsm leaves the END tag after it, at the
            // packet's write pointer). The mark goes in the NOP's immediate.
            const uint32_t writePointer = ee.u32(getRegU32(ctx, 4) + 8u);
            uint8_t *last = ee.at(writePointer - 0x10u);
            uint32_t words[4];
            std::memcpy(words, last, 16);
            // pktCacheForAsm's setup codes are MSCALF (0x15).
            if (words[0] != 0u || (words[3] & ~0x01000000u) != (0x14000000u | (pc >> 3)))
            {
                ++g_stats.unmarked;
                static uint32_t s_logged = 0;
                if (s_logged++ < 10u)
                    std::fprintf(stderr, "[tmb:mesh] object %#x: header end %08x %08x %08x %08x, expected NOP .. MSCAL %#x: left to the stream\n", obj,
                                 words[0], words[1], words[2], words[3], pc);
                return;
            }

            Drawn drawn;
            drawn.setupPc = pc;
            drawn.frame = g_frame;
            drawn.object = obj;
            drawn.file = file->index;
            drawn.offset = offset;
            buildStrips(ee, obj, file->base, *it, pc, drawn);
            const uint64_t hash = (static_cast<uint64_t>(file->index) << 32 | offset) * 0x9e3779b97f4a7c15ull;
            const uint32_t occurrence = g_keysThisFrame[hash]++;
            drawn.key = (hash ^ (static_cast<uint64_t>(occurrence) * 0xc2b2ae3d27d4eb4full)) | 1u;
            for (uint32_t q = 0; q < 22u; ++q)
                for (int k = 0; k < 4; ++k)
                    drawn.check[q][k] = g_vu.w(q, k);

            const uint32_t mark = g_nextMark;
            g_nextMark = (g_nextMark + 1u) & 0xffffffu;
            if (g_nextMark == 0u)
                g_nextMark = 1u;
            words[0] = mark; // a VIF NOP (cmd 0, no interrupt) whatever its immediate and NUM
            std::memcpy(last, words, 4);
            g_drawn[mark] = std::move(drawn);
            ++g_stats.drawn;
        }

        void pktAddVu1ObjOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (active())
                ++g_stats.cCalls; // hierObjectNode's C path: left to the stream for now
            pktAddVu1Obj__FP9Vu1DmaPktP6QwDataP11_hierobjecti_0x1eadd0(rdram, ctx, runtime);
        }

        // At each view hier__Fi sends the constants block (VU 5-8 among it: the light directions until a car sends
        // its own), then calls hierCacheForAsm.
        void hierCacheForAsmOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            hierCacheForAsm_0x2032f4(rdram, ctx, runtime);
            if (!active())
                return;
            const Ee ee{rdram, runtime->memory().getScratchpad()};
            for (uint32_t q = 0; q < 3u; ++q)
                std::memcpy(g_lastLights[q], ee.at(kConstVu5 + 16u + q * 16u), 16);
        }
    }

    bool enabled() { return active(); }

    // The adapter, at the setup MSCAL a marked header ends with: the object's draws (false: not one of ours).
    bool take(uint32_t mark, Drawn &out)
    {
        const auto it = g_drawn.find(mark);
        if (it == g_drawn.end())
            return false;
        out = std::move(it->second);
        g_drawn.erase(it);
        ++g_stats.taken;
        return true;
    }

    // The end of a published frame: objects never reached in the stream are dropped (VU1 drew them, or their chain
    // was not sent), and every 250 frames the counts.
    void endFrame()
    {
        if (!active())
            return;
        ++g_frame;
        g_keysThisFrame.clear();
        for (auto it = g_drawn.begin(); it != g_drawn.end();)
            if (g_frame - it->second.frame > 2u)
            {
                ++g_stats.unclaimed;
                it = g_drawn.erase(it);
            }
            else
                ++it;
        if (g_frame % 250u == 0u)
        {
            std::fprintf(stderr,
                         "[tmb:mesh] per 250 frames: %llu objects at the hook (%llu by the C path), %llu drawn from the pack (%llu triangles), "
                         "%llu taken in the stream, %llu unclaimed; left to the stream: %llu by program (%llu drawn with a group GS-state block), %llu outside "
                         "the loaded files, %llu not in the pack, %llu sprites / ribbons / skinned, %llu unmarked; %llu texture number mismatches\n",
                         static_cast<unsigned long long>(g_stats.calls), static_cast<unsigned long long>(g_stats.cCalls),
                         static_cast<unsigned long long>(g_stats.drawn), static_cast<unsigned long long>(g_stats.triangles),
                         static_cast<unsigned long long>(g_stats.taken), static_cast<unsigned long long>(g_stats.unclaimed),
                         static_cast<unsigned long long>(g_stats.program), static_cast<unsigned long long>(g_stats.extra),
                         static_cast<unsigned long long>(g_stats.noFile), static_cast<unsigned long long>(g_stats.noObject),
                         static_cast<unsigned long long>(g_stats.kindOther), static_cast<unsigned long long>(g_stats.unmarked),
                         static_cast<unsigned long long>(g_stats.idMismatch));
            g_stats = Stats{};
        }
    }

    void install(PS2Runtime &runtime)
    {
        if (!active())
            return;
        pack();
        runtime.registerFunction(kFileOnlyNgpFile, &fileOnlyNgpFileOverride);
        runtime.registerFunction(kFileAddNgpFile, &fileAddNgpFileOverride);
        runtime.registerFunction(kHierCacheForAsm, &hierCacheForAsmOverride);
        runtime.registerFunction(kPktAddVu1ObjAsm, &pktAddVu1ObjAsmOverride);
        runtime.registerFunction(kPktAddVu1Obj, &pktAddVu1ObjOverride);
        std::fprintf(stderr, "[tmb:mesh] TMB_NATIVE_MESHES=1: rigid objects drawn from the mesh pack at pktAddVu1ObjAsm\n");
    }
}
