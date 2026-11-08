# Native junk jet and blimp flight forward transforms

[Gas tank forwards](NATIVE_GASTANK_FORWARD.md) follow with three more environment sites.

Two waypoint-flyer forward returns use native XYZ arithmetic after the
[falling helicopter forwards](NATIVE_HELI_DEATH_FORWARD.md). Known forward coverage
grows from 150 to 152 of 269 returns (117 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `143468` | `JunkJet::UpdateJet` (`143310`) | Speed vector at `+20` through the jet's freshly rebuilt coordinate-system basis, added to its position |
| `10eca0` | `Blimp::UpdateBlimp` (`10eae8`) | The same for the blimp |

Each frame, the flyer steers toward its current waypoint (`atan2f`, bounded and
smoothed heading/pitch) and rebuilds its matrix with `mathfRotMatrixRPH`. It
smooths its speed and moves by the transformed speed vector. It then measures the
Manhattan distance to the waypoint and advances it when close. The jet then
updates its engine sound and grid membership, the blimp its grid membership and
sound. The result is the owner's frame vector at `+0`, read only by
`mathfAddVector`; frame `+10`/`+14` hold angles, and `+0c` is never read. Neither
owner reads VU registers, the accumulator or flags. The existing native helper
computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. Unknown and historical callers keep the original ABI. No engine or
gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/flyer-forward`, against parent `7e19e2b` (PR #63).
Parent headless/window binaries are preserved there and match PR #63's qualified
build. Generated bodies, retail data and runtime recordings stay outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 255 and 256): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Full entries run the steering, matrix rebuild, speed smoothing, transform,
  position update and waypoint check, with both a distant and a nearby waypoint.
  They stop at the jet's `JetSound::updateJetSound` or the blimp's
  `hdReparentCsGrid`, and reaching that boundary is asserted. The parked scenario
  (jet inactive, blimp switched off) returns without a transform. Producer reach is
  asserted in every case.
- Helper-boundary outputs are checked against an independent identity or
  quarter-turn result. Full entries rebuild the basis from the steering angles
  first, so those are covered by the native/original comparison. The published
  position is asserted exactly against the original FPU addition. Each site
  publishes in 480 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  at frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 718,
  real libm errno cases 827 → 847). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 131,584 generated sequences / 6,208 whole-memory
  comparisons. Historical forward ABI coverage is 9,152 / 572; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `7e19e2b`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The junk jet return `143468` is live: it runs
natively 3,902 times on the one-player route with no recorded difference. The
blimp return `10eca0` is cold on these routes and is qualified by the generated
owner suite above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. In the qualification one-player window run, the game ran
at 15-30 game frames per second from about game frame 3,750 (607 audio underruns,
4.9 s of silence). Other desktop programs were using CPU at the time. Two
back-to-back reruns of that route under the same conditions separate the build
from the machine. The new build averaged 59.7 game frames per second after frame
3,700 (11 underruns, 58.9 ms). The parent build, run right after, averaged
47.8 (49 underruns, 248.5 ms). The junk jet site ran about 1,706 times in each.
The slowdown is therefore machine load, not this change. Reviewed captures show
the intro and credits, menus, vehicle and level selection, driving with the HUD
and four simultaneous views. Each route has five distinct captures. Stereo
48 kHz, 16-bit WAV recordings contain nonzero audio.

Audio device underruns in the other routes: 30.6 ms silence in four-player and
20.9 ms in the movie run. These are functional checks, not a performance or
flawless-playback claim. The blimp in actual play, full campaign and physical
controller/hardware acceptance remain open.
