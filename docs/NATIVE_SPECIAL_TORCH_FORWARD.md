# Native launcher special-weapon torch forward transforms

Ten more `Launcher::UpdateSpecial` forward returns use native XYZ arithmetic after
the [Ferris wheel and final boss batch](NATIVE_FERRIS_BOSS_FORWARD.md). Known forward
coverage grows from 226 to 236 of 269 returns (33 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`. With these, every forward return in
`Launcher::UpdateSpecial` (`14dae0`) is native.

They are the flame-torch specials of three vehicle types:

| Return PC | Vehicle type | Producer and consumer |
| --- | --- | --- |
| `14f190` | `180` (tank car) | A locked-on target within 250: the offset in frame `+60` goes through the turret matrix at frame `+80`, plus the vehicle position, and is line-tested from the vehicle (`hdLosCsTest` end point) |
| `14f1c4` | `180` | Without a close target: the same offset through the vehicle's coordinate system, then the same line test |
| `14f3e8` | `180` | On a line-test hit: `(0, 5, 0)` in frame `+0` goes through the vehicle's system and is handed to `DispatchHit` |
| `14f654` | `1e0` (Minion) | The nozzle offset at `+3a0` goes through the matrix at frame `+80` into the launcher's `+620`, plus the vehicle position |
| `14f68c` | `1e0` | The offset at `+3b0` goes through the matrix at frame `+40` into frame `+0` and is added into `+620`. That point is then aimed at the target (`mathfRotationFromPointToPoint`), measured and line-tested |
| `14f72c` | `1e0` | The flame offset in frame `+20` goes through the torch matrix at `+2b0`, plus `+620`, and is line-tested from `+620` |
| `14f954` | `1e0` | On a hit: `(0, 5, 0)` through the vehicle's system, handed to `DispatchHit` |
| `14fb7c` | `220` (Tanker) | The table offset `23d1f0` goes through the matrix at frame `+70` into `+620`, plus the vehicle position |
| `14fbb8` | `220` | `23d200` goes through the matrix at frame `+30` into frame `+0`, added into `+620` |
| `14fc24` | `220` | Like `14f72c`, line-tested from `+620` |

A consumer audit followed every path from each return until the owner returns or
the slot is rewritten whole. It found these consumers:

- `mathfAddVector`.
- `hdLosCsTest`, whose readers use the points only as XYZ (see the
  [weapon hit-test batch](NATIVE_WEAPON_HIT_FORWARD.md)).
- `DispatchHit` and all of its handlers.
- `mathfRotationFromPointToPoint`.
- `particleCreateFx` and `particleTruncateFlameThrower`. The latter passes the point
  only to `mathfSubVector`.
- `Weapons::CreateWeapon`, which copies `+620` with `mathfCopyVector`.

The torch sound updaters get other fields. None of these reads W.

The owner never loads frame `+0c`, `+2c` or `+6c`. Launcher `+62c`, the W of the
persistent `+620`, has no reader anywhere. The owner reads no VU registers,
accumulator or flags. The existing native helper computes separately rounded XYZ,
preserves destination W and leaves VU state untouched. Unknown and historical callers
keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/special-torch-forward`, against parent `366c6b2`
(PR #82). Parent headless/window binaries are preserved there and match PR #82's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 5,120 generated sequences / 320 whole-memory comparisons (caller
  kinds 331 to 340): helper-boundary continuations and entries at the call-return
  label before each transform. They run across four rounding modes, FTZ/DAZ, RAM
  aliases, identity/quarter-turn bases and arbitrary W payloads. The entry labels are
  `14f17c`, `14f0d4` (with the target distance at least 250), `14f3d4`, `14f644`,
  `14f67c`, `14f718`, `14f940`, `14fb68`, `14fba4` and `14fc10`.
- Each run stops at the first call after the result has been consumed:
  - `hdLosCsTest` for the line tests;
  - `DispatchHit` for the hits;
  - `mathfMulMatrix` after the `+620` launch points;
  - `mathfRotationFromPointToPoint` after the frame `+0` additions.
- A transform read only by `DispatchHit` is recorded at that call with a zero base.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result.
- Published values are asserted exactly against the original FPU arithmetic: the
  launcher's `+620`, the line-test end point (and that `hdLosCsTest` receives that
  address), and `DispatchHit`'s point and address. Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+0c`, `+2c`, `+6c` or launcher `+62c`) is normalized for test comparison,
  after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 703,
  real libm errno cases 843 → 816). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 174,592 generated sequences / 8,896 whole-memory
  comparisons. Historical forward ABI coverage is 14,528 / 908; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `366c6b2`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The ten returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 907.3 ms of silence in one-player, 35.5 ms in four-player and 26.4 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The one-player window run overlapped a long read-only audit on the machine and reported 907.3 ms of silence. A back-to-back one-player rerun of new and parent builds then gave 78.4 ms and 59.9 ms at 59.4 and 59.7 frames/s. The torch specials in actual play, full campaign and physical controller/hardware acceptance remain open.
