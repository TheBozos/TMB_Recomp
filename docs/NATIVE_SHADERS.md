# Native mesh shader parameters

The native mesh path no longer builds or reads a `VuImage`. `tmb_shader_parameters.h` defines named
object/camera/clip matrices, directional and point lights, tint, texture offsets, depth scale, fog and
morph weight. `tmb_shading.h` consumes those records directly for strips, sprites, ribbons, skin colours
and reflections. There is no quadword address, wraparound, `setQw`, word bank or packed-input fallback
in this runtime path. BozoEngine remains pinned at `4a8f70a`.

Scene light selection now produces per-light direction, position, colour, cone and attenuation fields
directly. It retains selection's four-light limit, the shipped shader's three-light contribution, the
eighth-input colour alias and the original off-diagonal move rounding. The old eleven-row light packing
is gone; the fourth selection's colour is a normal light field rather than other colours' W components.

Each object gets fresh parameters. The scene's run owns the last opaque transform/directions and retained
morph weight. Alpha headers do not replace the opaque cache. A pool-less rigid resume retains its setup,
projection and lights under the existing rules; a morph without a new instance keeps the last weight.
View setup refreshes default directions and invalidates the previous draw. Historical checker inputs
are decoded from their original context ABI into the same native parameter types; player hosts read
scene records. An unused `Drawn.check` snapshot of packed constants is also removed.

The previous project-owned shading implementation, including its packed input, is retained only under
`tests/fixtures`. It is a numerical reference, not a runtime adapter or microprogram interpreter.

## Validation

Local evidence: `D:/BozoScratch/shader-parameters`, against main `9d7372f`, the same USA disc, engine and
input scripts. All runs use isolated user folders. Builds and checks here are functional, not timing runs.

- All six native suites and 12 release-tool tests pass. The player, headless and historical checker build.
- The new disc-free shader suite compares 32,768 synthetic parameter sets in four host FP modes with
  the previous packed implementation: all three strip colour modes, lit/unlit skin colours, scaled and
  unscaled projection, fog, 16,401 visible sprites, 147,912 emitted ribbon vertices and dry/water
  reflections agree bit-for-bit. Ribbon fixtures cover emitted geometry, parity and clipping. These
  finite inputs do not establish arbitrary NaN payload equivalence.
- The scene suite still passes 32,768 complete transform/FOV/LOD sequences, 32,768 original light
  selections and 96,684 actual point-shading comparisons. Active light fields are checked directly;
  unused positions and the fourth selection's unconsumed geometry are excluded. Binding isolation and
  destruction/reuse now also check retained morph state. Generated-helper routing and VU sentinels pass.
- The shader suite also builds/runs directly with GCC and only the runtime include folder: no disc,
  generated game C++, SDL or engine library is needed. Windows/Linux CI runs this standalone check.
- Default 1P (9,500 frames) and actual 4P (8,500) recordings are byte-for-byte identical to main's complete
  recordings, including RAM hashes, scratchpad, draws/counts, sound/samples, calls, clocks and threads.
  Both corresponding strict recordings are also identical to main and to their default runs. Native
  defaults still execute zero VU0 programs; strict collision diagnostics retain their explicit oracle.
- Windowed movie/menu (4,201 frames) and 1P/4P (6,501 each) finish through loading into active arenas.
  Captures were inspected directly for decoded movies, readable menus/loading, textured vehicles,
  lighting, fog, HUD/effects and four independent snowy views. All five movie captures exactly match
  the prior batch's PNGs. Real-time gameplay captures vary even between two runs of the unchanged
  saved baseline executable; they are visual smoke evidence, not a pixel-parity assertion.
- Device logs retain dropped frames and 23.4 / 55.5 / 26.4 ms of inserted silence in movie/1P/4P runs.
  Exact deterministic sound is not a claim of flawless device audio or subjective listening acceptance.

## Remaining scope

Other generated VU macro math and game helper callers still remain, as do scalar packet builders,
legacy graphics extents and game-owned constant buffers used by several hooks. This change removes
the mesh shader's compatibility parameter storage; it does not claim total emulation removal, full
campaign qualification, console visual parity or a performance improvement. It publishes source
changes without updating the earlier player release or migrating the engine pin.
