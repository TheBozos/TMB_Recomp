# Native camera and rear-view transforms

Nine forward-transform returns now use native record calculations, extending the
[peel-out increment](NATIVE_PEEL_OUT.md). Forward coverage grows from 80 to 89
sites; inverse coverage stays at 89. BozoEngine remains pinned to `4a8f70a`.

| Return PC | Owner | Checked consumer boundary |
| --- | --- | --- |
| `1137cc` | `Camera::Update` | Position addition and flag clearing through height-query handoff |
| `11398c` | `Camera::Update` | Coordinate-system position publication and camera-mode gate |
| `113a78` | `Camera::Update` | Position addition, target/history records, scalar state and matrix publication |
| `113b28` | `Camera::Update` | Position addition through rotation-from-points handoff |
| `113c2c` | `Camera::Update` | Position addition, three-axis smoothing, history and matrix publication |
| `113f28` | `Camera::Update` | Target reconstruction, vehicle-relative difference, history and smoothing through LOS handoff |
| `1142d8`, `114394` | `Camera::Update` | Left/right collision-probe positions through LOS handoff |
| `1fdd3c` | `viewSetRearViewPos` | Global position/orientation publication and complete return |

The existing native helper snapshots its operands, computes separately rounded
XYZ and retains destination W. Historical hosts and unknown callers retain the
complete original ABI. Selection, smoothing, angles, collision queries and game
checkpoints remain compiled game code.

## Coupled inline position consumer

The `11398c` path also replaces four instructions at `11399c..1139a8` before
their VU loads, addition and store. XYZ comes directly from vehicle position and
transformed offset records. Original `VADD.xyz` retains `VF13.W`, loaded from the
third camera-basis column by the immediately preceding transform. The native
consumer publishes that basis record's W directly. All inputs are read before
publication, including an in-place position alias. No register shadow,
inter-call cache or VU reconstruction is introduced. Published XYZW is exact.

## Generated comparisons

Evidence is under `D:/BozoScratch/camera-transforms`, against main `00b7414`
(PR #41). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 4,608 generated producer/consumer sequences / 288 whole-memory
  comparisons. Aggregate coverage is 76,288 / 2,752. Four host rounding modes,
  denormal controls, RAM aliases, all four camera slots, aliased camera/vehicle
  coordinate systems and arbitrary basis/output W payloads are covered.
- Each camera site has 256 helper-boundary cases and 256 cases entering preceding
  generated setup, covering offsets, vehicle dimensions, desired positions and
  smoothing/history updates. They stop at the real boundaries in the table;
  no query result is stubbed. These are not complete camera-update entry tests.
- Rear-view has 256 helper-boundary cases and 256 actual function entries,
  including non-primary-player early returns, both layout branches,
  positive/negative distances and real rotation/sine/cosine producers. Rotation
  inputs stay within half a radian; wider argument reduction is outside these fixtures.
- CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and final
  memory agree. Forward-output W in offset/desired-target records is excluded
  only after real consumers run. The coupled coordinate-system position's W
  is compared exactly. Native leaves and the new inline
  consumer leave VU state untouched.
- Complete rear-view entries retain the original rotation producer's
  `mathfUnitMatrixAsm` VU effects. A test captures state at entry to the native
  leaf and checks it remains unchanged through that leaf and the entire scalar
  consumer. This does not replace the identity producer or claim whole-function
  VU removal.
- Historical forward ABI coverage grows to 5,120 / 320 cases / whole-memory
  checks; inverse stays at 5,696 / 89. Existing hierarchy coverage retains
  786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete
  BSP callers / 32 whole-memory and 2,048 unknown ABI cases.
- Player, headless runner, historical checker and all test targets build.
  All eleven native suites and twelve Python tooling tests pass. The hierarchy
  suite passes again with arbitrary published basis-W payloads.

## Replay qualification

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent, including raw RAM,
scratchpad, calls, draws, sound, threads, counts and EE clocks. Both sampled full
32 MiB RAM snapshots match exactly. Strict/default recording files and snapshots
are identical. No observer, RAM mask or VU-state restoration is needed.

| New return | One-player calls | Four-player calls |
| --- | ---: | ---: |
| `113a78` | 2 | 4 |
| `113f28` | 3,900 | 15,188 |
| `1142d8` | 3,900 | 15,188 |
| `114394` | 3,900 | 15,188 |
| `1fdd3c` | 3,902 | 3,798 |
| Total | 15,604 | 49,366 |

The other four camera sites, including the coupled inline consumer, are cold in
these routes and qualified by generated comparisons. Total native forward calls
become 1,157,512 / 352,070. Default routes make zero original-helper comparisons
and execute zero VU0 microprograms. Counts establish reach, not a measured speedup.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, with isolated scratch
user data. Reviewed captures show decoded movie action (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), combat/movement with targeting and
effects (one-player 5,500 / 6,300), and four advancing snow-arena views
(four-player 5,500 / 6,300). These are bounded functional checks, not pixel-exact
windowed or console parity.

Windowed one-player reaches `113a78` once, the three hot camera sites 1,708 times
each, and rear-view 1,709 times: 6,834 new native calls. Four-player reaches them
4 / 7,192 each / 1,799 times respectively: 23,379 new calls. The other four sites
remain cold; all nine are cold in the movie route.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
20.9 / 82.9 / 30.3 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound is exact; flawless device audio
and subjective listening are not claimed.

## Remaining scope

These two helper inventories now contain 89 native forward and 89 native inverse
sites. Outside their allowlists remain 180 forward and 45 inverse sites, 225 in
total across the game. This structural inventory excludes other helper families
and inline VU operations. Full campaign, all camera modes in live play and
console visual/audio parity remain separate work.
