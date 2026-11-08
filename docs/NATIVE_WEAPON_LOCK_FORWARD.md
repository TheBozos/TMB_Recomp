# Native weapon lock-on, satellite and rock forward transforms

[Ricochet forwards](NATIVE_RICOCHET_FORWARD.md) follow with two more weapon sites.

Six weapon forward returns use native XYZ arithmetic after the
[Minion shield forwards](NATIVE_MINION_FORWARD.md). Known forward coverage grows
from 131 to 137 of 269 returns (132 remain). Known inverse coverage stays 134 of
134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `1b1700` | `Weapon::UpdateLockOnVehicle` (`1b16a8`) | Weapon offset through the target vehicle basis, added to the target position for the heading/pitch/distance aim |
| `1b1bbc` | `Weapon::UpdateLockAheadOfVehicle` (`1b1ae0`) | Same offset added to the predicted lead point for the aim |
| `1b339c` | `Weapon::UpdateSkillSatTrans` (`1b3350`) | Launch offset through the weapon basis, added to the satellite position (state 0) |
| `1b3634`, `1b371c` | `Weapon::UpdateSkillSatTrans` | Per-frame velocity through the satellite basis, in place, added to its position (states 1-2 and 3) |
| `1b3ef8` | `Weapon::UpdateRockTrans` (`1b3e28`) | Per-frame velocity through the weapon basis, in place, added to the rock position before its height test |

Every result is a stack-local vector read only through scalar XYZ code:
`mathfAddVector` (`1daef0`), and afterwards the rock's `hdHatTest`, which only
writes XYZ into that buffer. The destination W is never read again. None of the
owners reads VU registers, the accumulator or flags directly. Three sites transform
in place; the native helper reads its input before writing. The existing native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. Unknown and historical callers keep the original ABI. No engine or
gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/weapon-lock-forward`, against parent `145964b`
(PR #56). Parent headless/window binaries are preserved there. Generated bodies,
retail data and runtime recordings stay outside Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons:
  for each return, 256 helper-boundary sequences through the real continuation and
  256 full-owner entries, across four rounding modes, FTZ/DAZ, RAM aliases,
  identity/quarter-turn bases and arbitrary W payloads.
- Each reached transform's input and output are captured at the real helper and
  `mathfAddVector` boundaries and checked against an independent identity or
  quarter-turn result. Full satellite entries rebuild their basis from zero angles
  through the real `mathfRotMatrixPH`, so they are checked against identity.
- The published point is asserted exactly against the original FPU addition:
  lock-on and look-ahead aim points at `mathfHeadingFromPointToPoint`, the
  satellite position written back to its coordinate system, and the rock X/Y at
  `hdHatTest` (Z is lifted for the test). The real height-test consumer executes.
  Lock-on and look-ahead full entries also cover the no-target, inactive-state and
  copy-path gates without a transform (416 publishing cases each).
  Satellites and rock publish in all 512 cases each.
- Weapon owners reach real libm and matrix helpers that use VU0 themselves. Their
  VU state must match the original exactly, except the original leaf's write set
  (VF11-13, VF15, VF16, ACC, MAC/status). In that set, each word must be either the
  original's later real write or the value the native leaf found at its entry.
  Only forward scratch W at each owner's frame `+0c` is normalized for test
  comparison, after every real consumer.
- The rock's impact effect, weapon removal and impact sound are kept on the
  original path but not reached by these fixtures.
- Aggregate caller coverage is 123,904 generated sequences / 5,728 whole-memory
  comparisons. Historical forward ABI coverage is 8,192 / 512; inverse remains
  8,576 / 134. All earlier publication assertions remain enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `145964b`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The rock return `1b3ef8` runs natively 1,569 times
on the one-player route with no recorded difference; the other five sites are cold
on these routes, qualified by the generated owner/consumer suite above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show the intro and credits, menus,
vehicle and level selection, driving with the HUD and four simultaneous views.
Each route has five distinct captures. Stereo 48 kHz, 16-bit WAV recordings
contain nonzero audio. None of the six sites is reached within these window
lengths.

Audio device underruns remain: 51.4 ms silence in one-player, 32.9 ms in
four-player and 20.8 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A visible satellite or lock-on weapon
encounter, full campaign and physical controller/hardware acceptance remain open.
