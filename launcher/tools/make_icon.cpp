// Draws the launcher's icon and writes it as a .ico (256, 64, 48, 32, 24, 16 pixels, 32-bit): the build makes it, so no
// image is committed. The style of RecompWatcher's icon.svg: the PS2 BIOS's radial blue (#14328f at the bottom centre,
// #050b26, #01030a) and its glowing "boot towers", in a rounded square. No game art.
//
//   make_icon <out.ico>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
    struct Rgb
    {
        float r, g, b;
    };

    Rgb hex(uint32_t v) { return {((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f}; }
    Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

    // The background: radialGradient cx 50% cy 85% r 80%, stops 0 #14328f, 0.55 #050b26, 1 #01030a.
    Rgb background(float u, float v)
    {
        const float d = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.85f) * (v - 0.85f)) / 0.8f;
        if (d < 0.55f)
            return mix(hex(0x14328f), hex(0x050b26), d / 0.55f);
        return mix(hex(0x050b26), hex(0x01030a), std::min(1.0f, (d - 0.55f) / 0.45f));
    }

    struct Tower
    {
        float x, w, h;
    };
    // Five towers rising from the bottom, the middle one tallest.
    const Tower kTowers[] = {{0.17f, 0.11f, 0.36f}, {0.335f, 0.11f, 0.54f}, {0.5f, 0.12f, 0.72f}, {0.665f, 0.11f, 0.5f},
                             {0.83f, 0.11f, 0.32f}};

    // One sample (u, v in 0..1, v down): colour and coverage of the rounded square.
    void sample(float u, float v, float &r, float &g, float &b, float &a)
    {
        // The rounded square (corner radius 0.2).
        const float rad = 0.2f;
        const float cx = std::clamp(u, rad, 1 - rad), cy = std::clamp(v, rad, 1 - rad);
        if ((u - cx) * (u - cx) + (v - cy) * (v - cy) > rad * rad)
        {
            r = g = b = a = 0;
            return;
        }
        Rgb c = background(u, v);
        // The towers' glow: a halo around each, then the tower itself (light at the top, deep blue at the foot).
        float glow = 0;
        for (const Tower &t : kTowers)
        {
            const float top = 1.0f - t.h;
            const float dx = std::max(0.0f, std::fabs(u - t.x) - t.w / 2), dy = std::max(0.0f, top - v);
            const float d = std::sqrt(dx * dx + dy * dy);
            glow += 0.55f * std::exp(-d / 0.035f);
            if (dx == 0 && dy == 0)
            {
                const float k = (v - top) / t.h; // 0 at the top
                Rgb tower = mix(hex(0xc9dbff), hex(0x3a58d6), std::min(1.0f, k * 1.6f));
                tower = mix(tower, hex(0x0d1c66), std::max(0.0f, k - 0.6f) * 1.5f);
                // The cap: a bright line at the top.
                if (v - top < 0.025f)
                    tower = mix(tower, hex(0xeef4ff), 0.7f);
                c = mix(c, tower, 0.92f);
                glow = 0;
                break;
            }
        }
        const Rgb halo = hex(0x6f8dff);
        glow = std::min(glow, 0.8f);
        c = {c.r + halo.r * glow * 0.5f, c.g + halo.g * glow * 0.5f, c.b + halo.b * glow * 0.5f};
        r = std::min(1.0f, c.r);
        g = std::min(1.0f, c.g);
        b = std::min(1.0f, c.b);
        a = 1;
    }

    // BGRA rows top-down, 4x4 supersampled.
    std::vector<uint8_t> render(int size)
    {
        std::vector<uint8_t> out(size * size * 4);
        const int ss = 4;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                float R = 0, G = 0, B = 0, A = 0;
                for (int sy = 0; sy < ss; ++sy)
                    for (int sx = 0; sx < ss; ++sx)
                    {
                        float r, g, b, a;
                        sample((x + (sx + 0.5f) / ss) / size, (y + (sy + 0.5f) / ss) / size, r, g, b, a);
                        R += r * a;
                        G += g * a;
                        B += b * a;
                        A += a;
                    }
                uint8_t *p = &out[(y * size + x) * 4];
                const float n = ss * ss;
                const float alpha = A / n;
                p[0] = static_cast<uint8_t>(std::lround(A > 0 ? B / A * 255 : 0));
                p[1] = static_cast<uint8_t>(std::lround(A > 0 ? G / A * 255 : 0));
                p[2] = static_cast<uint8_t>(std::lround(A > 0 ? R / A * 255 : 0));
                p[3] = static_cast<uint8_t>(std::lround(alpha * 255));
            }
        return out;
    }

    void put16(std::vector<uint8_t> &v, uint32_t x) { v.insert(v.end(), {uint8_t(x), uint8_t(x >> 8)}); }
    void put32(std::vector<uint8_t> &v, uint32_t x)
    {
        v.insert(v.end(), {uint8_t(x), uint8_t(x >> 8), uint8_t(x >> 16), uint8_t(x >> 24)});
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: make_icon <out.ico>\n");
        return 2;
    }
    const int sizes[] = {256, 64, 48, 32, 24, 16};
    std::vector<std::vector<uint8_t>> images;
    for (int s : sizes)
    {
        // A BMP without its file header: BITMAPINFOHEADER (height doubled: the XOR image and the AND mask), the
        // pixels bottom-up, then the AND mask (all zero: the alpha channel decides).
        const std::vector<uint8_t> px = render(s);
        std::vector<uint8_t> img;
        put32(img, 40);
        put32(img, s);
        put32(img, s * 2);
        put16(img, 1);
        put16(img, 32);
        put32(img, 0);
        put32(img, s * s * 4);
        put32(img, 0);
        put32(img, 0);
        put32(img, 0);
        put32(img, 0);
        for (int y = s - 1; y >= 0; --y)
            img.insert(img.end(), px.begin() + y * s * 4, px.begin() + (y + 1) * s * 4);
        const int maskRow = ((s + 31) / 32) * 4;
        img.insert(img.end(), static_cast<size_t>(maskRow * s), 0);
        images.push_back(std::move(img));
    }
    std::vector<uint8_t> ico;
    put16(ico, 0);
    put16(ico, 1);
    put16(ico, static_cast<uint32_t>(images.size()));
    uint32_t offset = 6 + 16 * static_cast<uint32_t>(images.size());
    for (size_t i = 0; i < images.size(); ++i)
    {
        ico.push_back(static_cast<uint8_t>(sizes[i] >= 256 ? 0 : sizes[i]));
        ico.push_back(static_cast<uint8_t>(sizes[i] >= 256 ? 0 : sizes[i]));
        ico.push_back(0);
        ico.push_back(0);
        put16(ico, 1);
        put16(ico, 32);
        put32(ico, static_cast<uint32_t>(images[i].size()));
        put32(ico, offset);
        offset += static_cast<uint32_t>(images[i].size());
    }
    for (const auto &img : images)
        ico.insert(ico.end(), img.begin(), img.end());
    FILE *f = std::fopen(argv[1], "wb");
    if (!f || std::fwrite(ico.data(), 1, ico.size(), f) != ico.size())
    {
        std::fprintf(stderr, "make_icon: cannot write %s\n", argv[1]);
        return 1;
    }
    std::fclose(f);
    return 0;
}
