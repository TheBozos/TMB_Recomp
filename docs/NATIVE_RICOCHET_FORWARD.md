# Native ricochet forward transforms

Two `Weapon::UpdateRicoTrans` (`1b2ee0`) forward returns use native XYZ
arithmetic after the [weapon lock-on, satellite and rock forwards](NATIVE_WEAPON_LOCK_FORWARD.md).
Known forward coverage grows from 137 to 139 of 269 returns (130 remain). Known
inverse coverage stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Producer and consumer |
| --- | --- |
| `1b2fb0` | Per-frame velocity through the weapon basis, in place, added to the ricochet position before its height test; the stack vector then receives the height-test normal and is read by `Weapons::ComputeReflection` |
| `1b3118` | Speed vector through the basis the owner rebuilt from the reflected direction, added to the ricochet position |

Both results are the owner's stack vector at frame `+0`, read only as XYZ:
`mathfAddVector` (`1daef0`), `hdHatTest` (which writes only XYZ into it), and
`ComputeReflection` (`1b7200`), whose subtract, normalize and dot-product helpers
are scalar. The owner never reads frame `+0c`, and reads no VU registers,
accumulator or flags. The existing native helper computes separately rounded XYZ,
preserves destination W and leaves VU state untouched. Unknown and historical
callers keep the original ABI. No engine or gameplay formula changes.

`Weapon::RockHD` (`1b6cd8`, `1b6d3c`) was examined and left on the original path:
its transformed point is handed to `hdPntSphereTest`, which publishes the pointer
to the `hdHier` hierarchy traversal. That consumer needs its own audit.

## Generated comparisons

Evidence is under `D:/BozoScratch/ricochet-forward`, against parent `4d863cd`
(PR #57). Parent headless/window binaries are preserved there and match PR #57's
manifest. Generated bodies, retail data and runtime recordings stay outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 242 and 243): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Full entries execute the real height test and, when the resulting height is
  low, the real reflection branch: `ComputeReflection`, `mathfRotationFromVector`
  and `mathfRotMatrixPH` rebuild the basis before the second transform. The
  frame-count-zero scenario skips that branch. Producer reach is asserted for
  both sites in every case.
- Transform inputs and outputs are captured at the real helper and
  `mathfAddVector` boundaries. Where the fixture knows the basis (the first site,
  and second-site helper-boundary cases), the output is checked against an
  independent identity or quarter-turn result; the owner-rebuilt basis is covered
  by the native/original comparison. The published point is asserted exactly
  against the original FPU addition: X/Y at `hdHatTest` (Z is lifted for the
  test) and the final position after the second transform. The first site
  publishes in 768 cases, the second in 960.
- The VU rule from the previous increment applies: exact outside the original
  leaf's write set; inside it, the original's later write or the native leaf's
  entry value. Only forward scratch W at frame `+0c` is normalized for test
  comparison, after every real consumer.
- The particle effect paths of the ricochet's launch state stay on the original
  path and are not reached by these fixtures.
- The fixture fix also restores a satellite setup line from PR #57 that a
  misplaced comment had disabled (`vehicle+0x2370` now gets its fixed point
  instead of fill data). All earlier counts and assertions are unchanged.
- Aggregate caller coverage is 124,928 generated sequences / 5,792 whole-memory
  comparisons. Historical forward ABI coverage is 8,320 / 520; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `4d863cd`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The first ricochet return `1b2fb0` is live: it runs
natively 319 times on the one-player route with no recorded difference (the rock's
`1b3ef8` still runs 1,569 times, also exact). The reflection-branch return `1b3118`
is cold on these routes and is qualified by the generated owner suite above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames; `1b2fb0` runs natively 67 times in the one-player window.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio.

Audio device underruns remain: 55.9 ms silence in one-player, 40.7 ms in
four-player and 18.0 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A visible ricochet bounce off a wall, full
campaign and physical controller/hardware acceptance remain open.
