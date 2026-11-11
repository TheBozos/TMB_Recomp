// tmb-texture-pack: a texture replacement pack made from a texture dump with Real-ESRGAN, on the player's PC from their
// own disc (docs/TEXTURE_UPSCALING.md). The native counterpart of tools/tmb_texture_upscale.py, for the launcher's
// "Enhance textures" step: no Python packages, built with the game.
//
//   tmb-texture-pack <dump folder> <pack folder> --esrgan <realesrgan-ncnn-vulkan.exe> [--models <folder>]
//                    [--model realesrgan-x4plus] [--min-size 32] [--detail 1.0] [--skip SHELL1,FONT] [--jobs N]
//                    [--work <folder>]
//
// Per texture (index.tsv's rows, <identity>.png, GS alpha as the dump wrote it):
// - colour: transparent texels take their visible neighbours' colour, then Real-ESRGAN 4x over a 16-texel border taken
//   from the opposite edges (repeating textures stay seamless); the original's high frequencies (bicubic minus its
//   Gaussian blur, sigma 3 at the output size) added back, since the model flattens gravel and rust;
// - alpha: one value kept; two values cut at the midpoint of a bicubic; otherwise the visible shape (bicubic of the
//   visible mask cut at half) keeps everything outside it at 0, a few levels (<= 4) snapped to the nearest one;
// - flat areas (a texel equal to its 8 neighbours) kept exactly; menus and font (--skip) and textures under --min-size
//   left to the engine's own upscaling.
// Files already in the pack are kept (a run can be resumed). Output for the launcher: "step <name>", "progress <done>
// <total>", "error <text>", "done <count>" lines on stdout.

