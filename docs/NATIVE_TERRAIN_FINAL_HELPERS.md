# Native terrain debug, edit and wave transforms

Eight more inverse-helper returns use native record calculations, extending the
[car/particle increment](NATIVE_CAR_PARTICLE_HELPERS.md). The inverse allowlist grows from
74 to 82; the forward list stays at 66. BozoEngine remains pinned to `4a8f70a`.

| Return PCs of inverse helper `1da1e8` | Owner |
|---|---|
| `1f5414`, `1f5458` | Terrain grid-space debug line `1f53e0` |
| `1f54f4`, `1f5538` | Terrain grid-space two-colour line `1f54b8` |
| `1f55d4`, `1f5618` | Terrain grid-space debug box `1f55a0` |
| `1f56a8` | Terrain modification coordinates `1f5678` |
| `1f5904` | Terrain wave coordinates `1f5898` |

The existing native inverse snapshots operands before overlapping writes, preserves exact
output XYZW (including meaningful basis[0].W), and leaves VU registers, accumulator, flags
and data memory untouched. Historical hosts and unknown callers retain the complete original
ABI. Scalar translation, debug records, grid indexing, input polling, wave metadata/type
dispatch and game checkpoints remain compiled game code.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-terrain-final`, against main `199d9a6` (PR #34).
Generated game code, retail assets, recordings, snapshots and captures are excluded from Git.

- Consumer coverage grows from 56,832 to 60,928 sequences and from 1,776 to 1,904 whole-memory
  comparisons: 4,096 / 128 added. Four host rounding modes, denormal controls, RAM aliases
  and arbitrary meaningful W payloads remain covered. CPU outside retired VU, outgoing host
  FP effects, clocks/checkpoints and memory match exactly; native VU stays untouched.
- Paired debug endpoints run both inverse transforms and scalar world sums through the real
  line or two-colour vertex builder and actual `drawLines` dispatch. The box also runs real
  corner construction and its first line producer to the same dispatch. Complete colour RGBA
  and vertex XYZW are compared; rendering and subsequent box edges remain outside the fixture.
- The compared debug region expands to `7484f0`–`74870f`, covering the actual two-colour line
  records. The actual input-map word `66fcc4` is also initialized/restored/compared. The fixture
  uses a common region table; all older consumers rerun with the expanded memory checks.
- Terrain editing runs to return: translated XYZ, actual integer conversion/row-stride cell
  lookup, protected-cell branch, and real `inputGetInput` disabled-device branches. Both
  protected and unprotected cells are covered on a 9x9 grid, with coordinates around integer
  boundaries, zero and a subnormal. Button-driven height changes remain outside this fixture.
- Wave inputs use the actual six-entry type table: types 0/1/4/5 run scalar translation and
  the real rounded bounds/extent metadata to timer dispatch; type 2 reaches undulate dispatch,
  and type 3 reaches the random-number dispatch before shock-wave work. Four wave slots,
  radii 0/0.25/0.5/1/9.5/10.5, signed integer-adjacent coordinates and general bases are covered.
  Timer input, special-wave effects and later height-cache population are outside the fixture.
- Historical inverse coverage grows from 4,736 / 74 to 5,248 / 82 cases / whole-memory checks.
  Forward coverage stays at 3,648 / 228. The hierarchy suite retains 786,432 helper cases / 384
  whole-memory, 8,000 geometric edges, 1,024 complete recursive BSP callers / 32 whole-memory
  and 2,048 unknown-caller ABI cases. The math suite retains 1,048,576 cases / 512 whole-memory
  and 2,048 unknown-caller ABI cases.
- Player, headless, historical-checker and every native test target rebuild successfully;
  all eleven native suites and twelve Python tooling tests pass.

## Gameplay, loading and movies

Default and strict runs complete 9,500 one-player and 8,500 actual four-player frames.
Each complete default recording is byte-identical to the preceding main, including RAM,
scratchpad, library calls/counts, draws/counts, sound/sample counts, threads and EE clock.
Full 32 MB snapshots at one-player frame 7000 and four-player frame 6000 are exact too.
Strict contact, hierarchy, math and projection checks pass; their recordings and snapshots
are byte-identical to default. Default runs perform zero original comparisons and zero VU0
microprogram calls; strict diagnostic runs intentionally execute original oracles.

All eight new inverse counters remain zero in both default and strict routes. These sites
have generated-consumer, numerical and historical-ABI coverage without live-route reach.
This increment does not claim retired gameplay executions or a measured speedup.

Fresh windowed runs complete the configured 4,200-frame movie/menu route and 6,500-frame
one-player and actual four-player routes through clean exit, using isolated scratch user data.
Reviewed captures show decoded movie pictures, Junkyard loading, active one-player gameplay
and four advancing snow-arena views. Device logs report 17.7 / 42.4 / 76.2 ms of silence
(movie / one-player / four-player), with underruns and dropped frames. Deterministic sound
remains exact; flawless device audio, full campaign and console visual parity remain unclaimed.

## Remaining scope

A structural scan of the two helpers now counts 66 native forward and 82 native inverse
sites. Outside their allowlists remain 203 forward and 52 inverse sites, 255 in total across
the game, including code inside otherwise replaced functions. This is a source inventory,
not a live cost measurement, and excludes other helper families and inline VU operations.

Within the narrow hierarchy/collision owner range `1c0000`–`1dffff` and terrain range
`1ef000`–`1f5fff`, every direct call to these two transform helpers is now native. Other
helpers, inline VU, full campaign acceptance and console visual parity remain separate work.

The qualified routes still execute 193,096 / 151,213 original forward calls outside those
ranges in one-player / four-player. Active candidates include suspension return `1888e8`,
tire-height return `18b4c0`, particle emission `1e4818`, and camera returns `113f28` / `1142d8` /
`114394`. Their W/state consumers require separate audits before native routing. These are
reach counts, not timing measurements; local caller/owner evidence is in `remaining-forward-reach.json`.
