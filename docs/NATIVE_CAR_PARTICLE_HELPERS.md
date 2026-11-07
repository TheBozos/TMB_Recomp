# Native older car and hierarchy particle transforms

Fourteen remaining forward-helper returns use native XYZ record calculations, extending the
[terrain-query increment](NATIVE_TERRAIN_QUERY_HELPERS.md). The forward allowlist grows from
52 to 66; the inverse list stays at 74. BozoEngine remains pinned to `4a8f70a`.

| Return PCs of forward helper `1da298` | Owner |
|---|---|
| `1c8f10`, `1c8f3c`, `1c8fd8`, `1c9000`, `1c908c`, `1c90b4`, `1c9140`, `1c9168`, `1c91f4`, `1c921c` | Older car-collision debug corner pairs `1c8db0` |
| `1c959c`, `1c96e8` | Older car-collision debug point/normal records `1c8db0` |
| `1d4514`, `1d4560` | Initialized/new hierarchy particle-node positions `1d4498` |

The existing native leaf captures operands before overlapping writes, computes separately
rounded XYZ and preserves destination W without VU registers, accumulator, flags or data memory.
Historical hosts and unknown callers keep the complete original ABI. Collision queries, scalar
world-coordinate addition, debug-line record construction, view lookup, FX dispatch and game
checkpoints remain compiled game code.

## Generated consumer and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-car-particles`, against main `571020e` (PR #33).
Generated game code, retail assets, recordings, snapshots and captures are excluded from Git.

- Consumer coverage grows from 49,664 to 56,832 sequences and from 1,552 to 1,776 whole-memory
  checks: 7,168 / 224 added. Four host rounding modes, denormal controls, RAM aliases and
  arbitrary temporary W payloads remain covered. CPU outside retired VU, outgoing host FP
  effects, clocks/checkpoints and other memory match; native VU is untouched.
- Each car corner pair runs the real generated transforms and scalar world sums, selects
  either debug colour branch, then executes `terrainDrawLine` through actual `drawLines`
  dispatch. Its real producer reads endpoint XYZ and explicitly writes W=1 for both vertices.
  Colour RGBA, including meaningful alpha, is copied independently and compared exactly.
- The two later car returns run their transformed-point world sums and the same actual
  debug-line producer through `drawLines` dispatch. Rendering beyond that dispatch remains
  outside these bounded fixtures; no debug-GPU parity is claimed.
- The fixture now initializes, restores and compares the real debug records at
  `7484f0`–`7485bf`, in addition to the existing stack/data/global regions. All older cases
  rerun with that comparison; complete flat-memory checks remain too.
- Initialized particle nodes run the real view-index lookup and scalar XYZ sum through return.
  New nodes run view lookup and in-place world-position addition through modeled-FX creation
  dispatch. The complete FX allocator/emitter lifecycle is outside this fixture.
- Forward W is excluded only after the real scalar consumers have run. The debug producer's
  complete XYZW records and all other writes remain exact; no production W repair is added.
- Historical forward coverage grows from 2,752 / 172 to 3,648 / 228 cases / whole-memory checks.
  Historical inverse coverage stays at 4,736 / 74. The hierarchy suite retains 786,432 helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete recursive BSP callers / 32
  whole-memory and 2,048 unknown-caller ABI cases. The math suite retains 1,048,576 helper cases
  / 512 whole-memory and 2,048 unknown-caller ABI cases.
- Player, headless and historical-checker builds and twelve Python tooling tests pass. Every
  native test target was rebuilt/relinked against the current library; all eleven suites pass.

## Gameplay, loading and movies

Default and strict runs complete 9,500 one-player and 8,500 actual four-player frames.
Each complete default recording is byte-identical to the preceding main, including RAM,
scratchpad, library calls/counts, draws/counts, sound/sample counts, threads and EE clock.
The full 32 MB snapshots at one-player frame 7000 and four-player frame 6000 are also exact.
Strict contact, hierarchy, math and projection checks pass; their recordings and snapshots
match the default runs byte-for-byte. Default runs perform zero original comparisons and
zero VU0 microprogram calls. Strict diagnostic runs intentionally execute original oracles.

Optional forward-call tracing records 1,191,280 / 381,308 total helper calls in one-player /
four-player, with zero calls to any of the fourteen new returns in both default and strict
runs. These paths have generated-consumer, numerical and historical-ABI coverage, without
live-route reach. This batch does not claim retired gameplay executions or a measured speedup.

Fresh windowed runs complete the configured 4,200-frame movie/menu and 6,500-frame one-player
and actual four-player routes through clean exit, using isolated scratch user data. Reviewed
captures show decoded movie pictures, Junkyard loading, active one-player gameplay and four
advancing snow-arena views. Device logs report 31.0 / 31.6 / 7.5 ms of silence (movie /
one-player / four-player), with underruns and dropped frames; deterministic sound remains exact.
Flawless device audio, full campaign and console visual parity remain unclaimed.

## Remaining scope

The structural two-helper inventory is now 66 native forward and 74 native inverse sites,
with 203 / 60 sites outside their allowlists, 263 in total. This includes code inside otherwise
replaced functions; it is not a live cost measurement and excludes other helpers/inline VU.

The narrow hierarchy/collision owner range `1c0000`–`1dffff` and terrain range `1ef000`–`1f5fff`
now have no remaining forward sites for this helper. Eight inverses remain there: six terrain
debug-drawing calls and terrain modify/wave setup. This is not complete VU removal, full campaign
acceptance or console visual parity.
