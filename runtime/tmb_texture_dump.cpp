// The texture dump (tmb_texture_dump.h). A development tool for building texture replacement packs from the player's
// own disc: the PNGs it writes are the player's, made on their PC, and never part of the repository or a release.
//
// Each file is <identity as 16 hex digits>.png: the texture exactly as the renderer gets it, alpha included as the GS's
// (128 is opaque, up to 255 = 2.0, which most of TMB's palettes use), so the PNG round-trips to the same identity.
// Tools that want straight alpha double it (and must keep the values above 128). index.tsv gets one line per new
// file: identity, width, height, psm, the records it was decoded from (the palette: the one at CBP), tex0, texa, the
// texels' hash and the largest alpha.

#include "tmb_texture_dump.h"

#include "bozo/hash.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace tmb::textures::dump
{
    namespace
    {
        struct State
        {
            std::mutex mutex;
            bool resolved = false;
            std::filesystem::path folder; // empty: off
            std::unordered_set<uint64_t> written;
            uint64_t files = 0;
        };
        State &state()
        {
            static State s_state;
            return s_state;
        }

        void resolve(State &s)
        {
            if (s.resolved)
                return;
            s.resolved = true;
            const char *value = std::getenv("TMB_TEXTURE_DUMP");
            if (!value || !*value || std::string(value) == "0")
                return;
            if (std::string(value) == "1")
            {
                const char *user = std::getenv("PS2X_USER_DIR");
                s.folder = std::filesystem::path(user && *user ? user : ".") / "texture_dump";
            }
            else
                s.folder = std::filesystem::u8path(value);
            std::error_code ec;
            std::filesystem::create_directories(s.folder, ec);
            if (ec)
            {
                std::fprintf(stderr, "[tmb:texdump] cannot create %s: %s (no dump)\n", s.folder.u8string().c_str(), ec.message().c_str());
                s.folder.clear();
                return;
            }
            std::fprintf(stderr, "[tmb:texdump] dumping textures to %s\n", s.folder.u8string().c_str());
            const std::filesystem::path index = s.folder / "index.tsv";
            if (!std::filesystem::exists(index, ec))
            {
                std::ofstream out(index, std::ios::binary);
                out << "identity\twidth\theight\tpsm\ttexels\tpalette\ttex0\ttexa\ttexel_hash\tmax_alpha\n";
            }
        }

        // ---- a PNG with no dependency: zlib's stored blocks (the same approach as BozoEngine's app/png.cpp, which
        // the headless programs this runtime is part of do not link) ----

        uint32_t crc32(const uint8_t *data, size_t size, uint32_t crc = 0)
        {
            static const std::array<uint32_t, 256> table = []
            {
                std::array<uint32_t, 256> t{};
                for (uint32_t n = 0; n < 256u; ++n)
                {
                    uint32_t c = n;
                    for (int k = 0; k < 8; ++k)
                        c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
                    t[n] = c;
                }
                return t;
            }();
            crc = ~crc;
            for (size_t i = 0; i < size; ++i)
                crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
            return ~crc;
        }

        void putBe32(std::vector<uint8_t> &out, uint32_t v)
        {
            for (int shift = 24; shift >= 0; shift -= 8)
                out.push_back(static_cast<uint8_t>(v >> shift));
        }

        void chunk(std::vector<uint8_t> &out, const char type[4], const std::vector<uint8_t> &data)
        {
            putBe32(out, static_cast<uint32_t>(data.size()));
            const size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            putBe32(out, crc32(out.data() + start, out.size() - start));
        }

        bool writePng(const std::filesystem::path &path, const std::vector<uint8_t> &rgba, uint32_t width, uint32_t height)
        {
            std::vector<uint8_t> raw; // each row: filter 0, then its pixels
            raw.reserve((static_cast<size_t>(width) * 4u + 1u) * height);
            for (uint32_t y = 0; y < height; ++y)
            {
                raw.push_back(0);
                const uint8_t *row = rgba.data() + static_cast<size_t>(y) * width * 4u;
                raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4u);
            }
            std::vector<uint8_t> z = {0x78, 0x01};
            for (size_t at = 0;;) // raw is never empty: height >= 1, a filter byte per row
            {
                const size_t n = std::min<size_t>(65535u, raw.size() - at);
                const bool last = at + n == raw.size();
                z.push_back(last ? 1 : 0);
                z.push_back(static_cast<uint8_t>(n));
                z.push_back(static_cast<uint8_t>(n >> 8));
                z.push_back(static_cast<uint8_t>(~n));
                z.push_back(static_cast<uint8_t>(~n >> 8));
                z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + n));
                at += n;
                if (last)
                    break;
            }
            uint32_t a = 1, b = 0;
            for (uint8_t c : raw)
            {
                a = (a + c) % 65521u;
                b = (b + a) % 65521u;
            }
            putBe32(z, (b << 16) | a);

            std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
            std::vector<uint8_t> header;
            putBe32(header, width);
            putBe32(header, height);
            header.insert(header.end(), {8, 6, 0, 0, 0}); // 8 bits, RGBA, deflate, no filter choice, no interlace
            chunk(png, "IHDR", header);
            chunk(png, "IDAT", z);
            chunk(png, "IEND", {});
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
            return static_cast<bool>(out);
        }
    }

    bool enabled()
    {
        State &s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        resolve(s);
        return !s.folder.empty();
    }

    uint64_t identity(const bozo::render::Texture &texture)
    {
        const uint32_t size[2] = {texture.width, texture.height};
        return bozo::hash::xxh64(texture.rgba.data(), texture.rgba.size() * sizeof(uint32_t), bozo::hash::xxh64(size, sizeof(size), 0));
    }

    void note(const bozo::render::Texture &texture, const Source &source)
    {
        State &s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        resolve(s);
        if (s.folder.empty() || texture.width == 0u || texture.height == 0u ||
            texture.rgba.size() != static_cast<size_t>(texture.width) * texture.height)
            return;
        const uint64_t id = identity(texture);
        if (!s.written.insert(id).second)
            return;

        char name[32];
        std::snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(id));
        const std::filesystem::path path = s.folder / (std::string(name) + ".png");
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) // an earlier run's (its index line is there too)
            return;

        std::vector<uint8_t> rgba(texture.rgba.size() * 4u);
        uint32_t maxAlpha = 0;
        for (size_t i = 0; i < texture.rgba.size(); ++i)
        {
            const uint32_t c = texture.rgba[i], alpha = c >> 24;
            maxAlpha = std::max(maxAlpha, alpha);
            rgba[i * 4u + 0u] = static_cast<uint8_t>(c);
            rgba[i * 4u + 1u] = static_cast<uint8_t>(c >> 8);
            rgba[i * 4u + 2u] = static_cast<uint8_t>(c >> 16);
            rgba[i * 4u + 3u] = static_cast<uint8_t>(alpha); // GS alpha as is: lossless, 128 = opaque
        }
        if (!writePng(path, rgba, texture.width, texture.height))
        {
            std::fprintf(stderr, "[tmb:texdump] cannot write %s\n", path.u8string().c_str());
            return;
        }
        std::ofstream index(s.folder / "index.tsv", std::ios::binary | std::ios::app);
        char line[512];
        std::snprintf(line, sizeof(line), "%s\t%u\t%u\t%#x\t%s\t%s\t%#llx\t%#llx\t%016llx\t%u\n", name, texture.width, texture.height,
                      source.psm, source.texels.empty() ? "?" : source.texels.c_str(), source.palette.empty() ? "-" : source.palette.c_str(),
                      static_cast<unsigned long long>(source.tex0), static_cast<unsigned long long>(source.texa),
                      static_cast<unsigned long long>(source.texelHash), maxAlpha);
        index << line;
        ++s.files;
    }
}
