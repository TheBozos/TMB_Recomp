# Native camera matrix products

The player runtime calculates `vu0MulMatrixTP3x3` (`0x1dcc68`) and `vu0MulMatrix4x4`
(`0x1dcd50`) directly over typed column-major game records. Their current callers are in `viewUpdate`.
The default path reads both input matrices before writing the output, supports overlapping records,
and does not read or update VU registers, accumulator, flags, microcode or VU data memory.

Host SIMD performs separately rounded multiplication and addition, with contraction disabled. A scoped
floating-point control setting handles denormals and restores the caller's complete MXCSR. Rare values
outside IEEE's finite exponent range use the existing scalar number-format operations on record bits;
they do not create processor state. The transposed product preserves the original moved-component
normalization and final fourth column. All four output components are checked, including W.

The historical call-log host retains the generated helpers. `TMB_NATIVE_MATH_CHECK=1` compares the native
result with an original helper on a copied context, restoring the output before the native write. This is
an opt-in diagnostic; the normal player path never invokes the original helper.

## Validation

Artifacts are local under `D:/BozoScratch/native-math`; no game data is committed.

- Build and run the explicit `tmb-native-math-tests` target. Its 524,288 original/native cases compare
  record bytes, CPU return state, untouched VU state and restored host floating-point control across four
  host rounding modes. Inputs include finite game-like numbers, arbitrary bits, extreme exponents,
  denormals, overlapping inputs/outputs, address aliases, low address bits and scratchpad records.
  Another 256 comparisons check the complete flat guest memory, rather than just the output arena.
- A 9,500-frame strict gameplay route checks 28,906 transposed and 43,359 full products, with zero
  mismatches. A separate default run makes zero original-helper comparisons and produces identical
  per-frame records to the strict run.
- Against the native graphics main baseline, all 8,500 comparable frame records have identical draw
  hashes, batches, vertices, sound hashes/sample counts, calls, scratchpad, threads and EE cycles.

## Known state difference and remaining work

This is not a whole-RAM equivalence claim. RAM hashes differ from frame 5594 onward. Full 32 MB dumps
at gameplay points 5600 and 8500 each differ in exactly one word: `0x4a1f5c`, a HUD coordinate-system
position's fourth component. `updateHUDPosition` writes it at `0x13ce98`: the calculation writes XYZ,
then stores the full vector with W inherited from unrelated earlier VU work. Removing the camera helper's
register clobbers changes that inherited value. The traced reader is `hierCsUpdateAsm` (`0x201e98`),
whose camera-position calculation uses XYZ. No shadow VU writes or artificial W restoration are added.
The tested render/sound/state observations above remain exact; they do not prove every campaign path.

The vehicle 3x3 helper remains generated. Vehicle calculations, inline HUD transforms, hierarchy routines
and other VU0 compatibility state still need wider caller/dataflow replacements. This increment removes
only the two camera matrix helpers; it does not claim that all VU0 work is native records yet.
