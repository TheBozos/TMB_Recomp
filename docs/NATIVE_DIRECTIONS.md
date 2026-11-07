# Native BSP collision directions

The native player now calculates `mathfMulTransVec` (`1da298`) directly for three additional
`hdBspNode` return sites: `1cd618`, `1cda54` and `1cdabc`. Together they make 529,424 calls in the
qualified 1P route. The existing skid caller (`168b30`) remains native. These are caller-scoped
replacements; the other direction callers retain the original helper and its complete ABI.
BozoEngine remains at `4a8f70a`.

The calculation snapshots three basis columns and the input vector, computes separately rounded
basis-times-XYZ arithmetic and writes XYZ. Destination W is retained as opaque padding. Native code
neither reads nor maintains VU registers, accumulator, flags, microcode or data memory. Scalar CPU
registers and timing are unchanged, and the complete host floating-point control word is restored.
The existing rare-exponent record arithmetic, overlap, alignment and RAM-alias handling are reused.

## Consumer audit

The three calls create the local point/segment directions in `hdBspNode` (`1cd538..1cdeb4`). Their
output is the stack vector at `sp+40`; subsequent additions and copies consume XYZ. The inverse
transforms at `1cd658`, `1cda84` and `1cdae8` consume XYZ and produce the vector at `sp+50` before
scalar additions populate the segment endpoints. Those inverse helpers remain original at these
return sites; their output W comes from the inverse basis, independently of input W.

There are no direct VU register/control/flag reads in `hdBspNode`. Its scalar comparisons and BSP
classification continue in the generated code. This scope does not establish a W/flag contract for
other direction callers, nor replace the complete collision traversal.

`TMB_NATIVE_MATH_CHECK=1` compares native XYZ against the original leaf on a copied context and
restores memory before the native write. The normal player makes no such original calls for the
audited sites. The historical host retains the original helper even at these return addresses.
`TMB_NATIVE_DIRECTION_TRACE=1` optionally reports direction-helper frequencies by return address;
normal runs do not collect the caller map. Frequencies are not CPU timings.

## Validation

Evidence is local under `D:/BozoScratch/direction-callers`, against main `f946958` (PR #20).
No game code, captures, RAM dumps or disc data are committed.

- The matrix/direction suite passes 1,048,576 numerical/ABI comparisons and 512 complete flat-memory
  comparisons, including 262,144 direction cases distributed across all four audited return sites.
  Four host rounding modes, denormal controls, extreme/arbitrary input bits, overlap, aliases,
  scratchpad and low address bits are covered. Native leaf VU state remains untouched. Another
  2,048 comparisons retain the complete original ABI for unaudited callers.
- Another 1,536 cases run the real generated BSP continuations from each new direction call through
  the scalar vector consumers and original inverse transforms, ending before the next result-buffer
  operation. They include the two successive direction calls in the segment setup. The actual local
  stack-vector layout is used. CPU state outside intentionally retired VU effects, cycle/checkpoint
  counts, host FP control and all memory outside direction-output W agree. Forty-eight comparisons
  check the entire flat memory; historical-host direction ABI comparisons are exact too.
- All nine native suites and all 12 release-tool tests pass. Player, headless and checker builds pass.
- Default 1P completes 9,500 frames with 533,524 native directions, versus 4,100 before this change.
  That retires 529,424 more generated VU direction executions, about 44% of all direction calls in
  this route. The 4P route completes 8,500 frames; these new BSP sites are not reached there, so its
  native direction count remains 8,363. There is no measured speedup claim.
- Every frame's draw hashes/counts, deterministic sound hashes/sample counts, library calls/counts,
  EE clocks, scratchpad and thread records match the baseline in both routes. All 4P RAM hashes match
  too. Default runs make zero original comparisons and zero VU0 microprogram calls.
- Separate strict runs enable the native math, hierarchy and projection original comparisons.
  All 533,524 / 8,363 native direction XYZ comparisons pass (1P / 4P). Both complete frame recordings
  reproduce their corresponding default recordings byte-for-byte.
- Windowed default routes complete 4,201 movie/menu frames and 6,501 frames each for 1P and 4P.
  Direct captures show the decoded opening movie, menus, the 1P loading screen, active textured combat
  with rain, vehicle effects and HUD targeting, and all four snow-arena views after loading. Sound is
  active; device logs report 22.5 / 56.6 / 26.3 ms of silence and some dropped frames. Deterministic
  sound is exact; flawless device audio or subjective listening acceptance is not claimed.

## RAM boundary and remaining qualification

The 1P RAM hashes differ in 3,234 frames, first at 5593. At frame 7000 the complete 32 MB snapshot
differs in six bytes across just two stack W words: `8fef5c` and `8ff05c`. A temporary direction trace
identifies the corresponding outputs `8fef50` / `8ff050` at BSP frame bases `8fef10` / `8ff010`.
The original helper would store inherited VF16.W (`0`); the native helper retains the pre-existing
padding (`ffffffff` / `02000200`). There are no differences elsewhere at that snapshot. Temporary
watch tracing is removed from the production source and executable. No shadow VU state, padding
restoration or broader memory-hash mask is introduced.

Full campaign, console visual parity, unaudited direction callers and complete VU removal remain
outside this bounded qualification. Further BSP inverse directions and other hot direction callers
still need their own consumer audit.

The subsequent [`NATIVE_DIRECTION_CONSUMERS.md`](NATIVE_DIRECTION_CONSUMERS.md) increment adds the
vehicle collision-point, tire-height and light-node direction callers with their generated consumers.
