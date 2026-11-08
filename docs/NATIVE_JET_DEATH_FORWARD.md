# Native falling junk jet forward transforms

Six junk jet death forward returns use native XYZ arithmetic after the
[sinking blimp forwards](NATIVE_BLIMP_DEATH_FORWARD.md). Known forward coverage
grows from 157 to 163 of 269 returns (106 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

All six are in `JunkJet::UpdateDeath` (`143728`) and transform through the jet's
coordinate-system basis into the owner's frame vector at `+0`:

| Return PC | Path | Producer and consumer |
| --- | --- | --- |
| `143864` | Every frame | The velocity at `+20`, added to the jet's position |
| `143940` | High (above 50), 1-in-7 roll | A random explosion offset (±80, random Y/Z), transformed in place, added to the position, handed to `particleCreateFx` |
| `1439bc` | Low (0 to 50) | Fixed offset `(0,300,-20)`, the same way |
| `143a18` | Low | Fixed offset `(0,150,0)` |
| `143b0c` | Crash (at or below 0) | Fixed offset `(0,200,-20)` |
| `143b60` | Crash | Fixed offset `(0,100,0)` |

Each frame the falling jet steers toward its crash target (`atan2f`,
`boundFAngle`, `smoothEasyInAngle`), accumulates its roll, rebuilds its matrix with
`mathfRotMatrixRPH` and moves by the transformed velocity. The height then picks a
path. High, an occasional explosion. Low, two fixed bursts and one at the jet.
Crashing:

1. Nearby cars are damaged (`BlowCarsInJetCrash`).
2. The switch is set and debris thrown (`BlowDebris`).
3. Two fixed bursts and one at the jet.
4. The jet sound stops, its effects are killed and `junkyardAddPointsAfterJetCrash`
   runs.

The frame vector is read only by `mathfAddVector` and `particleCreateFx`. The
effect reads its point only through `mathfAddVector`, including in
`particlePostInitFx` and its recursive child effects. Frame `+0c` is never read; the
offsets' X/Y/Z are always rewritten before each in-place transform. The owner reads
no VU registers, accumulator or flags. The existing native helper computes
separately rounded XYZ, preserves destination W and leaves VU state untouched. It
reads its input before writing, so the in-place transforms are safe. Unknown and
historical callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/jet-death-forward`, against parent `17bc13a`
(PR #66). Parent headless/window binaries are preserved there and match PR #66's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons (caller
  kinds 262 to 267): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- The fixtures turn the particle system off (`gp-2f40 = 0`), as the game's own
  `particleCreateFx` allows. Each effect call then returns at once without reading
  its point, and the owner runs on to its real return. The test captures each
  effect's point as the call is made.
- Full entries run the steering, matrix rebuild, transform and position update. The
  height picks the path: high with and without the explosion roll, low, or
  crashing. The fixture seeds the game's actual libc `rand` state for the 1-in-7
  roll. High and low entries return normally. Crashing entries stop at
  `BlowCarsInJetCrash`, so the two crash points are covered from their helper
  boundaries. Those runs go through both crash bursts and the burst at the jet, and
  stop at `terminateJetSound`; reaching it is asserted.
- Helper-boundary outputs are checked against an independent identity or
  quarter-turn result. Full entries rebuild the basis from the steering angles
  first, so those are covered by the native/original comparison.
- Every published value is asserted exactly against the original FPU addition: the
  jet's position, and every effect point in call order. Cases where several
  transforms run check each one in order.
- Publication counts per site:

  | `143864` | `143940` | `1439bc` | `143a18` | `143b0c` | `143b60` |
  | ---: | ---: | ---: | ---: | ---: | ---: |
  | 1,792 | 544 | 1,024 | 1,280 | 256 | 512 |
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 714 → 704,
  real libm errno cases 862 → 813). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 137,216 generated sequences / 6,560 whole-memory
  comparisons. Historical forward ABI coverage is 9,856 / 616; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `17bc13a`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. None of the six jet death returns runs on these
routes, so they are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 62.6 ms of silence in one-player, 39.3 ms in four-player and 24.3 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. A junk jet shot down in actual play, full campaign and physical
controller/hardware acceptance remain open.
