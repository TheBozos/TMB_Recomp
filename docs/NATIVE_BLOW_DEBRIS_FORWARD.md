# Native BlowDebris forward transforms

[The last three persistent-field sites](NATIVE_PERSISTENT_FORWARD.md) complete the forward coverage.

The six `DebrisStuff::BlowDebris` forward returns use native XYZ arithmetic after the
[HUD laser batch](NATIVE_HUD_LASER_FORWARD.md). Known forward coverage grows from 260
to 266 of 269 returns (3 remain). Known inverse coverage stays 134 of 134; BozoEngine
stays at `4a8f70a`.

`DebrisStuff::BlowDebris` (`120480`) blows an object apart. The debris table at `s7`
has `0xb20` bytes per entry, and each piece's velocity sits at `+120 + 16k`. The
piece type (`*hierhead & 0x3f`) selects the branch:

| Return PC | Output | Producer and consumer |
| --- | --- | --- |
| `120788` | frame `+10` | Type 3: the piece's offset (hierhead `+10`) goes through the blast matrix at frame `+30`. Its X, Y, Z are added as scalars to a position into the piece's hierarchy node `+10`, then aimed (`mathfRotationFromVector`) |
| `120838` | velocity (persistent) | Type 3: a random spin vector in frame `+10` goes through the matrix into the piece's velocity |
| `1208dc` | frame `+10` | Type 4: like `120788`, in place |
| `1209a0` | velocity (persistent) | Type 4: like `120838` |
| `120b28` | velocity (persistent) | Other types: like `120838` |
| `120c3c` | frame `+20` | With a push requested: the push goes through the matrix; its X and Y are added to the velocity |

No code reads these outputs' W:
- **Frame outputs:** read as scalars X/Y/Z and by `mathfRotationFromVector` (XYZ).
  The owner never loads frame `+1c` or `+2c`.
- **Debris velocity:** the debris init (`120228`, `120350`) and `Debris::Update`
  (`120e20`) read and write only `+120/+124/+128`. Every other `+12c` load in the
  generated code is an unrelated integer field or a stack slot.

The owner reads no VU registers, accumulator or flags. `1208dc` is in place, so the
input is read whole before the output is written. The existing native helper computes
separately rounded XYZ, preserves destination W and leaves VU state untouched.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/blow-debris-forward`, against parent `c6d7787`
(PR #90). Parent headless/window binaries are preserved there and match PR #90's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons (caller
  kinds 365 to 370). They are helper-boundary continuations and entries at the
  call-return label before each transform (`120774`, `12080c`, `1208c8`, `120974`,
  `120aec`, `120c2c`), across four rounding modes, FTZ/DAZ, RAM aliases,
  identity/quarter-turn bases and arbitrary W payloads.
- These outputs are not consumed by `mathfAddVector`. At each run's stop, the
  harness therefore reads the pending transform's output back from its own
  destination, untouched since, and records it with a zero base. The stops are
  `mathfRotationFromVector`, `mathfCopyVector`, `mathfRPHFromMatrix` and
  `mathfRand`.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result. Published values are asserted exactly against the
  original FPU arithmetic: the hierarchy node position (`120788`, `1208dc`) and the
  velocity after the push (`120c3c`). Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward W (frame
  `+1c`, `+2c` or the velocity's `+12c`) is normalized for test comparison, after
  every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 707 → 709,
  real libm errno cases 859 → 819). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 189,952 generated sequences / 9,856 whole-memory
  comparisons. Historical forward ABI coverage is 16,448 / 1,028; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Strict/default recordings and snapshots are byte exact; strict comparisons
report no mismatch. Four returns are live on the one-player route: `120788` and
`120838` run natively 7 times each, `1208dc` and `1209a0` 34 times each.
`120b28` and `120c3c` are cold on these routes and are qualified by the generated
owner suite above.

**One recorded difference, proven to be a dead W slot.** This applies the rule
accepted for the [machine-gun increment](NATIVE_MACHINE_GUN_FORWARD.md). Against
parent `c6d7787`, the four-player recording matches on every field. The one-player
recording matches on every field except the end-of-frame RAM hash. That hash differs
on 309 frames (6,899 to 7,207, then matches again), and the sampled RAM image
differs in the same single word.

- A RAM dump right after frame 6,899 shows exactly one differing word, `0x8fefdc`:
  the parent has `00000000`, the native build `42700000`.
- A temporary logging build showed `1208dc`'s output at `0x8fefd0` (`sp =
  0x8fefc0`), so the word is that transform's dead frame `+1c` W slot.
- The same temporary build wrote a NaN pattern into the W of every BlowDebris
  output, including the persistent debris velocities. Over the full one-player
  route only the RAM hash differed from the parent: calls, draws, sound, thread
  state, EE timing and every count matched.

The experiment was not committed; the qualified binary was restored afterwards and
matches the recorded hash.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 55.8 ms of silence in one-player, 26.2 ms in four-player and 15.0 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Full campaign and physical controller/hardware acceptance remain open.