#include "bozo/render/png_read.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
    constexpr int kScale = 4, kPad = 16;

    std::mutex g_out;
    void say(const std::string &line)
    {
        std::lock_guard<std::mutex> lock(g_out);
        std::fputs((line + "\n").c_str(), stdout);
        std::fflush(stdout);
    }

    // ---- PNG writing: zlib with LZ77 (hash chains) and the fixed Huffman code (RFC 1950 / 1951) ----

    struct BitWriter
    {
        std::vector<uint8_t> out;
        uint32_t buffer = 0;
        int count = 0;
        void put(uint32_t bits, int n)
        {
            buffer |= bits << count;
            count += n;
            while (count >= 8)
            {
                out.push_back(static_cast<uint8_t>(buffer));
                buffer >>= 8;
                count -= 8;
            }
        }
        void putReversed(uint32_t code, int n) // Huffman codes go most significant bit first
        {
            uint32_t r = 0;
            for (int i = 0; i < n; ++i)
                r |= ((code >> i) & 1u) << (n - 1 - i);
            put(r, n);
        }
        void flush()
        {
            if (count > 0)
                out.push_back(static_cast<uint8_t>(buffer));
            buffer = 0;
            count = 0;
        }
    };

    void literal(BitWriter &w, int v)
    {
        if (v < 144)
            w.putReversed(0x30 + v, 8);
        else if (v < 256)
            w.putReversed(0x190 + v - 144, 9);
        else if (v < 280)
            w.putReversed(v - 256, 7);
        else
            w.putReversed(0xc0 + v - 280, 8);
    }

    constexpr uint16_t kLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    constexpr uint8_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    constexpr uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    std::vector<uint8_t> deflate(const std::vector<uint8_t> &data)
    {
        BitWriter w;
        w.put(0x78, 8);
        w.put(0x01, 8);
        w.put(1, 1); // the last (only) block
        w.put(1, 2); // fixed Huffman codes
        constexpr int kHashBits = 15, kWindow = 32768, kChain = 32;
        std::vector<int32_t> head(1u << kHashBits, -1), prev(data.size(), -1);
        const auto hash = [&](size_t i) { return ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & ((1u << kHashBits) - 1u); };
        size_t i = 0;
        while (i < data.size())
        {
            int bestLen = 0, bestDist = 0;
            if (i + 3 <= data.size())
            {
                const uint32_t h = hash(i);
                int32_t candidate = head[h];
                for (int chain = 0; candidate >= 0 && chain < kChain && static_cast<int>(i) - candidate <= kWindow; ++chain)
                {
                    const size_t limit = std::min<size_t>(258, data.size() - i);
                    size_t len = 0;
                    while (len < limit && data[candidate + len] == data[i + len])
                        ++len;
                    if (static_cast<int>(len) > bestLen)
                    {
                        bestLen = static_cast<int>(len);
                        bestDist = static_cast<int>(i) - candidate;
                        if (len == limit)
                            break;
                    }
                    candidate = prev[candidate];
                }
                prev[i] = head[h];
                head[h] = static_cast<int32_t>(i);
            }
            if (bestLen >= 3)
            {
                int s = 0;
                while (s < 28 && kLengthBase[s + 1] <= bestLen)
                    ++s;
                literal(w, 257 + s);
                w.put(static_cast<uint32_t>(bestLen - kLengthBase[s]), kLengthExtra[s]);
                int d = 0;
                while (d < 29 && kDistBase[d + 1] <= bestDist)
                    ++d;
                w.putReversed(static_cast<uint32_t>(d), 5);
                w.put(static_cast<uint32_t>(bestDist - kDistBase[d]), kDistExtra[d]);
                for (size_t k = 1; k < static_cast<size_t>(bestLen); ++k) // the matched bytes join the hash chains
                    if (i + k + 3 <= data.size())
                    {
                        const uint32_t h = hash(i + k);
                        prev[i + k] = head[h];
                        head[h] = static_cast<int32_t>(i + k);
                    }
                i += static_cast<size_t>(bestLen);
            }
            else
                literal(w, data[i++]);
        }
        literal(w, 256);
        w.flush();
        uint32_t a = 1, b = 0;
        for (uint8_t c : data)
        {
            a = (a + c) % 65521u;
            b = (b + a) % 65521u;
        }
        for (int s = 24; s >= 0; s -= 8)
            w.out.push_back(static_cast<uint8_t>(((b << 16) | a) >> s));
        return w.out;
    }

    uint32_t crc32(const uint8_t *p, size_t n, uint32_t c = 0)
    {
        static const std::array<uint32_t, 256> table = []
        {
            std::array<uint32_t, 256> t{};
            for (uint32_t k = 0; k < 256; ++k)
            {
                uint32_t v = k;
                for (int j = 0; j < 8; ++j)
                    v = (v & 1u) ? 0xedb88320u ^ (v >> 1) : v >> 1;
                t[k] = v;
            }
            return t;
        }();
        c = ~c;
        for (size_t k = 0; k < n; ++k)
            c = table[(c ^ p[k]) & 0xffu] ^ (c >> 8);
        return ~c;
    }

    void be32(std::vector<uint8_t> &o, uint32_t v)
    {
        for (int s = 24; s >= 0; s -= 8)
            o.push_back(static_cast<uint8_t>(v >> s));
    }

    void chunk(std::vector<uint8_t> &o, const char *type, const std::vector<uint8_t> &d)
    {
        be32(o, static_cast<uint32_t>(d.size()));
        const size_t at = o.size();
        o.insert(o.end(), type, type + 4);
        o.insert(o.end(), d.begin(), d.end());
        be32(o, crc32(o.data() + at, o.size() - at));
    }

    // channels 3 (RGB) or 4 (RGBA), rows top-down; each row filtered with Sub (a good choice for photographs).
    bool writePng(const fs::path &path, const uint8_t *pixels, int width, int height, int channels)
    {
        std::vector<uint8_t> raw;
        raw.reserve((static_cast<size_t>(width) * channels + 1) * height);
        for (int y = 0; y < height; ++y)
        {
            raw.push_back(1);
            const uint8_t *row = pixels + static_cast<size_t>(y) * width * channels;
            for (int x = 0; x < width * channels; ++x)
                raw.push_back(static_cast<uint8_t>(row[x] - (x >= channels ? row[x - channels] : 0)));
        }
        std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'}, header;
        be32(header, static_cast<uint32_t>(width));
        be32(header, static_cast<uint32_t>(height));
        header.insert(header.end(), {8, static_cast<uint8_t>(channels == 4 ? 6 : 2), 0, 0, 0});
        chunk(png, "IHDR", header);
        chunk(png, "IDAT", deflate(raw));
        chunk(png, "IEND", {});
        const fs::path temp = path.string() + ".part";
        {
            std::ofstream out(temp, std::ios::binary);
            out.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
            if (!out)
                return false;
        }
        std::error_code ec;
        fs::rename(temp, path, ec); // whole files only: an interrupted run leaves no half-written replacement
        return !ec;
    }

    // ---- images ----

    struct Image // float channels, row-major
    {
        int w = 0, h = 0, c = 0;
        std::vector<float> v;
        float &at(int x, int y, int k) { return v[(static_cast<size_t>(y) * w + x) * c + k]; }
        float at(int x, int y, int k) const { return v[(static_cast<size_t>(y) * w + x) * c + k]; }
    };

    int wrap(int v, int n) { return ((v % n) + n) % n; }

    // A border of `pad` texels taken from the opposite edges.
    Image wrapPad(const Image &in, int pad)
    {
        Image out{in.w + 2 * pad, in.h + 2 * pad, in.c, {}};
        out.v.resize(static_cast<size_t>(out.w) * out.h * out.c);
        for (int y = 0; y < out.h; ++y)
            for (int x = 0; x < out.w; ++x)
                for (int k = 0; k < in.c; ++k)
                    out.at(x, y, k) = in.at(wrap(x - pad, in.w), wrap(y - pad, in.h), k);
        return out;
    }

    // Bicubic (Keys, a = -0.5, as Pillow's) by `scale`, pixel centres aligned; edges clamped.
    float cubic(float t)
    {
        t = std::fabs(t);
        if (t < 1.0f)
            return (1.5f * t - 2.5f) * t * t + 1.0f;
        if (t < 2.0f)
            return ((-0.5f * t + 2.5f) * t - 4.0f) * t + 2.0f;
        return 0.0f;
    }

    Image bicubic(const Image &in, int scale)
    {
        Image mid{in.w * scale, in.h, in.c, {}};
        mid.v.assign(static_cast<size_t>(mid.w) * mid.h * mid.c, 0.0f);
        for (int x = 0; x < mid.w; ++x)
        {
            const float s = (x + 0.5f) / scale - 0.5f;
            const int base = static_cast<int>(std::floor(s));
            float wsum = 0, wt[4];
            for (int t = 0; t < 4; ++t)
                wsum += wt[t] = cubic(s - static_cast<float>(base - 1 + t));
            for (int y = 0; y < in.h; ++y)
                for (int k = 0; k < in.c; ++k)
                {
                    float acc = 0;
                    for (int t = 0; t < 4; ++t)
                        acc += wt[t] * in.at(std::clamp(base - 1 + t, 0, in.w - 1), y, k);
                    mid.at(x, y, k) = acc / wsum;
                }
        }
        Image out{mid.w, in.h * scale, in.c, {}};
        out.v.assign(static_cast<size_t>(out.w) * out.h * out.c, 0.0f);
        for (int y = 0; y < out.h; ++y)
        {
            const float s = (y + 0.5f) / scale - 0.5f;
            const int base = static_cast<int>(std::floor(s));
            float wsum = 0, wt[4];
            for (int t = 0; t < 4; ++t)
                wsum += wt[t] = cubic(s - static_cast<float>(base - 1 + t));
            for (int x = 0; x < out.w; ++x)
                for (int k = 0; k < in.c; ++k)
                {
                    float acc = 0;
                    for (int t = 0; t < 4; ++t)
                        acc += wt[t] * mid.at(x, std::clamp(base - 1 + t, 0, mid.h - 1), k);
                    out.at(x, y, k) = acc / wsum;
                }
        }
        return out;
    }

    Image gaussian(const Image &in, float sigma)
    {
        const int r = static_cast<int>(std::ceil(sigma * 3.0f));
        std::vector<float> kernel(2 * r + 1);
        float sum = 0;
        for (int i = -r; i <= r; ++i)
            sum += kernel[i + r] = std::exp(-(i * i) / (2.0f * sigma * sigma));
        for (float &k : kernel)
            k /= sum;
        Image tmp = in, out = in;
        for (int y = 0; y < in.h; ++y)
            for (int x = 0; x < in.w; ++x)
                for (int k = 0; k < in.c; ++k)
                {
                    float acc = 0;
                    for (int i = -r; i <= r; ++i)
                        acc += kernel[i + r] * in.at(std::clamp(x + i, 0, in.w - 1), y, k);
                    tmp.at(x, y, k) = acc;
                }
        for (int y = 0; y < in.h; ++y)
            for (int x = 0; x < in.w; ++x)
                for (int k = 0; k < in.c; ++k)
                {
                    float acc = 0;
                    for (int i = -r; i <= r; ++i)
                        acc += kernel[i + r] * tmp.at(x, std::clamp(y + i, 0, in.h - 1), k);
                    out.at(x, y, k) = acc;
                }
        return out;
    }

    // ---- the steps ----

    struct Texture
    {
        std::string identity;
        uint32_t w = 0, h = 0;
        std::vector<uint32_t> rgba; // GS alpha, R in the low byte
        Image rgb;                  // colour with transparent texels filled from their neighbours
    };

    Image bleed(const Texture &t)
    {
        Image rgb{static_cast<int>(t.w), static_cast<int>(t.h), 3, {}};
        rgb.v.resize(static_cast<size_t>(t.w) * t.h * 3);
        std::vector<uint8_t> known(t.rgba.size());
        bool any = false, all = true;
        for (size_t i = 0; i < t.rgba.size(); ++i)
        {
            for (int k = 0; k < 3; ++k)
                rgb.v[i * 3 + k] = static_cast<float>((t.rgba[i] >> (8 * k)) & 0xffu);
            known[i] = (t.rgba[i] >> 24) != 0u;
            any |= known[i] != 0;
            all &= known[i] != 0;
        }
        if (!any || all)
            return rgb;
        const int w = rgb.w, h = rgb.h;
        for (int pass = 0; pass < 16; ++pass)
        {
            std::vector<uint8_t> next = known;
            bool grew = false, done = true;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const size_t i = static_cast<size_t>(y) * w + x;
                    if (known[i])
                        continue;
                    float acc[3] = {0, 0, 0};
                    int n = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const size_t j = static_cast<size_t>(wrap(y + dy, h)) * w + wrap(x + dx, w);
                            if ((dx || dy) && known[j])
                            {
                                for (int k = 0; k < 3; ++k)
                                    acc[k] += rgb.v[j * 3 + k];
                                ++n;
                            }
                        }
                    if (n)
                    {
                        for (int k = 0; k < 3; ++k)
                            rgb.v[i * 3 + k] = acc[k] / static_cast<float>(n);
                        next[i] = 1;
                        grew = true;
                    }
                    else
                        done = false;
                }
            known.swap(next);
            if (done || !grew)
                break;
        }
        return rgb;
    }

    std::vector<uint8_t> upscaleAlpha(const Texture &t)
    {
        const int w = static_cast<int>(t.w), h = static_cast<int>(t.h), W = w * kScale, H = h * kScale;
        std::vector<uint8_t> values;
        Image a{w, h, 1, {}}, mask{w, h, 1, {}};
        a.v.resize(t.rgba.size());
        mask.v.resize(t.rgba.size());
        for (size_t i = 0; i < t.rgba.size(); ++i)
        {
            const uint8_t v = static_cast<uint8_t>(t.rgba[i] >> 24);
            a.v[i] = v;
            mask.v[i] = v ? 255.0f : 0.0f;
            values.push_back(v);
        }
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        std::vector<uint8_t> out(static_cast<size_t>(W) * H, values[0]);
        if (values.size() == 1)
            return out;
        const auto up = [&](const Image &img)
        {
            const Image big = bicubic(wrapPad(img, kPad), kScale);
            Image crop{W, H, 1, {}};
            crop.v.resize(static_cast<size_t>(W) * H);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    crop.at(x, y, 0) = big.at(x + kPad * kScale, y + kPad * kScale, 0);
            return crop;
        };
        const Image big = up(a);
        if (values.size() == 2)
        {
            const float mid = (values[0] + values[1]) / 2.0f;
            for (size_t i = 0; i < out.size(); ++i)
                out[i] = big.v[i] >= mid ? values[1] : values[0];
            return out;
        }
        const bool hasZero = values[0] == 0;
        const Image shape = hasZero ? up(mask) : Image{};
        std::vector<uint8_t> visible(values.begin() + (hasZero ? 1 : 0), values.end());
        const bool has128 = std::binary_search(values.begin(), values.end(), uint8_t{128});
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (hasZero && shape.v[i] < 128.0f)
            {
                out[i] = 0;
                continue;
            }
            float v = std::clamp(big.v[i], static_cast<float>(visible.front()), static_cast<float>(visible.back()));
            if (values.size() <= 4)
            {
                uint8_t best = visible[0];
                for (uint8_t level : visible)
                    if (std::fabs(level - v) < std::fabs(best - v))
                        best = level;
                v = best;
            }
            else if (has128 && std::fabs(v - 128.0f) <= 2.0f)
                v = 128.0f;
            out[i] = static_cast<uint8_t>(std::lround(v));
        }
        return out;
    }

    // Texels equal to their 8 neighbours (colour and alpha within 1, wrap-around).
    std::vector<uint8_t> flatTexels(const Texture &t)
    {
        const int w = static_cast<int>(t.w), h = static_cast<int>(t.h);
        std::vector<uint8_t> flat(t.rgba.size(), 1);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const uint32_t c = t.rgba[static_cast<size_t>(y) * w + x];
                for (int dy = -1; dy <= 1 && flat[static_cast<size_t>(y) * w + x]; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const uint32_t d = t.rgba[static_cast<size_t>(wrap(y + dy, h)) * w + wrap(x + dx, w)];
                        bool same = true;
                        for (int k = 0; k < 4; ++k)
                            same &= std::abs(static_cast<int>((c >> (8 * k)) & 0xffu) - static_cast<int>((d >> (8 * k)) & 0xffu)) <= 1;
                        if (!same)
                        {
                            flat[static_cast<size_t>(y) * w + x] = 0;
                            break;
                        }
                    }
            }
        return flat;
    }

    bool load(const fs::path &dump, const std::string &identity, Texture &t, std::string &why)
    {
        t.identity = identity;
        if (!bozo::render::readPng((dump / (identity + ".png")).string(), t.w, t.h, t.rgba, &why))
            return false;
        t.rgb = bleed(t);
        return true;
    }

    bool finish(const fs::path &dump, const std::string &identity, const fs::path &esrganOut, const fs::path &pack, float detail,
                std::string &why)
    {
        Texture t;
        if (!load(dump, identity, t, why))
            return false;
        uint32_t ow = 0, oh = 0;
        std::vector<uint32_t> model;
        if (!bozo::render::readPng(esrganOut.string(), ow, oh, model, &why))
            return false;
        const int w = static_cast<int>(t.w), h = static_cast<int>(t.h), W = w * kScale, H = h * kScale;
        if (ow != static_cast<uint32_t>((w + 2 * kPad) * kScale) || oh != static_cast<uint32_t>((h + 2 * kPad) * kScale))
        {
            why = "Real-ESRGAN's output has another size";
            return false;
        }
        Image hf;
        if (detail != 0.0f)
        {
            const Image big = bicubic(wrapPad(t.rgb, kPad), kScale);
            const Image blur = gaussian(big, 3.0f);
            hf = big;
            for (size_t i = 0; i < hf.v.size(); ++i)
                hf.v[i] = big.v[i] - blur.v[i];
        }
        const std::vector<uint8_t> alpha = upscaleAlpha(t), flat = flatTexels(t);
        std::vector<uint8_t> pixels(static_cast<size_t>(W) * H * 4);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                uint8_t *p = &pixels[(static_cast<size_t>(y) * W + x) * 4];
                const size_t src = static_cast<size_t>(y / kScale) * w + x / kScale;
                if (flat[src])
                {
                    for (int k = 0; k < 4; ++k)
                        p[k] = static_cast<uint8_t>(t.rgba[src] >> (8 * k));
                    continue;
                }
                const uint32_t m = model[static_cast<size_t>(y + kPad * kScale) * ow + x + kPad * kScale];
                for (int k = 0; k < 3; ++k)
                {
                    float v = static_cast<float>((m >> (8 * k)) & 0xffu);
                    if (detail != 0.0f)
                        v += detail * hf.at(x + kPad * kScale, y + kPad * kScale, k);
                    p[k] = static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
                }
                p[3] = alpha[static_cast<size_t>(y) * W + x];
            }
        if (!writePng(pack / (t.identity + ".png"), pixels.data(), W, H, 4))
        {
            why = "cannot write the replacement";
            return false;
        }
        return true;
    }

    // Real-ESRGAN over a folder; progress from its output folder while it runs.
    int runEsrgan(const fs::path &exe, const fs::path &models, const std::string &model, const fs::path &in, const fs::path &out,
                  size_t total)
    {
        std::vector<std::string> args = {exe.string(), "-i", in.string(), "-o", out.string(), "-n", model, "-m", models.string(),
                                         "-s", std::to_string(kScale), "-f", "png"};
        const auto count = [&]
        {
            size_t n = 0;
            std::error_code ec;
            for (fs::directory_iterator it(out, ec), end; !ec && it != end; it.increment(ec))
                ++n;
            return n;
        };
#if defined(_WIN32)
        std::wstring command;
        for (const std::string &a : args)
        {
            const std::wstring wa = fs::path(a).wstring();
            command += L"\"" + wa + L"\" ";
        }
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                            nullptr, nullptr, &si, &pi))
        {
            CloseHandle(si.hStdOutput);
            return -1;
        }
        while (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT)
            say("progress " + std::to_string(count()) + " " + std::to_string(total));
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(si.hStdOutput);
        return static_cast<int>(code);
