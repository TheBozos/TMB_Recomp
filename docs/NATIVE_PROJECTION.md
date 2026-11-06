# Native projection and weather helpers

This increment replaces `mathfMulVec4x4` (`1da238`) at the audited returns `1dc438`
(`mathfTransform3dTo2d`) and `1e8cfc` (`particleUpdateRainViewport`). It also replaces
`mathfUnitMatrixAsm` (`1db0f0`) at the weather path's identity return `1e8c48`.
Other callers and the historical checker retain the original complete ABI. Engine `4a8f70a` is unchanged.

The point helper takes ordinary matrix/point records and writes all four numerical output components.
Input W is ignored; each matrix row's fourth component is its translation. Separately rounded products
and cyclic reduction order are retained: `a+(b+(c+d))`, `b+(c+(d+a))`, `c+(d+(a+b))`, `d+(a+(b+c))`.
Inputs are snapped before output writes, including overlap. The identity path writes a native identity
record and the constant CPU ABI results previously returned alongside it.

Neither path reads or changes VU registers, accumulator, flags or data memory. Host FP control is
restored completely. Explicit `TMB_NATIVE_PROJECTION_CHECK=1` runs the original generated leaf on a
copied context and compares the complete output record; normal player runs do not execute that oracle.

## Validation

Evidence: `D:/BozoScratch/math-helpers`, against main `a415f42`, with the same disc, engine, build and input
scripts. Agent runs use private user directories. These are functional checks with no timing claim.

- `tmb-native-projection-tests` passes 786,432 original/native helper cases across four host FP modes,
  bounded values, arbitrary bit patterns, denormal/extended values, overlap, RAM aliases, low address
  bits and scratchpad. There are 192 complete flat-memory helper comparisons. CPU ABI, preserved VU
  state and complete restored MXCSR are checked.
- 2,048 generated caller cases cover complete `mathfTransform3dTo2d`, including null-pointer bypasses,
  actual view getters, perspective division and output writes; and the active weather continuation
  from `1e8c50` through scalar matrix setup, projection, depth adjustment and epilogue. Another 64
  complete flat-memory comparisons cover these cases. This is not a whole-weather-function fixture.
- 4,096 fallback cases check unknown callers and the historical host's complete original ABI. Another
  1,024 actual helper/consumer sequences reproduce the padding chain below. The same suite also passes
  with the strict copied-context oracle enabled, including the identity helper's diagnostic path.
- All seven native suites and 12 release-tool tests pass; player, headless and historical checker build.
- Default 1P (9,500 frames) and actual 4P (8,500) match main on every draw hash/count, sound hash/sample
  count, library call field, EE clock, scratchpad and thread record. Separate strict/default recordings
  are byte-identical. RAM differences are bounded below; no hash mask is widened.
- The native point path executes 58,574 screen / 5,560 weather transforms in 1P and 114,110 / 15,192 in
  4P. Strict runs perform 64,134 / 129,302 original point comparisons. The owned identity return has
  zero live hits in these routes: its native and diagnostic coverage is fixture-only. Other identity
  callers still execute their original implementation. Default VU0 program counts remain zero.
- Windowed movie/menu (4,201 frames) and 1P/4P (6,501 each) complete through loading. Direct capture
  inspection covers decoded movies, readable loading, arena vehicles/effects, targeting labels/health
  bars and four snowy views. These are bounded smoke checks, not console or pixel parity.
  Device logs retain dropped frames and 24.8 / 55.0 / 21.8 ms of silence; deterministic sound is exact,
  without a flawless-device-audio or subjective listening claim.

## RAM boundary and remaining work

RAM hashes differ on 1P frames `6998..7026` (29 frames) and 4P frames `4730..8499` (3,770 frames).
The checked 32 MB snapshots isolate one differing word at 1P frame 7,000 (`26077c`), and two at 4P
frame 6,000 (`25da7c`, `26077c`), with no other differences. These are `Vehicle+52c`, W of its peeling-out
offset at `+520`.

The original projection leaves its fourth result in VF16.W. The unconverted direction helper `1da298`,
called from `vehicleUpdatePeelingOut` at return `1abd64`, calculates XYZ but stores that old VF16.W too.
The native vehicle offset consumer at `1ab29c..1ab2a8` reads XYZ only and preserves its destination W.
The sequence fixture executes the actual original/native projection, the original direction ABI and
the production native vehicle consumer: the offset's W is the sole differing memory word, while XYZ
and the consumed output remain identical. No artificial VF16 or padding restoration is added.

This evidence bounds the checked snapshots/dataflow, not all possible campaign consumers. Other VU
macro helper callers, scalar packet builders and legacy graphics buffers/extents remain separate work.
The published player release and engine pin are not changed by this source increment.
