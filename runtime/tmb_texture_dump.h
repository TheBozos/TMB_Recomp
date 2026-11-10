// The texture dump for replacement packs (docs/TEXTURE_UPSCALING.md): with TMB_TEXTURE_DUMP set, every texture the
// native renderer is given is written once as a PNG (its GS alpha as is) named by its identity, with a line in index.tsv saying where it
// came from. Off by default; nothing changes in what the game or the renderer sees.
#pragma once

#include "bozo/render/texture.h"

#include <cstdint>
#include <string>

namespace tmb::textures::dump
{
    // Where a decoded texture came from, for the index (the pack record it was decoded from, its palette's).
    struct Source
    {
        std::string texels;  // "JUNKYARD.TEX#12", "SHELL.RTX#3", "FONT#0" ("" unknown)
        std::string palette; // the palette record's, "" for a direct-colour texture
        uint32_t psm = 0;
        uint64_t tex0 = 0, texa = 0;
        uint64_t texelHash = 0; // XXH64 of the record's texels (its psm, width and height as the seed)
    };

    // TMB_TEXTURE_DUMP: unset, empty or "0" off; "1" into $PS2X_USER_DIR/texture_dump (the current folder's
    // texture_dump without PS2X_USER_DIR); anything else is the folder itself.
    bool enabled();

    // The texture's identity: XXH64 of its RGBA texels seeded with XXH64 of {width, height}, the value
    // bozo::render::Renderer's draw-list hash gives the same texture.
    uint64_t identity(const bozo::render::Texture &texture);

    // Writes the texture (once per identity and run; an existing file is kept) and its index line.
    void note(const bozo::render::Texture &texture, const Source &source);
}
