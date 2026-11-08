# Native AI inverses

Five AI inverse-transform returns now use native record arithmetic, extending the
[rigid-body increment](NATIVE_RIGID_BODY_INVERSES.md). Inverse coverage grows
from 112 to 117 sites; forward coverage stays at 95. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Consumers |
| --- | --- | --- |
| `1029f8` | `aiMoveBetweenPoints` | Lateral correction, angle wrapping, smooth steering and control-pad updates |
| `102c20` | `aiUpdateControlPadForTwoStageAttack` | Lateral correction, smooth steering and composed attack-range update |
| `102d74` | `aiDriveToAttackRange` | Relative velocity, range prediction and throttle/brake selection |
| `105c68` | `aiCheckForGoodShot` | Target motion, weapon/range/angle gates, look policy and attack-state publication |
| `105eec` | `aiCheckForGoodRearShot` | Rear target motion, weapon class, look policy and attack-state publication |

The existing native inverse snapshots basis/input records, computes separately
rounded XYZ and copies first-basis W as meaningful data. All four output
components stay exact. Native calls leave VU registers, accumulator, flags and
data memory untouched. Unknown callers and historical hosts retain the original
ABI. AI branches, angle wrapping, driver steering and attack-state changes stay
compiled game code.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/ai-inverses`, against main `c7c798e` (PR #46).
Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons.
  Aggregate coverage is 93,696 / 3,840. Four host rounding modes, denormal
  controls, RAM aliases, rotated bases and arbitrary first-basis W are covered.
- Each return has 256 inverse-boundary cases through its real consumer/return
  and 256 producer cases. Four owners have complete entries (1,024 cases).
  The larger navigation owner starts at its bounded steering producer
  `102944` (256 cases), then completes its real scalar tail; the whole navigation
  state machine and its earlier pathfinding calls are outside these fixtures.
- Producer reach assertions distinguish active paths and pre-transform gates:
  896 producer cases reach the selected inverse and 384 skip it. The navigation
  prefix covers seven rejection gates and its active correction path. Two-stage
  attack entries also run the real composed attack-range function.
- The real `boundFAngle` and `driverSmoothlyTurn` execute, including angle wraps,
  left/right steering and control-pad stores. Range entries exercise throttle,
  brake, special attack range, weapon range and predicted relative motion.
- Shot entries cover low/high turning rate, low target speed, lateral thresholds,
  weapon classes, range/angle rejection, look policy and real attack-state stores.
  The deterministic game-mode branch of look policy is used; its optional random
  branch remains outside these fixtures. Successful attack-state publication is
  required for both front and rear shots, not merely early rejection.
- CPU outside retired VU, outgoing host FP effects, clocks/checkpoints, per-site
  inverse counts, steering/look/attack helper counts and final memory agree.
  Native VU stays untouched through the complete consumers. No new W exclusion
  or VU reconstruction is added; inverse XYZW is compared in full.
- Whole-memory producer samples select active transforms. Historical inverse ABI
  coverage grows to 7,488 / 117 cases / whole-memory checks. Forward remains
  5,504 / 344. Existing hierarchy coverage retains 786,432 helper cases / 384
  whole-memory, 8,000 geometric edges, 1,024 complete BSP callers / 32 whole-memory
  and 2,048 unknown ABI cases.

## Replay qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass. The final
caller suite requires successful front/rear attack-state publication in 392 / 352
comparison cases respectively.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical too. Strict/default recording files and snapshots are byte exact;
strict result comparisons report no mismatch. No new RAM-padding difference is
introduced relative to the rigid-body parent.

All five new sites execute in 1P: navigation 640 calls, two-stage steering 448,
attack range 1,370, front shot 1 and rear shot 1,473 (3,932 total). Four-player
makes no AI inverse calls in this four-human scenario. The front-shot runtime
sample is sparse; its broader branches and successful publication are qualified
by the generated-code fixtures.

## Window qualification

Fresh isolated runs complete 6,500 one-player, 6,500 actual four-player and
4,200 movie frames. Reviewed captures show level loading, advancing gameplay,
AI opponents, driving/combat/effects, four simultaneous views, decoded intro
images and return to the menu. Each route has five distinct captures.

Windowed 1P reaches 2,545 new inverse calls: two-stage steering 292, attack range
1,412, front shot 1 and rear shot 840; the navigation inverse is cold in this
windowed sample but reached in the headless replay. Four-player and movies make
no AI inverse calls. Stereo 48 kHz WAV captures contain nonzero audio. Device
underruns remain: 60.2 ms silence in 1P, 33.6 ms in 4P and 17.6 ms in movies.
These are functional checks, not a timing or flawless-playback claim. Full
campaign, all navigation branches, random look-policy paths and physical
controller/hardware acceptance remain outside this increment.

## Remaining scope

Current generated source contains 134 inverse-helper and 269 forward-helper
returns. Of these two families, 17 inverse and 174 forward sites remain outside
the native allowlists: 191 total. This is a call-site inventory, not a count of
all remaining emulation. Remaining inverse groups include driver/boss helpers,
HUD/radar, rollover, damage, lighting, a tire helper and a particle helper.
Additional inline arithmetic and forward transforms remain separate work.
