// TMB's textures for the native renderer, from the native texture pack converted offline from the disc's files
// (tools/tmb_texture_pack.py, scripts/recomp.sh): nothing is read from the game's texture records, GS packets or GS
// memory while the game runs. The runtime only follows TMB's texture code, at hooks:
// - which disc file goes where: fileReadf / zipInflateAll (name, destination), then fileOnlyTexFile /
//   fileAddTexFile / fileOnlyResFile / fileAddResFile (destination) list the level's .TEX and .RTX files in order;
// - texmInit sets up each .TEX record in order (texmSetupTexture: its texture id, width, height, format): the n-th
//   setup is the pack's n-th record of those files, so a texture id names a pack record;
// - texmLoadResTexture loads each .RTX record in order (sceGsSetDefLoadImage: its VRAM block, format, size): a palette's or
//   an H texture's VRAM block names a pack record;
// - fontInit: the font's VRAM blocks (the pack's FONT file).
// A draw's TEX0 then finds its texture: TBP0 -> the texture id the game's own table (texInfo: TBP | ... | resident
// << 31 per id, word 1 of 4) has there, CBP -> the palette loaded at that block.
//
// Each hook checks the pack record against the game's own arguments (format, size) and reports a mismatch.
//
// The pack's container, its loader and the decoders (PSM, CSM1 palettes, TEXA alpha) are the fork's
// (runtime/native/native_texture_pack.h, tools/native_pack/texture.py); what is TMB's here is the key scheme above.

#include "tmb_port.h"
#include "bozo/render/texture.h"
#include "bozo/render/texture_pack.h"
#include "tmb_meshes.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef TMB_TEXTURE_PACK
#define TMB_TEXTURE_PACK "tmb_textures.pack"
#endif

namespace tmb::textures
{
    namespace
    {
        using Texture = bozo::render::Texture;

        constexpr uint32_t kTexInfo = 0x004D1930u; // texInfo[1000], 16 bytes each
        constexpr uint32_t kFileReadf = 0x001C21D0u;
        constexpr uint32_t kZipInflateAll = 0x00200A20u;
        constexpr uint32_t kFileOnlyTexFile = 0x001C2B60u;
        constexpr uint32_t kFileAddTexFile = 0x001C2940u;
        constexpr uint32_t kFileOnlyResFile = 0x001C2BA8u;
        constexpr uint32_t kFileAddResFile = 0x001C29D8u;
        constexpr uint32_t kTexmInit = 0x001F8868u;
        constexpr uint32_t kTexmSetupTexture = 0x001F8F60u;
        constexpr uint32_t kTexmResInit = 0x001F88F8u;
        constexpr uint32_t kSceGsSetDefLoadImage = 0x00203F30u;
        constexpr uint32_t kTexmLoadResTexture = 0x001F89B8u;
        constexpr uint32_t kResLoadReturn = 0x001F8AB4u; // texmLoadResTexture's call of sceGsSetDefLoadImage
        constexpr uint32_t kFontInit = 0x001C3E48u;

        using bozo::render::packs::psmIndexed;
        using bozo::render::packs::psmIsH;
        using Record = bozo::render::packs::TextureRecord;
        bool isH(uint32_t psm) { return psmIsH(psm); }
        bool indexed(uint32_t psm) { return psmIndexed(psm); }

