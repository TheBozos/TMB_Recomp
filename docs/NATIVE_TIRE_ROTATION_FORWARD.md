# Native tire-pair correction forward transforms

[Combo-launch forwards](NATIVE_COMBO_FORWARD.md) follow with five vehicle combo sites.

Two vehicle-orientation forward returns use native XYZ arithmetic after the
[machine-gun muzzle forwards](NATIVE_MACHINE_GUN_FORWARD.md). Known forward coverage
grows from 167 to 169 of 269 returns (100 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `18b10c` | `Vehicle::tireUpdateCarRotation` (`18adb8`) | The first axle's height correction `(0,0,-c)` (`c` at `+1e58`) through the vehicle basis at `+d0`, into frame `+50`, added to that tire pair's midpoint into frame `+70` |
| `18b19c` | The same | The second axle's correction (`c` at `+1e68`), added into frame `+80` |

`tireUpdateCarRotation` rebuilds a vehicle's orientation from its tires each frame.
For the 4- and 5-wheel layouts (`+1790`), each axle with a nonzero correction moves
its tire-pair midpoint along the vehicle's up direction. Instead, layout 6 scales
the up vector (`mathfScaleVector`) for the first axle and layout 3 for the second,
with no transform. Then it:

1. rebuilds the forward axis from the corrected midpoints (difference, average,
   `mathfNormalize`);
2. rebuilds the side and up axes with `mathfCrossProduct`;
3. applies the per-layout tilt (`mathfRotMatrixRPH`) and `mathfMulMatrix` through
   the vehicle's coordinate system.

Both transforms write frame `+50`. It is read only by `mathfAddVector`; on the other
paths `mathfScaleVector` and `mathfSetFVector` write only its X/Y/Z. Frame `+5c` is
never read. The corrected midpoints in frame `+70`/`+80` are read only by
`mathfSubVector`. The owner reads no VU registers, accumulator or flags. The
existing native helper computes separately rounded XYZ, preserves destination W
and leaves VU state untouched. Unknown and historical callers keep the original
ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/tire-rotation-forward`, against parent `027357a`
(PR #69). Parent headless/window binaries are preserved there and match PR #69's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 272 and 273): helper-boundary continuations and owner entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary
  W payloads.
- Owner entries start inside the owner at `18b088`, right after its up-vector
  normalize, with the frame and saved registers as its prologue leaves them. This
  is the same approach as the earlier boss fixture. The earlier part of the owner
  contains no transform and is not run.
  - From that point, each entry runs both axle blocks and the whole rebuild through
    the real math helpers to the owner's real return.
  - Scenarios cover both 4- and 5-wheel layouts, each axle on its own, layout 6
    (scale only) and a 2-wheel layout (no corrections).
- The basis at `+d0` is fixture-known in every case, so every output is checked
  against an independent identity or quarter-turn result.
- Both corrected midpoints are asserted exactly against the original FPU addition,
  after the owner has finished. Cases where both run check both, in order. The
  sites publish in 608 and 864 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+5c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so one
  data-dependent count in earlier fixtures moved (vehicle tire jumps 705 → 700).
  All scenario-defined counts and assertions are unchanged.
- Aggregate caller coverage is 140,288 generated sequences / 6,752 whole-memory
  comparisons. Historical forward ABI coverage is 10,240 / 640; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `027357a`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The first-axle return
`18b10c` is live: it runs natively 17 times on the one-player route with no
recorded difference. The second-axle return `18b19c` is cold on these routes and
is qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 63.2 ms of silence in one-player, 27.3 ms in four-player and 23.2 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Full campaign and physical controller/hardware acceptance remain open.
