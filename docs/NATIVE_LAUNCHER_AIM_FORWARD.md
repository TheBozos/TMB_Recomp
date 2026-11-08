# Native launcher turret aim forward transforms

Three launcher turret-aim forward returns use native XYZ arithmetic after the
[ricochet forwards](NATIVE_RICOCHET_FORWARD.md). Known forward coverage grows from
139 to 142 of 269 returns (127 remain). Known inverse coverage stays 134 of 134;
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `150efc` | `Launcher::UpdateHearse` (`150e68`) | Turret offset table through the vehicle basis, in place, added to the shooter position |
| `1511ec` | `Launcher::UpdateMinion` (`151060`) | Launcher offset through the aim basis built from the vehicle's roll/pitch/heading |
| `1513f4` | `Launcher::UpdateTanker` (`151358`) | Turret offset table through the same kind of aim basis |

Each result is the owner's frame vector at `+10`. `mathfAddVector` adds the
shooter position, and the real `mathfRotationFromPointToPoint` (`1dbd58`, scalar
subtract and angles) turns it into turret angles toward the target that
`Launcher::GetClosestCar` chose. Those angles are bounded, smoothed and published
through `mathfRotMatrixRPHNoTrans` into the turret node. Frame `+1c` (the output
W) is never read, and the owners read no VU registers, accumulator or flags. The
existing native helper computes separately rounded XYZ, preserves destination W
and leaves VU state untouched. Unknown and historical callers keep the original
ABI. No engine or gameplay formula changes.

`Launcher::UpdateZoomies` (`14d294`) was examined and left for later: its
consumer continues into `GetLockOn` and `TheGame::CreateWeapon`.

## Generated comparisons

Evidence is under `D:/BozoScratch/launcher-aim-forward`, against parent `3e3740f`
(PR #58). Parent headless/window binaries are preserved there and match PR #58's
qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 1,536 generated sequences / 96 whole-memory comparisons
  (caller kinds 244 to 246): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Full entries run the real `GetClosestCar` over one live candidate in the game
  object's embedded vehicle array. A new 0x2600-byte snapshot region covers that
  array, including the Hearse's target flag at `+24bc`, which is asserted. The
  no-target scenario (a type-5 launcher with no car within 50 units, or the
  Minion's alternate type-5 path) skips the transform, and producer reach is
  asserted in every case.
- Transform inputs and outputs are captured at the real helper and
  `mathfAddVector` boundaries and checked against an independent identity or
  quarter-turn result wherever the fixture knows the basis: all Hearse cases, and
  helper-boundary Minion/Tanker cases. Full Minion/Tanker entries build their aim
  basis with `mathfRPHFromMatrix`/`mathfRotMatrixRPH`; those are covered by the
  native/original comparison. The aim point consumed by
  `mathfRotationFromPointToPoint` is asserted exactly against the original FPU
  addition. Each site publishes in 480 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  at frame `+1c` is normalized for test comparison, after every real consumer.
- The new snapshot region changes the shared random fill sequence, so two
  data-dependent counts in earlier suites moved (vehicle tire jumps 711 → 704,
  real libm errno cases 825 → 833). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 126,464 generated sequences / 5,888 whole-memory
  comparisons. Historical forward ABI coverage is 8,512 / 532; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `3e3740f`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The three launcher sites are cold on these routes
(no Hearse, Minion or Tanker special is fired), so the generated owner suite above
is their qualification. The earlier ricochet site `1b2fb0` still runs natively
319 times on the one-player route, exact.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show the intro and credits, menus,
vehicle and level selection, driving with the HUD and four simultaneous views.
Each route has five distinct captures. Stereo 48 kHz, 16-bit WAV recordings
contain nonzero audio.

Audio device underruns remain: 54.0 ms silence in one-player, 31.2 ms in
four-player and 17.1 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A visible Hearse, Minion or Tanker
special firing, full campaign and physical controller/hardware acceptance remain
open.
