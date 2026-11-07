# Native older BSP and hierarchy setup helpers

Fifteen more hierarchy returns use the existing native record calculations: eight inverse and
seven forward additions bring the allowlists to fifty and forty-four entries respectively.
This extends the [traversal/setup increment](NATIVE_HIERARCHY_TRAVERSAL_SETUP.md). BozoEngine stays
at `4a8f70a`; no engine or renderer change is included.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1c7860`, `1c7904`, `1c7ae4`, `1c7d00` | Older BSP node `1c7758` |
| Inverse `1da1e8` | `1c9984`, `1c9b3c` | Hierarchy roots `1c9858` |
| Inverse `1da1e8` | `1ca3b8` | Collision grid `1c9bf8` |
| Inverse `1da1e8` | `1caa48` | Hierarchy traversal `1ca798` |
| Forward `1da298` | `1c7818`, `1c78c4`, `1c7a90` | Older BSP point/segment setup `1c7758` |
| Forward `1da298` | `1c7bc8`, `1c7bf8`, `1c7de8`, `1c7e14` | Older BSP result publication `1c7758` |

Native leaves snapshot inputs before overlapping writes, retain separately rounded XYZ arithmetic
and leave VU registers, accumulator, flags and data memory untouched. Inverse XYZW is exact,
including meaningful basis[0].W. Forward XYZ is exact while destination W is preserved. Historical
hosts and unknown callers retain the complete original helper ABI. Scalar normalization, matrix
products, result packing, traversal/query dispatch, clocks and checkpoints remain compiled game
code. Other helper returns and inline VU operations remain; this is not complete VU removal.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-legacy-bsp`, against main `c0d2fc7` (PR #29).
Generated game code, disc data, recordings, RAM dumps and captures are excluded from the commit.

- The consumer harness grows from 25,600 to 33,280 sequences and from 800 to 1,040 whole-memory
  comparisons (7,680 / 240 added). Four host FP modes, denormal controls, RAM aliases and arbitrary
  W payloads are covered. CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and
  other memory match exactly; native VU remains untouched.
- BSP setup runs paired local/world transforms, point-mode selection, zero/copy/normalization,
  exact/adjacent unit-axis decisions, real scalar additions and segment construction, then stops
  at the next BSP query dispatch. It does not replace or claim complete query-root coverage.
- Both result paths run scalar endpoint reconstruction and packed material/flag publication. The
  looped point path stops at the next point iteration; the segment path runs through return.
- Root continuations publish real packed matrix-stack state and arguments before stopping at
  traversal push. Grid/traversal continuations retain capacity-counter updates at 0 and 1199 and
  compare the following scalar matrix-product arguments. General traversal is outside these
  fixtures; its existing compiled functions remain responsible for children and callbacks.
- Reference forward W and VU effects remain live through real consumers. Only final unused
  forward-output W is normalized afterward; a later inverse write retires that exclusion so
  meaningful inverse W remains compared exactly. No production W contract or arithmetic changes.
- Historical forward checks grow to 2,240 cases / 140 whole-memory comparisons (448 / 28 added).
  Historical inverse checks cover all fifty returns: 3,200 cases / 50 whole-memory comparisons.
  The hierarchy suite retains 786,432 helper cases, 384 whole-memory checks, 8,000 geometric edges,
  1,024 recursive BSP callers / 32 whole-memory checks and 2,048 unknown-caller ABI cases.
- The math suite retains 1,048,576 helper cases, 512 whole-memory checks and 2,048 unknown-caller
  ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass.

## Gameplay, loading and movies

The default one-player route completes 9,500 frames and actual four-player completes 8,500.
Every non-RAM frame-record field matches baseline throughout: scratchpad, calls/counts,
draws/counts, sound/sample counts, thread state and EE clock. Four-player's entire recording,
including every RAM frame, is baseline-byte-identical. Default runs perform zero original
comparisons and zero VU0 microprogram calls.

Strict contact, hierarchy, math and projection checks pass. Strict/default complete recordings
and snapshots are byte-identical for both routes.

One-player executes seven of the eight new inverse sites, 45,829 calls combined, and all seven
new forward sites, 12,663 calls. Root return `1c9984` has generated-consumer and historical-ABI
coverage without a live-route claim. Four-player reaches only root return `1c9b3c`, 9,685 calls.
The batch retires another 58,492 / 9,685 original helper executions in one-player / four-player.
These are counts, not timing or FPS measurements.

One-player RAM hashes differ on 529 frames in three intervals: 5900–5937, 8018–8290 and
9135–9352. The complete 32 MB snapshot at frame 7000 is baseline-identical, as is four-player at
frame 6000. A targeted frame-8150 snapshot differs by six bytes in two W words, `8ff13c` and
`5253ec`: baseline zero versus native `008ff050`. Paired observers identify forward return
`1c7818` writing inherited VF16.W in the original, while native preserves the destination's
existing payload. The output temporary is SP+`20`, with unused W at SP+`2c`.

That stack slot is later reused by the collision-node path: the existing native XYZ addition
at `1cb540` preserves W in the result record at `8ff130`, and the following full-record GPR
copy carries it into `5253e0`. The two snapshots' changed W values follow this same write chain.
All 20,895 observed contact-copy events pair at equal cycles; the 1,865 differing copies read
that same stack record and carry zero / `008ff050`, with no changed XYZ computation. Both
observer passes reproduce the qualified baseline/native recordings and targeted snapshots
exactly. This is bounded snapshot/consumer evidence, not a classification of every RAM
difference or possible campaign consumer.

Observers were removed, restored sources forced to rebuild, and both the native math and
generated objects match their original qualified hashes. All three executables contain no
observer markers. No padding restoration, hash mask or shadow VU state is shipped.
All eleven native suites pass again. Final clean default replays reproduce both complete
qualified recordings and snapshots byte-for-byte.

Fresh window routes complete 4,201 movie/menu frames and 6,501 each for one-player and actual
four-player. Reviewed captures show decoded movie pictures, Junkyard loading and active rainy
combat, and four advancing snow-arena views. Device logs report 16.4 / 53.1 / 37.9 ms of silence
(movie / one-player / four-player), with underruns and dropped frames. Deterministic sound
matches; flawless device audio, full campaign acceptance and console visual parity remain unclaimed.
