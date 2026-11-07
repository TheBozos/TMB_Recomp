# Native hierarchy helper consumers

The later [query/lighting increment](NATIVE_HIERARCHY_QUERY_RESULTS.md) adds twelve further returns,
bringing the inverse/forward allowlists to twenty-six / twenty-seven.

Twelve more hierarchy return sites use the existing native record calculations. The inverse
allowlist grows from twenty to twenty-four entries; forward directions grow from nine to seventeen.
BozoEngine remains pinned at `4a8f70a`. This change extends the
[sphere/LOD increment](NATIVE_SPHERE_LOD.md), without an engine or renderer change.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1d0de0`, `1d0dfc`, `1d12d8` | Segment collision grid `1d0988` |
| Inverse `1da1e8` | `1d4b94` | Control-node BSP segment setup `1d4ab8` |
| Forward `1da298` | `1cfd18`, `1cfde8` | Height collision-volume results `1cfb10` |
| Forward `1da298` | `1d0224`, `1d0250` | Height BSP results `1d0190` |
| Forward `1da298` | `1d25ec`, `1d261c` | Line-of-sight BSP results `1d2560` |
| Forward `1da298` | `1d2784`, `1d2804` | Line-of-sight volume results `1d26f0` |

The player leaves snapshot matrix/input records before writing, retain separately rounded XYZ
arithmetic, and leave VU registers, accumulator, flags and data memory untouched. Inverse output
matches all four components, including basis[0].W; forward output preserves destination W.
The actual scalar dot products, interpolation, result publication, squared-distance calculations,
packed metadata, traversal, callbacks, clocks/checkpoints and epilogues remain compiled game code.
Historical hosts and unaudited return sites retain the original complete helper ABI. Other helper
callers and inline VU operations remain; this is not complete VU removal.

## Numerical and consumer checks

Local evidence is under `D:/BozoScratch/hierarchy-helpers`, against main `4b36de2` (PR #25).
Generated game code, disc data, captures, recordings and RAM dumps are excluded from the commit.

- `tmb-native-hierarchy-tests` adds 6,144 real generated consumer sequences and 192 complete-memory
  comparisons across all twelve returns. They cover four host FP modes, denormal controls, RAM
  aliases and arbitrary forward-output W payloads. CPU state outside retired VU, complete outgoing
  host FP effects, clocks/checkpoints and other memory must match; native VU state must be untouched.
- The original forward helpers run with their actual W values and VU effects through every real
  consumer in a sequence. Only the final unused forward-output W fields are normalized afterward.
  W-dependent scalar results, register effects or other copied memory would therefore fail the
  comparison. Inverse outputs are compared without a component mask.
- Grid sequences stop before the following matrix helper or hierarchy push; the control sequence
  stops at the BSP query dispatch. Height normal publication runs through its following dot product
  to the double-conversion boundary; its offset tail runs through scalar addition and return.
  Both BSP result tails and the line-of-sight volume tail exercise their actual scalar publication,
  distance/metadata branches and return. These tests start at helper calls, not at complete query roots.
- The hierarchy suite also retains 786,432 helper comparisons, 384 complete-memory checks, 8,000
  geometric edges, 1,024 recursive BSP callers (32 complete-memory checks), 2,048 unknown-caller ABI
  cases, and now 1,536 historical inverse cases across all twenty-four returns (24 complete-memory
  checks). Another 512 historical forward cases/32 complete-memory checks cover the eight new returns.
- The math suite retains 1,048,576 helper comparisons, 512 complete-memory checks and 2,048
  unknown-caller ABI cases, with overlap, address alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python tooling
  tests pass. After temporary diagnostics, restored sources were forced to rebuild; the native math
  object matches its original qualified hash and the three executables contain no observer marker.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Every
non-RAM frame-record field matches the baseline throughout: scratchpad, calls/counts, draws/counts,
sound/sample counts, thread state and EE clock. Strict contact, hierarchy, math and projection
comparisons pass; strict/default recording and snapshot files are identical. Defaults make zero
original comparisons and zero VU0 microprogram calls.
After diagnostic removal, another clean default replay of both full routes reproduces the qualified
recording and snapshot files byte-for-byte; all eleven native suites also pass on the restored build.

The four new inverse returns retire another 185,191 / 65,299 original helper executions in
one-player / four-player. The control node accounts for 182,727 / 55,901; the first two grid returns
each account for 1,232 / 4,699. The third grid return `1d12d8` has generated-consumer/unit coverage
but is absent from these live routes. All eight new forward returns run in one-player, totaling
24,308 executions; four-player totals 692. Thus this batch retires another 209,499 / 65,991 original
helper executions. These counts are not timing or FPS measurements.

RAM hashes differ on 3,447 one-player frames (first 5591, last 9499) and 3,741 four-player frames
(4759 through 8499). Complete 32 MB snapshots at one-player frame 7000 and four-player frame 6000
each differ by two bytes in one stack word: `8ff21c` changes from 60.0 to zero; `8ff16c` from 89.0
to zero, respectively. Paired baseline/native observers directly trace both to forward return
`1d0250`, the height BSP temporary at SP+`10` with unused W at SP+`1c`. The original helper stores
inherited VF16.W; native XYZ publication preserves the existing zero. Diagnostic snapshots match
their corresponding qualified snapshots exactly. The scalar addition following this helper reads
only XYZ; generated consumer tails retain the complete CPU/FP/memory checks described above.
This is bounded snapshot/consumer evidence, not proof that every RAM difference or campaign path
has been exercised. No hash mask, padding restoration, shadow VU state or observer is shipped.

Fresh window runs complete 4,201 game frames for movies/menus and 6,502 each for one-player and
actual four-player (4,200 / 6,501 / 6,501 counted after the first presented picture). Reviewed captures
show decoded movie pictures, Junkyard loading and rainy combat, and four advancing snow-arena views.
Device logs report 21.6 / 49.9 / 40.3 ms of silence (movie / one-player / four-player), with underruns
and dropped audio frames. Deterministic sound records match. Full campaign acceptance, flawless
device audio and console visual parity remain unclaimed.
