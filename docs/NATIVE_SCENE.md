# Native renderer hierarchy

The USA player's renderer hierarchy now uses typed scene, frame, matrix, clip and light records. Its
producers, traversal decisions, C getters/setters and mesh consumers change together. There is no
compatibility register bank, addressable VU memory, uploaded image selector or microprogram dispatch in
this calculation path. BozoEngine remains at the independently qualified `4a8f70a`.

## Boundaries

`runtime/tmb_scene.cpp` contains record calculations; `runtime/tmb_scene_hooks.cpp` connects 197 audited
instruction sites to the existing scalar game traversal. These cover view-cache inputs, root/coordinate/
child transforms, inner/outer FOV tests, object projection, light selection, scaled distance/LOD,
position save/restore and matrix accessors. Scalar branches, bookkeeping, checkpoints and game memory
records retain their existing owners. Packet-only vector payload stores are retired.

`tmb_meshes.cpp` consumes the native object matrix, directional/selected point lights and alpha depth.
Its original context reads are confined to the historical checker host. Each player runtime owns a
scene through a game-thread `Binding`; destruction releases it and a subsequent run starts fresh.
Windowed hosts bind inside `game.run`, where traversal actually executes. C setters may initialise a
frame one basis at a time; reads require the requested basis and traversal requires the complete frame.
Invalid indices, incomplete light lists and uninitialised frames fail explicitly.

The owner ELF has 21 `callms` sites: 14 are replaced here; the native height helper already bypasses one;
the six old 2D helper sites reject if reached. All 21 sites are therefore adapted or guarded. Ordinary
native overlays do not use those six helpers in the qualified routes. Reaching another unsupported
route is an error, rather than silently invoking compatibility microcode. Only explicit
`TMB_NATIVE_HIERARCHY_CHECK=1` permits the existing height oracle to execute the original helper on a
copied context; that diagnostic saves/restores its library image state, including on exception.

Arithmetic retains the shipped separately rounded/truncated operations, including the scaled-X factor
on the light-selection Y-squared term. The eighth input colour originally aliases the first selected
colour: selection clears that slot, then the first accepted light replaces it. Native records preserve
this particular dependency without retaining its memory layout. Unused light-position slots and their
padding are zeroed; active numerical lanes and resulting shading are compared below.

## Validation

Local artifacts: `D:/BozoScratch/renderer-hierarchy`, compared with TMB main `31f54d1` and the same engine,
disc, scripts and build configuration. All game runs use isolated scratch user directories. These are
functional checks, with no timing or speedup claim.

- `tmb-native-scene-tests` passes 32,768 complete root/camera/three-child/FOV/object/LOD sequences in four
  host FP modes against the original hierarchy programs, plus 32,768 light selections and 96,684 actual
  point-shading comparisons. Geometry uses finite inputs; arbitrary IEEE NaN payload/sign propagation
  is not promised. Only inactive position lanes/padding are excluded from light-record comparisons;
  the complete rendered colours are compared without that exclusion.
- The actual freshly generated view-cache, coordinate, child and getter/setter helpers execute through
  the production instruction dispatcher with no microprogram/device service. Complete VU context and
  4 KB VU-memory sentinels stay untouched. Partial frame construction, rejection of an unsupported
  program, separate runtime owners and destruction/address reuse pass.
- Existing native matrix/direction, HUD, vehicle and collision-hierarchy suites pass, as do all 12
  release-tool regressions. Player, headless and historical checker targets build successfully.
- Default 1P (9,500 frames) and 4P (8,500) complete with **zero VU0 program calls**. Every frame's draw
  hash/count, sound hash/sample count, library calls, EE clocks, scratchpad and thread records match
  main. RAM is different, as bounded below. This does not imply zero hardware accesses throughout the
  rest of the port.
- Separate strict runs reproduce both default recordings byte-for-byte. Their copied-context collision
  helpers perform 4,977,182 / 1,560,611 original comparisons, including 168,137 / 36,227 diagnostic-only
  height microprogram calls. Scene comparisons are in the dedicated test suite, not this live flag.
- The 1P route executes 949,504 coordinate transforms, 433,552 child transforms, 4,998,340 FOV tests,
  1,436,327 object projections, 879,562 light selections and 894,750 LOD calculations. The 4P route
  executes 1,319,416 / 648,187 / 6,524,809 / 1,982,686 / 0 / 769,035 respectively. Live point-light
  coverage therefore comes from 1P; unit tests also exercise eight-input selection and the colour alias.
- Windowed default runs finish 4,201 movie/menu frames and 6,501 frames each for 1P and actual 4P,
  through loading into active arenas. Direct capture inspection covers decoded movie pictures, menus,
  the stage-loading screen, textured gameplay, vehicle/HUD effects and four independent snowy views.
  Audio logs report 18.0 / 35.9 / 54.5 ms of inserted silence and dropped device frames. Deterministic
  sound remains exact; flawless device audio or subjective listening acceptance is not claimed.

## RAM boundary and remaining work

Raw RAM differs from frame 2 because retired vector stores no longer populate unused graphics payloads.
At the 2,000-update gameplay snapshot, all 17,256 differing words lie in the owner's named packet-storage
intervals (names and starts from the USA ELF symbols):

| Packet storage | Interval, end exclusive | Different words |
|---|---|---:|
| `hierObjPkt` | `564a80..5eca80` | 154 |
| `hierObjPkt2` | `5eca80..630a80` | 8,595 |
| `hierAlphaPkt` | `630a80..64ea80` | 4,554 |
| `hierAlphaPkt2` | `64ea80..66ca80` | 3,953 |

There are zero differences outside these intervals at that snapshot. `hierGetBigBufAddr` also returns
`564a80`. This evidence bounds that snapshot, not every intermediate frame or campaign path. No stale
packet/vector restoration, shadow register bank or broader memory-hash mask is added.

Other game math helpers and VU macro instructions remain. The native shading adapter still packs its
parameters through `VuImage`, and scalar packet-building code still survives around several native
hooks. The historical checker/original program implementation remains for comparison. This increment
does not claim total VU/emulation removal, full campaign qualification or console visual parity.
