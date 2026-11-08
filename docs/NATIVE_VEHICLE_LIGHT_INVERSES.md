# Native vehicle and object-light inverses

Six more inverse returns use native record arithmetic, following the
[driver/boss increment](NATIVE_DRIVER_BOSS_INVERSES.md). Inverse coverage grows
from 122 to 128 of 134 returns. Forward coverage stays at 96 of 269, and
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `18bc9c` | `tireCalcLastRotChange` | Local angular delta, previous-delta XYZ copy and meaningful output W |
| `1ac85c` | `vehicleInitRollOver` | Rollover angular response, trigonometric/clamp branches and angular-state copies |
| `1af120`, `1af148` | `vehicleReactToWeaponDamage` | Direction normalization, tire-jump decisions and damage-state publication |
| `1eb4d4`, `1eb58c` | `plightInObject` | Local light position/direction, distance/dot consumers and lighting-table publication |

The existing inverse implementation preserves separately rounded XYZ and copies
first-basis W as meaningful data. All four output components remain exact; no W
exclusion is added. Native calls leave VU registers, accumulator, flags and data
memory untouched. Unknown callers and historical hosts retain the original ABI.
Game branches and scalar consumers remain compiled game code.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/remaining-inverses`, against main `99efa7f`
(PR #48). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons.
  Aggregate coverage is 99,840 / 4,224. Each site has 256 helper-boundary cases
  through its actual tail and 256 complete owner-entry cases. Four host rounding
  modes, denormal controls, RAM aliases, rotated bases and arbitrary basis W
  are covered. Whole-memory entry samples reach active transforms.
- Tire entries execute the actual matrices-to-axis, axis-to-matrix and
  matrix-to-RPH producers before the inverse. Successful delta publication and
  the XYZ copy to the previous-delta record are required, with meaningful W
  checked explicitly. A special vehicle/state gate skips the transform.
  The separate boolean path that calls rollover is outside these tire fixtures;
  rollover itself has its own complete-entry fixtures.
- Rollover entries execute the actual normal-to-matrix producer, inverse,
  scalar trigonometric/threshold/clamp consumers and both angular-state copies.
  Signed lateral inputs, positive/negative roll rates, tilted/flat vehicles,
  vehicle-class scaling and the early state gate are covered. Successful state
  publication is required in 224 entry cases.
- Weapon-damage entries execute both upright-basis and generated-normal-basis
  routes, sign changes, real normalization, Z clamp, the four-tire loop and
  real tire-jump/free-fall helpers. Fixtures include zero vectors, blocked tires,
  existing positive tire velocity, damage below/above 50 and the damage-scaling
  flag. Switched vehicle classes require the other inverse site to execute.
  Successful tire-jump execution occurs in 707 comparison cases. The optional
  `collisSlideVehicle` call is avoided by the fixture inputs; its composed path
  and all weapon types are not claimed here.
- Lighting entries execute the complete eight-slot/four-output loop, including
  scalar position producers, both inverses, squared-distance rejection, actual
  dot consumers, attenuation, ambient/nonambient stores, the fourth output lane
  and the published-light flag. Fixtures include one/four active lights,
  missing indices, disabled lighting, oversized radius and invisible coordinate
  systems. Four-light nonambient cases require four real dot consumers.
  Successful light publication occurs in 192 entry cases. Arbitrary light-table
  contents and all spatial overlap configurations are outside these fixtures.
- Per-site reach assertions distinguish active transforms from gates. CPU outside
  retired VU, host FP effects, clocks/checkpoints, helper/action counts and final
  memory agree. Native VU remains untouched through the complete scalar tails.
  No runtime RAM masking or VU-state reconstruction is introduced.
- Historical inverse ABI coverage grows to 8,192 cases / 128 whole-memory checks;
  forward stays at 5,568 / 348. Existing coverage retains 786,432 hierarchy helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete BSP callers /
  32 whole-memory and 2,048 unknown ABI cases. Front/rear AI attack-publication
  assertions still pass in 392 / 352 cases.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recording files and snapshots are byte exact;
strict comparisons report no mismatch.

One-player reaches 11 tire-delta calls, one rollover call and 14 upright-damage
calls. Four-player reaches four tire-delta calls. The alternate damage inverse
and both lighting inverses are cold in these replay routes; their behavior is
qualified by the complete generated owner entries, not claimed as runtime route
coverage. The one-call rollover sample is sparse; broader branch coverage comes
from its fixtures.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing gameplay,
AI opponents, driving/combat effects and vehicle rollover, four simultaneous
views, decoded intro images and return to the menu. Each route has five distinct
captures. Windowed 1P reaches nine tire-delta, eight rollover and nine upright
damage calls; 4P reaches four tire-delta calls. The other new sites are cold in
these window routes and covered by generated entries.

Stereo 48 kHz, 16-bit WAV captures contain nonzero audio. Device underruns remain:
46.7 ms silence in 1P, 38.7 ms in 4P and 24.8 ms in movies. These are functional
checks, not a timing or flawless-playback claim. Full campaign, all vehicle/weapon
combinations and physical-controller/hardware acceptance remain outside this
increment.

## Remaining scope

Six inverse returns remain outside the native allowlist:

| Return PC | Owner |
| --- | --- |
| `12d0b0` | `forceFieldTakeHit` |
| `136ce8` | `healthStandInit` |
| `13f814`, `13fe54` | `Hud::BuildRadarStuff` |
| `14037c` | `Hud::AddRadarSignal` |
| `1e9778` | `particleDraw` |

Together with 173 remaining forward returns, these two families have 179 sites
outside the native allowlists. This is a call-site inventory, not a count of all
remaining emulation. Additional inline arithmetic remains separate work.
