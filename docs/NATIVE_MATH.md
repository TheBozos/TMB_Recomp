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
- The separate 4P route checks 70,136 transposed and 105,204 full products with zero mismatches.
  Every one of its 8,500 frames matches the baseline's draw/sound/call/clock/scratchpad/thread fields.
  A separate default run reproduces all 8,500 strict-run records exactly.
- With original comparisons disabled, the windowed no-input movie route completes 4,201 frames and
  the scripted 1P and 4P routes each complete 9,001. Direct captures show the opening movie, menus,
  vehicle selection and active gameplay, including all four split-screen views after loading. These
  are bounded smoke checks, not full campaign or console visual-parity acceptance. WAVs contain active
  output; window logs report 21.8/33.2/34.6 ms of silence respectively, so this is not a flawless
  device-audio or subjective listening claim. Deterministic sound hashes above are exact.
- All 12 release-tool tests and eight engine CTest checks pass. Windows and Linux PR CI pass.

## Matrix-only state difference (before native HUD transforms)

This is not a whole-RAM equivalence claim. RAM hashes differ from frame 5594 onward. Full 32 MB dumps
at gameplay points 5600 and 8500 each differ in exactly one word: `0x4a1f5c`, a HUD coordinate-system
position's fourth component. `updateHUDPosition` writes it at `0x13ce98`: the calculation writes XYZ,
then stores the full vector with W inherited from unrelated earlier VU work. Removing the camera helper's
register clobbers changes that inherited value. The traced reader is `hierCsUpdateAsm` (`0x201e98`),
whose camera-position calculation uses XYZ. No shadow VU writes or artificial W restoration are added.
The tested render/sound/state observations above remain exact; they do not prove every campaign path.

For 4P, RAM hashes start differing at frame 4703. The point-8500 dump has exactly four differing words:
`0x4a1afc`, `0x4a1b9c`, `0x4a1c3c` and `0x4a1cdc`. These are position.W in four coordinate records;
the four HUD pointers at `0x23c548..0x23c554` point to those records, and `updateHUDPosition` loads
that array at `0x13cd1c`. Thus the final 4P snapshot isolates the same inherited-W store for each view.

The six inline HUD transforms are lowered separately; [`NATIVE_HUD.md`](NATIVE_HUD.md) records their
W-preservation contract, validation and limits. [`NATIVE_VEHICLE.md`](NATIVE_VEHICLE.md) covers the vehicle
3x3 product, inline vehicle calculations and audited skid direction. Other helper callers, hierarchy
routines and VU0 compatibility state still need wider caller/dataflow replacements.
These increments do not claim that all VU0 work uses native records yet.

[`NATIVE_HIERARCHY.md`](NATIVE_HIERARCHY.md) now covers the collision/height hierarchy's native sphere test,
audited inverse directions and BSP plane-distance pairs. [`NATIVE_SCENE.md`](NATIVE_SCENE.md) covers the
subsequent coordinated replacement of the renderer hierarchy's coupled matrix/FOV/light stack. Other
direction/helper callers and VU macro blocks remain.

[`NATIVE_PROJECTION.md`](NATIVE_PROJECTION.md) covers the subsequent native row-wise projection and
weather identity boundaries, including their original reduction order and caller-specific scope.

[`NATIVE_DIRECTIONS.md`](NATIVE_DIRECTIONS.md) extends native direction math to the three hot BSP
collision setup callers, with generated-consumer checks and an explicit stack-W boundary.

[`NATIVE_CAMERA_TRANSFORMS.md`](NATIVE_CAMERA_TRANSFORMS.md) subsequently adds camera-update
and rear-view forward transforms, including the coupled inline position consumer's exact W contract.
