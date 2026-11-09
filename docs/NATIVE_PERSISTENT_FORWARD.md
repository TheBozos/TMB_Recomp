# Native persistent-field forward transforms (last three)

The last three forward returns use native XYZ arithmetic after the
[BlowDebris batch](NATIVE_BLOW_DEBRIS_FORWARD.md). Known forward coverage reaches
**269 of 269 returns**; known inverse coverage stays 134 of 134. BozoEngine stays at
`4a8f70a`. Every inventoried forward-transform (`mathfMulTransVec`, `1da298`) return
is now native. Unknown and historical callers keep the original ABI.

Each of these writes a long-lived object field, not the stack:

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `14aff4` | `Launcher::FireSpecial`, type `1a0` | The horn push in frame `+30` goes through the vehicle basis copied to `+190` into the vehicle's velocity `+1e0`. Nothing reads it before the horn sound and the owner's return |
| `112524` | `BurnMan::Spawn` | The spawn offset in frame `+10` goes through the new pedestrian's coordinate system into its position (`cs +10`). `mathfAddVector` adds the vehicle position (the output is its first operand); the grid reparent and later updates read it as XYZ |
| `117710` | `Vehicle::collisCheapTires` (free fall) | The tire offset in frame `+0` goes through the plane matrix at frame `+10` into tire 0's `+50`. `smoothEasyIn` reads its three components as scalars |

An audit looked for readers of each field's W anywhere in the generated code:

- **Vehicle velocity W (`+1ec`):** no 32-bit load has a vehicle base, there are no
  64-bit loads at `+1e8`, and every place that passes `+1e0` on reads XYZ only.
  `vehicleUpdatePosition` rewrites the whole field with a 128-bit store each frame
  before reading it.
- **Pedestrian position (`cs +1c`):** `hierInitCs` never writes it, and the hierarchy
  update (native here) ignores it. `requestScream` copies the whole position into a
  streaming-sound slot, but that slot's W (`+3c`) has no reader: the sound code reads
  the position only as XYZ.
- **Tire offset (`+5c`):** no reader of `+5c` on a tire, or of the absolute offsets
  of the four tires.

The owners read no VU registers, accumulator or flags after these calls. The native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/persistent-forward`, against parent `363eb61`
(PR #91). Parent headless/window binaries are preserved there and match PR #91's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,536 generated sequences / 96 whole-memory comparisons (caller
  kinds 371 to 373). They are helper-boundary continuations and entries at the
  call-return label before each transform (`14afd8`, `1124ec`, `117700`), across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Each run stops at the first consumer after the result: the horn sound,
  `hdReparentCsGrid` and `smoothEasyIn`. The velocity and tire results are read
  back from their fields at the stop. The pedestrian's position (output plus
  vehicle position) is asserted exactly against the original FPU arithmetic.
  Publications: 512 for each site.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result. Only the forward W of each field (`+1ec`, `cs
  +1c`, tire `+5c`) is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 712,
  real libm errno cases 819 → 842). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 191,488 generated sequences / 9,952 whole-memory
  comparisons. Historical forward ABI coverage is 16,640 / 1,040; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `363eb61`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. `112524` (BurnMan spawn) is live on the one-player route: it runs natively once with no recorded difference. `14aff4` and `117710` are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 49.8 ms of silence in one-player, 7.5 ms in four-player and 0.0 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Full campaign and physical controller/hardware acceptance remain open.
