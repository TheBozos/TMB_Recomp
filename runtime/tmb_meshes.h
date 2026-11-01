// TMB's 3D objects drawn natively from the mesh pack at their per-object hook (runtime/tmb_meshes.cpp), handed to the
// adapter (runtime/tmb_native.cpp) at their place in the frame.
#pragma once

#include "ps2_runtime.h"
#include "runtime/native/native_renderer.h"
#include "runtime/native/native_texture_cache.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace tmb::meshes
{
    // One batch of an object: its material as the strip program keeps it (the GS state around it, ALPHA_1, TEST_1,
    // ZBUF_1, FOGCOL, is the adapter's at the object's place) and its triangles in the object's space.
    struct Batch
    {
        uint64_t tex0 = 0;
        bool textured = false, blended = false, fogged = false;
        // GS registers the strip starting it set from the object's register block besides TEX0_1 (the adapter's
        // useMaterial follows them the same way): ALPHA_1, TEST_1, ZBUF_1.ZMSK.
        bool setsAlpha = false, setsTest = false, setsZbuf = false, setsTex0 = false;
        uint64_t alpha = 0, test = 0;
        bool depthMasked = false;
        std::shared_ptr<const ps2x::native::TextureCache::Texture> texture;
        std::vector<ps2x::native::Vertex> vertices;
    };

    // One object drawn from the pack, waiting for its place in the stream.
    struct Drawn
    {
        uint32_t setupPc = 0;
        uint32_t resumes = 0; // a resume (setupPc 0xc78): the setup it goes on with, which the stream's must be
        float matrix[16] = {}; // object to GS screen (columns), the z row times the depth factor
        bool screenSpace = false; // sprites and ribbons: their vertices in screen space (matrix: the setup's, unscaled)
        uint64_t key = 0;      // Renderer::setKey
        std::vector<Batch> batches;
        uint32_t check[24][4] = {}; // VU 0-21 as the hook built them (TMB_NATIVE_MESHES_CHECK: the adapter compares)
        uint32_t frame = 0;
        uint32_t object = 0, file = 0, offset = 0;
        // TMB_NATIVE_MESHES_CHECK: each strip's vertices before triangles (sprites and ribbons: its triangles; the
        // adapter compares its own translation).
        std::vector<std::vector<ps2x::native::Vertex>> checkStrips;
    };

    // TMB_NATIVE_MESHES=1 with TMB_NATIVE=1.
    bool enabled();
    // The adapter, at the setup MSCAL whose header carries this mark (its NOP's immediate): the object's draws.
    bool take(uint32_t mark, Drawn &out);
    // A published frame's end.
    void endFrame();
    void install(PS2Runtime &runtime);
}
