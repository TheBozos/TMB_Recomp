# Native AI hunt and bait forward transforms

Two AI-driving forward returns use native XYZ arithmetic after the
[Zoomies and HalTrans weapon forwards](NATIVE_ZOOMIES_HAL_FORWARD.md). Known forward
coverage grows from 178 to 180 of 269 returns (89 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `103534` | `Vehicle::aiPerformBeadOnHuntCs` (`103308`) | A side offset `(x,0,0)` through the vehicle's basis into frame `+10`, added to the driver's bead point and made relative to the vehicle for the steering angle |
| `106af0` | `Vehicle::aiUpdateBaitModeOld` (`1069f8`) | Offset `(0,130,0)` through the target vehicle's basis into frame `+10`, added to the target's position, used to pick the closest attractor point |

When hunting, the AI keeps a side offset. It is used as is when the AI info flag
at `+5b8` is set or the info distance at `+20` is at least 150, and otherwise clamped
to ±20. The offset is added to the bead point,
made relative to the vehicle, and `atan2f` of its X/Y becomes the steering heading.
The driver then checks speed or drives to attack range and turns smoothly.

In the old bait mode, once the target's counter at `+20d0` reaches 600 and no point
has been picked (`driver+20c` is -1), the AI projects a point 130 ahead of the
target. It hands that point
to `attractGetClosestPoint`, which reads it only through `mathfSubVector`. The
closest attractor point becomes the driver's new bead.

In both owners the result is the frame vector at `+10`. It is read only by
`mathfAddVector`, `mathfSubVector`, `attractGetClosestPoint` and scalar X/Y loads.
Frame `+1c` is never read. Neither owner reads VU registers, the accumulator or
flags. The existing native helper computes separately rounded XYZ, preserves
destination W and leaves VU state untouched. Unknown and historical callers keep
the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/ai-bead-forward`, against parent `71e5997`
(PR #72). Parent headless/window binaries are preserved there and match PR #72's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 283 and 284): helper-boundary continuations and owner entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- Hunt entries start inside the owner at `1034c4`, right after the side offset is
  built, with the frame as its prologue leaves it.
  - From there they run the unclamped, +20 and -20 cases, the transform, the bead
    addition, the vehicle-relative subtraction and `atan2f`.
  - They stop at `aiDriveToAttackRange`; reaching it is asserted.
- Bait entries run from the owner's start through its counter and mode checks, the
  transform and the real attractor search over two points, to the owner's return.
  When a point has already been picked, they return without a transform.
- Both bases are fixture-known, so every case is checked against an independent
  identity or quarter-turn result.
- Published values are asserted exactly against the original FPU arithmetic: the
  hunt heading vector after both the addition and the subtraction, and the bait
  point, the picked index and the bead pointer. The sites publish in 512 and 480
  cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+1c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 705 → 706,
  real libm errno cases 810 → 853). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 145,920 generated sequences / 7,104 whole-memory
  comparisons. Historical forward ABI coverage is 10,944 / 684; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `71e5997`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. Neither AI return runs on these routes, so both are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 58.2 ms of silence in one-player, 35.7 ms in four-player and 24.3 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. AI hunting and baiting in actual play, full campaign and physical controller/hardware acceptance remain open.
