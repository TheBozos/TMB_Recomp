# Native BSP and vehicle inverse directions

Four more `mathfMulVec` (`1da1e8`) returns use native record calculations: BSP collision setup
`1cd658`, `1cda84`, `1cdae8`, and vehicle collision-point offset `1170fc`. This extends the audited
inverse allowlist from eleven to fifteen entries. The existing inverse implementation snapshots its
inputs, preserves separate rounding, and writes all four output components, including basis[0].W.
The player leaf leaves VU registers, accumulator, flags and data memory untouched. Historical-host
and unknown-caller paths retain the original complete ABI. BozoEngine remains pinned at `4a8f70a`.

Together with the preceding forward-direction replacements, these BSP and vehicle setup sequences
now execute both transforms without VU context state. The surrounding scalar collision traversal,
terrain calculations and other inline VU operations still execute. This is not complete VU removal.

## Numerical and caller checks

Evidence is local under `D:/BozoScratch/bsp-inverses`, against main `4a65a8b` (PR #22).
Generated game code, disc data, captures and RAM dumps are not committed.

- The hierarchy suite passes 786,432 helper cases, rotating through all fifteen inverse sites;
  384 complete flat-memory checks, 8,000 geometric edges, 1,024 recursive BSP caller cases
  (32 complete-memory checks), and 2,048 unknown-caller ABI cases remain passing. Another 960
  historical-host inverse cases (15 complete-memory checks) compare the full CPU/VU ABI and memory.
- The actual generated BSP continuations pass 1,536 paired forward/inverse cases and 48 full-memory
  comparisons. Another 2,048 vehicle/light continuation cases and 64 full-memory comparisons cover
  collision-point setup, tire loops/clamps and light normalization/attenuation. Both sets now require
  the complete initial VU state to remain untouched. Inverse output W is compared exactly; only the
  previously established forward-direction destination-W boundary is excluded.
- The math suite retains 1,048,576 helper comparisons, 512 complete-memory checks, arbitrary input
  bits, four host FP modes, denormal controls, overlap, RAM aliases, low address bits and scratchpad.
- A further 512 generated body-contact consumer cases (16 full-memory checks) vary position.W
  independently through the actual XYZ subtraction, stopping at its next dot-product call. CPU,
  VU, clocks/checkpoints, complete host FP effects and other memory must remain identical. This
  proves that consumer boundary; it does not claim coverage of every possible contact consumer.
- Player, headless and historical checker builds, all ten native suites, and twelve Python tests pass.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. The new
sites retire another 653,080 / 60,768 original inverse-helper executions, respectively. The three
BSP returns execute in the one-player route; four-player exercises the vehicle return. Total native
inverse counts are 712,794 / 62,880. These counts are not a timing or FPS measurement.

Every non-RAM frame-record field matches the baseline throughout both routes: scratchpad, calls,
draws, sound, thread state, EE clock, and associated counts. Four-player RAM also matches throughout.
Default runs make zero original comparisons and zero VU0 microprogram calls. Strict math, hierarchy
and projection checks pass and reproduce each default recording byte-for-byte.

The one-player RAM hash differs on 3,781 frames, beginning at frame 5595. The complete 32 MB snapshot
at frame 7000 differs by six bytes in three words: `25e06c`, `263a6c`, `26946c`, each changing from
zero to `42700000` (60.0). These are position.W in the first 0x40-byte vehicle contact result at
Vehicle+`b10`, not position XYZ, radius, a matrix coefficient or a native inverse output mismatch.
The four-player frame-6000 snapshot is byte-for-byte identical.

Temporary local generated-store tracing identifies the copy at `hdTerrain` `1cc904`: it copies Z/W
from its temporary result into each contact record. The preceding inline transform at `1cc828`-
`1cc838` calculates XYZ into VF16 and stores its inherited W in the terrain query point at `5253b0`.
`terrainGetHAT` carries that W through its position copy (`1efb38`/`1efb3c` on the flat path), then
`hdTerrain` copies the result into the vehicle record. Retiring the inverse helper changes the old
VF16 residue. `collisCheckBodyResult` reads position through XYZ-only `mathfSubVector` (return
`118204`); the varied-W fixture above checks that boundary. The traced snapshot matches the ordinary
native run. All temporary generated instrumentation was restored byte-for-byte before the final
build. No RAM-hash mask, padding restoration or shadow VU state is added.

Windowed runs complete 4,201 movie/menu frames and 6,501 frames each of one-player and four-player.
Reviewed captures show the decoded opening movie, level loading, rainy combat with targeting and
vehicle effects, and all four snow-arena views advancing with driving/firing. Device logs report
330.1 / 54.2 / 33.1 ms of silence (movie / one-player / four-player) and dropped audio frames;
deterministic sound records remain exact. Flawless device audio, full-campaign acceptance and console
visual parity are not claimed by these bounded tests.

The next useful removal is the remaining inline terrain/contact transform and its scalar consumers,
using native records through the complete result chain. Other inverse callers still need their own
consumer audits before entering the allowlist.
