# Native driver and boss inverses

Five driver/boss inverse returns now use native record arithmetic, following the
[AI increment](NATIVE_AI_INVERSES.md). The coupled final-boss forward transform
also becomes native, so the local offset can reach its real vehicle-position
consumer without VU transport. Inverse coverage grows from 117 to 122 of 134
sites, forward coverage from 95 to 96 of 269. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `1265a8`, `1265d0` | `driverGetOffsetFromPoint` | Path-point local offsets, Y comparison and probe staging |
| `1288b0`, `1288c4` | `driverCheckDrivingLOS` | Local X signs, LOS state, random steering choice and publication |
| `112118` | `FinalBoss::CheckCollision` | Captured-vehicle local offset and effect-coordinate staging |
| `11112c` (forward) | `FinalBoss::UpdateBoss` | Local offset to world XYZ, captured-vehicle position and attachment flags |

Inverse arithmetic is unchanged: separately rounded XYZ and meaningful first-basis
W remain exact. The forward writes XYZ and retains destination W; its sole scratch
W at `SP+1c` is audited after the actual XYZ publication and flag consumers.
No inverse W is excluded. Native calls leave VU registers, accumulator, flags and
data memory untouched. Unknown callers and historical hosts keep the original ABI.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/driver-boss-inverses`, against main `bd12a90`
(PR #47). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons.
  Aggregate coverage is 96,768 / 4,032. Four rounding modes, denormal controls,
  RAM aliases, rotated bases and arbitrary first-basis W are covered.
- Each of the six sites has 256 helper-boundary cases and 256 producer cases.
  Path offsets include complete `126500` entries, six pre-transform rejection
  gates, and active Y comparisons. The positive probe path stages its actual
  150-unit vector and stops at forward helper `1da298`, before the unrelated
  LOS query. The optional lateral-rotation producer is disabled in these fixtures.
- Driving-LOS producer fixtures begin at `128880`, after earlier geometry,
  LOS and tire queries. They execute actual LOS-state publication, transform
  gates, opposite/same/zero X tests, the generated seeded random helper and
  steering-choice stores through the function return. Those earlier queries
  are outside this bounded fixture; they are not replaced with test stubs.
- Boss inverse producers begin at `1120ec`: actual position subtraction,
  in-place inverse, three real random calls and effect-coordinate staging execute
  up to particle creation `1e6610`. The resulting local offset then enters the
  real `UpdateBoss` position producer at `11110c`, transforms forward, translates,
  publishes vehicle XYZ and updates attachment flags, stopping before the next
  collision call `111f80`. Standalone forward cases cover that same consumer.
  Identity and rotated bases must recover the known captured-vehicle position.
  Earlier collision queries, hit dispatch, particle creation, sound and the rest
  of the boss state machine are outside these fixtures.
- CPU outside retired VU, host FP effects, clocks/checkpoints, per-site inverse
  counts, random-call counts and final memory agree. Native VU remains untouched
  through these consumers. Only the audited forward scratch W is normalized in
  the comparison after the real consumer; no runtime RAM mask or VU reconstruction
  is introduced. Whole-memory producer samples select active transforms.
- Historical inverse ABI coverage is 7,808 / 122 cases / whole-memory checks;
  historical forward coverage is 5,568 / 348. Existing coverage retains 786,432
  hierarchy helper cases / 384 whole-memory checks, 8,000 geometric edges,
  1,024 complete BSP callers / 32 whole-memory checks and 2,048 unknown ABI cases.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass. Existing
front/rear AI attack-publication assertions still pass (392 / 352 cases).

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recording files and snapshots are byte exact, with
no strict comparison mismatches.

One-player reaches all four new driver sites: 16 calls each to the path-offset
returns and 39 each to the driving-LOS returns (110 total). Four-player makes no
new driver/boss calls in this four-human scenario. The boss sites are cold in
these replay routes; their attachment behavior is qualified by the generated-code
fixtures above, not a complete boss fight.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing gameplay,
AI opponents, driving/combat/effects, four simultaneous views, decoded intro
images and return to the menu. Each route has five distinct captures.

Windowed one-player reaches the two path-offset sites nine times each. The
other new sites are cold in this shorter window sample; driving-LOS is reached
in the headless replay and boss publication in the generated fixtures.
Four-player and movies make no new driver/boss calls. Stereo 48 kHz, 16-bit WAV
captures contain nonzero audio. Device underruns remain: 55.0 ms silence in 1P,
28.3 ms in 4P and 27.8 ms in movies. These are functional checks, not a timing
or flawless-playback claim. Full campaign, a complete final-boss fight and
physical-controller/hardware acceptance remain outside this increment.

## Remaining scope

Of these two helper families, 12 inverse and 173 forward sites remain outside the
native allowlists: 185 total. This is a call-site inventory, not a count of all
remaining emulation. Additional inline arithmetic remains separate work.
