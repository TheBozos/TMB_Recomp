# Native machine-gun muzzle forward transforms

Two weapon-initialization forward returns use native XYZ arithmetic after the
[helicopter gun and walking pedestrian forwards](NATIVE_ENVHELI_PED_FORWARD.md).
Known forward coverage grows from 165 to 167 of 269 returns (102 remain). Known
inverse coverage stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `1b79f8` | `Weapon::InitWeapon` (`1b77d0`), weapon type 0 | The vehicle's first muzzle offset (`+2510`) through the new weapon's coordinate-system basis, added to its position |
| `1b7a14` | The same | The second muzzle offset (`+2520`), on the alternate barrel |

When a vehicle fires its machine gun, `InitWeapon` sets up the new round. The
game's weapon-type jump table selects the machine-gun case. It aims the round
(only when no lock is held, using `mathfPitchFromPointToPoint` and
`mathfRotMatrixPH`), sets its timers and speeds from game tables, and updates the
vehicle's counter at `+22c4`. It then places the round at one of the vehicle's two
muzzles. The vehicle's toggle at `+2230` alternates
the barrel each shot. The result is the frame vector at `+0`, read only by
`mathfAddVector`. The round is then linked into the hierarchy
(`hierSetCsEpNode`). Frame `+0c` is never read in this or any other weapon-type
case of the owner. The owner reads no VU registers, accumulator or flags.

The existing native helper computes separately rounded XYZ, preserves destination
W and leaves VU state untouched. Unknown and historical callers keep the original
ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/machine-gun-forward`, against parent `e298f20`
(PR #68). Parent headless/window binaries are preserved there and match PR #68's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 270 and 271): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Full entries run the weapon-type dispatch, the locked and unlocked aim gates, and
  the `+22c4` counter branch. Each places the round and stops at
  `hierSetCsEpNode`; reaching it is asserted. A non-gun weapon type returns
  without a transform.
- The weapon's basis is fixture-known at entry as well as at the helper boundary,
  so every case is checked against an independent identity or quarter-turn result.
- The round's position is asserted exactly against the original FPU addition, and
  the barrel toggle is asserted for each barrel. Each site publishes in 480 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so one
  data-dependent count in earlier fixtures moved (real libm errno cases 828 → 853).
  All scenario-defined counts and assertions are unchanged.
- Aggregate caller coverage is 139,264 generated sequences / 6,688 whole-memory
  comparisons. Historical forward ABI coverage is 10,112 / 632; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Both sites are live on the one-player route, running natively 81 (`1b79f8`)
and 82 (`1b7a14`) times. Strict mode compares every native XYZ against the original
leaf and reports no mismatch. Strict/default recordings and snapshots are byte
exact.

**One recorded difference, and why it is expected.** Against parent `e298f20`,
the four-player recording matches on every field and its 32 MiB snapshot is
identical. The one-player recording matches on every field except the RAM hash.
That hash differs on 354 of 9,500 frames (from frame 5,674, in bursts while the gun
fires), and the frame-7,000 snapshot is identical.

- A RAM dump right after frame 5,674 shows exactly one differing word, `0x8ff32c`.
  The parent has `00000000`, the stale VU W that the original leaf stores; the
  native build has `00333373`, the previous stack contents, which the native helper
  leaves in place.
- Logging the destination in a temporary build showed `InitWeapon`'s output at
  `sp = 0x8ff320`. So `0x8ff32c` is its dead frame `+0c` W slot, which nothing
  reads; it is overwritten by later stack use.
- To show nothing reads it, the same temporary build wrote a NaN pattern
  (`7fc0dead`) into that W after each of these two transforms. Over the full
  one-player route, every recorded field still matched the parent except the RAM
  hash: calls, draws, sound, thread state, EE timing and every count.
- Earlier live sites were byte exact only because their dead W slots happened to be
  overwritten within the same frame. This is the first site where the stale word
  survives until the end-of-frame RAM hash.

The experiment was not committed. The qualified binaries were rebuilt from the
committed source, and both headless routes were rerun on them with the same result.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 55.6 ms of silence in one-player, 26.0 ms in four-player and 22.6 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Full campaign and physical controller/hardware acceptance remain open.
