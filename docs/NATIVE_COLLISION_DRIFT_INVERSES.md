# Native vehicle collision and drift inverses

Sixteen inverse-transform returns now use native record arithmetic, extending the
[weapon/skid increment](NATIVE_WEAPON_SKID_TRANSFORMS.md). Inverse coverage grows
from 89 to 105 sites; forward coverage stays at 95. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Consumer |
| --- | --- | --- |
| `1161f4`, `116618` | `collisSetupHitPoints` | Local axis selection and hit-point records |
| `119090`, `11927c` | `collisAverageEnvironmentCollisions` | Hit location and slope response |
| `11985c`, `119870`, `119e64` | `collisReflectOffCollision` | Reflection decisions and local velocity |
| `11afb0`, `11b018` | `collisCalcCar2CarForces` | Local force scaling and force decisions |
| `11b9a8` | `collisSetCar2CarFlags` | Local velocity publication before hit dispatch |
| `11c38c`, `11c3c0` | `collisCar2CarMorph` | Velocity and normalized hit location |
| `11c540` | `collisCheckDrivableSlopeHit` | Slope accumulation and wheel response |
| `1ab558`, `1ab8f4`, `1abad4` | `vehicleUpdateDrift` | Drift exit, direction checks and velocity publication |

The existing inverse snapshots three basis columns and the input, calculates
separately rounded XYZ, and copies the first basis column's W as meaningful data.
All four result components remain exact. Native calls do not touch VU registers,
accumulator, flags or data memory. Unknown callers and historical hosts keep the
complete original ABI. Collision dispatch, force application, orientation/matrix
producers and tire-plane updates remain generated game code.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/collision-drift-inverses`, against main `a2905f2`
(PR #44). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 8,192 sequences / 512 whole-memory comparisons. Aggregate
  coverage is 87,552 / 3,456. Four host rounding modes, denormal controls and
  cached/uncached RAM aliases and arbitrary basis W payloads are covered.
- Each return has 256 inverse-boundary cases and 256 generated producer cases.
  Producers comprise 3,712 bounded call-argument sequences and 384 complete
  drift entries (128 exiting drift and 256 inactive drift).
- All sixteen sites have explicit original/native call counts and native leaf
  VU-preservation assertions. CPU outside retired VU, outgoing host FP effects,
  checkpoints, call counts and final memory agree. No new W exclusions are used.
- Ten tails run through their real return: averaged hit location, slope response,
  reflected velocity, second car-force result, both morph inverses, drivable
  slope and all three drift returns. Active drift direction fixtures continue
  through slowdown and the final velocity inverse with tire-plane updates gated
  off. Complete drift entries cover the speed threshold's reset/retain branches.
- Six tails have explicit boundaries: hit-point setup stops before its subsequent
  forward-transform loop; the first two reflection decisions stop before the
  displacement subtraction; the first car-force result is scaled and normalized
  before the second matrix-from-normal producer; collision flags stop at hit-info
  initialization after velocity publication. These fixtures do not claim full
  collision dispatch, force application or all orientation/tire update paths.
- Historical inverse ABI coverage grows to 6,720 / 105 cases / whole-memory
  checks. Forward remains 5,504 / 344. Existing hierarchy coverage retains
  786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete
  BSP callers / 32 whole-memory and 2,048 unknown ABI cases.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical too. Strict/default recording files and snapshots are byte exact;
strict result comparisons report no mismatch. New native inverse calls total
34,927 in 1P and 15,194 in 4P. Return `11927c` remains cold in these replays and is
qualified by its reached generated-code fixture.

Fresh windowed checks use isolated user folders and complete 6,500 one-player,
6,500 four-player and 4,200 movie frames. Captures show level loading followed
by moving gameplay, four simultaneous views, driving/firing/effects, decoded
intro images and return to the menu. Each route has five distinct captures.
The new inverse calls total 16,872 / 7,202 in windowed 1P / 4P; movies make none.
Stereo 48 kHz WAV captures contain nonzero audio on all routes. Device underruns
remain: 53.4 ms silence in 1P, 25.4 ms in 4P and 270.5 ms in movies. The movie
run overlapped functional checks; this is not a clean timing comparison or a
claim of flawless device playback. Mixer records in the paired headless runs
remain exact. Full campaign, every collision dispatch path and physical
controller/hardware acceptance are outside this increment.

## Remaining scope

The current generated source contains 134 inverse-helper returns and 269 forward
returns. Of these two families, 29 inverse and 174 forward sites remain outside
the native allowlists: 203 total. This is a call-site inventory, not a count of
all remaining emulation. Rigid-body inverses, AI, driver/boss helpers, HUD/radar,
rollover, damage, lighting and additional inline arithmetic remain separate work.
