# Native FinalBoss movement forward transforms

[Launcher special-weapon forwards, first batch](NATIVE_SPECIAL_GUNS_FORWARD.md) follow with five UpdateSpecial sites.

Two FinalBoss movement forward returns use native XYZ arithmetic after the
[FinalBoss gun-shot forwards](NATIVE_BOSS_SHOTS_FORWARD.md). Known forward coverage
grows from 194 to 196 of 269 returns (73 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `111788` | `FinalBoss::UpdateTrans` (`111550`) | The boss velocity at `+50` (its Y just smoothed toward the forward speed) through the boss's own basis at `+a0` into frame `+0`, added to its coordinate-system position; the boss is then reparented in the grid |
| `111cc4` | `FinalBoss::UpdateDeath` (`111c50`) | The same velocity after the death roll rebuilds the coordinate-system matrix, added to the position, then probed with `hdHatTest` |

Both results are the frame vector at `+0`. It is read only by `mathfAddVector`.
`hdHatTest` receives the same slot only as an output, which it writes with
`mathfCopyVector`/`mathfSetFVector`. Frame `+0c` is never read. Neither owner reads
VU registers, the accumulator or flags. The existing native helper computes
separately rounded XYZ, preserves destination W and leaves VU state untouched.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

The third non-shot boss site, `FinalBoss::CheckCollision` (`11201c`), hands its
point to `hdPntSphereTest`. It stays with the deferred collision-system group
(Ferris wheel and RockHD).

## Generated comparisons

Evidence is under `D:/BozoScratch/boss-move-forward`, against parent `75e7827`
(PR #75). Parent headless/window binaries are preserved there and match PR #75's
qualified build. Generated bodies, retail data and runtime recordings
stay outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 299 and 300): helper-boundary continuations and owner entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- `UpdateTrans` entries start after the forward-speed smoothing, at the label where
  its result is stored into the velocity, and vary that speed's sign.
- `UpdateDeath` entries start after the death roll's `mathfRotMatrixRPH`.
- Each runs the transform and the position update. They stop at
  `hdReparentCsGrid` (`UpdateTrans`) or `hdHatTest` (`UpdateDeath`); reaching it is
  asserted.
- The boss basis is fixture-known, so every case is checked against an independent
  identity or quarter-turn result. The new position is asserted exactly against the
  original FPU addition. Each site publishes in 512 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 701,
  real libm errno cases 859 → 830). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 154,112 generated sequences / 7,616 whole-memory
  comparisons. Historical forward ABI coverage is 11,968 / 748; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `75e7827`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. Neither boss movement return runs on these routes, so both are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 57.7 ms of silence in one-player, 925.2 ms in four-player and 24.1 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The qualification four-player window run logged 925.2 ms of silence. Two back-to-back full reruns of that route gave 31.9 ms (new build) and 22.8 ms (parent), so this was machine load, not this change. The boss fight in actual play, full campaign and physical controller/hardware acceptance remain open.
