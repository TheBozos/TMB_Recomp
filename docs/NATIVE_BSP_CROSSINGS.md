# Native BSP point and segment-crossing transforms

Thirteen further hierarchy returns use the existing native record calculations. Seven inverse
and six forward additions bring both allowlists to thirty-three entries. This extends the
[query/lighting increment](NATIVE_HIERARCHY_QUERY_RESULTS.md). BozoEngine remains pinned at
`4a8f70a`; no shared engine or renderer change is included.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1cdf80`, `1ce028`, `1ce220` | BSP point setup and normalization `1cdeb8` |
| Inverse `1da1e8` | `1ce644`, `1ce674` | Both-endpoints-outside crossing `1ce588` |
| Inverse `1da1e8` | `1ce7b4` | Start-outside crossing `1ce738` |
| Inverse `1da1e8` | `1cea40` | End-outside crossing `1ce958` |
| Forward `1da298` | `1cdf40`, `1cdfe4`, `1ce1cc` | BSP point setup `1cdeb8` |
| Forward `1da298` | `1ce37c`, `1ce3a4` | BSP point-result publication `1cdeb8` |
| Forward `1da298` | `1cdd3c` | BSP segment-result publication `1cd538` |

The player leaves snapshot operands before overlapping writes, retain separately rounded XYZ
arithmetic, and leave VU registers, accumulator, flags and data memory untouched. Inverse XYZW
matches the original, including basis[0].W; forward XYZ matches while destination W is preserved.
Scalar normalization, axis tests, averages, crossing-point emission, duplicate/capacity guards,
packed result fields, clocks/checkpoints and query traversal remain compiled game code.
Historical hosts and unaudited callers retain their complete original helper ABI. Other helper
returns and inline VU operations remain; this is not complete VU removal.

## Numerical and generated-consumer checks

Local evidence is under `D:/BozoScratch/hierarchy-bsp-crossings`, against main `b3d03f6` (PR #27).
Generated game code, disc data, captures, recordings and RAM dumps are excluded from the commit.

- The consumer harness grows from 12,288 to 18,944 generated sequences and from 384 to 592
  complete-memory comparisons (6,656 / 208 added). Four host FP modes, denormal controls, RAM
  aliases and arbitrary W payloads are covered. CPU outside retired VU, complete outgoing host
  FP effects, clocks/checkpoints and other memory must match; native VU must be untouched.
- Original forward W and VU effects remain live throughout the real consumer sequences. Only
  final unused forward-output W fields are normalized afterward. A full inverse write can reuse
  that same record and replace opaque W with meaningful basis[0].W: the reference harness now
  removes the earlier forward-W exclusion at that write, so the later inverse W is compared
  exactly. No production arithmetic or W contract was changed for this fixture correction.
- BSP setup sequences retain their real point-mode, zero/copy, short-normalization and unit-box
  decisions, then stop at the following segment-query dispatch. Exact/adjacent unit-axis values
  are included. Point-result publication runs through packed material/flag fields and return;
  the segment-result tail stops at its next point-loop boundary after scalar result addition.
- Both/start/end-outside sequences run through the actual generated point emitter and return,
  including axis classifications, averaging, duplicate flags and counts 0, 48 and 49 at the
  50-point capacity boundary. Start/end cases include exact/adjacent positive and negative
  five-unit representations. These are helper-consumer tails, not complete query-root coverage.
- Historical forward checks grow to 1,536 cases / 96 complete-memory comparisons (384 / 24 added).
  Historical inverse checks cover all thirty-three returns: 2,112 cases / 33 complete-memory checks.
  The hierarchy suite retains 786,432 helper comparisons, 384 complete-memory checks, 8,000 geometric
  edges, 1,024 recursive BSP callers / 32 complete-memory checks and 2,048 unknown-caller ABI cases.
- The math suite retains 1,048,576 helper comparisons, 512 complete-memory checks and 2,048
  unknown-caller ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Every
non-RAM frame-record field matches the baseline throughout: scratchpad, calls/counts, draws/counts,
sound/sample counts, thread state and EE clock. The entire four-player recording is also
byte-identical to the baseline, including every RAM frame. Strict contact, hierarchy, math and
projection checks pass; strict/default recordings and snapshots are identical for both routes.
Defaults perform zero original comparisons and zero VU0 microprogram calls.

The six new forward returns execute 23,889 / 0 times in one-player / four-player. The seven new
inverse returns execute 24,255 / 2 times. All thirteen sites run in one-player; four-player reaches
only `hdStartOut` and `hdEndOut`, once each, with other sites covered by generated consumers/units.
The batch retires another 48,144 / 2 original helper executions. These are counts, not timing or
FPS measurements.

One-player RAM hashes differ on 3,906 frames (5594 through 9499). A complete 32 MB snapshot at
frame 7000 differs by two bytes in one stack word, `8fedfc`: baseline zero versus native 1.0.
Paired baseline/native observers identify forwards `1cdf40` and `1cdfe4`, whose output temporary
is SP+`20` with unused W at SP+`2c`. The original stores inherited VF16.W (including transient
values); native preserves the existing 1.0. Diagnostic snapshots reproduce the qualified
baseline/native snapshots exactly. The following inverse consumes XYZ; its own later output W
is meaningful and checked exactly, as described above. The four-player frame-6000 snapshot is
baseline-identical. This is bounded snapshot/consumer evidence, not a classification of every
possible RAM difference or campaign consumer.

Temporary observers were removed, restored sources were forced to rebuild, and the native math
object matches its qualified hash. All three executables contain no observer marker; all eleven
native suites pass again. No new hash mask, padding restoration or shadow VU state is shipped.
Clean post-diagnostic default replays reproduce both complete qualified recordings and snapshots
byte-for-byte.

Fresh window runs complete 4,201 game frames for movies/menus and 6,501 each for one-player and
actual four-player. Reviewed captures show decoded movie pictures, Junkyard loading and active
rainy combat, and four advancing snow-arena views. Device logs report 27.4 / 50.9 / 175.3 ms of
silence (movie / one-player / four-player), with underruns and dropped audio frames. Deterministic
sound records match. Full campaign acceptance, flawless device audio and console visual parity
remain unclaimed.
