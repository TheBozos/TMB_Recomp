# Native hierarchy query and lighting transforms

Twelve further hierarchy returns use the existing native record implementations: ten forward
directions and two inverse directions. The audited allowlists now contain twenty-seven forward
and twenty-six inverse returns. This extends [the grid/result increment](NATIVE_HIERARCHY_HELPERS.md).
BozoEngine remains pinned at `4a8f70a`; no shared engine or renderer change is included.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1cd400` | Animated collision transform `1cd180` |
| Inverse `1da1e8` | `1cd768` | BSP auxiliary segment setup `1cd538` |
| Forward `1da298` | `1cbbc8`, `1cbc2c` | Point-contact result `hdPunt`, `1cba48` |
| Forward `1da298` | `1cc634`, `1cc6b4` | Line-contact result `1cc588` |
| Forward `1da298` | `1ccec8`, `1ccef8` | Terrain point check `1cce58` |
| Forward `1da298` | `1cd884`, `1cd8b4` | BSP auxiliary result publication `1cd538` |
| Forward `1da298` | `1d41c8` | Burning attachment point `1d4188` |
| Forward `1da298` | `1d9cac` | Light-volume colour coordinates `1d9c30` |

The leaves snapshot operands before overlapping writes and preserve the existing separately rounded
arithmetic. Inverse XYZW matches the original, including basis[0].W; forward XYZ matches while
destination W is preserved. The player path leaves VU registers, accumulator, flags and data memory
untouched. Scalar interpolation, normalization, result packing, matrix allocation/index bookkeeping,
colour sampling/interpolation, view lookup, clocks/checkpoints and callbacks remain compiled game
code. Historical hosts and unaudited callers retain their original full ABI. Other helper returns
and inline VU operations remain; this is not complete VU removal.

## Numerical and consumer checks

Local evidence is under `D:/BozoScratch/hierarchy-query-results`, against main `9a8e698` (PR #26).
Generated game code, disc data, captures, recordings and RAM dumps are excluded from the commit.

- The hierarchy consumer harness grows from 6,144 to 12,288 generated sequences, with 384
  complete-memory comparisons (6,144 / 192 added). Four host FP modes, denormal controls, RAM
  aliases and arbitrary output W payloads are covered. The reference runs the real original
  forward helpers and leaves their W/VU effects alive throughout each consumer sequence. Only
  final unused forward-output W fields are normalized afterward. CPU outside retired VU, complete
  outgoing host FP effects, clocks/checkpoints and other memory must match; native VU is untouched.
  Inverse XYZW is compared without a component mask.
- Point and line tails run through scalar reconstruction, squared distance where applicable, packed
  material/result fields and return. Terrain tails stop at result-buffer clearing after their actual
  zero/copy, normalization and unit-box branches; exact and adjacent axis limits are included.
- The animated-node tail exercises matrix-capacity values 0, 1199 and 1200, the retained scalar matrix
  product and model-mode index publication through return. BSP inverse setup stops at its segment
  query dispatch; auxiliary result publication stops at the next point-loop boundary after metadata
  writes. These are helper-consumer sequences, not complete query-root/callback coverage.
- The burning-attachment sequence runs view lookup and XYZ addition to the burning-point callback
  dispatch. The light-volume sequence runs the full remaining colour tail, including wrapping,
  disabled sampling, four-texel interpolation, double-precision scalar helpers and RGBA packing.
  Half-texel neighbouring representations and independent colour payloads are included. This does
  not claim a new particle producer or a replacement for the complete light-volume owner.
- Historical forward checks grow to 1,152 cases / 72 complete-memory comparisons (640 / 40 added).
  The hierarchy suite retains 786,432 helper comparisons, 384 complete-memory checks, 8,000 geometric
  edges, 1,024 recursive BSP callers / 32 complete-memory checks and 2,048 unknown-caller ABI cases.
  Historical inverse checks now cover all twenty-six returns: 1,664 cases / 26 complete-memory checks.
- The math suite retains 1,048,576 helper comparisons, 512 complete-memory checks and 2,048
  unknown-caller ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python tooling
  tests pass. After temporary diagnostics, restored sources are forced to rebuild; the math object
  matches its original qualified hash, all three executables lack observer markers and all eleven
  suites pass again on the restored build.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Every
non-RAM frame-record field matches the baseline throughout: scratchpad, calls/counts, draws/counts,
sound/sample counts, thread state and EE clock. Strict contact, hierarchy, math and projection
comparisons pass; strict/default recordings and snapshots are identical. Defaults perform zero
original comparisons and zero VU0 microprogram calls.
After diagnostic removal, clean default replays of both full routes reproduce the qualified
recording and snapshot files byte-for-byte.

The new forward returns retire 59,521 / 21,822 original helper executions in one-player / four-player;
all ten run in one-player. The two new inverse returns execute 15,991 and 7 times in one-player,
and are absent from the four-player route, with differential consumer/unit coverage retained.
The batch therefore retires another 75,519 / 21,822 original helper executions. These are counts,
not timing or FPS measurements.

RAM hashes differ on 3,421 one-player frames (first 5594, last 9499) and 2,713 four-player frames
(first 4703, last 8499). Complete 32 MB snapshots at one-player frame 7000 and four-player frame
6000 each differ by one byte in a stack W word: `8fedcc` is zero versus native bits `000000d6`;
`8fef6c` is zero versus native bits `00000007`, respectively. Paired observers directly identify
line-result return `1cc6b4`, whose output is SP+`20` and unused W is SP+`2c`. At identical EE clocks,
the original overwrites the existing integer payload with inherited VF16.W zero; native preserves
it. Diagnostic snapshots exactly reproduce their corresponding baseline/native snapshots.
The following scalar consumers use XYZ, with complete tail checks above. This is bounded snapshot
evidence, not a classification of every raw-RAM difference or campaign consumer. No new hash mask,
padding restoration, shadow VU state or observer is shipped.

Fresh window runs complete 4,201 game frames for movies/menus and 6,501 each for one-player and
actual four-player. Reviewed captures show decoded movie pictures, Junkyard loading and rainy
combat, and four advancing snow-arena views. Device logs report 25.2 / 64.5 / 27.8 ms of silence
(movie / one-player / four-player), with underruns and dropped audio frames. Deterministic sound
records match. Full campaign acceptance, flawless device audio and console visual parity remain
unclaimed.
