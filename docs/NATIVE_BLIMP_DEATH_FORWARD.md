# Native sinking blimp forward transforms

[Falling junk jet forwards](NATIVE_JET_DEATH_FORWARD.md) follow with six more environment sites.

Two blimp death forward returns use native XYZ arithmetic after the
[gas tank forwards](NATIVE_GASTANK_FORWARD.md). Known forward coverage grows from
155 to 157 of 269 returns (112 remain). Known inverse coverage stays 134 of 134;
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `10eee0` | `Blimp::UpdateDeath` (`10ee18`) | The blimp's velocity at `+20` through its rebuilt coordinate-system basis, into the static vector `0x4fc200`, added to its position |
| `10efa0` | `Blimp::UpdateDeath` (`10ee18`) | A random explosion offset in `0x4fc200`, transformed in place, added to the position and handed to the effect and explosion sound |

While the blimp is more than 100 units above the ground (`hdHatTest`), it rolls
(smoothed speed, accumulated and clamped roll, `mathfRotMatrixRPH`), moves by the
transformed velocity and sinks by 0.5. A 1-in-9 roll then builds an explosion offset
(±80 by a raw `rand` bit, random Y/Z), transforms it in place and adds the position.
The result goes to `particleCreateFx` and `ExplosionSound::playBlimpExplosion`.
Cars within 150 (Manhattan, 2D) of the blimp are set on fire. Near the ground it
crash-lands instead, with no transform.

`0x4fc200` is a function-local static: no other code in the game references it.
Its W (`0x4fc20c`) is never read. After the first transform it is read only by
`mathfAddVector`. The explosion offset's X/Y/Z are rewritten before the second
transform, which reads only X/Y/Z. The explosion point is then read only through:

- `mathfAddVector` in `particleCreateFx` and its child effects;
- `mathfManhatDist` in `calculateRangedVolume`;
- `mathfSubVector` in `calculatePanAngle`, both reached from
  `calcDualMonoMixVolPan`.

The owner reads no VU registers, accumulator or flags. The existing native helper
computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transform is safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/blimp-death-forward`, against parent `bb68515`
(PR #65). Parent headless/window binaries are preserved there and match PR #65's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 260 and 261): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Full entries run the real `hdHatTest` on the fixture's empty height grid, with
  the ground set either 1,030 below the blimp (drift) or 30 below (crash landing).
  They then run the roll smoothing and clamping, matrix rebuild, transform, position
  update and sink. The fixture seeds the game's actual libc `rand` state, so the
  1-in-9 explosion roll is taken or skipped as intended. With no explosion, the
  owner checks the empty vehicle lists and returns normally. Explosion and crash
  cases stop at `particleCreateFx`.
- Helper-boundary outputs are checked against an independent identity or
  quarter-turn result. Full entries rebuild the basis from the roll first, so those
  are covered by the native/original comparison.
- The published values are asserted exactly against the original FPU arithmetic:
  the drift position (X/Y, and Z after the 0.5 sink), and the explosion point in
  `0x4fc200`. Cases where both transforms run check both, in order. The drift site
  publishes in 704 cases, the explosion site in 544.
- The VU rule from the earlier weapon increments applies. Only the static's forward
  W at `0x4fc20c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 714,
  real libm errno cases 820 → 862). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 134,144 generated sequences / 6,368 whole-memory
  comparisons. Historical forward ABI coverage is 9,472 / 592; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `bb68515`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. Neither blimp death return runs on these routes, so
both are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 58.5 ms of silence in one-player, 53.4 ms in four-player and 36.9 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. A blimp destroyed in actual play, full campaign and physical
controller/hardware acceptance remain open.
