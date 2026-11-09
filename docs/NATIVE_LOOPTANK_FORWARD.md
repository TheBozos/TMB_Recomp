# Native LoopTank forward transforms

[The first FireSpecial batch](NATIVE_FIRE_SPECIAL_FORWARD.md) follows with vehicle types 20, 40 and 60.

The two `LoopTank::update` forward returns use native XYZ arithmetic after the
[special-weapon torch batch](NATIVE_SPECIAL_TORCH_FORWARD.md). Known forward coverage
grows from 236 to 238 of 269 returns (31 remain). Known inverse coverage stays 134 of
134; BozoEngine stays at `4a8f70a`.

`LoopTank::update` (`1517a8`) copies a table of six segments (`4e7250`) into frame
`+80..+13f`. A loop then sends both points of each segment through the loop's
matrix at frame `+40`, in place:

| Return PC | Producer and consumer |
| --- | --- |
| `151c08` | The segment's first point (frame `+80 + 0x20k`) |
| `151c18` | Its second point (frame `+90 + 0x20k`) |

After both transforms, `mathfAddVector` adds the tank's position (coordinate system
at `+80`) to each point. After the loop, the segment selected by `+14` is
line-tested (`hdLosCsTest`). On a hit, `mathfSubVector` reads both points and
`DispatchHit` gets the difference.

The take-hit info's point pointer is set to the segment's first point. Only
`FinalBoss::TakeHit` and the Minion take-hit path dereference that pointer:
- `mathfManhatDist` reads it as XYZ.
- `forceFieldTakeHit` loads 128 bits but subtracts with the VU XYZ mask, so the W
  lane is not used.

All other readers of the take-hit info use other fields. The owner never loads any of
the array's W slots, and it reads no VU registers, accumulator or flags. The existing
native helper computes separately rounded XYZ, preserves destination W and leaves VU
state untouched. Unknown and historical callers keep the original ABI. No engine or
gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/looptank-forward`, against parent `fa84a39`
(PR #83). Parent headless/window binaries are preserved there and match PR #83's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 341 and 342). They are helper-boundary continuations and entries at the
  call-return label after `drawInit` (`151bc8`), across four rounding modes,
  FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W payloads.
- Each run transforms the first segment's two points, adds the position to each and
  stops when the selected segment is marked (`mathfSetFVector`). By then both
  points have been consumed.
- The harness pairs each addition with the transform that wrote its first operand,
  because both transforms run before either addition and the point is the
  addition's first operand. The first segment's W slots (frame `+8c`, `+9c`) are
  the only forward W normalized for test comparison.
- Every basis is fixture-known, so both points are checked against an independent
  identity or quarter-turn result. Both world points are asserted exactly against
  the original FPU arithmetic. Publications: 768 for `151c08` and 1,024 for
  `151c18`.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 703 → 705,
  real libm errno cases 816 → 802). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 175,616 generated sequences / 8,960 whole-memory
  comparisons. Historical forward ABI coverage is 14,656 / 916; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `fa84a39`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The two returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 172.4 ms of silence in one-player, 8.9 ms in four-player and 40.2 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The one-player window run reported 172.4 ms of silence. A back-to-back one-player rerun of new and parent builds gave 50.7 ms and 33.7 ms at 59.7 frames/s. The loop tank in actual play, full campaign and physical controller/hardware acceptance remain open.
