# Native gas tank forward transforms

Three gas tank forward returns use native XYZ arithmetic after the
[junk jet and blimp flight forwards](NATIVE_FLYER_FORWARD.md). Known forward coverage
grows from 152 to 155 of 269 returns (114 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `135cf8` | `GasTank::UpdateGasTank` (`135c20`) | The rolling tank's velocity at `+30` through its own stored basis at `+40`, added to its coordinate-system position |
| `136200` | `GasTank::UpdateFalling` (`135fd8`) | A burst offset `(0,140,0)` through a random-heading matrix, transformed in place and added to the tank's home position |
| `1362ec` | `GasTank::UpdateFalling` (`135fd8`) | The same with offset `(0,150,0)` on the second, independent roll |

`UpdateGasTank` smooths the tank's spin, moves it by the transformed velocity, then
accumulates its spin angles and rebuilds its matrix with `mathfRotMatrixRPH`. Below
the floor it bounces: the height is clamped, the position is copied to the frame
vector, and sparks and a bounce sound follow. Otherwise it measures the Manhattan
distance to its home position from the game's table and lands when close (switch,
animation, falling sound and debris). The transform result is the frame vector at
`+10`. It is read only by `mathfAddVector`, then overwritten by `mathfCopyVector`
or `mathfSetFVector` (both XYZ-only) before `particleCreateFx` sees it. Frame `+1c`
is never read.

`UpdateFalling` waits for the fall animation to pass 75%. Then, unless the tank is
already down, a 1-in-16 roll throws a burst 140 units up along a random heading
(`mathfRotMatrixH`), and a 1-in-9 roll throws one 150 units up. Each offset is
transformed in place in the frame vector at `+20`, added to the home position,
its height randomized, and handed to `particleCreateFx`. That function reads its
point only through `mathfAddVector`, including in `particlePostInitFx` and its
recursive child effects. Frame `+2c` is never read; `+20..+28` are always rewritten
by `memset`, the per-axis random stores or `mathfSetFVector` before each use.

Neither owner reads VU registers, the accumulator or flags. The existing native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transforms are safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/gastank-forward`, against parent `7b71504` (PR #64).
Parent headless/window binaries are preserved there and match PR #64's qualified
build. Generated bodies, retail data and runtime recordings stay outside Git.

- Added coverage is 1,536 generated sequences / 96 whole-memory comparisons
  (caller kinds 257 to 259): helper-boundary continuations and full-owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Rolling-tank entries run the spin smoothing, transform, position update, matrix
  rebuild and home check. Scenarios cover a free roll (normal return), a bounce
  (stops at the spark effect; clamped height and bounce factor asserted), a landing
  near home (stops at `hierSetSwitch`; landed flag asserted) and an inactive tank
  (no transform).
- Falling-tank entries run the real animation query (no animation is loaded, so the
  percentage is -1), the tank-down gate and the real libc `rand` rolls. The fixture
  seeds the game's actual `rand` state so each roll takes the intended branch.
  Entries cover the first burst, the second burst, neither roll (normal return) and
  a tank already down (no transform). Burst cases stop at `particleCreateFx`.
- Helper-boundary outputs, and rolling-tank entries whose basis is fixture-known,
  are checked against an independent identity or quarter-turn result. Falling
  entries build a random-heading basis first, so those are covered by the
  native/original comparison. The published position (rolling) or burst point X/Y
  (falling) is asserted exactly against the original FPU addition. The rolling site
  publishes in 480 cases, the bursts in 480 and 448.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+1c` rolling, `+2c` falling) is normalized for test comparison, after
  every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 718 → 709,
  real libm errno cases 847 → 820). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 133,120 generated sequences / 6,304 whole-memory
  comparisons. Historical forward ABI coverage is 9,344 / 584; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `7b71504`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. None of the three gas tank returns runs on these
routes, so they are qualified by
the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 73.0 ms of silence in one-player, 30.2 ms in four-player and 25.7 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Gas tanks in actual play, full campaign and physical controller/hardware
acceptance remain open.
