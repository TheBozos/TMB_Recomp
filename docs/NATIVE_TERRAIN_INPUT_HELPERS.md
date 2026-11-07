# Native terrain bounds and query-input transforms

Eleven further inverse-helper returns use the existing native record calculation. The inverse
allowlist grows from fifty-three to sixty-four entries; the forward list stays at fifty-two.
This extends the [point/ray increment](NATIVE_POINT_RAY_HELPERS.md). BozoEngine stays at
`4a8f70a`; no engine or renderer change is included.

| Return PCs of inverse helper `1da1e8` | Owner |
|---|---|
| `1ef318` | Terrain animated-node setup `1ef090` |
| `1ef44c`, `1ef484`, `1ef4c0`, `1ef4fc` | Terrain-node corner bounds `1ef3c0` |
| `1ef68c`, `1ef6d4` | Terrain-node height extrema `1ef3c0` |
| `1efc5c`, `1efca0` | Terrain height-query endpoints `1efb98` |
| `1efef8` | Quick height-query coordinates `1efe10` |
| `1f06b4` | Material-query coordinates `1f0550` |

Native leaves snapshot operands before overlapping writes, retain separately rounded XYZ
arithmetic and leave VU registers, accumulator, flags and data memory untouched. All output
XYZW matches the original, including meaningful basis[0].W. Historical hosts and unknown
callers retain the complete original helper ABI. Scalar height scans, bounds publication,
integer grid math, traversal/query dispatch, clocks and checkpoints remain compiled game code.
Other helper returns and inline VU operations remain; this is not complete VU removal.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-terrain-inputs`, against main `279e3a1` (PR #31).
Generated game code, disc data, recordings, RAM dumps and captures are excluded from the commit.

- The consumer harness grows from 38,912 to 44,544 sequences and from 1,216 to 1,392 whole-memory
  comparisons (5,632 / 176 added). Four host FP modes, denormal controls, RAM aliases and arbitrary
  W payloads are covered. CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and
  other memory match exactly; native VU remains untouched.
- Terrain-node continuations run through return: corner transformation/addition, scalar XY
  minima/maxima, actual unsigned-height scan, transformed height extrema, per-node table writes
  and aggregate bounds. Width/height 0, 1, 2 and 3, first/subsequent node publication and heights
  0, `561e`, `561f`, `ffff` exercise empty grids and the original minimum-height sentinel.
- The compared fixture region now starts at `4c0000` rather than `4d0000`, retaining the same
  upper bound and explicitly covering the terrain bounds table at `4ce290`. All older consumer
  cases rerun with this expanded memory comparison; complete flat-memory checks remain too.
- Animated-node continuations run packed material/state publication and either return or stop
  at the actual child-push boundary. Height endpoints retain real context translation, paired
  transforms and integer-cell arguments before the terrain-cell query dispatch.
- Quick-height/material continuations retain context translation, valid-grid bounds checks and
  integer coordinate conversion through the real row-stride multiplication dispatch. Identity
  bases and stored values adjacent to integer 1 and 7, zero and a positive subnormal are covered.
  Complete terrain-cell/interpolation queries remain outside those bounded fixtures.
- Historical inverse checks cover all sixty-four returns: 4,096 cases / 64 whole-memory checks.
  Historical forward checks remain 2,752 / 172. The hierarchy suite retains 786,432 helper cases,
  384 whole-memory checks, 8,000 geometric edges, 1,024 recursive BSP callers / 32 whole-memory
  checks and 2,048 unknown-caller ABI cases.
- The math suite retains 1,048,576 helper cases, 512 whole-memory checks and 2,048 unknown-caller
  ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Strict
contact, hierarchy, math and projection checks pass; strict/default recordings and snapshots
are byte-identical. Every non-RAM frame-record field matches baseline throughout: scratchpad,
calls/counts, draws/counts, sound/sample counts, thread state and EE clock. Default runs perform
zero original comparisons and zero VU0 microprogram calls.

The six terrain-node returns execute once each in both routes. Material-query return `1f06b4`
executes 500,428 / 250,178 times in one-player / four-player, retiring 500,434 / 250,184 helper
executions in total. Animated-node, paired height-query and quick-height returns have generated-
consumer/historical coverage without live-route coverage. These are counts, not timing or FPS
measurements.

RAM hashes differ on 3,906 one-player frames (5593–9499) and 3,797 four-player frames
(4703–8499). The complete 32 MB snapshots at one-player frame 7000 and four-player frame 6000
differ by four and nine bytes respectively, in two and three fourth-component words.

- One-player `4a26dc`: the original forward at BurnMan spawn return `112524` stores inherited
  VF16.W, baseline `42700000` versus native zero. Its following scalar vector addition consumes
  XYZ. This forward remains outside the native allowlist and retains its complete original ABI.
- One-player `40a85c`: `AmbientSoundManager::requestScream` copies that same position from
  `4a26d0` into its position record with two 64-bit stores. A direct copy observer confirms the
  changed high word at destination `40a85c` at identical cycle 35028251672 in both runs.
- Four-player `3a7d9c`, `3a7dac`, `3a7dbc`: the original camera forward at return `113f28`
  stores inherited VF16.W, baseline `42b20000` versus native `c51bc000`. All 11,391 watched calls
  pair at identical cycles, 3,797 for each field. The following scalar vector sums consume XYZ.
  This forward also retains the original complete ABI.

Retiring the inverse's VU clobbers changes the register payload later original forwards inherit;
the new inverses' actual output XYZW remains exact. All observer recordings and snapshots
reproduce their qualified baseline/native files byte-for-byte in both routes, including a
separate one-player sound-position copy trace. This is bounded source/write/snapshot evidence,
not a classification of every RAM difference or possible campaign consumer.

Observers were removed and restored sources forced to rebuild. The native math, hierarchy and
generated objects match their original qualified hashes; all three executables contain no
observer markers. All eleven native suites pass again. Final clean default replays reproduce
both complete qualified recordings and snapshots byte-for-byte. No padding restoration, hash
mask or shadow VU state is shipped.

Fresh windows complete the configured 4,200-frame movie/menu route and 6,500-frame one-player
and actual four-player routes through clean exit. Reviewed captures show decoded movie pictures,
Junkyard loading and active rainy gameplay, and four advancing snow-arena views. Device logs
report 22.2 / 51.2 / 30.1 ms of silence (movie / one-player / four-player), with underruns and
dropped frames. Deterministic sound matches. Flawless device audio, full campaign acceptance
and console visual parity remain unclaimed.
