# Native launcher special-weapon forward transforms (first batch)

[The second special-weapon batch](NATIVE_SPECIAL_VOLLEY_FORWARD.md) follows with vehicle types 1c0 and 140.

Five `Launcher::UpdateSpecial` forward returns use native XYZ arithmetic after the
[FinalBoss movement forwards](NATIVE_BOSS_MOVE_FORWARD.md). Known forward coverage
grows from 196 to 201 of 269 returns (68 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

`Launcher::UpdateSpecial` (`14dae0`) runs each vehicle's special weapon, dispatched
on the vehicle type at `+20a8`. This batch covers three of its branches:

| Return PC | Vehicle type | Producer and consumer |
| --- | --- | --- |
| `14dc44` | `60` | Table offset `23cfa0` through the vehicle's coordinate system, added to the vehicle position into the launcher's `+620` |
| `14dccc` | `60` | Table offset `23cfb0` through a matrix at frame `+40`, built from the turret angles (`+290` plus the vehicle's), added to `+620`; then `Weapons::CreateWeapon` |
| `14dea8` | `120` | Table offset `23cfc0` through the vehicle's coordinate system, added into `+620` |
| `14df2c` | `120` | Table offset `23cfd0` through the turret matrix at frame `+30`, added to `+620`; then `CreateStaticWeapon` from `+620` |
| `14e020` | `c0` | The launcher's offset at `+400` through the vehicle's coordinate system into `+620`; after `GetLockOn`, `TheGame::CreateWeapon` |

Each fires when its countdown at `+98` runs out. Every result is the frame vector at
`+0`, read only by `mathfAddVector`, which writes X/Y/Z into `+620`. The
environment-gun increment showed that `CreateStaticWeapon` reads only X/Y/Z.
Frame `+0c` is never read in the owner, and the W of `+620` is not written by
these paths. The owner reads no VU registers, accumulator or flags. The existing
native helper computes separately rounded XYZ, preserves destination W and leaves
VU state untouched. Unknown and historical callers keep the original ABI. No engine
or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/special-guns-forward`, against parent `8f0fdf3`
(PR #76). Parent headless/window binaries are preserved there and match PR #76's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons
  (caller kinds 301 to 305): helper-boundary continuations and owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Owner entries for types `60` and `c0` run from the owner's start: the type
  dispatch, the countdown, both transforms of a pair with the real angle bounding
  and `mathfRotMatrixRPH`, and for `c0` the real `GetLockOn`. Type `120` entries
  start right after the branch's `UpdateHearse` call, whose own forward transform is
  covered by its earlier increment. Each stops at its weapon creation; reaching it
  is asserted. Without a countdown, types `60`/`c0` return, and type `120` stops at
  its gun-sound update.
- The first transform of each pair uses the fixture's vehicle basis, and so does
  `c0`, so their cases are checked against an independent identity or quarter-turn
  result. The second transform's matrix is rebuilt from the turret angles, so it is
  checked independently only when a case starts at its own helper boundary.
- The final launch point in `+620` is asserted exactly against the original FPU
  addition, and for type `120` also as the origin `CreateStaticWeapon` receives.
  Publications: 704/960, 704/960 and 480.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 701 → 702,
  real libm errno cases 830 → 801). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 156,672 generated sequences / 7,776 whole-memory
  comparisons. Historical forward ABI coverage is 12,288 / 768; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `8f0fdf3`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The type-60 returns `14dc44` and `14dccc` are live: each runs natively 66 times on the one-player route with no recorded difference. The other three are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 53.8 ms of silence in one-player, 29.6 ms in four-player and 21.3 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The other special weapons in actual play, full campaign and physical controller/hardware acceptance remain open.
