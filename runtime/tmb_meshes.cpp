// TMB's 3D objects drawn natively from the mesh pack (tools/tmb_mesh_pack.py: the disc's NGP files converted offline,
// generated_us/tmb_meshes.pack) and the EE state the game computes per object, at its per-object hook: no VIF or GIF
// packet is read for them. On with TMB_NATIVE=1; TMB_NATIVE_MESHES=0 turns it off. The kinds: rigid strips
// (setups 0xcc8, 0xd08, 0xce8, 0x1700), pool-less objects (the resume, 0xc78: the setup and pool of the object before
// them go on), sprites (0x3000), ribbons (0x3270), environment-mapped strips (0x1db8), skinned meshes (0x70, 0x90,
// 0xb0: their bones' matrices where skeletonTraverse wrote them, (*(inst + 0x14))[obj + 0x68]) and morphing objects
// (flag 42: setups 0xd28, 0xd48, 0xd68; three in the game, in CITY, MINICITY and FREEWAY).
//
// The morphing objects: their setups are the rigid ones' code (0xd28 = 0xcc8 unlit, 0xd48 = 0xce8 directional, 0xd68
// = 0xd08 scaled: the same BAL to 0x1c08 and the same strip loop in vi13). What differs is their pool's vertex pass,
// MSCAL 2 (0x10 -> 0xd88) instead of MSCAL 0, which moves each position by its delta (the V3-32 @373 block, which the
// pack keeps) times VU 31 before the strips read it (it writes the moved position back at 181 + i). VU 31 is the
// morph weight pktAddVu1ObjAsm sends for flag 42 (UNPACK S-32 @31: in every lane the instance's float table entry
// obj+0x3c, (*(inst + 4))[*(s16 *)(obj + 0x3c)]; without an instance it sends none and VU 31 keeps the last one).
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
// The draws are submitted in the order the game sends the objects, which its own calls give (no packet is read):
// a view's opaque objects in traversal order, then its alpha ones by the depth-sorted list pktLinkAlphaObjAsm builds
// (129 buckets of depth / 8, the farthest first, each bucket's newest first), when the view's chain is complete:
// hier__Fi's at its hierCheckVifComplete (right before its kick), hier2p's (a split screen's second view of a pair) at
// hier2pFlushDma, a view hier__Fi does not kick itself (the shell's menus) at the frame's end. Under the view's clip,
// before the particles drawn after it (particleDraw). The GS state of each draw (ALPHA_1, TEST_1, ZBUF_1.ZMSK) follows
// the objects in that order: the traversal's GS context block when the header brings one (a node's: its writes from the
// pack, recorded offline from the NGP files; the game's default one, ctxGetDefaultGSCtx: from EE memory), then the
// strips' register blocks. The skid marks' object (skidBuffer, which SkidMarks::draw fills each frame) is drawn from the
// marks themselves (skidSnapshot). Not drawn: hierObjectNode's C path (pktAddVu1Obj: not called in the arenas and
// menus seen), skinned objects without bones and resumes the pack path does not follow; their context blocks still
// count for the state.
//
// TMB_NATIVE_VIF=1 (runtime/tmb_native.cpp): the adapter follows the VIF1 stream again; the hook marks the header's
// last qword (its VIF NOP's unused immediate) and the adapter submits the draws at that object's setup in the stream,
// with the stream's GS state (the comparison mode). With TMB_NATIVE_MESHES_CHECK=1 too, the draws stay in the game's
// order and the adapter reports, per object, the stream's order and GS state against them ([tmb:order]), besides its
// VU 0-21, matrix and strips against its own translation.

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
    bool vifObserved();
    void applyViewClip();
    uint32_t fogColour();
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
                return !(value && *value == '0') && tmb::native::enabled(); // on with TMB_NATIVE=1 unless =0
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
            uint32_t poolFrom = 0xffffffffu; // pool-less: the object whose pool it uses
            const uint8_t *body = nullptr; // its positions' count
        };
        struct PackFile
        {
            std::string name;
            uint32_t size = 0, crc = 0;
            std::vector<PackObject> objects; // by offset
            // The GS context blocks of the image (by offset): the A+D writes the hier objects' draws depend on.
            struct Context
            {
                uint32_t offset = 0;
                bool setsAlpha = false, setsTest = false, setsZbuf = false;
                uint64_t alpha = 0, test = 0;
                bool depthMasked = false;
            };
            std::vector<Context> contexts;
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
            if (s_pack.blob.size() < 12u || std::memcmp(p, "TMBMESH5", 8) != 0)
            {
                std::fprintf(stderr, "[tmb:mesh] %s: not a mesh pack of this version (TMBMESH5: run tools/tmb_mesh_pack.py again)\n", path);
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
                    o.poolFrom = rd<uint32_t>(p);
                    o.body = p;
                    const uint32_t positions = rd<uint32_t>(p), aux = rd<uint32_t>(p);
                    p += static_cast<size_t>(positions) * 16u * (aux ? 2u : 1u);
                    const uint32_t sources = rd<uint32_t>(p);
                    p += static_cast<size_t>(sources) * 20u;
                    const uint32_t params = rd<uint32_t>(p);
                    p += static_cast<size_t>(params) * 20u;
                    const uint32_t skin = rd<uint32_t>(p);
                    p += static_cast<size_t>(skin) * 20u;
                    const uint32_t strips = rd<uint32_t>(p);
                    for (uint32_t s = 0; s < strips; ++s)
                    {
                        p += 12;
                        const uint32_t vertices = rd<uint32_t>(p);
                        p += static_cast<size_t>(vertices) * 16u;
                    }
                }
                objects += count;
                const uint32_t contexts = rd<uint32_t>(p);
                file.contexts.resize(contexts);
                for (PackFile::Context &c : file.contexts)
                {
                    c.offset = rd<uint32_t>(p);
                    const uint32_t writes = rd<uint32_t>(p);
                    for (uint32_t w = 0; w < writes; ++w)
                    {
                        const uint32_t address = rd<uint32_t>(p);
                        const uint64_t value = rd<uint64_t>(p);
                        if (address == 0x42u)
                            c.setsAlpha = true, c.alpha = value;
                        else if (address == 0x47u)
                            c.setsTest = true, c.test = value;
                        else if (address == 0x4eu)
                            c.setsZbuf = true, c.depthMasked = ((value >> 32) & 1u) != 0u;
                    }
                }
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

        // The opaque object sent last (the chain's order), when this path drew it: what the resume (MSCAL 0xc78, the
        // pool-less objects after it) goes on with, its setup, its object-to-screen matrix (vf1-4, before the depth
        // factor) and the point lights VU1 then holds.
        struct Previous
        {
            bool valid = false;
            uint32_t file = 0, offset = 0, poolFrom = 0, pc = 0;
            float screen[16] = {};
            uint32_t pointLights[11][4] = {};
        } g_previous;

        std::unordered_map<uint32_t, Drawn> g_drawn; // mark -> the object's draws
        uint32_t g_nextMark = 1;
        uint32_t g_frame = 0;
        std::unordered_map<uint64_t, uint32_t> g_keysThisFrame;
        const uint8_t *g_rdramForDump = nullptr; // TMB_NATIVE_MESHES_DUMP
        uint32_t g_firstSkinned = 0;              // the frame of the first skinned object at the hook

        struct Stats
        {
            uint64_t calls = 0, cCalls = 0, drawn = 0, taken = 0, unclaimed = 0, noFile = 0, noObject = 0, kindOther = 0,
                     program = 0, extra = 0, unmarked = 0, triangles = 0, idMismatch = 0, resumed = 0, resumeOther = 0, sprites = 0, ribbons = 0,
                     skinned = 0, reflective = 0, morphed = 0, contextsPack = 0, contextsDefault = 0, contextsMissing = 0,
                     contextsDiffering = 0, skid = 0;
        } g_stats;

        void vf(const R5900Context *ctx, int reg, float out[4]) { _mm_storeu_ps(out, ctx->vu0_vf[reg]); }

        // ---- the game's order ----
        //
        // hier__Fi (and hier2p, the split screen's second view of a pair) builds a view's chain: the opaque objects in
        // traversal order (pktAddVu1ObjAsm), each alpha one linked into a depth-sorted list of 129 buckets
        // (pktInitReverseAlphaOT, pktLinkAlphaObjAsm: bucket depth / 8 + 1, 1 up to 8, clamped at 1016; a bucket's newest
        // first), which pktAttachAlphaOT hangs after them, farthest bucket first. hier__Fi kicks its chain before it
        // returns (hierFlushObjQ), hier2p's waits for hier2pFlushDma (the next view's turn in rtMain). The draws are
        // submitted there, in that order, under the view's clip: what reached VU1 in that order. The particles,
        // drawn after the view (particleDraw), come after them.

        constexpr uint32_t kHierCheckVifComplete = TMB_ADDR(0x001D59D8u);
        constexpr uint32_t kHierKickReturn = TMB_ADDR(0x001D4610u); // hier__Fi's call of hierCheckVifComplete, before its kick
        constexpr uint32_t kHier2pFlushDma = TMB_ADDR(0x001D3B00u);
        constexpr uint32_t kHier2pCacheReturn = TMB_ADDR(0x001D3D34u); // hier2p's call of hierCacheForAsm, its view's start
        constexpr uint32_t kPktLinkAlphaObjAsm = TMB_ADDR(0x001EC194u);

        bool g_inHier2pFlush = false;
        uint64_t g_unsentFlushes = 0; // views submitted at the frame's end (not kicked by hier__Fi)
        uint64_t g_listDrops = 0, g_submitted[2] = {}, g_built[2] = {}, g_kicks[2] = {};
        struct ViewList
        {
            std::vector<Drawn> opaque;
            std::vector<std::vector<Drawn>> buckets = std::vector<std::vector<Drawn>>(129);
            bool pending = false; // an alpha object drawn at the hook, waiting for its pktLinkAlphaObjAsm
            // Built by hier2p: sent only by hier2pFlushDma. On the frame a split screen ends hier2p builds its view once
            // more and nothing sends it (the game drops the chain), so it is dropped too rather than drawn in the next view.
            bool hier2p = false;
            Drawn pendingAlpha;
            uint32_t frame = 0;
            bool empty() const
            {
                if (!opaque.empty())
                    return false;
                for (const std::vector<Drawn> &b : buckets)
                    if (!b.empty())
                        return false;
                return true;
            }
            void clear()
            {
                opaque.clear();
                for (std::vector<Drawn> &b : buckets)
                    b.clear();
                pending = false;
                hier2p = false;
            }
        };
        // The view being built (the hook's objects): hier__Fi's until its kick, hier2p's until hier2pFlushDma.
        ViewList g_lists[1];
        constexpr int g_building = 0;

        // The GS registers the hier objects' draws depend on, as their context and register blocks leave them in the
        // order the objects are sent (they persist from one object to the next, as in the GS).
        struct GsState
        {
            uint64_t alpha = 0x44, test = 0;
            bool depthMasked = false;
        } g_gs;

        void apply(GsState &gs, const Batch &writes)
        {
            if (writes.setsAlpha)
                gs.alpha = writes.alpha;
            if (writes.setsTest)
                gs.test = writes.test;
            if (writes.setsZbuf)
                gs.depthMasked = writes.depthMasked;
        }

        ps2x::native::Material::Blend blendFor(uint64_t alpha)
        {
            const uint32_t b = (alpha >> 2) & 3u, d = (alpha >> 6) & 3u;
            if (b == 2u && d == 1u)
                return ps2x::native::Material::Blend::Additive; // (A - 0) * C + Cd
            return ps2x::native::Material::Blend::Alpha;
        }

        // A batch's material as the GS state has it (TMB_NATIVE_MESHES_CHECK compares it with the stream's).
        struct Mat
        {
            uint64_t alpha = 0; // ALPHA_1 when blended, else 0
            uint64_t test = 0;
            bool depthMasked = false;
            bool operator==(const Mat &o) const { return alpha == o.alpha && test == o.test && depthMasked == o.depthMasked; }
        };
        Mat matFor(const GsState &gs, const Batch &batch)
        {
            return Mat{batch.blended ? gs.alpha : 0u, gs.test, gs.depthMasked};
        }

        // TMB_NATIVE_VIF=1 with TMB_NATIVE_MESHES_CHECK=1: per mark, the hook's order and materials and the stream's.
        struct CheckRecord
        {
            uint32_t frame = 0;
            uint64_t hookOrder = ~0ull, streamOrder = ~0ull;
            std::vector<Mat> hook;
            std::vector<Batch> sets; // the batches' register writes (no vertices), the context's first
            bool hasContext = false;
            GsState stream;
            bool streamSeen = false;
            uint32_t object = 0, index = 0, frameOfKick = 0;
            bool alphaList = false;
        };
        std::unordered_map<uint32_t, CheckRecord> g_checks;
        uint64_t g_hookSeq = 0, g_streamSeq = 0;
        uint32_t g_submitIndex = 0; // the object's place in its view's chain

        bool checking()
        {
            static const bool s_on = std::getenv("TMB_NATIVE_MESHES_CHECK") != nullptr;
            return s_on;
        }

        void submit(const Drawn &drawn)
        {
            ps2x::native::Renderer &renderer = ps2x::native::Renderer::instance();
            renderer.setKey(drawn.screenSpace ? 0u : drawn.key); // sprites: screen space, not interpolated
            static const float kScreenSpace[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            if (drawn.hasContext)
                apply(g_gs, drawn.context);
            CheckRecord *record = nullptr;
            if (checking() && drawn.mark != 0u)
            {
                record = &g_checks[drawn.mark];
                record->frame = drawn.frame;
                record->hookOrder = g_hookSeq++;
                record->hasContext = drawn.hasContext;
                record->object = drawn.object, record->alphaList = drawn.alphaList, record->index = g_submitIndex++;
                if (drawn.hasContext)
                    record->sets.push_back(drawn.context);
            }
            const uint32_t fog = tmb::native::fogColour();
            for (const Batch &batch : drawn.batches)
            {
                apply(g_gs, batch);
                if (record)
                {
                    Batch sets;
                    sets.setsAlpha = batch.setsAlpha, sets.alpha = batch.alpha, sets.setsTest = batch.setsTest, sets.test = batch.test;
                    sets.setsZbuf = batch.setsZbuf, sets.depthMasked = batch.depthMasked, sets.blended = batch.blended;
                    sets.textured = !batch.vertices.empty(); // (whether it draws)
                    record->sets.push_back(sets);
                    if (!batch.vertices.empty())
                        record->hook.push_back(matFor(g_gs, batch));
                }
                if (batch.vertices.empty())
                    continue;
                ps2x::native::Material material;
                material.textured = batch.textured && batch.texture != nullptr;
                material.tex0 = batch.tex0;
                material.blend = batch.blended ? blendFor(g_gs.alpha) : ps2x::native::Material::Blend::Opaque;
                material.depthWrite = !g_gs.depthMasked;
                const uint32_t atst = (g_gs.test >> 1) & 7u, aref = (g_gs.test >> 4) & 0xffu;
                if ((g_gs.test & 1u) && (atst == 5u || atst == 6u))
                {
                    material.alphaTest = atst == 6u || aref > 0u;
                    material.alphaRef = static_cast<uint8_t>(atst == 6u ? aref : aref - 1u);
                }
                material.fog = batch.fogged;
                material.fogColour = fog;
                renderer.draw(material, drawn.screenSpace ? kScreenSpace : drawn.matrix, batch.vertices.data(),
                              static_cast<uint32_t>(batch.vertices.size()), material.textured ? batch.texture : nullptr);
            }
        }

        // A GS context block as the traversal keeps it (s4: the default one, ctxGetDefaultGSCtx, or a node's): its first
        // quadword's VIF codes FLUSH, DIRECT n, then a PACKED GIF tag of n - 1 A+D writes. The writes this path follows.
        bool readContext(const Ee &ee, uint32_t at, Batch &out)
        {
            const uint32_t code = ee.u32(at + 12u);
            if (((code >> 24) & 0x7eu) != 0x50u)
                return false;
            const uint64_t tag = ee.u64(at + 16u), regs = ee.u64(at + 24u);
            const uint32_t nloop = tag & 0x7fffu, flg = (tag >> 58) & 3u, nreg = (tag >> 60) & 0xfu;
            if (flg != 0u || nreg != 1u || (regs & 0xfu) != 0xeu || nloop + 1u > (code & 0xffffu))
                return false;
            for (uint32_t q = 0; q < nloop; ++q)
            {
                const uint64_t value = ee.u64(at + 32u + q * 16u);
                const uint32_t address = ee.u32(at + 40u + q * 16u) & 0xffu;
                if (address == 0x42u)
                    out.setsAlpha = true, out.alpha = value;
                else if (address == 0x47u)
                    out.setsTest = true, out.test = value;
                else if (address == 0x4eu)
                    out.setsZbuf = true, out.depthMasked = ((value >> 32) & 1u) != 0u;
            }
            return true;
        }

        // The context block at `at`: from the pack when it is in a loaded NGP file, else the game's default context
        // (the executable's data, as defaultContextInit / ctxGetDefaultGSCtx leave it in EE memory).
        bool contextAt(const Ee &ee, uint32_t at, Batch &out)
        {
            const PackFile *file = nullptr;
            uint32_t base = 0;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                for (const Loaded &l : g_loaded)
                    if (at >= l.base && at < l.base + l.size)
                    {
                        file = l.file;
                        base = l.base;
                        if (!file)
                            break;
                    }
            }
            if (!file)
            {
                if (at >= 0x00980000u && (at & 0x1ffffffu) < 0x02000000u && base != 0u)
                {
                    ++g_stats.contextsMissing; // in a loaded file the pack does not have
                    return false;
                }
                ++g_stats.contextsDefault;
                return readContext(ee, at, out);
            }
            const auto it = std::lower_bound(file->contexts.begin(), file->contexts.end(), at - base,
                                             [](const PackFile::Context &c, uint32_t v) { return c.offset < v; });
            if (it == file->contexts.end() || it->offset != at - base)
            {
                ++g_stats.contextsMissing;
                static uint32_t s_logged = 0;
                if (s_logged++ < 20u)
                    std::fprintf(stderr, "[tmb:mesh] GS context at %#x (%s + %#x): not in the pack\n", at, file->name.c_str(), at - base);
                return false;
            }
            ++g_stats.contextsPack;
            out.setsAlpha = it->setsAlpha, out.alpha = it->alpha;
            out.setsTest = it->setsTest, out.test = it->test;
            out.setsZbuf = it->setsZbuf, out.depthMasked = it->depthMasked;
            if (checking()) // the pack's against the block as the game has it now
            {
                Batch now;
                if (!readContext(ee, at, now) || now.setsAlpha != out.setsAlpha || now.alpha != out.alpha || now.setsTest != out.setsTest ||
                    now.test != out.test || now.setsZbuf != out.setsZbuf || now.depthMasked != out.depthMasked)
                {
                    ++g_stats.contextsDiffering;
                    static uint32_t s_logged = 0;
                    if (s_logged++ < 20u)
                        std::fprintf(stderr, "[tmb:mesh] GS context at %#x (%s + %#x): the pack's differs from EE memory's\n", at, file->name.c_str(),
                                     at - base);
                }
            }
            return true;
        }

        void readConstants(const Ee &ee, uint32_t from, uint32_t firstQw, uint32_t count)
        {
            for (uint32_t q = 0; q < count; ++q)
                g_vu.setQw(firstQw + q, ee.at(from + q * 16u));
        }

        // The setup pktAddVu1ObjAsm picks for these flags (its upper half, fp), clip state (s6), point lights and
        // skeleton, in its order.
        uint32_t setupFor(uint32_t fp, uint32_t s6, bool pointLights, bool skinned)
        {
            if (fp & 0x1000u)
                return 0x0c78u; // bit 44: MSCAL 0xc78, the resume (the pool-less objects: the setup before them goes on)
            if (skinned)
                return (fp & 0x100u) ? 0x0070u : s6 == 1u ? 0x00b0u : 0x0090u; // directional, unlit (scaled), unlit
            if ((fp >> 1) & 7u)
                return 0x1db8u; // bits 33-35: environment map
            const uint32_t sprite = (fp >> 5) & 3u;
            if (sprite == 1u)
                return 0x3270u; // ribbon
            if (sprite == 2u)
                return 0x3000u; // sprite
            if ((fp >> 10) & 1u)
                return (fp & 0x100u) ? 0x0d48u : s6 == 1u ? 0x0d68u : 0x0d28u; // bit 42: morph (directional, scaled, unlit)
            if (pointLights)
                return 0x1700u;
            if (fp & 0x100u)
                return 0x0ce8u;
            return s6 == 1u ? 0x0d08u : 0x0cc8u;
        }

        // The object's strips from the pack, shaded as the setup's strip program shades them.
        // screen: the setup's matrix before the depth factor when it is not this object's (a resume), else null.
        // bones: a skinned object's bone matrices in EE memory (16 floats each), else 0.
        void buildStrips(const Ee &ee, uint32_t obj, uint32_t fileBase, const PackObject &object, uint32_t pc, Drawn &out,
                         const float *screen = nullptr, uint32_t bones = 0u)
        {
            const tmb::vu1::Shading mode = pc == 0x0ce8u || pc == 0x0d48u ? tmb::vu1::Shading::Directional
                                         : pc == 0x1700u ? tmb::vu1::Shading::Point
                                                         : tmb::vu1::Shading::Unlit;
            // Sprites and ribbons: screen-space vertices, from the setup's matrix without the depth factor (their
            // programs scale z themselves).
            // The lit skinned strips (0x640) do not scale z either; the unlit ones (0x878, 0x9e8) do.
            const bool sprite = pc == 0x3000u, ribbon = pc == 0x3270u;
            out.screenSpace = sprite || ribbon;
            const bool depthScaled = !out.screenSpace && pc != 0x0070u;
            if (screen)
            {
                std::memcpy(out.matrix, screen, sizeof(out.matrix));
                if (depthScaled)
                    for (int j = 0; j < 4; ++j)
                        out.matrix[j * 4 + 2] *= g_vu.f(4, 2);
            }
            else
                tmb::vu1::objectToScreen(g_vu, out.matrix, depthScaled);
            const float *m = out.matrix;
            const uint8_t *p = object.body;
            const uint32_t positionCount = rd<uint32_t>(p), aux = rd<uint32_t>(p);
            // x, y, z, w: the pack's, then each pool's from where the game keeps it in the loaded file (the V3-32 data of
            // its packet: a few meshes have their vertices moved by the game, a car's headlight beams among them).
            static thread_local std::vector<float> s_positions;
            s_positions.assign(reinterpret_cast<const float *>(p), reinterpret_cast<const float *>(p) + static_cast<size_t>(positionCount) * 4u);
            // The morph deltas (the pools' @373 blocks), per position.
            const float *deltas = aux ? reinterpret_cast<const float *>(p + static_cast<size_t>(positionCount) * 16u) : nullptr;
            p += static_cast<size_t>(positionCount) * 16u * (aux ? 2u : 1u);
            const uint32_t sources = rd<uint32_t>(p);
            for (uint32_t i = 0; i < sources; ++i)
            {
                const uint32_t offset = rd<uint32_t>(p), first = rd<uint32_t>(p), count = rd<uint32_t>(p);
                const uint32_t pass = rd<uint32_t>(p), deltaOffset = rd<uint32_t>(p);
                if (first + count > positionCount)
                    continue;
                if (offset != 0xffffffffu)
                {
                    const uint8_t *from = ee.at(fileBase + offset);
                    for (uint32_t v = 0; v < count; ++v)
                        std::memcpy(&s_positions[static_cast<size_t>(first + v) * 4u], from + v * 12u, 12);
                }
                // The morph pass (0x10 -> 0xd88): each position plus its delta times VU 31 (MULAw.xyz ACC, vf5, vf0w;
                // MADD.xyz vf10, vf7, vf6), the deltas read where the game keeps them in the loaded file (else the pack's).
                if (pass == 0x10u && (deltas || deltaOffset != 0xffffffffu))
                {
                    const uint8_t *from = deltaOffset != 0xffffffffu ? ee.at(fileBase + deltaOffset) : nullptr;
                    for (uint32_t v = 0; v < count; ++v)
                    {
                        float delta[3];
                        if (from)
                            std::memcpy(delta, from + v * 12u, 12);
                        else
                            std::memcpy(delta, deltas + static_cast<size_t>(first + v) * 4u, 12);
                        float *pos = &s_positions[static_cast<size_t>(first + v) * 4u];
                        for (int k = 0; k < 3; ++k)
                            pos[k] += delta[k] * g_vu.f(31, k);
                    }
                    ++g_stats.morphed;
                }
            }
            const float *positions = s_positions.data();
            // The parameter batches (environment map: the alpha terms of the strips after them).
            const uint32_t params = rd<uint32_t>(p);
            const uint8_t *paramAt = p;
            p += static_cast<size_t>(params) * 20u;
            const bool reflective = pc == 0x1db8u;
            tmb::vu1::Reflection reflection;
            // A skinned object's vertex pass: each position and its normal blended by its bones' matrices, as they are
            // in EE memory now (the chain sends them from there).
            const uint32_t skinCount = rd<uint32_t>(p);
            static thread_local std::vector<float> s_skinNormals;
            if (bones != 0u && skinCount == positionCount)
            {
                s_skinNormals.assign(static_cast<size_t>(positionCount) * 3u, 0.0f);
                for (uint32_t i = 0; i < skinCount; ++i)
                {
                    const uint8_t *v = p + static_cast<size_t>(i) * 20u;
                    const int count = std::min<int>(v[0], 3);
                    int32_t raw[3];
                    std::memcpy(raw, v + 8, 12);
                    const float weight[3] = {count == 1 ? 1.0f : tmb::vu1::skinWeight(raw[0]), tmb::vu1::skinWeight(raw[1]), tmb::vu1::skinWeight(raw[2])};
                    float matrix[3][16];
                    const float *matrices[3] = {matrix[0], matrix[1], matrix[2]};
                    for (int b = 0; b < 3; ++b)
                        std::memcpy(matrix[b], ee.at(bones + 64u * v[4 + b]), 64);
                    const float n[3] = {1.0f - static_cast<float>(v[1]) / 128.0f, 1.0f - static_cast<float>(v[2]) / 128.0f, 1.0f - static_cast<float>(v[3]) / 128.0f};
                    float *pos = &s_positions[static_cast<size_t>(i) * 4u];
                    const float bind[3] = {pos[0], pos[1], pos[2]};
                    tmb::vu1::skinVertex(count, matrices, weight, bind, n, pos, &s_skinNormals[static_cast<size_t>(i) * 3u]);
                }
            }
            p += static_cast<size_t>(skinCount) * 20u;
            const bool skin = bones != 0u && skinCount == positionCount;
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
                for (uint32_t i = 0; i < params; ++i)
                {
                    uint32_t before;
                    std::memcpy(&before, paramAt + 20u * i, 4);
                    if (before == si)
                    {
                        float terms[4];
                        std::memcpy(terms, paramAt + 20u * i + 4u, 16);
                        reflection = tmb::vu1::reflection(terms + 1);
                    }
                }
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
                if (sprite || ribbon)
                {
                    // One sprite (one point, one colour, four corners' texture coordinates) or one ribbon segment (two
                    // points, four colours and texture coordinates): the converter checked the shapes.
                    std::vector<ps2x::native::Vertex> &triangles = batch->vertices;
                    const size_t from = triangles.size();
                    const auto vertexAt = [&](uint32_t v) { return vertices + v * 16u; };
                    const auto pointOf = [&](uint32_t v, float out4[4])
                    {
                        uint16_t index;
                        std::memcpy(&index, vertexAt(v), 2);
                        if (index < positionCount)
                            std::memcpy(out4, positions + static_cast<size_t>(index) * 4u, 16);
                        else
                            std::memset(out4, 0, 16);
                    };
                    if (sprite && vertexCount == 4u)
                    {
                        float point[4];
                        pointOf(0u, point);
                        const uint8_t *vx = vertexAt(0u);
                        const uint32_t rgba[4] = {vx[6], vx[7], vx[8], vx[9]};
                        int32_t st[4][2];
                        for (uint32_t v = 0; v < 4u; ++v)
                        {
                            int16_t pair[2];
                            std::memcpy(pair, vertexAt(v) + 10, 4);
                            st[v][0] = pair[0];
                            st[v][1] = pair[1];
                        }
                        ps2x::native::Vertex quad[4];
                        static const int kOrder[6] = {0, 1, 2, 1, 2, 3};
                        if (tmb::vu1::spriteQuad(g_vu, m, point, rgba, st, quad))
                            for (int i : kOrder)
                                triangles.push_back(quad[i]);
                    }
                    else if (ribbon && vertexCount == 4u)
                    {
                        // The strip as the program reads it from TOP: the tag (its NLOOP, the points), point j at 1 + 3j,
                        // colour j at 2 + 3j, texture coordinates j at 3 + 3j (Z and W from the row hierSetVu1Regs sets,
                        // 0x1000, by its mask 0x50505050).
                        float points[2][4];
                        pointOf(0u, points[0]);
                        pointOf(2u, points[1]);
                        const auto read = [&](uint32_t r, uint32_t words[4])
                        {
                            std::memset(words, 0, 16);
                            if (r == 0u)
                                words[0] = 2u;
                            else if (r % 3u == 1u && (r - 1u) / 3u < 2u)
                                std::memcpy(words, points[(r - 1u) / 3u], 16);
                            else if (r % 3u == 2u && (r - 2u) / 3u < 4u)
                            {
                                const uint8_t *vx = vertexAt((r - 2u) / 3u);
                                for (int k = 0; k < 4; ++k)
                                    words[k] = vx[6 + k];
                            }
                            else if (r % 3u == 0u && (r - 3u) / 3u < 4u)
                            {
                                int16_t pair[2];
                                std::memcpy(pair, vertexAt((r - 3u) / 3u) + 10, 4);
                                words[0] = static_cast<uint32_t>(static_cast<int32_t>(pair[0]));
                                words[1] = static_cast<uint32_t>(static_cast<int32_t>(pair[1]));
                                words[2] = words[3] = 0x1000u;
                            }
                        };
                        tmb::vu1::ribbonStrip<ps2x::native::Vertex>(g_vu, m, read, triangles);
                    }
                    if (s_check)
                        out.checkStrips.emplace_back(triangles.begin() + static_cast<std::ptrdiff_t>(from), triangles.end());
                    continue;
                }
                if (reflective)
                {
                    // An environment-mapped strip (0x1ec8, the water's 0x2248): texture coordinates from the reflected
                    // eye vector, alpha by the view angle.
                    strip.clear();
                    for (uint32_t v = 0; v < vertexCount; ++v)
                    {
                        const uint8_t *vx = vertices + v * 16u;
                        uint16_t index;
                        std::memcpy(&index, vx, 2);
                        ps2x::native::Vertex outV;
                        if (index >= positionCount)
                        {
                            outV.rgba = 0xdeadu;
                            strip.push_back(outV);
                            continue;
                        }
                        const int32_t normal[3] = {vx[2], vx[3], vx[4]};
                        const int32_t rgba[4] = {vx[6], vx[7], vx[8], vx[9]};
                        tmb::vu1::reflectVertex(g_vu, m, reflection, positions + static_cast<size_t>(index) * 4u, normal, rgba, outV);
                        strip.push_back(outV);
                    }
                    const size_t from = batch->vertices.size();
                    for (size_t i = 2; i < strip.size(); ++i)
                    {
                        if (strip[i].rgba == 0xdeadu || strip[i - 1].rgba == 0xdeadu || strip[i - 2].rgba == 0xdeadu)
                            continue;
                        batch->vertices.push_back(strip[i - 2]);
                        batch->vertices.push_back(strip[i - 1]);
                        batch->vertices.push_back(strip[i]);
                    }
                    if (s_check)
                        out.checkStrips.emplace_back(batch->vertices.begin() + static_cast<std::ptrdiff_t>(from), batch->vertices.end());
                    continue;
                }
                if (skin)
                {
                    // A skinned strip (0x640, 0x878, 0x9e8): the blended positions, the colour lit by the blended normal
                    // (0x70) or not, texture coordinates plus VU 4.
                    strip.clear();
                    for (uint32_t v = 0; v < vertexCount; ++v)
                    {
                        const uint8_t *vx = vertices + v * 16u;
                        uint16_t index;
                        std::memcpy(&index, vx, 2);
                        ps2x::native::Vertex outV;
                        if (index >= positionCount)
                        {
                            outV.rgba = 0xdeadu;
                            strip.push_back(outV);
                            continue;
                        }
                        const float *pos = positions + static_cast<size_t>(index) * 4u;
                        outV.x = pos[0];
                        outV.y = pos[1];
                        outV.z = pos[2];
                        const uint32_t rgba[4] = {vx[6], vx[7], vx[8], vx[9]};
                        outV.rgba = tmb::vu1::skinShade(g_vu, pc == 0x0070u, &s_skinNormals[static_cast<size_t>(index) * 3u], rgba);
                        int16_t st[2];
                        std::memcpy(st, vx + 10, 4);
                        outV.u = static_cast<float>(st[0]) / 4096.0f + uOffset;
                        outV.v = static_cast<float>(st[1]) / 4096.0f + vOffset;
                        const float w = m[3] * pos[0] + m[7] * pos[1] + m[11] * pos[2] + m[15];
                        outV.fog = w > 0.0f ? tmb::vu1::fogFactor(g_vu, 1.0f / w) : 0.0f;
                        strip.push_back(outV);
                    }
                    const size_t from = batch->vertices.size();
                    for (size_t i = 2; i < strip.size(); ++i)
                    {
                        if (strip[i].rgba == 0xdeadu || strip[i - 1].rgba == 0xdeadu || strip[i - 2].rgba == 0xdeadu)
                            continue;
                        batch->vertices.push_back(strip[i - 2]);
                        batch->vertices.push_back(strip[i - 1]);
                        batch->vertices.push_back(strip[i]);
                    }
                    if (s_check)
                        out.checkStrips.emplace_back(batch->vertices.begin() + static_cast<std::ptrdiff_t>(from), batch->vertices.end());
                    continue;
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

        // ---- the skid marks ----
        //
        // SkidMarks::draw (rtMain, after the views) builds one hier object in skidBuffer from the marks, which the
        // next frame's traversal sends (its header from the template at this+0x203a0, ALPHA_1 0x44 in its register
        // block): per batch of up to 47 marks a pool of their four corners (this + 64 * mark: x, y, z at +0, +0x10,
        // +0x20, +0x30), then per mark a strip of four vertices (unlit, setup 0xcc8: PRIM 0x7c, a triangle strip, textured,
        // fogged, blended; the batch's first strip brings the texture of slot 0), colour the mark's (this+0x26168 + 4 *
        // mark), alpha min(smcSkidColors[material].a, (0x1e0 - age) / 2) (material this+0x278d8 + 4 * mark, age
        // this+0x29048 + 4 * mark), texture coordinates (1, 1), (0, 1), (1, 0), (0, 0) for the corners 0-3. The marks
        // follow a list from this+0x2a7b8 (next: this+0x17708 + 12 * mark, -1 ends it), this+0x2a7c0 of them. Kept here
        // as draw sees them (the update after it moves them), in the pack's object format, for the hook.
        constexpr uint32_t kSkidDraw = TMB_ADDR(0x00169610u);
        constexpr uint32_t kSkidBuffer = TMB_ADDR(0x00448D70u);
        constexpr uint32_t kSkidColors = TMB_ADDR(0x00448AC0u);
        struct Skid
        {
            bool valid = false;
            std::vector<uint8_t> body;
            std::vector<PackObject> objects; // one: the skid object
        } g_skid;

        template <typename T>
        void put(std::vector<uint8_t> &out, T value)
        {
            const size_t at = out.size();
            out.resize(at + sizeof(T));
            std::memcpy(out.data() + at, &value, sizeof(T));
        }

        void skidSnapshot(const Ee &ee, uint32_t self)
        {
            g_skid.valid = false;
            const int32_t count = static_cast<int32_t>(ee.u32(self + 0x2a7c0u));
            if (ee.u32(self + 0x203a0u) == 0u || count <= 0)
                return;
            const auto next = [&](int32_t mark) { return static_cast<int32_t>(ee.u32(self + 0x17708u + 12u * static_cast<uint32_t>(mark))); };
            const int32_t head = static_cast<int32_t>(ee.u32(self + 0x2a7b8u));
            std::vector<float> positions;
            struct Strip
            {
                bool first;
                uint8_t rgba[4];
            };
            std::vector<Strip> strips;
            int32_t remaining = count, corners = head, mark = head;
            while (remaining > 0)
            {
                const int32_t batch = remaining < 48 ? remaining : 47;
                remaining = remaining < 48 ? 0 : remaining - 47;
                // The pool: the marks' corners while the list lasts (VU memory keeps what was there past its end: zeros).
                for (int32_t k = 0; k < batch; ++k)
                {
                    for (uint32_t c = 0; c < 4u; ++c)
                    {
                        const bool have = corners != -1;
                        const uint32_t at = self + static_cast<uint32_t>(corners) * 64u + c * 16u;
                        positions.push_back(have ? ee.f32(at) : 0.0f);
                        positions.push_back(have ? ee.f32(at + 4u) : 0.0f);
                        positions.push_back(have ? ee.f32(at + 8u) : 0.0f);
                        positions.push_back(0.0f);
                    }
                    if (corners != -1)
                        corners = next(corners);
                }
                for (int32_t k = 0; k < batch; ++k)
                {
                    const uint32_t m = static_cast<uint32_t>(mark);
                    const uint32_t colour = ee.u32(self + 0x26168u + 4u * m);
                    const uint32_t material = ee.u32(self + 0x278d8u + 4u * m);
                    const int32_t fade = (0x1e0 - static_cast<int32_t>(ee.u32(self + 0x29048u + 4u * m))) / 2;
                    const int32_t limit = ee.at(kSkidColors + 4u * material + 3u)[0];
                    const uint32_t word = colour | (static_cast<uint32_t>(std::min(limit, fade)) << 24);
                    Strip strip;
                    strip.first = k == 0;
                    std::memcpy(strip.rgba, &word, 4);
                    strips.push_back(strip);
                    mark = next(mark);
                }
            }
            std::vector<uint8_t> &body = g_skid.body;
            body.clear();
            const uint32_t positionCount = static_cast<uint32_t>(positions.size() / 4u);
            put<uint32_t>(body, positionCount);
            put<uint32_t>(body, 0u); // no aux
            for (float f : positions)
                put<float>(body, f);
            put<uint32_t>(body, 0u); // no pool sources: the positions are the pack's
            put<uint32_t>(body, 0u); // no parameters
            put<uint32_t>(body, 0u); // not skinned
            put<uint32_t>(body, static_cast<uint32_t>(strips.size()));
            for (size_t k = 0; k < strips.size(); ++k)
            {
                put<uint16_t>(body, 0x7cu); // PRIM: triangle strip, Gouraud, textured, fogged, blended
                put<uint16_t>(body, 3u);    // PRE, EOP
                put<uint32_t>(body, strips[k].first ? 1u : 0u);
                put<int32_t>(body, strips[k].first ? 0 : -1);
                put<uint32_t>(body, 4u);
                for (uint32_t v = 0; v < 4u; ++v)
                {
                    put<uint16_t>(body, static_cast<uint16_t>(k * 4u + v));
                    for (int b = 0; b < 4; ++b) // normal, flags
                        put<uint8_t>(body, 0u);
                    for (int b = 0; b < 4; ++b)
                        put<uint8_t>(body, strips[k].rgba[b]);
                    put<int16_t>(body, static_cast<int16_t>((v & 1u) ? 0 : 0x1000));
                    put<int16_t>(body, static_cast<int16_t>(v < 2u ? 0x1000 : 0));
                    put<uint16_t>(body, 0u);
                }
            }
            if (g_skid.objects.empty())
                g_skid.objects.resize(1);
            PackObject &object = g_skid.objects[0];
            object.offset = kSkidBuffer; // its "file" starts at 0
            object.kind = 0u;
            object.body = body.data();
            g_skid.valid = true;
        }

        void skidDrawOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (active() && hookOrder())
            {
                const Ee ee{rdram, runtime->memory().getScratchpad()};
                skidSnapshot(ee, getRegU32(ctx, 4));
            }
            draw__9SkidMarks_0x169610(rdram, ctx, runtime);
        }

        // An object this path does not draw: its GS context still changes the GS state for those after it (in the
        // chain's order): kept in the view's list without draws.
        void stateOnly(const Ee &ee, uint32_t extra, bool opaque)
        {
            if (extra == 0u || !hookOrder())
                return;
            Drawn drawn;
            drawn.frame = g_frame;
            drawn.alphaList = !opaque;
            drawn.hasContext = contextAt(ee, extra, drawn.context);
            if (!drawn.hasContext)
                return;
            ViewList &list = g_lists[0];
            list.frame = g_frame;
            if (opaque)
                list.opaque.push_back(std::move(drawn));
            else
            {
                list.pendingAlpha = std::move(drawn);
                list.pending = true;
            }
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
            g_rdramForDump = rdram;
            const Ee ee{rdram, runtime->memory().getScratchpad()};
            const uint32_t fp = ee.u16(obj + 0xcu); // lh $fp, 0xC($s5): the flags' bits 32-47
            // TMB_NATIVE_MESHES_DUMP: the objects of the kinds other than the rigid strips, once each (a skinned one with
            // its per-instance chain and bones), for the offline comparison with the dump.
            if (static const bool s_dumpLog = std::getenv("TMB_NATIVE_MESHES_DUMP") != nullptr; s_dumpLog)
            {
                static std::unordered_map<uint32_t, bool> s_seen;
                if (!s_seen[obj] && (ee.u32(obj + 0x64u) != 0u || ((fp >> 5) & 3u) || ((fp >> 1) & 7u) || (fp & 0x1400u)))
                {
                    s_seen[obj] = true;
                    const uint32_t index = ee.u32(obj + 0x68u);
                    const bool skinned = ee.u32(obj + 0x64u) != 0u && inst != 0u;
                    if (skinned && g_firstSkinned == 0u)
                        g_firstSkinned = g_frame + 1u;
                    std::fprintf(stderr, "[tmb:mesh] dump object %#x inst %#x flags %#x skeleton %#x spr38 %#x index %u chain %#x bones %#x\n", obj, inst, fp,
                                 ee.u32(obj + 0x64u), ee.u32(spr + 0x38u), index, skinned ? ee.u32(ee.u32(inst + 0x18u) + 4u * index) : 0u,
                                 skinned ? ee.u32(ee.u32(inst + 0x14u) + 4u * index) : 0u);
                }
            }
            const bool opaque = getRegU32(ctx, 31) == kOpaqueReturn;
            if (!opaque)
                g_lists[g_building].pending = false; // until this one is drawn here

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
            // VU 31: a morphing object's weight (flag 42: UNPACK S-32 @31 of the float table's entry obj+0x3c, in every
            // lane), sent only with an instance; VU1 keeps the last one otherwise.
            if (((fp >> 10) & 1u) && haveTable)
            {
                const int32_t index = ee.s16(obj + 0x3cu);
                const float weight = ee.f32(table + 4u * static_cast<uint32_t>(index));
                const float vu31[4] = {weight, weight, weight, weight};
                g_vu.setQw(31, vu31);
            }
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
            // A skinned object (a skeleton at obj+0x64, and its instance): its bones' matrices where skeletonTraverse
            // wrote them, (*(inst + 0x14))[obj + 0x68] (the per-instance chain the header CALLs sends them to TOP).
            const bool skinned = ee.u32(obj + 0x64u) != 0u;
            const uint32_t boneLists = skinned && inst != 0u ? ee.u32(inst + 0x14u) : 0u;
            const uint32_t bones = boneLists != 0u ? ee.u32(boneLists + 4u * ee.u32(obj + 0x68u)) : 0u;
            const uint32_t pc = skinned && bones == 0u ? 0u : setupFor(fp, s6, pointLights, skinned);
            // An opaque object this path does not draw: a resume after it is left to the stream too.
            const Previous previous = g_previous;
            if (opaque)
                g_previous.valid = false;
            if (pc == 0u)
            {
                ++g_stats.program;
                stateOnly(ee, extra, opaque);
                return;
            }
            // The resume: the setup before it goes on (in the chain's order, which only the opaque objects keep), with
            // this header's VU 4-5 (and point lights).
            const bool resume = pc == 0x0c78u;
            // (Only after the rigid strip setups: the resume's way back into the others' loops is not known here.)
            const bool rigidBefore = previous.pc == 0x0cc8u || previous.pc == 0x0d08u || previous.pc == 0x0ce8u || previous.pc == 0x1700u;
            if (resume && (!opaque || !previous.valid || !rigidBefore))
            {
                ++g_stats.program;
                stateOnly(ee, extra, opaque);
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
            // The skid marks' object (skidBuffer, which SkidMarks::draw fills): drawn from the marks themselves.
            const bool skid = obj == kSkidBuffer && g_skid.valid && !resume;
            if (!skid && (!file || !file->file))
            {
                ++g_stats.noFile;
                static std::unordered_map<uint32_t, bool> s_seen;
                if (!s_seen[obj] && s_seen.size() < 40u)
                {
                    s_seen[obj] = true;
                    std::fprintf(stderr, "[tmb:mesh] object %#x (setup %#x, flags %#x, inst %#x, %s): outside the loaded NGP files (file %s)\n", obj, pc, fp,
                                 inst, opaque ? "opaque" : "alpha", tmb::textures::nameAt(obj).c_str());
                }
                stateOnly(ee, extra, opaque);
                return;
            }
            const uint32_t fileIndex = skid ? 0xffffu : file->index, fileBase = skid ? 0u : file->base;
            const uint32_t offset = obj - fileBase;
            const std::vector<PackObject> &objects = skid ? g_skid.objects : file->file->objects;
            const auto it = std::lower_bound(objects.begin(), objects.end(), offset, [](const PackObject &o, uint32_t v) { return o.offset < v; });
            if (it == objects.end() || it->offset != offset)
            {
                ++g_stats.noObject;
                stateOnly(ee, extra, opaque);
                return;
            }
            // The pack's kind as the flags pick the program: rigid or pool-less for the strip setups, sprite, ribbon.
            const uint32_t kindFor = pc == 0x3000u ? 2u : pc == 0x3270u ? 3u : skinned && !resume ? 4u : 1u;
            if (kindFor == 1u ? it->kind > 1u : it->kind != kindFor)
            {
                ++g_stats.kindOther;
                stateOnly(ee, extra, opaque);
                return;
            }
            // A resume draws a pool-less object with the pool (and setup) the object before it left: that object must be
            // its pool's (or another pool-less object of that pool).
            if (resume && (it->kind != 1u || previous.file != fileIndex ||
                           (it->poolFrom != previous.offset && it->poolFrom != previous.poolFrom)))
            {
                ++g_stats.resumeOther;
                stateOnly(ee, extra, opaque);
                return;
            }
            const uint32_t drawPc = resume ? previous.pc : pc;
            if (resume && drawPc == 0x1700u && !pointLights)
                for (uint32_t q = 0; q < 11u; ++q)
                    g_vu.setQw(165u + q, previous.pointLights[q]);

            // TMB_NATIVE_VIF=1 (the adapter follows the stream): the header's last qword, NOP, BASE, OFFSET, MSCAL
            // (pktAddVu1ObjAsm leaves the END tag after it, at the packet's write pointer), gets the mark in the NOP's
            // immediate. In the game's order (the default) nothing of the packet is read or written.
            bool marking = tmb::native::vifObserved();
            uint8_t *last = nullptr;
            uint32_t words[4] = {};
            if (marking)
            {
                const uint32_t writePointer = ee.u32(getRegU32(ctx, 4) + 8u);
                last = ee.at(writePointer - 0x10u);
                std::memcpy(words, last, 16);
                // pktCacheForAsm's setup codes are MSCALF (0x15).
                if (words[0] != 0u || (words[3] & ~0x01000000u) != (0x14000000u | (pc >> 3)))
                {
                    ++g_stats.unmarked;
                    static uint32_t s_logged = 0;
                    if (s_logged++ < 10u)
                        std::fprintf(stderr, "[tmb:mesh] object %#x: header end %08x %08x %08x %08x, expected NOP .. MSCAL %#x: left to the stream\n", obj,
                                     words[0], words[1], words[2], words[3], pc);
                    if (!hookOrder())
                        return;
                    marking = false; // the comparison: drawn in the hook order all the same, not compared
                }
            }

            Drawn drawn;
            drawn.alphaList = !opaque;
            // The traversal's GS context (s4), when this header brings it: a node's, in an NGP file (its writes from the
            // pack, recorded offline), or the game's default one (ctxGetDefaultGSCtx: the executable's, in EE memory).
            if (extra != 0u)
                drawn.hasContext = contextAt(ee, extra, drawn.context);
            drawn.setupPc = pc;
            drawn.resumes = resume ? drawPc : 0u;
            drawn.frame = g_frame;
            drawn.object = obj;
            drawn.file = fileIndex;
            drawn.offset = offset;
            // The setup's matrix: this object's, or (a resume) the one before it, which 0xc78 keeps (it runs 0x1700's
            // setup again, which makes it anew).
            float screen[16];
            if (resume && drawPc != 0x1700u)
                std::memcpy(screen, previous.screen, sizeof(screen));
            else
                tmb::vu1::objectToScreen(g_vu, screen, false);
            buildStrips(ee, obj, fileBase, *it, drawPc, drawn, screen, it->kind == 4u ? bones : 0u);
            g_stats.skid += skid ? 1u : 0u;
            if (opaque)
            {
                g_previous.valid = true;
                g_previous.file = fileIndex;
                g_previous.offset = offset;
                g_previous.poolFrom = it->kind == 1u ? it->poolFrom : offset;
                g_previous.pc = drawPc;
                std::memcpy(g_previous.screen, screen, sizeof(screen));
                std::memcpy(g_previous.pointLights, &g_vu.words[165u * 4u], sizeof(g_previous.pointLights));
            }
            const uint64_t hash = (static_cast<uint64_t>(fileIndex) << 32 | offset) * 0x9e3779b97f4a7c15ull;
            const uint32_t occurrence = g_keysThisFrame[hash]++;
            drawn.key = (hash ^ (static_cast<uint64_t>(occurrence) * 0xc2b2ae3d27d4eb4full)) | 1u;
            for (uint32_t q = 0; q < 22u; ++q)
                for (int k = 0; k < 4; ++k)
                    drawn.check[q][k] = g_vu.w(q, k);
            for (int k = 0; k < 4; ++k)
                drawn.check[22][k] = g_vu.w(31, k); // a morphing object's weight

            if (marking)
            {
                const uint32_t mark = g_nextMark;
                g_nextMark = (g_nextMark + 1u) & 0xffffffu;
                if (g_nextMark == 0u)
                    g_nextMark = 1u;
                // A VIF NOP (cmd 0, no interrupt) whatever its immediate and NUM. The resume's last qword is three NOPs
                // and the MSCAL: each carries the mark (the adapter keeps the last NOP's before the MSCAL).
                for (int k = 0; k < 3; ++k)
                    if (k == 0 || words[k] == 0u)
                        words[k] = mark;
                    else
                        break;
                std::memcpy(last, words, 12);
                drawn.mark = mark;
            }
            if (hookOrder())
            {
                ViewList &list = g_lists[g_building];
                ++g_built[g_building];
                list.frame = g_frame;
                if (marking) // the comparison: the adapter takes a copy at the mark (checked, not drawn)
                    g_drawn[drawn.mark] = drawn;
                if (opaque)
                    list.opaque.push_back(std::move(drawn));
                else
                {
                    list.pendingAlpha = std::move(drawn);
                    list.pending = true;
                }
            }
            else
                g_drawn[drawn.mark] = std::move(drawn);
            ++g_stats.drawn;
            g_stats.resumed += resume ? 1u : 0u;
            g_stats.sprites += pc == 0x3000u ? 1u : 0u;
            g_stats.ribbons += pc == 0x3270u ? 1u : 0u;
            g_stats.skinned += it->kind == 4u ? 1u : 0u;
            g_stats.reflective += pc == 0x1db8u ? 1u : 0u;
        }

        void pktAddVu1ObjOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (active())
            {
                ++g_stats.cCalls; // hierObjectNode's C path: left to the stream for now
                g_previous.valid = false;
                if (static const bool s_dumpLog = std::getenv("TMB_NATIVE_MESHES_DUMP") != nullptr; s_dumpLog)
                {
                    const Ee ee{rdram, runtime->memory().getScratchpad()};
                    const uint32_t obj = getRegU32(ctx, 6), inst = ee.u32(0x70000020u);
                    static std::unordered_map<uint32_t, bool> s_seen;
                    if (!s_seen[obj])
                    {
                        s_seen[obj] = true;
                        const uint32_t index = ee.u32(obj + 0x68u);
                        const bool skinned = ee.u32(obj + 0x64u) != 0u && inst != 0u;
                        std::fprintf(stderr, "[tmb:mesh] dump C object %#x inst %#x flags %#x skeleton %#x index %u chain %#x bones %#x\n", obj, inst,
                                     ee.u16(obj + 0xcu), ee.u32(obj + 0x64u), index, skinned ? ee.u32(ee.u32(inst + 0x18u) + 4u * index) : 0u,
                                     skinned ? ee.u32(ee.u32(inst + 0x14u) + 4u * index) : 0u);
                    }
                }
            }
            pktAddVu1Obj__FP9Vu1DmaPktP6QwDataP11_hierobjecti_0x1eadd0(rdram, ctx, runtime);
        }

        // pktLinkAlphaObjAsm links the alpha object pktAddVu1ObjAsm just built into the view's depth-sorted list, by the
        // depth in VU0 vf30.x: its bucket as it computes it.
        void pktLinkAlphaObjAsmOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (active() && hookOrder())
            {
                ViewList &list = g_lists[g_building];
                if (list.pending)
                {
                    float depth[4];
                    vf(ctx, 30, depth);
                    int32_t bucket = 1;
                    if (depth[0] > 8.0f)
                        bucket = FPU_CVT_W_S((depth[0] > 1024.0f ? 1016.0f : depth[0]) * 0.125f) + 1;
                    bucket = std::clamp(bucket, 0, 128);
                    list.buckets[static_cast<size_t>(bucket)].push_back(std::move(list.pendingAlpha));
                    list.pending = false;
                }
            }
            pktLinkAlphaObjAsm_0x1ec194(rdram, ctx, runtime);
        }

        // The draws of a view's chain in the order the chain sends them: the opaque objects, then the alpha list's
        // buckets from the farthest, each bucket's newest first.
        void submitList(ViewList &list)
        {
            if (list.empty())
            {
                list.clear();
                return;
            }
            tmb::native::applyViewClip();
            g_submitIndex = 0;
            const int which = g_inHier2pFlush ? 1 : 0; // hier__Fi's, hier2p's
            ++g_kicks[which];
            g_submitted[which] += list.opaque.size();
            for (const std::vector<Drawn> &b : list.buckets)
                g_submitted[which] += b.size();
            for (const Drawn &drawn : list.opaque)
                submit(drawn);
            for (size_t b = list.buckets.size(); b-- > 0;)
                for (size_t i = list.buckets[b].size(); i-- > 0;)
                    submit(list.buckets[b][i]);
            list.clear();
        }

        // A view built and not kicked: the menus' (sent with the frame) now; hier2p's never sent by the game, dropped.
        void sendUnkicked(ViewList &list)
        {
            if (list.empty())
                return;
            if (list.hier2p)
            {
                ++g_listDrops;
                list.clear();
                return;
            }
            ++g_unsentFlushes;
            submitList(list);
        }

        // hier__Fi, its chain built (the alpha list attached when it has objects) and queued, asks hierCheckVifComplete
        // whether VIF1 is free: kicked now (hierFlushObjQ) or by hierDmaHandler when the transfer before it ends, but in
        // this order either way. Its view's draws are submitted there. (Not at hier__Fi's return: a recompiled function
        // can return to the dispatcher before its end. Not at hierFlushObjQ: the DMA handler calls it too.) A view with
        // no object kicks nothing.
        void hierCheckVifCompleteOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (active() && hookOrder() && getRegU32(ctx, 31) == kHierKickReturn)
                submitList(g_lists[0]);
            hierCheckVifComplete__Fv_0x1d59d8(rdram, ctx, runtime);
        }

        void hier2pFlushDmaOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            g_inHier2pFlush = true;
            if (active() && hookOrder())
                submitList(g_lists[0]); // what hier2p built, at its view's turn (nothing is built in between)
            g_inHier2pFlush = false;
            hier2pFlushDma__Fv_0x1d3b00(rdram, ctx, runtime);
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
            g_previous.valid = false;
            // A view before it whose chain hier__Fi did not kick (the menus send theirs with the frame): its objects first.
            if (hookOrder())
            {
                sendUnkicked(g_lists[0]);
                g_lists[0].hier2p = getRegU32(ctx, 31) == kHier2pCacheReturn;
            }
        }
    }

    bool enabled() { return active(); }

    // The game's order unless the adapter follows the stream (TMB_NATIVE_VIF=1), where TMB_NATIVE_MESHES_CHECK=1 keeps
    // it to compare the two.
    bool hookOrder()
    {
        return !tmb::native::vifObserved() || checking();
    }

    void streamReached(uint32_t mark, uint64_t alpha, uint64_t test, bool depthMasked)
    {
        CheckRecord &record = g_checks[mark];
        record.streamOrder = g_streamSeq++;
        record.stream = GsState{alpha, test, depthMasked};
        record.streamSeen = true;
        if (record.frame == 0u)
            record.frame = g_frame;
    }

    namespace
    {
        // TMB_NATIVE_VIF=1 with TMB_NATIVE_MESHES_CHECK=1, at a frame's end: the objects of the frame before, the hook's
        // order and materials against the stream's.
        void compareOrders()
        {
            static uint64_t s_objects = 0, s_unreached = 0, s_unsubmitted = 0, s_orderDiffs = 0, s_matDiffs = 0, s_batches = 0;
            std::vector<std::pair<uint64_t, uint32_t>> hook, stream;
            std::vector<uint32_t> done;
            for (auto &[mark, record] : g_checks)
            {
                if (g_frame - record.frame < 2u)
                    continue;
                done.push_back(mark);
                if (record.hookOrder == ~0ull)
                {
                    ++s_unsubmitted;
                    continue;
                }
                ++s_objects;
                if (!record.streamSeen)
                {
                    static uint32_t s_logged = 0;
                    if (s_logged++ < 30u)
                        std::fprintf(stderr, "[tmb:order] mark %u: object %#x (%s, #%u in its view, frame %u) not reached in the stream\n", mark,
                                     record.object, record.alphaList ? "alpha" : "opaque", record.index, record.frame);
                    ++s_unreached;
                    continue;
                }
                hook.push_back({record.hookOrder, mark});
                stream.push_back({record.streamOrder, mark});
                // The stream's materials: its state at the mark, then the object's writes.
                GsState gs = record.stream;
                size_t k = 0;
                bool differs = false;
                for (size_t i = 0; i < record.sets.size(); ++i)
                {
                    const Batch &sets = record.sets[i];
                    apply(gs, sets);
                    if (i == 0u && record.hasContext)
                        continue;
                    if (!sets.textured) // draws nothing
                        continue;
                    ++s_batches;
                    const Mat m = matFor(gs, sets);
                    if (k < record.hook.size() && !(record.hook[k] == m))
                    {
                        differs = true;
                        static uint32_t s_logged = 0;
                        if (s_logged++ < 40u)
                            std::fprintf(stderr, "[tmb:order] mark %u batch %zu: hook alpha %#llx test %#llx zmsk %d, stream alpha %#llx test %#llx zmsk %d (context %d; object %#x, %s, #%u in its view)\n",
                                         mark, k, static_cast<unsigned long long>(record.hook[k].alpha), static_cast<unsigned long long>(record.hook[k].test),
                                         record.hook[k].depthMasked ? 1 : 0, static_cast<unsigned long long>(m.alpha), static_cast<unsigned long long>(m.test),
                                         m.depthMasked ? 1 : 0, record.hasContext ? 1 : 0, record.object, record.alphaList ? "alpha" : "opaque",
                                         record.index);
                    }
                    ++k;
                }
                s_matDiffs += differs ? 1u : 0u;
            }
            for (uint32_t mark : done)
                g_checks.erase(mark);
            std::sort(hook.begin(), hook.end());
            std::sort(stream.begin(), stream.end());
            for (size_t i = 0; i < hook.size(); ++i)
                if (hook[i].second != stream[i].second)
                {
                    ++s_orderDiffs;
                    static uint32_t s_logged = 0;
                    if (s_logged++ < 20u)
                        std::fprintf(stderr, "[tmb:order] position %zu of %zu: hook mark %u, stream mark %u\n", i, hook.size(), hook[i].second, stream[i].second);
                }
            static uint32_t s_frames = 0;
            if (++s_frames % 250u == 0u)
                std::fprintf(stderr, "[tmb:order] %llu objects submitted at the hook: %llu not reached in the stream, %llu out of the stream's order, %llu with a "
                                     "material differing (%llu batches); %llu marked, never submitted\n",
                             static_cast<unsigned long long>(s_objects), static_cast<unsigned long long>(s_unreached), static_cast<unsigned long long>(s_orderDiffs),
                             static_cast<unsigned long long>(s_matDiffs), static_cast<unsigned long long>(s_batches), static_cast<unsigned long long>(s_unsubmitted));
        }
    }

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
    void flushUnsent()
    {
        if (!active() || !hookOrder())
            return;
        sendUnkicked(g_lists[0]);
    }

    void endFrame()
    {
        if (!active())
            return;
        ++g_frame;
        g_keysThisFrame.clear();
        if (checking() && tmb::native::vifObserved())
            compareOrders();
        // A view built and never kicked (its chain not sent): dropped.
        for (ViewList &list : g_lists)
            if (!list.empty() && g_frame - list.frame > 2u)
            {
                ++g_stats.unclaimed;
                list.clear();
            }
        for (auto it = g_drawn.begin(); it != g_drawn.end();)
            if (g_frame - it->second.frame > 2u)
            {
                ++g_stats.unclaimed;
                it = g_drawn.erase(it);
            }
            else
                ++it;
        // TMB_NATIVE_MESHES_DUMP=<file>,<frame>: EE memory at that frame (a diagnostic, to compare the loaded NGP
        // images with the disc's offline).
        if (static const char *s_dump = std::getenv("TMB_NATIVE_MESHES_DUMP"); s_dump && g_rdramForDump)
        {
            static std::string s_path;
            static uint32_t s_at = [&]
            {
                std::string spec(s_dump);
                const size_t comma = spec.rfind(',');
                s_path = spec.substr(0, comma);
                return comma == std::string::npos ? 0u : static_cast<uint32_t>(std::strtoul(spec.c_str() + comma + 1, nullptr, 10));
            }();
            if (s_at == 0u && g_firstSkinned != 0u)
                s_at = g_firstSkinned + 300u; // frame 0: 300 frames after the first skinned object
            if (g_frame == s_at)
                if (FILE *f = std::fopen(s_path.c_str(), "wb"))
                {
                    std::fwrite(g_rdramForDump, 1, 0x2000000u, f);
                    std::fclose(f);
                    std::fprintf(stderr, "[tmb:mesh] EE memory dumped to %s at frame %u\n", s_path.c_str(), g_frame);
                }
        }
        if (g_frame % 250u == 0u)
        {
            if (hookOrder())
                std::fprintf(stderr, "[tmb:mesh] per 250 frames, the game's order: %llu objects built; views kicked by hier__Fi %llu (%llu objects), by "
                                     "hier2pFlushDma %llu (%llu objects), sent unkicked %llu; %llu dropped\n",
                             static_cast<unsigned long long>(g_built[0]), static_cast<unsigned long long>(g_kicks[0]),
                             static_cast<unsigned long long>(g_submitted[0]), static_cast<unsigned long long>(g_kicks[1]),
                             static_cast<unsigned long long>(g_submitted[1]), static_cast<unsigned long long>(g_unsentFlushes),
                             static_cast<unsigned long long>(g_listDrops));
            g_listDrops = g_built[0] = g_built[1] = g_kicks[0] = g_kicks[1] = g_submitted[0] = g_submitted[1] = g_unsentFlushes = 0;
            std::fprintf(stderr,
                         "[tmb:mesh] per 250 frames: %llu objects at the hook (%llu by the C path), %llu drawn from the pack (%llu triangles), "
                         "%llu taken in the stream, %llu unclaimed; left to the stream: %llu by program (%llu drawn with a group GS-state block), %llu outside "
                         "the loaded files, %llu not in the pack, %llu of another kind in the pack, %llu unmarked; %llu texture number mismatches; "
                         "%llu resumes drawn, %llu resumes not after their pool's object; %llu sprites, %llu ribbons, %llu skinned, %llu environment-mapped, "
                         "%llu morphing drawn, %llu skid marks' objects; GS contexts: %llu from the pack, %llu the game's default, %llu not in the pack, %llu differing from EE memory\n",
                         static_cast<unsigned long long>(g_stats.calls), static_cast<unsigned long long>(g_stats.cCalls),
                         static_cast<unsigned long long>(g_stats.drawn), static_cast<unsigned long long>(g_stats.triangles),
                         static_cast<unsigned long long>(g_stats.taken), static_cast<unsigned long long>(g_stats.unclaimed),
                         static_cast<unsigned long long>(g_stats.program), static_cast<unsigned long long>(g_stats.extra),
                         static_cast<unsigned long long>(g_stats.noFile), static_cast<unsigned long long>(g_stats.noObject),
                         static_cast<unsigned long long>(g_stats.kindOther), static_cast<unsigned long long>(g_stats.unmarked),
                         static_cast<unsigned long long>(g_stats.idMismatch), static_cast<unsigned long long>(g_stats.resumed),
                         static_cast<unsigned long long>(g_stats.resumeOther), static_cast<unsigned long long>(g_stats.sprites),
                         static_cast<unsigned long long>(g_stats.ribbons), static_cast<unsigned long long>(g_stats.skinned),
                         static_cast<unsigned long long>(g_stats.reflective), static_cast<unsigned long long>(g_stats.morphed),
                         static_cast<unsigned long long>(g_stats.skid), static_cast<unsigned long long>(g_stats.contextsPack), static_cast<unsigned long long>(g_stats.contextsDefault),
                         static_cast<unsigned long long>(g_stats.contextsMissing), static_cast<unsigned long long>(g_stats.contextsDiffering));
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
        runtime.registerFunction(kPktLinkAlphaObjAsm, &pktLinkAlphaObjAsmOverride);
        runtime.registerFunction(kHierCheckVifComplete, &hierCheckVifCompleteOverride);
        runtime.registerFunction(kSkidDraw, &skidDrawOverride);
        runtime.registerFunction(kHier2pFlushDma, &hier2pFlushDmaOverride);
        std::fprintf(stderr, "[tmb:mesh] TMB_NATIVE_MESHES=1: hier objects drawn from the mesh pack at pktAddVu1ObjAsm\n");
    }
}
