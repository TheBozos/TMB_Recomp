# Native hierarchy collision calculations

The later [BSP/vehicle inverse increment](NATIVE_BSP_INVERSES.md) extends the eleven-entry allowlist
described below to fifteen and records its paired continuation checks and contact-W RAM boundary.
The subsequent [sphere/LOD increment](NATIVE_SPHERE_LOD.md) adds five collision-node/point returns,
bringing the inverse allowlist to twenty, and qualifies their scalar consumers.
The later [helper-consumer increment](NATIVE_HIERARCHY_HELPERS.md) extends it to twenty-four and
also retires eight height/BSP/line-of-sight forward returns.
The subsequent [query/lighting increment](NATIVE_HIERARCHY_QUERY_RESULTS.md) extends the inverse
allowlist to twenty-six and the forward allowlist to twenty-seven.
The later [BSP crossing increment](NATIVE_BSP_CROSSINGS.md) brings both allowlists to thirty-three,
with actual scalar crossing-point emission and meaningful inverse-W overwrite checks.

This increment replaces three calculation boundaries in the USA collision/height hierarchy. The player path
uses game records directly and leaves VU registers, accumulator, flags and data memory untouched.
BozoEngine stays at the previously qualified `4a8f70a`; no engine API or renderer change is included.

- `hdHatSphereTestAsm` (`1cf7c0`) now calculates the height-test sphere/projection result directly. It retains
  the game's strict boundary rules, separately rounded arithmetic and scalar large-object shortcut. The
  shortcut returns without reading any vector pointers. Radius comes from the first record's W; the scalar
  argument controls the shortcut. The old `0xab0` microprogram is not called on this player path.
- Eleven audited `mathfMulVec` (`1da1e8`) call sites in `hdColGrid`, `hdHatTraverse`, `hdTraverse` and animated
  hierarchy nodes use native inverse direction transforms. Inputs are snapped before writing, including
  overlapping records. All four output components match: W is copied from the first basis record.
- `bspPlaneTest2Vec` (`201660`) uses native plane-distance pairs for `bspVecTestIn` (return `20134c`). The
  two dot products keep their different X/Y addition orders. Only XY belongs to the distance result; Z/W
  remain the destination's existing payload. The BSP caller reads only XY and performs its interpolation,
  recursive branches, classification writes, timing and checkpoints in the existing scalar generated code.

Other inverse-transform callers and other plane-helper entries retain their original complete ABI. In
particular, the unused `bspInterp` helper expects the old register inputs; it is not silently redirected.
The historical checker retains the original helpers. `TMB_NATIVE_HIERARCHY_CHECK=1` independently executes
original helpers on copied contexts and compares their meaningful outputs; normal player runs do not.

## Validation

Local artifacts are under `D:/BozoScratch/hierarchy`, against published TMB `ce7eb9f` / engine `4a8f70a`.

- `tmb-native-hierarchy-tests`: 786,432 original/native helper cases, 384 complete flat-memory comparisons,
  8,000 geometric boundary/degenerate cases and 1,024 complete recursive BSP callers pass. Another 32
  complete-memory comparisons cover the callers, with the documented XY/preserved-ZW record contract.
  These check saved registers, scalar interpolation, classifications, return and exact cycle/checkpoint
  counts. Another 2,048 cases check the original ABI of unaudited inverse/plane callers. Native helpers
  make no microprogram calls, preserve VU state and restore the caller's complete MXCSR.
- Four host FP modes, denormal controls, record overlap, RAM aliases, low address bits and scratchpad are
  covered. Inverse/plane comparisons include arbitrary input bits. Sphere geometry comparisons use finite
  coordinates/radii, including extreme exponents, signed zero, tangencies and zero-length segments. They
  do not promise the retired microprogram's incidental IEEE NaN payload/sign propagation.
- Existing native matrix/direction, HUD and vehicle suites pass, together with 12 release-tool regressions.
- Strict 1P (9,500 frames) and 4P (8,500) complete with 4,977,182 / 1,560,611 original helper comparisons
  and zero mismatches. The sphere wrappers run 1,064,321 / 105,211 times (896,184 / 68,984 scalar shortcuts);
  native BSP pairs run 3,853,147 / 1,453,288 times. Six inverse sites are reached by 1P and two by 4P;
  the other five sites have differential unit coverage, without a live-route coverage claim.
- Separate default runs make zero original comparisons and reproduce both strict recordings byte-for-byte.
  Against main, every frame has identical draw hashes/counts, sound hashes/sample counts, library call hashes/
  counts, EE clocks, scratchpad and thread records. RAM differs from frame 5591 / 4700; see below. Default
  runs avoid 168,137 / 36,227 height-test microprogram invocations; this is a call count, not a speedup claim.
- Windowed default runs complete 4,201 movie/menu frames and 6,501 frames each for 1P and 4P, through
  loading into active arenas. Direct captures show decoded movie pictures, menus, textured combat, HUD,
  effects and four snow-arena views with driving/firing. Audio logs report 15.9 / 34.7 / 29.4 ms of silence
  and some dropped device frames. Deterministic sound is exact; flawless device audio or subjective
  listening acceptance is not claimed. These are bounded smoke routes, not full campaign qualification.

## RAM boundary

Removing register clobbers deliberately changes raw RAM left by subsequent full-vector stores. A temporary
write trace identified `bspPlaneTest2Vec` at its audited caller storing stale VF12 Z/W into recursive stack
records. Its scalar caller reads only the two distances. The native helper now preserves those unused
destination components, without transferring register state.

At the 2,000-update gameplay snapshot, the final native build differs from main in 330 words: 329 in the
inactive stack range `8fdbe8..8ff358`, plus `23decc`. The latter is a fourth-component padding word copied
by `mathfMulMatrix3x3` (`1da428`, return `1ee020`) in `RigidBody::computeAuxiliarys`: the temporary input
matrix initializes only its three numerical rows/components. This trace is evidence for that snapshot,
not a whole-campaign proof that every raw-RAM difference is harmless. No stale register restoration,
padding restoration, shadow VU bank or widened memory-hash mask is added. Temporary tracing is absent
from the regenerated production output and the engine checkout remains clean.

## Remaining work

The later [traversal/setup increment](NATIVE_HIERARCHY_TRAVERSAL_SETUP.md) extends inverse/forward
qualification to database traversal and older collision, terrain and animated-node setup.
The [older BSP/root increment](NATIVE_LEGACY_BSP_HELPERS.md) adds setup/result transforms and
root/grid/traversal inverse leaves, retaining real query and traversal dispatch.
The [point/ray increment](NATIVE_POINT_RAY_HELPERS.md) adds coordinate-system/root inverses and
complete point-contact/ray result tails.

The subsequent [`NATIVE_SCENE.md`](NATIVE_SCENE.md) increment replaces the renderer hierarchy's root/child
matrix stack, FOV, distance/LOD and point-light selection together with their producers, accessors and mesh
consumers. Other game math helpers and VU macro blocks remain. Neither increment claims complete VU
removal or full campaign/console visual parity.
