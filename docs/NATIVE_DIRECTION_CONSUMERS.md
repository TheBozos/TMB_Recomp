# Native vehicle and light directions

The subsequent [BSP/vehicle inverse increment](NATIVE_BSP_INVERSES.md) also retires the inverse leaf
following vehicle return `11708c`, with unchanged VU state through the paired setup sequence.

This increment extends the native `mathfMulTransVec` (`1da298`) path to four more audited return sites:

| Return | Caller | Immediate consumers |
| --- | --- | --- |
| `11708c` | `Vehicle::collisSetupHitPoints` | XYZ point addition, then original inverse/scalar point setup |
| `1880e0` | `Vehicle::suspCheckTireHeight` | Z displacement, scalar suspension clamps and tire-height write |
| `1d4284` | `hierLightNode` | XYZ view translation and scalar normalization |
| `1d42e8` | `hierLightNode` | Scalar XYZ dot product and light attenuation |

It reuses the native basis-times-XYZ calculation and destination-W contract documented in
[`NATIVE_DIRECTIONS.md`](NATIVE_DIRECTIONS.md). No VU registers, accumulator, flags or data memory are
read or maintained on these native leaf paths. Other return sites and the historical host retain the
original helper's complete ABI. BozoEngine remains at `4a8f70a`.

## Consumer audit

The collision-point loop writes the direction to the local vector at `sp+10`. `mathfAddVector` reads
only XYZ to produce the vehicle's world point. The loop then replaces the temporary XYZ before its
original inverse transform and final scalar addition. Direction W contributes to no numerical result
in these consumers; the original inverse still loads the complete vector into its compatibility state.
The method has no direct VU register/control/flag reads.

The tire-height loop transforms the local tire point into `sp+10` and reads only its Z at `sp+18`.
Generated scalar code computes and clamps the displacement before writing the tire height. Remaining
tire iterations reuse that vector, including null/flagged-tire branches. There are no calls after the
direction helper in that loop except later direction iterations, and no direct VU state reads.

The light-node position and direction vectors are at `sp+10` and `sp+30`. Their consumers are scalar
XYZ addition, normalization and dot product; light record indices/flags and attenuation are written
explicitly. Their W components are not copied into light parameters. The generated continuation
retains view lookup, normalization's small-vector branch, light-type branches, count/cursor updates
and the saved-register epilogue. `hierLightNode` has no direct VU state reads.

These are scoped helper replacements, not complete native collision, suspension or light builders.
The existing optional original check and caller-frequency trace remain available. No measured speedup
is claimed from counts alone.

## Validation

Local evidence is under `D:/BozoScratch/collision-directions`, against main `5820992` (PR #21).
No generated game code, disc data, captures or RAM dumps are committed.

- The existing math suite passes 1,048,576 helper numerical/ABI comparisons and 512 full flat-memory
  comparisons. Its 262,144 direction cases now cover all eight audited return sites, with arbitrary
  input bits, extreme exponents, four host FP modes, denormal controls, record overlap, RAM aliases,
  low address bits and scratchpad. Native leaf VU state and complete host MXCSR remain untouched.
- Another 2,048 cases execute the actual generated vehicle/light continuations, including collision
  point addition/inverse setup through the next loop boundary, remaining tire loops and clamps,
  and both light continuations through their epilogue. They check the CPU state outside intentionally
  retired VU effects, complete memory outside direction-output W, exact cycles/checkpoints and host
  FP effects. Sixty-four cases compare the complete flat memory. Historical-host helper ABI checks
  are exact at all four new sites. The previous 1,536 BSP continuation cases still pass.
- The scalar normalization can raise host exception flags. Each reference/native caller test starts
  from the same complete MXCSR and compares the original continuation's complete outgoing effects;
  only the native leaf promises to leave MXCSR unchanged.
- All ten native suites and 12 release-tool tests pass. Player, headless and checker builds pass.
- Default 1P completes 9,500 frames with 855,975 native directions, up from 533,524. The actual 4P
  route completes 8,500 frames with 207,441, up from 8,363. This retires another 322,451 / 199,078
  generated direction-helper executions in these routes. All four new sites run in both routes.
- Every baseline frame-record field matches in both default routes, including RAM, scratchpad,
  draw/sound hashes and counts, library calls/counts, EE clocks and thread state. Default runs make
  no original comparisons and no VU0 microprogram calls.
- Strict runs enable native math, hierarchy and projection original checks. All 855,975 / 207,441
  direction XYZ comparisons pass; both complete recordings reproduce their default files byte-for-byte.
- Complete 32 MB snapshots at 1P frame 7000 and 4P frame 6000 match the baseline byte-for-byte.
  No additional memory-hash mask, padding restoration or shadow VU state is introduced.

Live 4P qualification also exposed a timing-sensitive native-flare continuation issue. The subsequent
fix and its additional native-particle regression target are documented in
[`NATIVE_PARTICLE_CONTINUATIONS.md`](NATIVE_PARTICLE_CONTINUATIONS.md). Final results below use both changes.

Windowed final runs complete 4,201 movie/menu frames and 6,501 frames each for 1P and 4P. Direct final
captures show the decoded opening movie, loading screen, active rainy combat with targeting/HUD and
vehicle effects, and all four snow-arena views with driving/firing. The 4P run advances through the
previous stall point. Device logs report 17.8 / 46.2 / 30.9 ms of silence (movie / 1P / 4P) and some
dropped frames; deterministic sound remains exact. Flawless device audio or subjective listening
acceptance is not claimed. Full campaign, console visual parity and complete VU removal remain
outside the bounded qualification.
