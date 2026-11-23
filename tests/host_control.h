// The host's floating-point control word (MXCSR) in the native tests.
//
// BozoEngine's ps2f takes the host's own float add / mul where it gives the PS2's bits (operands and result normal,
// rounded to nearest: BozoEngine e1518bf). So the game's original code is exact only when the host rounds to nearest,
// as the game runs, and it leaves the sticky status flags set as SSE does. The tests run the original at the caller's
// word rounding to nearest (its flush-to-zero and denormals-are-zero kept, which both sides follow), the native code at
// the caller's varied word, and compare the control word without the six status flags (nothing reads them).
#pragma once

namespace tmb::test
{
    // The word the original runs at: the caller's, rounding to nearest, the status flags clear.
    inline unsigned referenceControl(unsigned control) { return control & ~0x603fu; }
    // The control part equal: rounding, flush to zero, denormals-are-zero, the masks (not the status flags).
    inline bool sameControl(unsigned a, unsigned b) { return ((a ^ b) & ~0x3fu) == 0; }
}
