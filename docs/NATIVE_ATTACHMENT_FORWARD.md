# Native attachment and environment forward transforms

Five attachment/environment forward returns use native XYZ arithmetic after the
[vehicle-collision forward increment](NATIVE_COLLISION_FORWARD.md). Coverage
grows from 119 to 124 of 269 known forward returns (145 remain). Known inverse
coverage stays 134 of 134. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `129e58` | `EnvHeli::UpdateTrans` | Translated helicopter position, height smoothing and real coordinate-system grid reparenting |
| `137cac` | `Cable::Update` | Parent-space offset, pickup/cable position, grid membership and lifetime/type-dispatch staging |
| `1b2d1c` | `Weapon::UpdateGasCanTrans` | Smoothed, frame-scaled movement, real height query, child position and lifetime fields |
| `1b3d30` | `Weapon::UpdateSawBladeTrans` | Smoothed, frame-scaled movement and translated coordinate-system position |
| `1b3e00` | `Weapon::UpdateTaxiSpikeTrans` | Smoothed, frame-scaled movement and translated coordinate-system position |

Native returns compute separately rounded XYZ, preserve destination W and leave
VU registers, accumulator, flags and data memory untouched. The original scalar
smoothing, frame accessors, angle updates, height queries and grid/lifetime logic
remain. Unknown/historical callers retain the original ABI. No engine or game
motion formula is changed.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/attachment-forward`, against parent `2ee3dc9`
(PR #54). Parent headless/window binaries are preserved there for diagnostics.
Generated bodies, retail data and runtime recordings stay outside Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons.
  Each return has 256 helper-boundary sequences through its real tail and 256
  producer-entry sequences. Four rounding modes, FTZ/DAZ, RAM aliases, rotated
  bases, zero/tiny/signed vectors, zero/one/two frame counts, arbitrary W payloads
  and actual in-place scratch outputs are covered.
- Helicopter entries complete the original owner: local movement, world-position
  addition, scalar height smoothing and `hdReparentCsGrid`. Enabled and disabled
  grid gates are included.
- Cable entries retain the parent's offset transform, actual child-coordinate
  position publication and grid update, disabled-child state and lifetime
  increments through 301. Expiry cases start from the real owner and stop at
  `ChangePickupType` after position/grid publication, timer reset and type-index
  staging. Pickup replacement itself is not stubbed or claimed as complete-owner
  coverage; other entries complete the owner.
- Grid entries execute real sphere-cell calculations, pool allocation, linked-list
  insertion and old-cell removal/deallocation. Explicit bucket/node ownership and
  cached-cell assertions verify translated positions reach the correct real cell.
  There are 128 successful removal/reinsertion cases, plus disabled-grid cases.
- Gascan entries complete the original movement/smoothing/frame-count path,
  parked three-angle update, query/publish gates, child-coordinate publication and
  lifetime fields. Height fixtures use the actual `hdHatTest`, disabled-terrain
  `terrainGetHAT` and an empty resident collision grid, preserving the original
  miss/fallback behavior. They do not claim terrain-hit coverage. No height or
  collision result is stubbed. Expired-state entries use the actual non-player
  message gate; player-message dispatch is outside these fixtures.
- Saw/taxi entries complete the original smoothing, three actual frame-accessor
  calls, local movement transform and world-coordinate addition.
- Explicit per-site and smoothing/frame/grid/query/angle counts are required.
  Known position assertions pass 32 cases per owner, including gascan child
  positions. Cable enabled/disabled and expiry states, gascan expiry state,
  grid cache IDs, bucket pointers and node ownership are checked before any W
  normalization. CPU outside retired VU, clocks/checkpoints, host FP effects,
  action counts and final memory agree. Native VU stays untouched throughout.
- Opaque forward W is restricted to the real owner's `SP+0c`. Exact output counts
  and addresses distinguish helper boundaries from complete-entry frames of
  `0x30`, `0x40` or `0x60` bytes. Disabled producer gates have no exclusion. Original W
  remains intact through every real consumer, then is normalized for comparison
  only. Meaningful inverse W remains fully compared in the earlier suites.
  No runtime RAM mask, recorded-hash normalization or VU-bank reconstruction is
  introduced.
- Aggregate caller coverage is 117,248 generated sequences / 5,312 whole-memory
  checks. Historical forward ABI coverage grows to 7,360 / 460; inverse remains
  8,576 / 134. Existing hierarchy/BSP/edge/unknown-ABI coverage and earlier AI,
  vehicle/light, final inverse, driver and collision assertions stay enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass; every
previous publication assertion remains enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `2ee3dc9`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recording files and snapshots are byte exact,
with no strict mismatch. This increment adds no RAM-hash difference.

Default one-player reaches cable `137cac` 3,902 times and taxi-spike `1b3e00`
494 times; four-player reaches cable 3,798 times. Helicopter, gascan and saw-blade
sites are cold on these headless routes and are qualified by their generated
owners and real consumers described above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing driving,
AI opponents, combat/effects, four simultaneous views, decoded intro footage and
return to the menu. Each route has five distinct captures. Stereo 48 kHz,
16-bit WAV recordings contain nonzero audio.

Windowed one-player reaches cable 1,706 times and gascan 46 times; four-player
reaches cable 1,816 times. Movies make no calls to these five sites. Helicopter
and saw-blade sites remain cold across these runtime routes, and taxi-spike
runtime reach comes from the headless one-player route. Complete generated
entries retain the stated cold-site coverage and expiry/query limits.

Device underruns remain: 30.2 ms silence in 1P, 29.5 ms in 4P and 18.4 ms in movies.
These are functional checks, not a timing or flawless-playback claim. Full
campaign, every attachment/terrain-hit/pickup-replacement configuration and
physical-controller/hardware acceptance remain outside this increment.
