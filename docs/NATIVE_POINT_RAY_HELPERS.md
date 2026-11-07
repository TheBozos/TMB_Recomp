# Native point-contact, ray and root helper transforms

Eleven further hierarchy returns use the existing native record calculations. Three inverse
and eight forward additions bring the allowlists to fifty-three and fifty-two entries.
This extends the [older BSP increment](NATIVE_LEGACY_BSP_HELPERS.md). BozoEngine stays at
`4a8f70a`; no engine or renderer change is included.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1c4fe0` | Coordinate-system collision setup `1c4ec0` |
| Inverse `1da1e8` | `1c51dc`, `1c53b4` | Older hierarchy roots `1c50a0` |
| Forward `1da298` | `1c8094`, `1c80ac` | Point-contact result normal `1c7eb0` |
| Forward `1da298` | `1c8120`, `1c8150` | Point-contact result position `1c7eb0` |
| Forward `1da298` | `1c85cc`, `1c85e0` | Ray result normal `1c8280` |
| Forward `1da298` | `1c8608`, `1c863c` | Ray result position `1c8280` |

Native leaves snapshot operands before overlapping writes, retain separately rounded XYZ
arithmetic and leave VU registers, accumulator, flags and data memory untouched. Inverse XYZW
matches the original, including meaningful basis[0].W. Forward XYZ matches while destination W
is preserved. Historical hosts and unknown callers retain the full original helper ABI.
Scalar contact reconstruction, world sums, result packing, traversal dispatch, clocks and
checkpoints remain compiled game code. Other helper returns and inline VU operations remain;
this is not complete VU removal.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-point-ray`, against main `e7cdaff` (PR #30).
Generated game code, disc data, recordings, RAM dumps and captures are excluded from the commit.

- The consumer harness grows from 33,280 to 38,912 sequences and from 1,040 to 1,216 whole-memory
  comparisons (5,632 / 176 added). Four host FP modes, denormal controls, RAM aliases and arbitrary
  W payloads are covered. CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and
  other memory match exactly; native VU remains untouched.
- Point-contact continuations retain the selected plane distance, scalar contact reconstruction,
  offset subtraction, paired normal/position transforms, world-space addition, packed material/
  flags and return. Both indexed and single-result modes are covered, with four result slots and
  direct-node versus inherited material metadata.
- Ray continuations retain selected endpoint subtraction, paired normal/position transforms,
  world-space addition, metadata publication and return, including stack-owned inputs and four
  selected point offsets/result slots. These are real result tails, not complete query-root tests.
- Coordinate-system/root continuations run real packed matrix-stack setup into the actual
  traversal-push boundary. Child traversal and callbacks remain outside those fixtures.
- Reference forward W and VU effects stay live throughout real consumers. Only final unused
  forward-output W is normalized afterward; a later inverse write retires that exclusion so
  meaningful inverse W stays compared exactly. No production arithmetic or W contract changes.
- Historical forward checks grow to 2,752 cases / 172 whole-memory comparisons (512 / 32 added).
  Historical inverse checks cover all fifty-three returns: 3,392 cases / 53 whole-memory comparisons.
  The hierarchy suite retains 786,432 helper cases, 384 whole-memory checks, 8,000 geometric edges,
  1,024 recursive BSP callers / 32 whole-memory checks and 2,048 unknown-caller ABI cases.
- The math suite retains 1,048,576 helper cases, 512 whole-memory checks and 2,048 unknown-caller
  ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Strict
contact, hierarchy, math and projection checks pass; strict/default recordings and snapshots
are byte-identical. Every non-RAM frame-record field matches baseline throughout: scratchpad,
calls/counts, draws/counts, sound/sample counts, thread state and EE clock. Four-player's entire
recording and frame-6000 snapshot also match baseline byte-for-byte. Default runs perform zero
original comparisons and zero VU0 microprogram calls.

One-player executes inverse return `1c53b4` 2,047 times and all eight new forward returns 528
times combined. Returns `1c4fe0` and `1c51dc` have generated-consumer/historical-ABI coverage
without a live-route claim. Four-player reaches none of the eleven new sites. This retires
another 2,575 original helper executions in the one-player route; it is a count, not a timing
or FPS measurement.

One-player RAM hashes differ on 3,047 frames, first at 5810 and last at 9499. The complete
32 MB frame-7000 snapshot differs by 46 bytes in 23 fourth-component words: twelve records at
`4062ec..40639c`, ten at `406e0c..406e9c`, and stack word `8fefdc`. Baseline words are zero;
native words contain `42700000`. Paired observers identify all their writes in the existing
original forward helper, at debris callers `120838`, `1208dc` and `1209a0`. All 44 watched
forward calls pair at identical cycles; 32 differing stores cover exactly those 23 addresses.
They store inherited VF16.W. The retired root inverse at `1c53b4` used to overwrite VF16.W;
the native inverse leaves VU untouched while retaining exact meaningful output XYZW.

The global fields belong to debris velocity records. The compiled debris update consumes
their XYZ at offsets `120`, `124`, `128`, with no fourth-component numerical input in that
update. The intermediate stack forward at `1208dc` similarly feeds scalar XYZ position sums.
These debris callers remain outside the native allowlist and retain their original complete
ABI. Both observer recordings and snapshots reproduce the qualified baseline/native files
exactly. This is bounded source/write/snapshot evidence, not a classification of every RAM
difference or possible campaign consumer.

Observers were removed and restored sources forced to rebuild. Both native math/hierarchy
objects match their original qualified hashes, and all three executables contain no observer
markers. All eleven native suites pass again. Final clean default replays reproduce both
complete qualified recordings and snapshots byte-for-byte. No padding restoration, hash mask
or shadow VU state is shipped.

Fresh windows run the configured 4,200-frame movie/menu route and 6,500-frame one-player and
actual four-player routes through clean exit. Reviewed captures show decoded movie pictures,
Junkyard loading and active rainy gameplay, and four advancing snow-arena views. Device logs
report 18.1 / 52.6 / 35.4 ms of silence (movie / one-player / four-player), with underruns and
dropped frames. Deterministic sound matches. Flawless device audio, full campaign acceptance
and console visual parity remain unclaimed.
