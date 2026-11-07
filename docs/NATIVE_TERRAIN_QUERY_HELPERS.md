# Native terrain LOS, cell and ray transforms

Ten further inverse-helper returns use native record calculations, extending the
[terrain-input increment](NATIVE_TERRAIN_INPUT_HELPERS.md). The inverse allowlist grows
from 64 to 74; the forward list stays at 52. BozoEngine remains at `4a8f70a`.

| Return PCs of inverse helper `1da1e8` | Owner |
|---|---|
| `1f11a8`, `1f11ec` | Terrain line-of-sight endpoint setup `1f10c0` |
| `1f1b88`, `1f1c38` | Terrain line-of-sight result reconstruction `1f10c0` |
| `1f2480`, `1f24c0` | Terrain cell BSP endpoint setup `1f2370` |
| `1f2e4c`, `1f2ef8` | Terrain cell result reconstruction `1f2370` |
| `1f3110`, `1f315c` | Terrain ray grid endpoints `1f30a0` |

The existing native inverse calculation preserves all output XYZW, including meaningful
basis[0].W, without VU register, accumulator, flag or data-memory state. Overlapping operands
are captured before writes. Historical hosts and unknown callers keep the complete original
helper ABI. Translation, grid walking, scalar intersection tests, normalization, metadata,
query dispatch and checkpoints remain compiled game code.

## Numerical and generated-consumer checks

Local evidence is under `D:/BozoScratch/hierarchy-terrain-los`, against main `c4afff2` (PR #32).
Generated code, retail assets, recordings, RAM dumps and captures are excluded from Git.

- The consumer harness grows from 44,544 to 49,664 sequences and from 1,392 to 1,552
  whole-memory checks. Four host rounding modes, denormal controls, RAM aliases and W
  payloads remain covered. CPU outside retired VU, outgoing host FP effects, clocks,
  checkpoints and memory match; native VU is untouched.
- Paired LOS/ray endpoint tests retain both inverse transforms and real context translations
  through segment-subtraction dispatch. They do not run the complete grid walk.
- Cell BSP inputs run both transforms, translation and actual segment record publication,
  including the indexed BSP pointer lookup, through real BSP query dispatch.
- LOS result tails retain translated position copy, normal construction, actual normalization,
  normal copy and distance publication through debug-normal draw dispatch.
- Cell result tails run to return, including translated position/meaningful W copy, normal
  normalization, distance and packed result flags. Both result branches are exercised.
- Historical inverse coverage grows to 4,736 cases / 74 whole-memory checks. Forward coverage
  stays at 2,752 / 172. The hierarchy suite retains 786,432 helper cases / 384 whole-memory,
  8,000 geometric edges, 1,024 complete recursive BSP callers / 32 whole-memory and 2,048
  unknown-caller ABI cases. The math suite retains 1,048,576 helper cases / 512 whole-memory
  and 2,048 unknown-caller ABI cases.
- Player, headless and historical-checker builds, all eleven native suites and twelve Python
  tooling tests pass. All eleven native suites pass again after removing local observers.

## Gameplay, loading and movies

Default and strict runs complete 9,500 one-player and 8,500 actual four-player frames.
Strict contact, hierarchy, math and projection comparisons pass; strict/default recordings
and snapshots are byte-identical. All non-RAM frame fields match the preceding main throughout:
scratchpad, library calls/hash/counts, draws/hash/counts, sound/hash/sample counts, threads and
EE clock. Default runs make zero original comparisons and zero VU0 microprogram calls.

| New return | One-player executions | Four-player executions |
|---|---:|---:|
| `1f2480`, `1f24c0` | 3,319 each | 435,307 each |
| `1f2e4c` | 931 | 0 |
| `1f2ef8` | 828 | 0 |
| `1f3110`, `1f315c` | 192,000 each | 140,024 each |
| **Total** | **392,397** | **1,150,662** |

The four LOS returns have generated-consumer and historical coverage without live reach in
these routes. These are helper execution counts, not a measured speedup.

RAM hashes differ on 3,769 one-player frames (5731–9499) and 3,797 four-player frames
(4703–8499). Full 32 MB snapshots at frames 7000 / 6000 differ by 62 / 9 bytes in 31 / 3
fourth-component words. Paired temporary observers identify the original helpers writing them:

- One-player `26bb7c`: peeling-out forward return `1abd60`, output at Vehicle+`520`.
  Its inherited VF16.W is `c4f00000` in the sampled baseline versus zero natively. The
  audited later vehicle-coordinate sum consumes XYZ via the already-native vehicle block.
- One-player `4062ec`–`40640c` and `406e0c`–`406e9c`, stride 16: debris velocity vectors
  written by the original `BlowDebris` forwards `120838` / `1209a0`. Sampled W is `42700000`
  or `c4f00000` versus zero. `Debris::Update` reads velocity XYZ at offsets `120`/`124`/`128`
  for scalar integration; its fourth component is not part of those calculations.
- One-player `8fefdc`: shared stack temporary, last written by original debris forward
  `1208dc`; sampled W is `42700000` versus zero. Immediate position reconstruction reads
  XYZ. The same address also receives native ray inverse output earlier, whose W agrees.
- Four-player `3a7d9c`, `3a7dac`, `3a7dbc`: original camera forward return `113f28`.
  All 11,391 watched writes pair at identical cycles, 3,797 per field, with inherited W
  `c51bc000` versus zero; following scalar vector additions consume XYZ.

The one-player traces pair all 92 watched calls at identical cycles, with 57 differing W
writes: six peeling-out, 34 / seven debris-velocity and ten debris-temporary writes. Every one
of the 31 differing snapshot words equals its last watched helper write before the snapshot
in both runs. All four observer recordings and snapshots reproduce the corresponding qualified
baseline/native files byte-for-byte. Those original forwards remain outside the allowlist and
retain their complete ABI. Retiring inverse VU clobbers changes the payload they later inherit;
the newly native inverses' output XYZW is exact.

This is bounded writer/consumer/snapshot evidence, not a classification of all RAM differences
or all possible campaign consumers. No W restoration, memory-hash mask or shadow VU is added.
Observers were removed; original qualified math/hierarchy object hashes were restored and the
player, headless and checker executables contain no observer markers. Fresh observer-free
default replays reproduce both complete qualified recordings and snapshots byte-for-byte.

Fresh windowed runs complete the configured 4,200-frame movie/menu route and 6,500-frame
one-player and actual four-player routes through clean exit, using isolated scratch user data.
Reviewed captures show decoded movie pictures, Junkyard loading, active one-player gameplay
and four advancing snow-arena views. Device logs report 20.8 / 37.9 / 38.6 ms of silence
(movie / one-player / four-player), with underruns and dropped frames. Deterministic sound is
exact; flawless device audio, full campaign acceptance and console visual parity remain unclaimed.

## Remaining scope

A structural scan of direct generated call sites before this increment counted 269 forward
and 134 inverse sites for these two helpers, of which 52 / 64 were native. This increment
leaves 217 / 60 sites outside their allowlists, 277 in total. This is a source inventory,
including paths inside otherwise replaced functions, not a live cost or speedup measurement.
It excludes other helper families and inline VU operations.

Within owners in the hierarchy/collision range `1c0000`–`1dffff` and terrain range
`1ef000`–`1f5fff`, 22 sites remain: twelve older `hdCarTest` forward returns, two
`hierParticleNode` forward returns, six debug terrain drawing inverses, and the terrain
modify/wave setup inverses. This is not complete VU removal or campaign/console parity.
