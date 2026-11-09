# Native FireSpecial forward transforms (types c0, 1e0, 220)

[The UpdateLauncher batch](NATIVE_UPDATE_LAUNCHER_FORWARD.md) follows with its three aim and muzzle points.

Six more `Launcher::FireSpecial` forward returns use native XYZ arithmetic after the
[FireSpecial type a0 batch](NATIVE_FIRE_LOCKON_FORWARD.md). Known forward coverage
grows from 249 to 255 of 269 returns (14 remain). Known inverse coverage stays 134 of
134; BozoEngine stays at `4a8f70a`. With these, every frame-output forward return in
FireSpecial is native. The remaining one, `14aff4`, writes the persistent vehicle
velocity and is handled with the other persistent slots.

| Return PC | Vehicle type | Producer and consumer |
| --- | --- | --- |
| `14acf0` | `c0` | The offset at `+400` goes through the vehicle's coordinate system into frame `+0`, added to the vehicle position into the launcher's `+620` |
| `14b3ec` | `1e0` (Minion) | The nozzle at `+3a0` goes through the matrix rebuilt at frame `+90` into `+620`, plus the vehicle position |
| `14b424` | `1e0` | The offset at `+3b0` goes through the matrix at frame `+40` into frame `+0`, added into `+620` |
| `14b618` | `220` (Tanker) | `23d1f0` goes through the matrix at frame `+30` into `+620`, plus the vehicle position; then it is aimed at the target (`mathfRotationFromPointToPoint`) |
| `14b65c` | `220` | `23d200` goes through frame `+30` into frame `+0`. Its only consumer is the next transform |
| `14b670` | `220` | That point goes through the turret node's matrix (`+2a0`, `+10`) in place, then is added into `+620`. `mathfSubVector` measures from `+620` |

The consumers are `mathfAddVector`, the next transform's input (whose W the
transform ignores), `mathfRotationFromPointToPoint` and `mathfSubVector`. They read
only XYZ. The weapon constructors copy `+620` with `mathfCopyVector`, and launcher
`+62c` has no reader. FireSpecial never loads frame `+0c`, and it reads no VU
registers, accumulator or flags.

`14b670` is in place, so the input is read whole before the output is written. The
existing native helper computes separately rounded XYZ, preserves destination W and
leaves VU state untouched. Unknown and historical callers keep the original ABI. No
engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/fire-torch-forward`, against parent `0676443`
(PR #86). Parent headless/window binaries are preserved there and match PR #86's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons (caller
  kinds 354 to 359). They are helper-boundary continuations and entries at the
  call-return label before each transform (`14acdc`, `14b3dc`, `14b414`, `14b604`,
  `14b648`, `14b65c`), across four rounding modes, FTZ/DAZ, RAM aliases,
  identity/quarter-turn bases and arbitrary W payloads.
- Each run stops at the first call after its result is consumed: `GetLockOn`,
  `mathfMulMatrix`, `mathfRotMatrixPH`, `mathfRotationFromPointToPoint` or
  `mathfSubVector`.
- A run from `14b648` reaches both Tanker turret transforms. The first output is
  recorded, with a zero base, when the second transform reads it.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result. The launcher's `+620` is asserted exactly
  against the original FPU arithmetic. Publications: 512 for each site, and 1,024
  for `14b670`, which every `14b648` run also reaches.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+0c` or launcher `+62c`) is normalized for test comparison, after every
  real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 714 → 708,
  real libm errno cases 835 → 831). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 184,320 generated sequences / 9,504 whole-memory
  comparisons. Historical forward ABI coverage is 15,744 / 984; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `0676443`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The six returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 56.7 ms of silence in one-player, 32.4 ms in four-player and 24.8 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The c0, Minion and Tanker specials in actual play, full campaign and physical controller/hardware acceptance remain open.
