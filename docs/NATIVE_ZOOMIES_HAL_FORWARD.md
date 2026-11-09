# Native Zoomies and HalTrans weapon forward transforms

[AI hunt and bait forwards](NATIVE_AI_BEAD_FORWARD.md) follow with two AI-driving sites.

Four weapon forward returns use native XYZ arithmetic after the
[combo-launch forwards](NATIVE_COMBO_FORWARD.md). Known forward coverage grows from
174 to 178 of 269 returns (91 remain). Known inverse coverage stays 134 of 134;
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `14d294` | `Launcher::UpdateZoomies` (`14d238`) | The launcher's muzzle offset at `+4c0` through the vehicle's coordinate system, added to its position into the launcher's `+620` before `GetLockOn` and `CreateWeapon` |
| `1b3874` | `Weapon::UpdateHalTrans` (`1b3778`), state 0 | The weapon's velocity (`+90`, scaled by the fields since the last update) through its basis at `+d0`, in place, added to its position |
| `1b39a4` | The same, state 1, target vehicle rolling | The vehicle's offset at `+2460` through the vehicle's coordinate system, added to the vehicle's position |
| `1b3b00` | The same, state 1, otherwise | A copy of the vehicle's `+2450` offset, the same way |

`UpdateZoomies` fires on every period-th update. The period is the global at
`gp-42f8`, and the counter is `+25c`. The result is the frame vector at `+0`, read
only by `mathfAddVector`, which writes X/Y/Z into `+620`. Frame `+0c` is never read.

`UpdateHalTrans` flies the weapon in state 0. In state 1 it follows its target
vehicle: when that vehicle's `+475` byte is clear, it smooths its position toward
the transformed point. X and Y go through `smoothEasyIn`, Z is stored directly,
then the angles are smoothed and the matrix is rebuilt
(`mathfRotMatrixRPHNoTrans`). The rolling test is `|cs+64|` against a limit. The
results are the frame vector at `+0`, read only by `mathfAddVector` and by scalar
X/Y/Z loads. Frame `+0c` is never read.

Neither owner reads VU registers, the accumulator or flags. The existing native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transform is safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/zoomies-hal-forward`, against parent `2b4f79b`
(PR #71). Parent headless/window binaries are preserved there and match PR #71's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 2,048 generated sequences / 128 whole-memory comparisons
  (caller kinds 279 to 282): helper-boundary continuations and full-owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Zoomies entries run the fire counter, the transform, the real
  `Launcher::GetLockOn` (nothing to lock on) and stop at `CreateWeapon`. A
  non-firing update checks the burst length and returns.
- HalTrans entries run state 0 to its return. In state 1 they run both branches
  through the real smoothing helpers and the matrix rebuild to the owner's return.
  State 2 and the no-target case return without a transform.
- Every basis is fixture-known at entry as well as at the helper boundary, so every
  case is checked against an independent identity or quarter-turn result.
- Published values are asserted exactly against the original FPU addition: the
  Zoomies launch point in `+620`, the flying weapon's position, and the attached
  weapon's directly stored height. The sites publish in 480, 480, 448 and 448
  cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 708 → 705,
  real libm errno cases 842 → 810). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 144,896 generated sequences / 7,040 whole-memory
  comparisons. Historical forward ABI coverage is 10,816 / 676; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `2b4f79b`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The HalTrans state-0 return
`1b3874` is live: it runs natively 18 times on the one-player route with no
recorded difference. The other three returns are cold on these routes and are
qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 52.1 ms of silence in one-player, 22.0 ms in four-player and 23.7 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Zoomies and an attached HalTrans weapon in actual play, full campaign and
physical controller/hardware acceptance remain open.