        struct Store
        {
            std::mutex mutex;
            bool loaded = false;
            bozo::render::packs::TexturePack pack; // file name -> its records in order
            std::unordered_map<uint32_t, std::string> nameAt;          // destination -> the disc file read there
            std::vector<std::string> texFiles, resFiles;               // the level's, in the game's order
            uint32_t texOrdinal = 0, resOrdinal = 0;
            std::unordered_map<uint32_t, const Record *> byId;    // texture id -> .TEX record
            // VRAM block -> the .RTX record loaded there, and where in the block (its first texel, as loaded)
            struct Placement
            {
                const Record *record = nullptr;
                uint32_t x = 0, y = 0;
            };
            std::unordered_map<uint32_t, Placement> byBlock;             // palettes
            std::unordered_map<uint64_t, const Record *> hAt;           // (block << 8 | psm) -> H texture
            std::vector<uint16_t> idAt = std::vector<uint16_t>(16384u, 0xffffu); // TBP -> texture id (texInfo, refresh)
            uint32_t fontTbp = ~0u;
            std::unordered_map<uint64_t, std::shared_ptr<const Texture>> decoded;
            uint64_t decodes = 0;
            const uint8_t *rdram = nullptr;
        };
        Store &store()
        {
            static Store s_store;
            return s_store;
        }

        void loadPack(Store &s)
        {
            if (s.loaded)
                return;
            s.loaded = true;
            const std::string pathText = tmb::meshes::packPath("TMB_TEXTURE_PACK", TMB_TEXTURE_PACK);
            const char *path = pathText.c_str();
            if (!s.pack.load(path))
            {
                std::fprintf(stderr, "[tmb:tex] no texture pack at %s (tools/tmb_texture_pack.py): the native draws are untextured\n", path);
                return;
            }
            std::fprintf(stderr, "[tmb:tex] texture pack %s: %zu files, %zu records\n", path, s.pack.fileCount(), s.pack.recordCount());
        }

        // "cdrom0:\LVL\JUNKYARD.TEX;1" -> "JUNKYARD.TEX"
        std::string baseName(const uint8_t *rdram, uint32_t address)
        {
            std::string name;
            for (uint32_t i = 0; i < 256u; ++i)
            {
                const char c = static_cast<char>(rdram[(address + i) & 0x1ffffffu]);
                if (c == '\0' || c == ';')
                    break;
                if (c == '\\' || c == '/' || c == ':')
                    name.clear();
                else
                    name.push_back(static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c));
            }
            return name;
        }

        // The n-th record of the listed files.
        const Record *nth(Store &s, const std::vector<std::string> &files, uint32_t n)
        {
            return s.pack.nth(files, n);
        }

        void mismatch(const char *what, uint32_t n, const Record *r, uint32_t psm, uint32_t width, uint32_t height)
        {
            static uint32_t s_reported = 0;
            if (s_reported++ < 20u)
                std::fprintf(stderr, "[tmb:tex] %s #%u: the game sets up psm %#x %ux%u, the pack has %s\n", what, n, psm, width,
                             height, r ? (std::to_string(r->psm) + " " + std::to_string(r->width) + "x" + std::to_string(r->height)).c_str() : "nothing");
        }