#else
        const pid_t pid = fork();
        if (pid == 0)
        {
            std::vector<char *> argv;
            for (std::string &a : args)
                argv.push_back(a.data());
            argv.push_back(nullptr);
            if (std::freopen("/dev/null", "w", stdout) && std::freopen("/dev/null", "w", stderr))
                execv(argv[0], argv.data());
            _exit(127);
        }
        if (pid < 0)
            return -1;
        int status = 0;
        while (waitpid(pid, &status, WNOHANG) == 0)
        {
            say("progress " + std::to_string(count()) + " " + std::to_string(total));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
    }

    std::vector<std::string> split(const std::string &s, char c)
    {
        std::vector<std::string> out;
        std::stringstream in(s);
        for (std::string part; std::getline(in, part, c);)
            out.push_back(part);
        return out;
    }
}

int main(int argc, char **argv)
{
    std::vector<std::string> positional;
    fs::path esrgan, models, work;
    std::string model = "realesrgan-x4plus", skipList = "SHELL1,FONT";
    int minSize = 32;
    float detail = 1.0f;
    unsigned jobs = std::max(1u, std::thread::hardware_concurrency() / 2);
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--esrgan")
            esrgan = fs::path(next());
        else if (a == "--models")
            models = fs::path(next());
        else if (a == "--model")
            model = next();
        else if (a == "--min-size")
            minSize = std::atoi(next().c_str());
        else if (a == "--detail")
            detail = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--skip")
            skipList = next();
        else if (a == "--jobs")
            jobs = static_cast<unsigned>(std::max(1, std::atoi(next().c_str())));
        else if (a == "--work")
            work = fs::path(next());
        else
            positional.push_back(a);
    }
    if (positional.size() != 2 || esrgan.empty())
    {
        std::fprintf(stderr, "usage: tmb-texture-pack <dump folder> <pack folder> --esrgan <realesrgan-ncnn-vulkan> [--models <folder>] "
                             "[--model realesrgan-x4plus] [--min-size 32] [--detail 1.0] [--skip SHELL1,FONT] [--jobs N] [--work <folder>]\n");
        return 2;
    }
    const fs::path dump = fs::path(positional[0]), pack = fs::path(positional[1]);
    if (models.empty())
        models = esrgan.parent_path() / "models";
    if (work.empty())
        work = pack.parent_path() / (pack.filename().string() + ".work");
    std::vector<std::string> skip;
    for (std::string s : split(skipList, ','))
        if (!s.empty())
        {
            std::transform(s.begin(), s.end(), s.begin(), [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
            skip.push_back(s.substr(0, s.find('.')));
        }

    std::ifstream index(dump / "index.tsv");
    if (!index)
    {
        say("error no texture dump in " + dump.string() + " (index.tsv)");
        return 1;
    }
    std::error_code ec;
    fs::create_directories(pack, ec);
    fs::remove_all(work, ec);
    fs::create_directories(work / "in", ec);
    fs::create_directories(work / "out", ec);

    // ---- the textures to make: padded colour into the model's folder ----
    say("step prepare");
    // One row per identity: the dump's parallel runs can each list a texture they all load, and two threads writing
    // the same file into the model's folder would collide.
    std::vector<std::string> rows;
    std::unordered_set<std::string> seen;
    std::string line;
    std::getline(index, line); // the header
    while (std::getline(index, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty() && seen.insert(line.substr(0, line.find('\t'))).second)
            rows.push_back(line);
    }
    std::vector<std::string> todo; // identities (re-read when finished: a whole dump does not stay in memory)
    std::mutex todoMutex;
    std::atomic<size_t> next{0}, prepared{0};
    std::atomic<bool> failed{false};
    std::string failure;
    const auto prepare = [&]
    {
        for (size_t r; (r = next++) < rows.size();)
        {
            const std::vector<std::string> f = split(rows[r], '\t');
            if (f.size() < 5)
                continue;
            const std::string file = f[4].substr(0, f[4].find('#'));
            const std::string base = file.substr(0, file.find('.'));
            if (std::find(skip.begin(), skip.end(), base) != skip.end() || std::atoi(f[1].c_str()) < minSize ||
                std::atoi(f[2].c_str()) < minSize || fs::exists(pack / (f[0] + ".png")))
                continue;
            Texture t;
            std::string why;
            if (!load(dump, f[0], t, why))
                continue;
            const Image padded = wrapPad(t.rgb, kPad);
            std::vector<uint8_t> bytes(padded.v.size());
            for (size_t i = 0; i < bytes.size(); ++i)
                bytes[i] = static_cast<uint8_t>(std::clamp(std::lround(padded.v[i]), 0L, 255L));
            if (!writePng(work / "in" / (t.identity + ".png"), bytes.data(), padded.w, padded.h, 3))
            {
                failed = true;
                continue;
            }
            std::lock_guard<std::mutex> lock(todoMutex);
            todo.push_back(t.identity);
            ++prepared;
        }
    };
    {
        std::vector<std::thread> pool;
        for (unsigned j = 0; j < jobs; ++j)
            pool.emplace_back(prepare);
        for (std::thread &t : pool)
            t.join();
    }
    if (failed)
    {
        say("error cannot write to " + work.string());
        return 1;
    }
    if (todo.empty())
    {
        say("done 0");
        fs::remove_all(work, ec);
        return 0;
    }

    // ---- the model ----
    say("step upscale");
    const int code = runEsrgan(esrgan, models, model, work / "in", work / "out", todo.size());
    if (code != 0)
    {
        say("error Real-ESRGAN stopped (exit code " + std::to_string(code) + "): it needs a Vulkan graphics driver");
        return 1;
    }

    // ---- the replacements ----
    say("step finish");
    next = 0;
    std::atomic<size_t> finished{0}, errors{0};
    const auto finishAll = [&]
    {
        for (size_t i; (i = next++) < todo.size();)
        {
            std::string why;
            if (!finish(dump, todo[i], work / "out" / (todo[i] + ".png"), pack, detail, why))
            {
                ++errors;
                say("warning " + todo[i] + ": " + why);
            }
            const size_t n = ++finished;
            if (n % 16 == 0 || n == todo.size())
                say("progress " + std::to_string(n) + " " + std::to_string(todo.size()));
        }
    };
    {
        std::vector<std::thread> pool;
        for (unsigned j = 0; j < jobs; ++j)
            pool.emplace_back(finishAll);
        for (std::thread &t : pool)
            t.join();
    }
    fs::remove_all(work, ec);
    say("done " + std::to_string(todo.size() - errors));
    return errors == todo.size() ? 1 : 0;
}
