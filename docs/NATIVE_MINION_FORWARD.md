# Native Minion shield forward transforms

Seven `Vehicle::minionTakeHit` forward returns use native XYZ arithmetic after
[attachment/environment forwards](NATIVE_ATTACHMENT_FORWARD.md). Known forward
coverage grows from 124 to 131 of 269 returns (138 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Producer and consumer |
| --- | --- |
| `1ada84` | First shield offset through the vehicle basis, world translation and hit-distance check |
| `1adb8c`, `1adba0` | Second shield offset through vehicle and shield bases, then world translation and hit-distance check |
| `1adca8`, `1adcbc` | Third shield offset through both bases and hit-distance check |
| `1addc4`, `1addd8` | Fourth shield offset through both bases and hit-distance check |

The existing native helper computes separately rounded XYZ, preserves destination
W and leaves VU registers, accumulator, flags and data memory untouched. Original
point-table producers, distance formula, damage/health logic, hierarchy switches,
HUD queue/primary messages and return behavior remain. Unknown/historical callers
retain the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/minion-forward`, against parent `cafd3f7`
(PR #55). Parent headless/window binaries are preserved there. Generated bodies,
retail data and runtime recordings stay outside Git.

- Added coverage is 3,584 generated sequences / 224 whole-memory comparisons.
  Each return has 256 helper-boundary sequences through its real continuation and
  256 full-owner-entry sequences. All seven sites are required to execute, with
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases, arbitrary
  matrix/source/destination W payloads and the actual in-place second transforms.
- Full entries execute the original point-table loads, inactive force-field
  bookkeeping, one or two basis transforms per shield, translation and the real
  distance approximation. Independent exact world coordinates are asserted at
  each distance call, then compared between native and original executions.
- All four shields have successful damage and destruction cases. Health changes
  from 100 to 80 on ordinary hits; the actual game/HUD calls publish primary and
  queued message IDs `d2` through `d5` in all four HUD slots. Retained-shield return
  values are checked. Exact 50-unit and distant misses leave health/UI unchanged.
- Destruction changes health from 10 to -10 and executes the real hierarchy switch
  update. Those cases stop at `figureOutForceField` after health/switch publication.
  Force-field rebuilding, effects and sound beyond that boundary are not stubbed
  or claimed as complete-owner coverage. Non-destruction fixtures complete the
  original owner with the impact-sound gate disabled and the force field inactive.
- Damage publications pass 176/240/240/240 cases across the four shields;
  destruction/switch publications pass the same counts. There are 896 exact
  distance-boundary rejection cases. Actual transform/field/distance/message/
  switch counts and distance inputs must agree before final memory comparison.
- Only forward scratch W at the owner's `SP+0c` is normalized for test comparison,
  after every real consumer. Full-entry frames are `0xd0` bytes; helper-boundary
  frames retain their actual stack position. Original W/VU writes stay live
  through the original consumers. CPU outside retired VU, clocks/checkpoints,
  host FP effects and memory agree; native VU remains untouched throughout.
  No runtime RAM mask, recording normalization or VU-state reconstruction is added.
- Aggregate caller coverage is 120,832 generated sequences / 5,536 whole-memory
  comparisons. Historical forward ABI coverage is 7,808 / 488; inverse remains
  8,576 / 134. Existing hierarchy/BSP/edge/unknown-ABI coverage and all earlier
  publication assertions remain enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `cafd3f7`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. No new recorded-RAM difference is introduced.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show vehicle/level selection, level
loading, advancing driving, opponents/effects, four simultaneous views, decoded
intro footage and return to the menu. Each route has five distinct captures.
Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio.

All seven Minion sites are cold on these runtime routes, including movies.
Their direct qualification is the generated-owner/consumer suite above, not a
visible Minion boss encounter. Active force-field hits, destruction effects,
sound dispatch and the subsequent force-field rebuild remain outside the new
fixtures; their original implementations are retained.

Audio device underruns remain: 47.4 ms silence in one-player and 27.2 ms in
four-player; this movie run logs zero underruns. These are functional checks,
not a performance or flawless-playback claim. Full campaign and physical
controller/hardware acceptance remain open.
