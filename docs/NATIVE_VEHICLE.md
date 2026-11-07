# Native vehicle spatial calculations

The player runtime now calculates the vehicle basis product (`vu0MulMatrix3x3`, `1dcc08`) directly over
matrix records. Its three calls are in `vehicleUpdatePosition` and `vehicleSetCsMat`. All four output
components and the final identity column retain the original ordered arithmetic and alias behavior.

Thirteen inline blocks in those two functions also use native records: four direction transforms,
seven sums and two differences. Their 68 load/arithmetic/store instructions no longer transport VU
registers, accumulator or flags. Each block snapshots its current operands at its existing store, then
writes XYZ while retaining the destination's fourth component. CPU branches, loops and checkpoints
stay at their original locations. This does not replace all the functions they call.

| Store | Calculation |
| --- | --- |
| `1aadc4` | Selected attachment transform |
| `1aaddc` | Attachment world position |
| `1aae44` | Repeated movement-step sum |
| `1aaf8c` | Attachment transform |
| `1aafa0` | Combined position |
| `1aafb4` | Position difference |
| `1aafc4` | Position update |
| `1aaff8` | Direction transform |
| `1ab008` | Position update |
| `1ab01c` | Position difference |
| `1ab140` | Light offset transform |
| `1ab150` | Light position |
| `1ab2a8` | Coordinate-system position |

The audited skid-mark call of `mathfMulTransVec` (`1da298`, return address `168b30`) uses the same native
direction calculation. The helper's other callers retain the generated path until their contracts are
audited. This distinction matters: the original stores an inherited `VF16.w`; its following skid-mark
vector-add consumer uses only XYZ. A memory trace found this store writing the four tire-position W
words after removing vehicle register clobbers. The native record boundary retains those destinations
without reconstructing any VU state. The resulting tested frame records match the baseline completely.

## Validation

Artifacts are local under `D:/BozoScratch/native-vehicle`, compared with main `74fd638` / engine `149bf90`.

- `tmb-native-vehicle-tests`: 425,984 comparisons against extracted generated blocks, 416 complete-memory
  cases and 1,024 ordered overlapping-block cases pass. Four host FP modes, arbitrary/extreme values,
  RAM aliases, scratchpad and quadword alignment are covered. Every CPU/VU byte and host MXCSR remain
  unchanged for the inline replacements.
- Another 1,024 tests execute the complete generated `vehicleSetCsMat` caller, including both helper
  calls, overlapping matrix input/output, saved registers, scalar tail, return and its exact 16 cycles /
  two checkpoints. Context and memory agree with the documented XYZ/destination-W contract; native
  VU state stays untouched.
- The matrix/direction suite passes 1,048,576 cases and 512 whole-memory comparisons. This includes
  262,144 skid-direction cases. Another 2,048 cases verify the complete original ABI for other callers.
- Original block extraction now writes to `native_original`, outside the recompiler's prefix-cleaned
  output folder. A forced recompiler regeneration followed by full game/test builds passes.
- The 9,500-frame 1P and 8,500-frame 4P strict routes exactly reproduce **every** baseline frame-record
  field: RAM, scratchpad, draw/sound hashes and counts, calls, threads and EE clocks.
- Live strict comparisons pass for 92,744 / 45,576 vehicle basis products, 187,011 / 60,768 inline blocks
  and 4,100 / 8,363 skid directions (1P / 4P). Twelve inline sites run in 1P; four run in 4P. The repeated
  movement-step sum at `1aae44` has original-block tests but was not reached by these scripts.

Independent default 1P/4P runs reproduce the strict records byte-for-byte, with zero original comparisons.
Windowed default runs complete 4,201 movie/menu frames and 6,501 frames each for 1P and 4P. Captures
directly show combat, movement, effects and all four snow-arena views after loading. The window logs
record 22.8/54.4/28.7 ms of silence respectively; deterministic sound is exact, but this is not a flawless
device-audio or subjective listening claim.

Existing camera/HUD tests and the 12 release-tool regressions pass. Full campaign, rare multi-step
movement, other direction-helper callers and the remaining hierarchy/VU math are not claimed qualified.

[The suspension/tire-height increment](NATIVE_SUSPENSION_TIRE.md) subsequently adds the two
forward transform leaves used by suspension ride-height and tire pivot reconstruction, with
generated consumer tails and complete tire-height entry-call comparisons.

[Native shadow transforms](NATIVE_SHADOWS.md) subsequently add the vehicle shadow vertex
transform and five weapon shadow motion/height-sample transforms, with generated vertex-loop
and height-query staging comparisons.

[Native tow-truck launcher transforms](NATIVE_TOW_TRUCK.md) subsequently add three forward
and one inverse site, with complete generated positioning, smoothing and orientation consumers.
