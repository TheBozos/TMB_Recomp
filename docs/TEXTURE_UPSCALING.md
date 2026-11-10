# Texture upscaling: investigation and plan

This document covers where TMB's textures come from in the native renderer, and how to identify each one across
runs. It compares the upscaling options and their legal limits, and gives a staged plan. It also documents the
first step that exists now: a texture dump behind an environment switch (`TMB_TEXTURE_DUMP`).

The investigation was written against main `777f964` (release 0.5.0) and BozoEngine `4a8f70a`. Everything here
concerns the USA build in native graphics mode. Nothing in this plan routes through GS emulation or the historical
checker.

## Status (2026-11-10): runtime upscaling is in the game

BozoEngine already had option B below as a renderer feature: the Soulcalibur III port's
[engine PR #83](https://github.com/TheBozos/BozoEngine/pull/83). TMB gets it with the pin at `db7591c` and needs
no code of its own, because `decodeRecord` marks every pack texture as an asset.

- **Settings:** `texture_upscale = off|2x|4x` and `texture_filter = edge|smooth` in `video.ini`, the F2 Video tab,
  or `PS2X_TEXTURE_UPSCALE` / `PS2X_TEXTURE_FILTER`. It is off by default.
- **Filters:** the filters are the engine's own MIT code, written from published method descriptions (no
  GPL xBR/xBRZ code).
- **Where it applies:** upscaling is present-side only, so the draw-list digests and replays do not change.
  Mip levels shift so that minified sampling is unchanged.
- **Measured on TMB's 1P route (windowed, RTX 5070 Ti):**
  - 4x kept about 250 MB of copies, made in 25 ms of worker time each.
  - 2x kept 64 MB, made in 7 ms each.
- **What it looks like:** edges of painted detail (grille bars, rust, decals) are smoother and less blocky. No
  detail is invented: it is filtering, not the offline AI path (option A).

## For players: the launcher's "Enhanced textures"

The launcher's Settings tab has an **Enhanced textures (AI)** section. It is opt-in, works from the player's own disc on
their PC, and nothing of the game is downloaded or shipped.

1. **Make enhanced textures** runs `tools/tmb_texture_enhance.py` with the kit's Python:
   - the staged `game/tmb-headless.exe` dumps every arena and car (`tools/tmb_texture_dump_all.py`, its pad scripts
     in `tools/texture_dump/`);
   - the staged `game/tmb-texture-pack.exe` (`bozo/tmb_texture_pack.cpp`, the native counterpart of
     `tools/tmb_texture_upscale.py`) upscales them with the kit's Real-ESRGAN (`kit/realesrgan`, from
     `tools/make_build_kit.py`'s `realesrgan` step: the pinned upstream release zip, with its licences);
   - the result goes into `texture_pack.new`, which replaces `<user folder>\texture_pack`. The dump and work
     folders are then removed.
2. Progress shows per step, with **Stop**: a stopped run goes on where it stopped next time. **Remove** deletes the
   pack, and **In the game: on/off** writes `video.ini`'s `texture_pack`. The game's own F2 → Video has the same
   switch.
3. On this PC (RTX 5070 Ti) the launcher made 2,162 replacements in 5 min 10 s. Run-to-run counts vary by a few
   textures (2,161 to 2,164).

`tools/tmb_build.py` builds and stages the two tools with the game (the compile and stage steps). A release whose kit
has no `realesrgan/` greys the button out with a note.

## Status (2026-11-10, later): AI texture packs (option A)

