# Native HUD point and direction transforms

`updateHUDPosition` now computes its six inline transforms directly over game records. The three point
blocks apply a basis to XYZ and add the current translation; the three direction blocks apply only the
basis. Each block snapshots its inputs at its existing final store, after earlier writes, then writes XYZ.
The destination's fourth component remains unchanged. It is an opaque payload here, rather than an
output from an unrelated VU register. The point-projection and vector-add consumers use XYZ.

| Original block | Final store | Calculation |
| --- | --- | --- |
| `13ce74..13ce98` | `13ce98` | Position plus translation |
| `13ceb4..13ced8` | `13ced8` | Projected HUD point plus translation |
| `13d03c..13d060` | `13d060` | Second HUD position plus translation |
| `13d210..13d22c` | `13d22c` | Targeting direction |
| `13d308..13d324` | `13d324` | Targeting direction |
| `13d408..13d424` | `13d424` | Warning direction |

The 54 original load/arithmetic/store instructions are lowered through the existing native instruction
dispatcher. The player path neither loads VU registers nor transports accumulator/flag state. Ordinary
CPU branches, calls and checkpoints retain their existing positions. The number-format and separately
rounded SIMD arithmetic follow [`NATIVE_MATH.md`](NATIVE_MATH.md).

## Independent original comparisons

`tools/tmb_hud_fixture.py` extracts the exact fallback instruction bodies from fresh BozoEngine output
in the build tree. It rejects missing, duplicate or non-straight-line memory/vector instructions.
No generated code or game data is committed. The historical host retains its original instruction path.

`TMB_NATIVE_HUD_CHECK=1` runs the extracted original block on a copied context and temporarily saved
output, compares XYZ, then restores the output before the native write. W is deliberately outside the
numeric comparison: the original store copies a stale register lane, whereas the native store preserves
the destination payload. Default execution never invokes the original block or reads VU state.

Build and run `tmb-native-hud-tests` for the original-block/production-dispatcher comparisons:

- 196,608 cases across all six blocks and four host FP modes compare every arena byte against the
  original calculation with the documented W-preservation contract. Inputs include arbitrary bits,
  finite game values, extreme exponents, denormals, scratchpad, address aliases, low address bits and
  partial/identical overlaps. Every CPU/VU context byte and the host MXCSR remain unchanged.
- 192 cases compare the complete flat guest memory. Before the final store, the original load/arithmetic
  sites leave memory unchanged on the native path.
- 1,024 two-block overlap cases require the second calculation to read the first block's updated input
  or basis. A function-entry snapshot would fail these cases.
- The existing 524,288 camera-matrix cases and 12 release-tool regressions still pass.

## Gameplay validation and limits

Local artifacts are under `D:/BozoScratch/native-hud`.

- The 9,500-frame 1P route checks each affine block 3,904 times, without an XYZ mismatch.
- The 8,500-frame 4P route checks each affine block 22,782 times, without an XYZ mismatch.
- Against the prior native-matrix main build, every comparable 1P and 4P draw hash, batch/vertex count,
  sound hash/sample count, call, EE cycle, scratchpad and thread record matches.
- The three direction blocks pass the generated-original tests above but were not reached by these
  ordinary gameplay scripts. Targeting-warning gameplay coverage and full campaign acceptance remain
  open. This is not a whole-function native claim: other HUD callees still contain compatibility math.

Separate default runs reproduce all strict 1P/4P frame records exactly, without invoking original HUD
blocks. RAM hashes differ because the native stores preserve destination W. At point 8500 the complete
1P RAM dump differs at two words (`4a1ebc`, `4a1f5c`); the 4P dump differs at five (`4a187c`, `4a191c`,
`4a19bc`, `4a1a5c`, `4a1c3c`). All are position.W in coordinate records reached through the two HUD
pointer arrays at `23c548` and `23c558`; the native value is the preserved zero in each. No other RAM
word differs at those snapshots, and no VU shadow restoration is used.