        void noteFile(uint8_t *rdram, R5900Context *ctx)
        {
            Store &s = store();
            std::lock_guard<std::mutex> lock(s.mutex);
            s.rdram = rdram;
            s.nameAt[getRegU32(ctx, 5)] = baseName(rdram, getRegU32(ctx, 4));
        }
        void fileReadfOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            noteFile(rdram, ctx);
            fileReadf__FPcPv_0x1c21d0(rdram, ctx, runtime);
        }
        void zipInflateAllOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            noteFile(rdram, ctx);
            zipInflateAll__FPcPv_0x200a20(rdram, ctx, runtime);
        }

        void listFile(R5900Context *ctx, bool tex, bool only)
        {
            Store &s = store();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::vector<std::string> &files = tex ? s.texFiles : s.resFiles;
            if (only)
                files.clear();
            // A new list of .TEX files: its records are set up from the first. The shell sets up its textures without
            // texmInit (back from a match the count would go on from the level's, past the shell's records).
            if (only && tex)
            {
                s.texOrdinal = 0;
                s.byId.clear();
                s.decoded.clear();
            }
            const auto it = s.nameAt.find(getRegU32(ctx, 4));
            files.push_back(it != s.nameAt.end() ? it->second : std::string("?"));
        }
        void fileOnlyTexFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            listFile(ctx, true, true);
            fileOnlyTexFile__FPci_0x1c2b60(rdram, ctx, runtime);
        }
        void fileAddTexFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            listFile(ctx, true, false);
            fileAddTexFile__FPci_0x1c2940(rdram, ctx, runtime);
        }
        void fileOnlyResFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            listFile(ctx, false, true);
            fileOnlyResFile__FPci_0x1c2ba8(rdram, ctx, runtime);
        }
        void fileAddResFileOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            listFile(ctx, false, false);
            fileAddResFile__FPci_0x1c29d8(rdram, ctx, runtime);
        }

        void texmInitOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                loadPack(s);
                s.rdram = rdram;
                s.texOrdinal = 0;
                s.byId.clear();
                s.decoded.clear();
            }
            texmInit__F10_vramAddrs_0x1f8868(rdram, ctx, runtime);
            Store &s = store();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::string files;
            for (const std::string &f : s.texFiles)
                files += " " + f;
            std::fprintf(stderr, "[tmb:tex] texmInit: %u textures set up from%s\n", s.texOrdinal, files.c_str());
        }

        // texmSetupTexture(id, width, height, psm, ...): once per .TEX record, in order.
        void texmSetupTextureOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                const uint32_t id = getRegU32(ctx, 4) & 0xffffu, width = getRegU32(ctx, 5), height = getRegU32(ctx, 6),
                               psm = getRegU32(ctx, 7) & 0x3fu;
                const uint32_t n = s.texOrdinal++;
                const Record *r = nth(s, s.texFiles, n);
                if (n == 0u)
                    std::fprintf(stderr, "[tmb:tex] texmSetupTexture: the first of %zu files (id %u, psm %#x %ux%u)\n", s.texFiles.size(), id, psm,
                                 width, height);
                if (r && r->psm == psm && r->width == width && r->height == height)
                    s.byId[id] = r;
                else
                    mismatch("texture", n, r, psm, width, height);
            }
            texmSetupTexture__FiiiiiP6QwDataii_0x1f8f60(rdram, ctx, runtime);
        }

        void texmResInitOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                loadPack(s);
                s.rdram = rdram;
                s.resOrdinal = 0;
                s.byBlock.clear();
                s.hAt.clear();
                s.decoded.clear();
            }
            texmResInit__F10_vramAddrsP6QwData_0x1f88f8(rdram, ctx, runtime);
            Store &s = store();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::string files;
            for (const std::string &f : s.resFiles)
                files += " " + f;
            std::fprintf(stderr, "[tmb:tex] texmResInit: %u palettes and resident textures loaded from%s\n", s.resOrdinal, files.c_str());
        }

        // The shell calls it itself when texmResInit's check (texmResCheckForData) found nothing to load yet; texmResInit
        // tail-jumps into it (a direct call: its records are counted from texmResInit's override).
        void texmLoadResTextureOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                loadPack(s);
                s.rdram = rdram;
                s.resOrdinal = 0;
                s.byBlock.clear();
                s.hAt.clear();
                s.decoded.clear();
            }
            texmLoadResTexture__FP6QwData_0x1f89b8(rdram, ctx, runtime);
            Store &s = store();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::fprintf(stderr, "[tmb:tex] texmLoadResTexture: %u palettes and resident textures loaded\n", s.resOrdinal);
        }

        // sceGsSetDefLoadImage(image, dbp, dbw, dpsm, x, y, w, h), called by texmLoadResTexture (its return address)
        // once per .RTX record, in order.
        void sceGsSetDefLoadImageOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                if (getRegU32(ctx, 31) == kResLoadReturn)
                {
                    const uint32_t block = getRegU32(ctx, 5) & 0x3fffu, psm = getRegU32(ctx, 7) & 0x3fu;
                    const uint32_t width = getRegU32(ctx, 10) & 0xffffu, height = getRegU32(ctx, 11) & 0xffffu;
                    const uint32_t n = s.resOrdinal++;
                    const Record *r = nth(s, s.resFiles, n);
                    if (r && r->psm == psm && r->width == width && r->height == height)
                    {
                        if (isH(psm)) // T8H / T4HL / T4HH share their blocks' words (top byte, its low / high nibble)
                            s.hAt[(static_cast<uint64_t>(block) << 8) | psm] = r;
                        else
                            s.byBlock[block] = {r, getRegU32(ctx, 8) & 0x7ffu, getRegU32(ctx, 9) & 0x7ffu};
                    }
                    else
                        mismatch("resident record", n, r, psm, width, height);
                }
            }
            bozo::port::callLibrary(runtime, ctx, kSceGsSetDefLoadImage); // the library (BozoEngine's), not the game's copy
        }

        void fontInitOverride(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                Store &s = store();
                std::lock_guard<std::mutex> lock(s.mutex);
                loadPack(s);
                s.fontTbp = static_cast<uint32_t>((static_cast<int32_t>(getRegU32(ctx, 4)) - 0x840) / 64);
            }
            fontInit__F10_vramAddrs_0x1c3e48(rdram, ctx, runtime);
        }

        // A record's entry as RGBA with the GS's alpha (0x80 is 1.0), as bozo::render::TextureCache gives it.
        uint32_t colour(const Record &r, uint32_t index, uint64_t texa)
        {
            return bozo::render::packs::recordColour(r, index, texa);
        }
    }

    // TBP -> texture id, from the game's table as it left it after building the frame the adapter reads (called at
    // each chain the adapter reads, before its draws).
    void refresh(const uint8_t *rdram)
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        if (rdram)
            s.rdram = rdram;
        if (!s.rdram)
            return;
        std::fill(s.idAt.begin(), s.idAt.end(), uint16_t{0xffff});
        const uint8_t *table = s.rdram + kTexInfo;
        for (uint32_t id = 0; id < 1000u; ++id)
        {
            uint32_t info;
            std::memcpy(&info, table + id * 16u + 4u, 4);
            if ((info >> 31) != 0u)
                s.idAt[info & 0x3fffu] = static_cast<uint16_t>(id);
        }
    }

    namespace
    {
        // The texture TEX0 shows, its texels found by the TBP (texInfo) or, with id set, by that texture id (the
        // number a hierobject keeps at +0x50, texmSetupTexture's id); the palette by CBP either way.
        std::shared_ptr<const Texture> lookupLocked(Store &s, uint64_t tex0, uint64_t texa, uint32_t id)
        {
            const uint32_t tbp = tex0 & 0x3fffu, psm = (tex0 >> 20) & 0x3fu;
            const uint32_t cbp = (tex0 >> 37) & 0x3fffu, csa = (tex0 >> 56) & 0x1fu;
            const uint32_t width = 1u << std::min<uint32_t>(10u, (tex0 >> 26) & 0xfu);
            const uint32_t height = 1u << std::min<uint32_t>(10u, (tex0 >> 30) & 0xfu);

            const Record *texels = nullptr, *palette = nullptr;
            const std::vector<Record> *font = s.pack.file("FONT");
            const Record *fontPalette = font && font->size() > 1u ? &(*font)[1] : nullptr;
            if (tbp == s.fontTbp && font)
            {
                texels = &(*font)[0];
                palette = fontPalette;
            }
            else if (isH(psm))
            {
                const auto it = s.hAt.find((static_cast<uint64_t>(tbp) << 8) | psm);
                texels = it != s.hAt.end() ? it->second : nullptr;
            }
            else if (id != ~0u)
            {
                const auto it = s.byId.find(id);
                texels = it != s.byId.end() && it->second->psm == psm ? it->second : nullptr;
            }
            else if (s.idAt[tbp] != 0xffffu)
            {
                const auto it = s.byId.find(s.idAt[tbp]);
                texels = it != s.byId.end() && it->second->psm == psm ? it->second : nullptr;
            }
            // A palette entry at (px, py) of the CLUT area at CBP (CSM1, CT32 / CT16 blocks of 8 x 8, the second column of
            // blocks at block + 1, the second row at + 2): from the record loaded over it.
            const auto paletteEntry = [&](uint32_t px, uint32_t py, uint32_t &out)
            {
                for (int pass = 0; pass < 2; ++pass)
                {
                    const uint32_t block = pass == 0 ? cbp : cbp + (px >> 3) + ((py >> 3) << 1);
                    const uint32_t lx = pass == 0 ? px : (px & 7u), ly = pass == 0 ? py : (py & 7u);
                    const auto it = s.byBlock.find(block);
                    if (it == s.byBlock.end() || !it->second.record)
                        continue;
                    const Store::Placement &p = it->second;
                    if (lx >= p.x && ly >= p.y && lx - p.x < p.record->width && ly - p.y < p.record->height)
                    {
                        out = colour(*p.record, (ly - p.y) * p.record->width + (lx - p.x), texa);
                        return true;
                    }
                }
                return false;
            };
            bool paletteFound = palette != nullptr;
            if (texels && indexed(psm) && !palette)
            {
                uint32_t ignored;
                for (uint32_t i = 0; i < 16u && !paletteFound; ++i)
                {
                    uint32_t px = 0, py = 0;
                    bozo::render::csm1Position(bozo::render::packs::psmEightBit(psm) ? i : (csa * 16u + i) & 0xffu, px, py);
                    paletteFound = paletteEntry(px, py, ignored);
                }
                if (const auto it = s.byBlock.find(cbp); it != s.byBlock.end())
                    palette = it->second.record;
            }
            if (!texels || (indexed(psm) && !paletteFound))
            {
                static std::unordered_map<uint64_t, bool> s_reported;
                const uint64_t reportKey = tex0 & ~(uint64_t{7} << 61);
                if (!s_reported[reportKey])
                {
                    s_reported[reportKey] = true;
                    std::fprintf(stderr, "[tmb:tex] TEX0 %#llx (tbp %u psm %#x %ux%u cbp %u): no %s set up there\n",
                                 static_cast<unsigned long long>(tex0), tbp, psm, width, height, cbp, texels ? "palette" : "texture");
                }
                return nullptr;
            }

            const uint64_t key = (reinterpret_cast<uintptr_t>(texels) * 0x9e3779b97f4a7c15ull) ^
                                 (reinterpret_cast<uintptr_t>(palette) * 0xc2b2ae3d27d4eb4full) ^
                                 (tex0 & ~((uint64_t{3} << 35) | (uint64_t{7} << 61))) ^ (texa * 0x165667b19e3779f9ull);
            if (const auto found = s.decoded.find(key); found != s.decoded.end())
                return found->second;

            uint32_t colours[256] = {};
            if (indexed(psm))
                bozo::render::packs::csm1Palette(psm, csa,
                                                 [&](uint32_t px, uint32_t py, uint32_t &out)
                                                 {
                                                     if (palette && palette == fontPalette)
                                                     {
                                                         if (px < palette->width && py < palette->height)
                                                             out = colour(*palette, py * palette->width + px, texa);
                                                     }
                                                     else
                                                         paletteEntry(px, py, out);
                                                 },
                                                 colours);

            Texture texture;
            texture.key = key;
            texture.version = ++s.decodes;
            bozo::render::packs::decodeRecord(texture, width, height, psm, *texels, colours, texa);
            // A texture with no texel showing: reported once (a palette or texels taken from the wrong record).
            if (std::all_of(texture.rgba.begin(), texture.rgba.end(), [](uint32_t c) { return (c >> 24) == 0u; }))
            {
                static std::unordered_map<uint64_t, bool> s_reported;
                if (!s_reported[tex0])
                {
                    s_reported[tex0] = true;
                    std::fprintf(stderr, "[tmb:tex] TEX0 %#llx (tbp %u psm %#x %ux%u cbp %u csa %u): every texel transparent (texels %ux%u psm %#x, palette %ux%u psm %#x)\n",
                                 static_cast<unsigned long long>(tex0), tbp, psm, width, height, cbp, csa, texels->width, texels->height,
                                 texels->psm, palette ? palette->width : 0u, palette ? palette->height : 0u, palette ? palette->psm : 0u);
                }
            }
            auto shared = std::make_shared<const Texture>(std::move(texture));
            s.decoded[key] = shared;
            return shared;
        }
    }

    // The texture a draw's TEX0 shows (null when the game set up none there: reported once).
    std::shared_ptr<const Texture> lookup(uint64_t tex0, uint64_t texa)
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        return lookupLocked(s, tex0, texa, ~0u);
    }

    // The same with the texels taken by texture id (a hierobject's texture number, +0x50), not from the TBP.
    std::shared_ptr<const Texture> lookupId(uint32_t id, uint64_t tex0, uint64_t texa)
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        return lookupLocked(s, tex0, texa, id);
    }

    // The texture id texInfo gives the TBP now (0xffff: none), to check a lookup by id against it.
    uint32_t idAtTbp(uint32_t tbp)
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        return s.idAt[tbp & 0x3fffu];
    }

    // The disc file fileReadf / zipInflateAll last read to this address ("" if none): the mesh pack's NGP binding.
    std::string nameAt(uint32_t address)
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        const auto it = s.nameAt.find(address);
        return it != s.nameAt.end() ? it->second : std::string();
    }

    // The font's texture (the pack's FONT, at the VRAM block fontInit gave it), for the 2D rewritten natively.
    std::shared_ptr<const Texture> font()
    {
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        loadPack(s);
        static std::shared_ptr<const Texture> s_font;
        const std::vector<Record> *records = s.pack.file("FONT");
        if (s_font || !records || records->size() < 2u)
            return s_font;
        const Record &texels = (*records)[0], &palette = (*records)[1];
        Texture texture;
        texture.width = texels.width;
        texture.height = texels.height;
        texture.key = 0xf0f7f0f7u;
        texture.version = ++s.decodes;
        texture.rgba.resize(static_cast<size_t>(texels.width) * texels.height);
        for (size_t i = 0; i < texture.rgba.size(); ++i)
        {
            uint32_t px = 0, py = 0;
            bozo::render::csm1Position(texels.texels[i] & 0xfu, px, py);
            texture.rgba[i] = px < palette.width && py < palette.height ? colour(palette, py * palette.width + px, 0u) : 0u;
        }
        s_font = std::make_shared<const Texture>(std::move(texture));
        return s_font;
    }

    void install(tmb::Registry &runtime)
    {
        runtime.registerFunction(kFileReadf, &fileReadfOverride);
        runtime.registerFunction(kZipInflateAll, &zipInflateAllOverride);
        runtime.registerFunction(kFileOnlyTexFile, &fileOnlyTexFileOverride);
        runtime.registerFunction(kFileAddTexFile, &fileAddTexFileOverride);
        runtime.registerFunction(kFileOnlyResFile, &fileOnlyResFileOverride);
        runtime.registerFunction(kFileAddResFile, &fileAddResFileOverride);
        runtime.registerFunction(kTexmInit, &texmInitOverride);
        runtime.registerFunction(kTexmSetupTexture, &texmSetupTextureOverride);
        runtime.registerFunction(kTexmResInit, &texmResInitOverride);
        runtime.registerFunction(kTexmLoadResTexture, &texmLoadResTextureOverride);
        runtime.registerFunction(kSceGsSetDefLoadImage, &sceGsSetDefLoadImageOverride);
        runtime.registerFunction(kFontInit, &fontInitOverride);
    }
}