The engine reads **texture replacements**
([BozoEngine `docs/texture-replace.md`](https://github.com/TheBozos/BozoEngine/blob/main/docs/texture-replace.md)):
`<identity>.png` files in `<user folder>\texture_pack`, or set by `texture_pack` in `video.ini` or
`PS2X_TEXTURE_PACK`. Each file is the texture's size or 2, 4 or 8 times it, read by worker threads. A replacement
takes precedence over texture upscaling, which still covers textures without one.

**Making a pack from your own disc:**

1. Dump the textures. `TMB_TEXTURE_DUMP=1` (or `=<folder>`) writes each texture once as `<identity>.png`, with
   `index.tsv`. Every 250 frames it also writes every texture of every object in the loaded mesh files, drawn or
   not, so one match covers its whole arena and the cars loaded. For everything at once, run:
   ```bash
   python tools/tmb_texture_dump_all.py <build>/tmb-headless.exe <disc.iso> <dump folder>
   ```
   It runs headless through:
   - the 21 story levels (`TMB_STORY_LEVEL`, with every arena, the bosses and the story's cars). Level n's player
     picks the car n RIGHT presses along on the car-select screen, so levels 0 to 14 load the 15 player-only car
     versions (`PCAR/xxP` files; about 10% of each car's textures differ from its AI version);
   - the four-player route;
   - two Challenge matches with other opponents.

   On the USA disc that is 2,289 textures (2,175 replacements in the pack): all arenas, every AI car and every player car.
2. Get Real-ESRGAN's ncnn-vulkan build yourself (xinntao/Real-ESRGAN, BSD-3-Clause; release v0.2.5.0,
   `realesrgan-ncnn-vulkan-20220424-windows.zip`).
3. Make the pack:
   ```bash
   python tools/tmb_texture_upscale.py <texture_dump> "%APPDATA%/TMB_Recomp/texture_pack" --esrgan <folder>/realesrgan-ncnn-vulkan.exe
   ```
   The tool needs Python with numpy and Pillow. It upscales 4x with `realesrgan-x4plus` over borders taken from
   the opposite edges, so repeating textures stay seamless, then adds the original's fine grain back, because the
   model flattens gravel and rust.
   - **Alpha:** keeps the GS alpha convention. See-through areas stay exactly see-through: the visible shape is
     smoothed, a few-level alpha is snapped back to its levels, and nothing spills into transparent texels.
   - **Flat areas** (a texel equal to its 8 neighbours) are kept exactly, so a black background stays black.
   - **Left out:** the menus and the font (`--skip`, default `SHELL1,FONT`) are left to the engine's upscaling,
     because the model garbles small lettering. Textures under 32 texels are left out too.

   With `PS2X_TEXTURE_PACK_RELOAD=1`, changed files are drawn again while playing.

Packs are the player's own: never commit, ship or share them.

**Measured** (Junkyard 1P route, 273 replacements from that route's dump, RTX 5070 Ti, Ryzen 7 7800X3D):
- Making the pack took 28 s, 11 s of it in Real-ESRGAN, and produced 71 MB of PNGs.
- In game, each replacement is read in about 7 ms on a worker. The 1P route kept 241 MB of copies and sent 309 MB
  of textures to the GPU.
- Frame pacing, measured with the bench lock held over the match's 2-second windows, holds 60:

  | Run | Game fps min / avg | Presents min | Longest gap |
  |---|---|---|---|
  | 1P, upscaling off (baseline) | 59.32 / 60.00 | 57.5 | 88 ms |
  | 1P, 4x upscaling | 59.47 / 60.03 | 57.5 | 47 ms |
  | 1P, pack | 59.58 / 60.00 | 59.9 | 25 ms |
  | 1P, pack + 4x upscaling | 59.72 / 60.02 | 59.4 | 28 ms |
  | 4P baseline | 59.94 / 60.00 | 59.9 | 22 ms |
  | 4P, pack + 4x upscaling | 59.90 / 60.00 | 59.9 | 22 ms |

  Before BozoEngine's upload budget (12 MB of larger copies per present), 1P at 4x upscaling fell to 31 presents/s
  for about 20 s while the arena's copies arrived. One of three 4P pack + 4x runs had a single 237 ms gap at a
  moment when no texture was sent; the other two did not.

**Full pack** (all arenas and cars, `tmb_texture_dump_all.py` then `tmb_texture_upscale.py`):
- The dump takes about 10 minutes with 3 headless jobs. The pack has 2,164 replacements (609 MB of PNGs) and
  takes about 4 minutes, 85 s of it in Real-ESRGAN.
- Final timing, bench lock held, game fps min / avg after the level load:

  | Scene | Off | Pack + 4x upscaling |
  |---|---|---|
  | Junkyard 1P route | 59.63 / 60.00 | 59.57 / 60.02 |
  | Suburbs | 59.91 / 60.00 | 59.40 / 59.98 |
  | Downtown | 59.66 / 60.01 | 59.91 / 60.00 |
  | Prison | 49.33 / 59.27 | 47.83 / 58.65 |
  | 4P | 59.57 / 59.89 | 59.08, 59.51, 59.96 / 60.00 (three runs) |

  Prison's story route dips below 60 with everything off too; in an earlier pair the pack run was the better one.
  Downtown and 4P show occasional present-only stutters (the game stays at 60) with or without a pack, with no
  texture uploads at those moments. That is an existing presentation issue, separate from textures.
- Two fixes came out of this:
  - The engine's texture workers run at the lowest thread priority.
  - The engine no longer makes an upscaled copy of a texture whose replacement is being read.
- **The menu hints bug:** "select" and "back" were drawn over dark boxes. The model tinted their black background
  and bicubic alpha spread into transparent texels. Fixed by the flat and alpha rules above and by leaving the
  menus to the engine.

**What it looks like:** ground, walls, rust and decals gain real detail, and edges are clean. The model can invent
shapes from blurred stripes, for example "teeth" on one truck's radiator panel. Hand-fixing a file and reloading is
the remedy. Textures the dump never saw (other arenas, cars, the story) are covered once they have been dumped.

## 1. Where textures come from

### Offline: the texture pack

`tools/tmb_texture_pack.py` converts the player's own disc during the build's `packs` step (the launcher's
"Prepare the game"). The output is `tmb_textures.pack` (BozoEngine's `PS2XTEX1`), which is never committed. For
each disc file (`*.TEX`, `*.RTX` and the ELF's font as `FONT`), it holds the records in the order the game sets
them up. Each record stores its PSM, width, height and unpacked texels: one byte per index for T4/T8/H formats,
and 2 or 4 bytes per texel for CT16/CT24/CT32.

On the current USA disc, the pack has 109 files and 14,105 records. About 6,800 are direct-colour records,
mostly CT32 palettes. About 6,800 are T4/T8 textures and about 500 are H-format resident textures. The largest
textures are 256×256. Decoded as RGBA8, the whole pack is about 238 MiB, and the largest level file
(`SUBURB.TEX`) is about 14 MiB. A level loads one `.TEX` plus the shell, car and resident files.

### At run time: `runtime/tmb_textures.cpp`

The runtime never reads GS memory or the game's texture records. It follows the game's texture code at hooks:

| Hook | What it records |
| --- | --- |
| `fileReadf`, `zipInflateAll` | which disc file was read to which address |
| `fileOnlyTexFile` / `fileAddTexFile`, `fileOnlyResFile` / `fileAddResFile` | the level's `.TEX` and `.RTX` lists, in order |
| `texmSetupTexture` | the n-th setup is the pack's n-th `.TEX` record, keyed by the game's texture id |
| `sceGsSetDefLoadImage` (from `texmLoadResTexture`) | the n-th `.RTX` record and the VRAM block it was loaded to (palettes, H textures) |
| `fontInit` | the font's VRAM block |

Each hook checks the game's PSM and size against the pack record. `lookupLocked(tex0, texa, id)` then turns a
draw's `TEX0` into a `bozo::render::Texture`: texels come from the texture id (meshes, particles) or from TBP
through `texInfo`. The palette is assembled from the RTX records placed at CBP (CSM1 layout, CSA for T4), and
alpha follows `TEXA`. `bozo::render::packs::decodeRecord` produces RGBA8 with **GS alpha (0x80 = 1.0)**. Results
are cached per (texels record, palette record, TEX0 without the clamp bits, TEXA). The cache is cleared at each
`texmInit` / `texmResInit` / `texmLoadResTexture`.

The consumers are:

- **Meshes** (`tmb_meshes.cpp:729`): `lookupId(obj+0x50[slot], TEX0 from the object's register block, 0)`.
- **Particles** (`tmb_particles.cpp:425`): `lookupId(id, tex0, kTexa)`.
- **Font and 2D** (`tmb_2d.cpp:160`): `tmb::textures::font()`. The other HUD/menu builders draw untextured
  geometry (`drawOverlay(..., nullptr)`). Today, the font is the only image-based 2D asset.
- **Movies** (`tmb_2d.cpp` `moviePicture`): FFmpeg-decoded pictures, a new image each frame (key `0x4d4f5649`).
  They are out of scope for texture replacement.
- **Render targets**: heat haze (`Material::sampleTarget`) and aux targets (`fromFrame`, `target`, `source`).
  These are frame copies, not assets, and are out of scope.

### In the engine: `BozoEngine/runtime/render/`

- `renderer.cpp` hashes each texture into the **draw-list hash** that recordings and replays compare: the XXH64
  of its RGBA, seeded with the XXH64 of `{width, height}`.
- `present_gl.cpp` `Gpu::texture(key, source)` uploads each `Texture` once per (TEX0, TEXA, region) key as
  `GL_RGBA8`. The upload uses `GL_LINEAR` (or `GL_NEAREST` per material), **no mipmaps**, and REPEAT or
  CLAMP_TO_EDGE per material. `overlayTexture` converts GS alpha to 255 for the 2D.
- The vertex UVs are **normalised (0..1)** (`Vertex::u/v`, the GS's S/Q and T/Q), and TMB never uses
  REGION_REPEAT. A texture with N times the pixels therefore needs no UV, draw or game change.

### The substitution point

There are two candidate places:

1. **In `lookupLocked`** (TMB), replacing the decoded `Texture`. This is simple, but it changes the draw-list hash.
   Every recording and replay comparison would then depend on the installed pack, so it is rejected.
2. **At the GPU upload in `present_gl.cpp`** (engine): `Gpu::texture` / `overlayTexture` look up the texture's
   identity in a replacement set and upload the replacement instead. The game, the `Texture` object, the draw
   list and its hashes stay unchanged, and a replacement is purely presentation. **Recommended.** This needs a
   public BozoEngine change first (team rule), and the change is generic, so every port on the engine can use it.

## 2. Stable identity

**Primary key: the decoded image.** The key is the XXH64 of the RGBA8 texels with GS alpha, seeded with the
XXH64 of `{width, height}`. It is exactly the value the renderer already computes for the draw-list hash
(`textureIdentity`), so the dump's file names, the replacement lookup and the recorded draw hashes share one
identity.

- It is stable across runs, machines and builds, because it depends only on the disc's data and the decoders.
  It changes only if a decoder changes, which would also be visible in the draw-hash comparisons.
- Palettes are already folded in: a T4/T8 record shown with two palettes (vehicle colour variants, a different
  CSA) gives two images and two identities. Each variant needs its own replacement, which is correct, because
  the colours differ.
- The `TEX0` size can exceed the record (`decodeRecord` pads with zeros), giving a different image and a
  different identity. That is also correct.
- Identical images from different records (common for small solid or gradient textures) share one identity
  and one replacement, which is a feature.

**Secondary keys, kept in the dump's index only:**

- `texel_hash`: the XXH64 of the record's texels, seeded with `{psm, width, height}`. It groups the palette
  variants of one record, so a tool can upscale them as a batch or apply one artist mask to all of them.
- The record's origin (`JUNKYARD.TEX#12`, `CARS.RTX#3`, `FONT#0`), the palette's origin, PSM, TEX0 and TEXA.
  These are diagnostics only and are not used as keys, because a record's ordinal depends on the pack layout.

**What could complicate this, and how TMB fares:**

- **CLUT animation.** The native path takes palettes from the pack, not from live EE memory, so a palette is
  static per record and VRAM block. If the game rewrote a palette from EE memory, the native renderer would not
  show it either. Animated surfaces in TMB scroll their UVs (the texture offset at `obj+0x20/+0x24` or the float
  table), which normalised UVs handle at any resolution.
- **Mipmaps.** The game's mip table (`texmActivateTexture`, up to seven levels) is bookkeeping only. The pack and
  the renderer use level 0. Replacements are identified by level 0 alone, and their mips should be generated on
  the GPU (see 4).
- **Render targets and movies.** They have no identity worth keeping and are excluded. The dump never sees them,
  because they never pass through `lookupLocked`.
- **GS alpha above 0x80.** Most of TMB's textures use it: in the measured run, 204 of 274 have texels above 0x80,
  and 181 reach 0xFF (2.0), mostly through palettes. The shader multiplies before clamping, so these values
  matter. The dump therefore stores the GS alpha byte **unchanged** (128 = opaque), which makes a PNG round-trip
  to the same identity. Tools that want straight alpha must convert it themselves and keep the range above 1.0.

## 3. Upscaling options and legality

| Option | Quality | Cost to the player | Legal footprint |
| --- | --- | --- | --- |
| **A. Offline AI upscaling** (ESRGAN family) into a pack built on the player's PC | Best for world, vehicle and photographic textures; models trained on game art also suit the HUD and font | One-time GPU job per level/car set; disk space for the pack | Clean if the pack is generated locally from the player's disc and the upscaler and model licences permit redistribution or download |
| **B. Runtime pixel-art filter** at upload (xBR/xBRZ, ScaleFX-style) | Good for the font and flat, aliased art; looks painterly on photographic textures | CPU or GPU work at upload; no pack | No game data at all; check the filter's code licence (xBRZ is GPL-3.0 and cannot go into an MIT runtime; the ScaleFX/xBR shaders need checking) |
| **C. Better sampling only**: mipmaps, trilinear and anisotropic filtering | Removes shimmering at distance; no new detail | Negligible | None |

**Recommendation: C first, then A as an optional, opt-in "Enhance textures" step. B only for the font, if anyone
asks for it.**

- **C** is a pure renderer improvement that every texture benefits from, and it is a prerequisite for A:
  upscaled textures without mipmaps alias badly at distance.
- **A** gives the "beyond the original" look the owner asked for. Run the upscaler outside the game:
  [Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan) is a single executable on any
  Vulkan GPU with no Python, and its code and the `realesrgan-x4plus` weights are BSD-3. Many community models
  are CC-BY-NC or carry no licence, so only explicitly licensed models may be offered by default.
- **Never ship game-derived data.** No PNG, replacement pack, palette or texel dump goes in the repository, a
  release, the build kit or a download. The pack is built on the player's PC from their own disc, like
  `tmb_textures.pack` and `tmb_meshes.pack`. A list of identities and record origins with **no pixels** (the
  dump's `index.tsv` holds only hashes, names and register values) is not game data, and could ship to tell the player's
  tool which palette variants exist (see plan step 5).
- **Hand-made packs** from the community are outside the project. The loader would accept them as a local folder,
  like any player-supplied file, but the project neither hosts nor links them.

## 4. Feasibility

**VRAM.** A level is about 10–14 MiB of RGBA8 today, plus the shell, cars and resident textures. At 4× (16× the
texels) with a full mip chain (×1.33), one level is about 200–300 MiB as RGBA8, or about 50–75 MiB as BC7. Both
fit easily on any GPU that runs the port. At 8×, the RGBA8 figure is four times that again. BC7 compression at
pack-build time, uploaded with `glCompressedTexImage2D` behind the extension check, is the recommended path past
4×. The texture cache is cleared per level, so replacements are released with it.

**Load time.** Decoding PNGs at runtime is slow (about 10–30 ms for a 1024² image). For development only, the
loader should read PNGs directly from a folder. For players, the "Prepare" step converts the PNGs into one
container with an identity index and, ideally, pre-built mips and BC7 data, read by memory-mapping. A replacement
is loaded at its first upload; with an mmapped container that costs well under a millisecond per texture. A level's
replacements can also be read ahead at `texmInit` on a worker thread.

**UV, wrap and clamp.**

- UVs are normalised, so they need no change.
- The replacement must have the **same aspect ratio and an integer scale** of the original. The loader rejects
  anything else and logs it.
- Repeating textures need **seamless** upscaling: before upscaling, pad the image by wrapping it (tile it 3×3 or
  add a wrapped border), upscale, then crop the centre. Clamped ones are padded by edge extension. The upload
  path knows the wrap mode per material, not per texture, so the dump should also record whether an identity was
  ever drawn with REPEAT (plan step 2).
- The game's `nearest` materials (`Material::nearest`) keep nearest sampling, which suits a pixel-art look. They
  will not look smoother unless they switch to linear filtering when a replacement is present.

**Alpha.**

- 3D textures go to the GPU with GS alpha (0..0xFF, where 0x80 is 1.0), and the shader multiplies by 255/128.
  Replacements keep that convention, as the dump writes it. Two concerns remain: an upscaler must not clip the
  alpha channel, and opaque texels (exactly 128) should stay exactly 128 after resampling.
- Alpha-tested cutouts (fences, foliage, particles with `alphaTest`) need their alpha upscaled separately with a
  sharp, threshold-preserving filter (nearest, or bicubic followed by a threshold at the material's `alphaRef`),
  then recombined.
- Before upscaling, bleed the colour of opaque texels into the transparent ones to avoid dark fringes, because the
  upscaler sees straight, not premultiplied, alpha.

**Plugging in.**

- **Engine (BozoEngine PR first):** `bozo::render::Replacements` holds an identity → image map with lazy load, and
  `Gpu::texture` / `overlayTexture` consult it before `makeTexture`. Add mip generation and `GL_LINEAR_MIPMAP_LINEAR`
  plus anisotropy for all textures (the C option) behind a video setting. Replay determinism holds because nothing
  above the GPU upload changes.
- **TMB:** call `Replacements::load(<user folder>/textures/…)` at startup when the setting is on, and add a "Textures"
  row in the video settings tab (Original / Enhanced) with a hot reload.
- **Launcher:** an optional step after "Prepare the game", "Enhance textures". It dumps or regenerates the textures
  from the player's pack, runs the upscaler, and builds the container in the user folder. It shows progress, can be
  resumed, and can be removed.

**Development workflow: dump, replace, hot-reload.**

1. Run with `TMB_TEXTURE_DUMP=1` and a scratch `PS2X_USER_DIR`. Each texture the renderer is given appears once as
   `texture_dump/<identity>.png`, with an `index.tsv` line.
2. Upscale or edit any PNG and put it in `<user>/textures/replace/<identity>.png` (the planned layout).
3. A hot-reload key (planned) re-reads that folder and drops the GPU copies of changed identities. The game itself
   is unaffected.

## 5. Plan

1. **Done (this change): texture dump.** `runtime/tmb_texture_dump.{h,cpp}`, with hooks in `tmb_textures.cpp`
   (`lookupLocked`, `font()`). It is off unless `TMB_TEXTURE_DUMP` is set: `1` writes to
   `$PS2X_USER_DIR/texture_dump`, any other value is a folder. It has no effect on the game, the renderer or the
   hashes. The PNG writer is dependency-free because the headless programs do not link `bozo_app`.
2. **Dump coverage.** Record per identity whether it was drawn with REPEAT or CLAMP and whether it is alpha-tested
   (from the mesh and particle batches). Write a script that plays every arena and car (existing pad scripts and
   `tools/arena_script.py`) to dump everything, then report the counts per level.
3. **Engine PR: sampling (option C).** Mipmaps, trilinear and anisotropic filtering behind a video setting
   (default on after visual checks). This is independently useful.
4. **Engine PR: replacements.** An identity-keyed loader (a PNG folder for development, a container for players),
   upload in `present_gl.cpp` (GS alpha kept), integer-scale checks, hot reload, and a log line
   per replaced texture. Add a test that the draw-list hash is identical with and without replacements.
5. **TMB: settings and launcher step.** Add the "Textures: Original / Enhanced" setting. Add the launcher step:
   regenerate the textures from the player's own pack, upscale them with a pinned upscaler and model (downloaded
   from upstream with a checksum, or included in the build kit only if both licences allow it), pad for wrap,
   handle alpha, encode as BC7, and write the container. To avoid making players play everything first, the
   generator can use a shipped **manifest without pixels** (identity, record origins, palette origin, PSM, TEX0,
   TEXA) from step 2's full dump. It reproduces each image from the player's pack and checks the identity (a
   mismatch means a different disc revision, and that entry is skipped).
6. **Qualification.** Recordings must be identical with and without a pack. Check visuals per arena, VRAM and
   load-time measurements (bench lock only for those), and a 4P run.

**Risks:**

- Upscaler output quality varies, and text and HUD need different models than world textures.
- AI artefacts on tiny textures (8×8 and 16×16 make up half the records): skip textures below a size threshold
  or upscale them with plain filtering.
- Seams on tiling textures without wrap padding.
- Disk and time cost of the launcher step (minutes to an hour, depending on GPU and scale).
- Licence drift of models.
- Identities change if a decoder fix lands. Mitigation: the dump index records `texel_hash` plus origins, so a
  tool can remap old replacements to new identities.

## 6. Prototype results

The dump was validated on a headless US build of this branch on the established 1P route
(`ch1p_frames.txt`, 9,500 frames: boot movies, shell, Junkyard match). Each run used a private `PS2X_USER_DIR`.
Local evidence is in `D:/BozoScratch/texture-upscaling`, and none of it is committed.

- The recordings with and without `TMB_TEXTURE_DUMP=1` are **byte-identical** (`.ref`, 760,240 bytes).
- The run wrote 274 PNGs (15 MB) with 274 index lines, and no texture had an unknown origin. By file: JUNKYARD.TEX
  165, SHELL1.TEX 30, JUNKYARD.RTX 28 (H textures), car files 51 (MGA, STA, RLA, SUA, …) and FONT 1. By size:
  256² 28, 128² 113, 64² 104, 32² 28, 16² 1.
- Of the 274 identities, only two pairs share a texel record (palette variants). This route shows mostly a single
  palette per record.
- All 274 PNGs decode back to RGBA whose XXH64 equals the file's name, the renderer's identity
  (`verify_dump.py` in the evidence folder, an independent Python XXH64).
- The font and a Junkyard ground texture were inspected visually.

This is one route. The full dump that steps 2 and 5 need (all arenas, all cars, story, 4P) is not done yet.
