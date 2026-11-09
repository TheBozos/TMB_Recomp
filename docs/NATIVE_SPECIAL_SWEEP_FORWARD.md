# Native launcher special-weapon forward transforms (type 100)

Three more `Launcher::UpdateSpecial` forward returns use native XYZ arithmetic after
the [second special-weapon batch](NATIVE_SPECIAL_VOLLEY_FORWARD.md). Known forward
coverage grows from 206 to 209 of 269 returns (60 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

All three are in the vehicle type `100` branch of `Launcher::UpdateSpecial`
(`14dae0`). The branch runs while no shot is held (`+284`) and the turret angle at
`+290` (stepped down by `gp-62b0`) stays above `gp-42a0`:

| Return PC | Producer and consumer |
| --- | --- |
| `14eb9c` | The turret node's matrix (node at `+2a0`, matrix at `+10`) is rebuilt from that angle with `mathfRotMatrixRPHNoTrans`; table offset `23d070` goes through it into frame `+0`, plus `23d060`, into frame `+20` |
| `14ec04` | The global sweep angle at `gp-3090` (stepped and bounded) builds a matrix at frame `+30`; table offset `23d0a0` goes through it into frame `+0`, added to frame `+20` |
| `14ec70` | The launcher's aim point at `+350`, smoothed toward frame `+20`, goes through the vehicle's coordinate system into frame `+0`, plus the vehicle position; this becomes the position of the object at `+34c` |

When the angle runs below the limit, the branch instead fires from the table
(`CreateStaticWeapon`), with no transform. All three results are the frame vector
at `+0`, read only by `mathfAddVector`. Frame `+20` is read by `mathfAddVector` and
as scalar `smoothEasyIn` targets. The owner never reads frame `+0c` or `+2c`. It
reads no VU registers, accumulator or flags. The existing native helper computes
separately rounded XYZ, preserves destination W and leaves VU state untouched.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/special-sweep-forward`, against parent `e1cf669`
(PR #78). Parent headless/window binaries are preserved there and match PR #78's
qualified build. Generated bodies, retail data and runtime
recordings stay outside Git.

- Added coverage is 1,536 generated sequences / 96 whole-memory comparisons (caller
  kinds 311 to 313): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Full entries run from the owner's start: the type dispatch, the angle step, the
  real node-matrix rebuild, all three transforms with the angle bounding,
  `mathfRotMatrixRPH` and the aim-point smoothing, to the owner's real return. With
  the angle below its limit they fire instead and stop at `CreateStaticWeapon`.
- The vehicle basis is fixture-known, so `14ec70` is always checked against an
  independent identity or quarter-turn result. The other two use rebuilt matrices,
  so they are checked independently only from their own helper boundary.
- Published values are asserted exactly against the original FPU arithmetic: the
  sweep point in frame `+20`, and the aimed object's position. Publications:
  928, 1,184 and 1,440.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so one
  data-dependent count in an earlier fixture moved (real libm errno cases 835 →
  829). All scenario-defined counts and assertions are unchanged.
- Aggregate caller coverage is 160,768 generated sequences / 8,032 whole-memory
  comparisons. Historical forward ABI coverage is 12,800 / 800; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `e1cf669`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The three type `100` returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 49.1 ms of silence in one-player, 31.8 ms in four-player and 14.4 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. A first window run on a loaded machine reported 9.7 s of one-player silence and a late capture. A back-to-back one-player rerun of parent and new builds then gave 43.8 ms and 51.2 ms at 59.7 frames/s, and the fresh window run above was taken after it. The other special weapons in actual play, full campaign and physical controller/hardware acceptance remain open.
