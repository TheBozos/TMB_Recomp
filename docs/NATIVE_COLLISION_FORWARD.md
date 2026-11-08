# Native vehicle-collision forward transforms

Six vehicle-collision forward returns use native XYZ record arithmetic after the
[driver forward increment](NATIVE_DRIVER_FORWARD.md). Coverage grows from 113
to 119 of 269 known forward returns (150 remain). Known inverse coverage stays
134 of 134. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `11ad1c`, `11ada0` | `collisCar2CarCollision` | Vehicle velocity/speed reconstruction, real relative-mass and coupled force calculation, through the effects-dispatch boundary |
| `11b26c` | `collisApplyForce` | Complete velocity update, normalization, speed, matrix, rotation and state-flag publication |
| `11bba8`, `11bbe8` | `collisSetCar2CarFlags` | Heading/angle producer prefix, impact-offset selection, translated take-hit point publication and damage staging before hit dispatch |
| `11c368` | `collisCar2CarMorph` | Complete velocity difference, meaningful inverses, scaled hit position and hit-location/state publication |

Native arithmetic retains separately rounded XYZ, destination W and untouched VU
registers, accumulator, flags and data memory. The original scalar collision,
mass, force, rotation, angle and hit-location logic remains. Unknown/historical
callers keep the original ABI. No engine or collision tuning is changed.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/contact-forward`, against immediate parent
`555f9b3` (PR #52 plus its UTF-8 documentation correction, PR #53). Generated
bodies, retail data and runtime artifacts stay outside Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons.
  Each return has 256 helper-boundary sequences through its actual consumers and
  256 producer-entry sequences in four rounding modes, with FTZ/DAZ, RAM aliases,
  rotated bases, zero/tiny/signed vectors and opaque basis/destination W payloads.
- Collision producer entries start at the actual owner entry and retain the
  velocity-reconstruction type/enable gates, signed speed bookkeeping, actual
  relative masses, both inverse projections and both coupled force applications.
  Controlled fixture thresholds enable the force paths. They stop at `11b2e8`,
  before the separate angles/effects owner. Effects, later collision flags and
  hit clearing are not claimed as complete-owner coverage by these fixtures.
- Force entries complete the real owner, including retained/scaled velocity,
  transformed force addition, normalization and mass-divided speed, matrix and
  Euler rotation publication, and the state flag. Known entries require exact
  velocities with identity and rotated bases.
- Impact producer prefixes start at `11ba50`, after unrelated early collision,
  take-hit allocation and state work. They retain actual heading/trigonometry,
  bounded-angle and directional-offset selection, type/enable gates, coordinate
  translation, take-hit point pointer publication and scalar damage staging.
  They stop at `142d98`, before dispatching damage/effects. Boundary cases run the
  same real tails. Known prefixes require exact translated front/right points;
  additional prefixes select rear/left offsets. Dispatch is not stubbed or skipped.
- Morph entries complete the real owner: forward velocity, subtraction, both
  existing inverse leaves, sign/scaling branches and real hit classification.
  Published location/state fields must agree, including meaningful first-basis W
  in the vehicle inverse record and reused stack record.
- Original zero-vector rotation paths execute real libm domain handling. Fixtures
  seed retail `_LIB_VERSION=1` (verified in the local ELF) and valid errno storage;
  actual errno writes and FP effects are compared (828 sequences reach errno).
  No libm/helper result is stubbed.
- Explicit per-return, relative-mass/force, normalization/matrix/rotation,
  heading/angle and hit-location counts are required. Known-state publication
  assertions cover 64 reconstructed velocities, 32 force updates, 64 impact
  points and 256 complete morph locations. Native VU remains untouched through
  the actual scalar producers and consumers.
- Forward W exclusions are restricted to reconstructed `Vehicle+1ec`, the real
  nested force frame's `SP+1c`, direct force frame `SP+1c`, or impact scratch
  `SP+1c`. Exact output counts/addresses account for actual frame sizes and reuse
  of the nested force frame. Reference W stays intact through all real consumers
  before comparison-only normalization. Morph inverse overwrites remove the
  forward-W exclusion and compare full XYZW. No runtime RAM mask or VU-bank
  reconstruction is introduced.
- Aggregate coverage is 114,688 generated caller sequences / 5,152 whole-memory
  checks. Historical forward ABI coverage is 7,040 / 440; inverse stays 8,576 /
  134. Existing hierarchy/BSP/edge/unknown-ABI coverage and earlier AI,
  vehicle/light, final inverse and driver publication assertions remain enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass; all earlier
publication assertions remain enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Draws, sound, scratchpad, calls, threads and EE timing/counts match the
parent in every frame. Four-player raw RAM hashes also match throughout. Both
sampled 32 MiB RAM images are exact. Strict/default recording files and snapshots
are byte exact, with no strict mismatch.

One-player has seven raw RAM-hash differences, at frames 5929, 6497, 6503-6506
and 8823. Isolated parent/native diagnostic executables retain the production
objects and substitute only the parent/native math object and identical diagnostic
frame-end observation. Their complete recordings match the corresponding
qualification recordings. Exact frame-end snapshots and direct call traces prove:

- Every affected snapshot differs only at `8ff2ac`: original zero, native `8ff270`.
  Neighboring captured frames have no other differences.
- Matching calls to `11b26c` have real force `SP=8ff290`, input at SP and output at
  `SP+10=8ff2a0`. Both enter with `W=8ff270`; the original writes zero, while
  native code retains that exact prior value.
- The complete real force owner consumes only output XYZ in its velocity addition,
  then uses normalized XYZ for speed/matrix/rotation and returns. Its local output
  record is no longer live after that return. The surviving bytes are stack W
  residue, not a physics/driver result. Existing meaningful inverse W stays exact.
- Production binaries and raw recordings are unchanged. No runtime RAM mask,
  recorded-hash normalization or VU-state reconstruction is used. The exception
  is documented explicitly rather than calling all one-player RAM hashes exact.

Default one-player reaches force `11b26c` 45 times and morph `11c368` 166 times;
velocity-reconstruction and impact-offset sites are cold on that route.
Four-player makes no calls to the six new sites. Fixtures qualify the cold paths
within the complete/bounded consumer scopes stated above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing driving,
AI activity, vehicle rollover, simultaneous four-view driving/effects, decoded
intro footage and return to the menu. Each route has five distinct captures.
Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio.

Windowed one-player reaches the two velocity-reconstruction returns 14/8 times,
force 53 times and morph 178 times. Impact offsets remain cold. Four-player and
movies make no calls to these six sites. Device underruns remain: 46.9 ms silence
in 1P, 30.6 ms in 4P and 16.8 ms in movies. These are functional checks, not a
timing or flawless-playback claim. Full campaign, every collision/effects path
and physical-controller/hardware acceptance remain outside this increment.
