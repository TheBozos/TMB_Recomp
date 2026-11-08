# Native Ferris wheel fall and smasher camera forward transforms

[Falling helicopter forwards](NATIVE_HELI_DEATH_FORWARD.md) follow with two more environment sites.

Two environment forward returns use native XYZ arithmetic after the
[environment gun forwards](NATIVE_ENVWEAPON_FORWARD.md). Known forward coverage
grows from 146 to 148 of 269 returns (121 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `12aad4` | `FerrisWheel::UpdateFalling` (`12aa98`) | The falling car's velocity through its coordinate system, added to its position; the resulting height selects the landing state at `gp-312c` |
| `169ed0` | `Smashers::SmasherHit` (`169d48`) | Camera offset `(0, -200, 60)` through the slot camera's basis, added to the hit vehicle's position and handed to `TheGame::DetachCamera` |

The Ferris wheel result is the owner's frame vector at `+0`, read only by
`mathfAddVector`. Later stack use starts at `+10`, so frame `+0c` is never read.
The smasher result is at frame `+10`. `mathfAddVector` and `Camera::DetachCamera`
read it only as XYZ: a scalar `mathfCopyVector` into the camera, then the real
point-to-point rotation and matrix rebuild. Frame `+1c` is never read. Neither
owner reads VU registers, the accumulator or flags; `vehicleStunCar` only writes
one vehicle field. The existing native helper computes separately rounded XYZ,
preserves destination W and leaves VU state untouched. Unknown and historical
callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/ferris-smasher-forward`, against parent
`9057db2` (PR #61). Parent headless/window binaries are preserved there and match
PR #61's qualified build. Generated bodies, retail data and runtime recordings
stay outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 251 and 252): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Ferris wheel full entries run the whole owner. Both landing states are covered
  (still falling above height 10, landed below), and the published position and
  state are asserted. All 512 cases publish.
- Smasher full entries run the stun, the slot search and the real
  `Camera::DetachCamera`. A new 0x20-byte snapshot region covers the game's camera
  mode table at `4ac0d0`, which DetachCamera writes. The camera's copied position,
  the claimed smasher slot and the camera mode are asserted. The inactive-smasher,
  no-camera-slot and occupied-slot scenarios return without a transform, and
  producer reach is asserted in every case. 416 cases publish.
- Transform outputs are checked against an independent identity or quarter-turn
  result in every reached case. Published points are exact against the original
  FPU addition.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+0c` and `+1c` respectively) is normalized for test comparison, after
  every real consumer.
- New caller kinds and the new region shift the shared random fill for later
  rounding modes, so two data-dependent counts in earlier fixtures moved (vehicle
  tire jumps 718 → 715, real libm errno cases 802 → 830). All scenario-defined
  counts and assertions are unchanged.
- Aggregate caller coverage is 129,536 generated sequences / 6,080 whole-memory
  comparisons. Historical forward ABI coverage is 8,896 / 556; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `9057db2`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. Both new sites are cold on these routes, so the
generated owner suite above is their qualification. The Cop SUV site `1502cc`
still runs natively 1,638 times (one-player) and 845 times (four-player), exact.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show the intro and credits, menus,
vehicle and level selection, combat with the HUD and four simultaneous views.
Each route has five distinct captures. Stereo 48 kHz, 16-bit WAV recordings
contain nonzero audio.

Audio device underruns remain: 46.1 ms silence in one-player, 69.8 ms in
four-player and 28.3 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A level with a falling Ferris wheel car or
a smasher hit in actual play, full campaign and physical controller/hardware
acceptance remain open.
