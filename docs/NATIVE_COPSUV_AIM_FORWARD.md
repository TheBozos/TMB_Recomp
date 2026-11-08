# Native Cop SUV laser-aim forward transforms

The two `Launcher::UpdateCopSUV` (`14ff58`) forward returns use native XYZ
arithmetic after the [launcher turret-aim forwards](NATIVE_LAUNCHER_AIM_FORWARD.md).
Known forward coverage grows from 142 to 144 of 269 returns (125 remain). Known
inverse coverage stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Branch | Producer and consumer |
| --- | --- | --- |
| `150094` | Mode `0x14` with a live player state | Laser offset table through the vehicle basis, in place, plus the shooter position, aimed at the player-state target |
| `1502cc` | Otherwise, a closest car within 600 units | Same offset and position, aimed at the car `GetClosestCar` chose |

As with the other launchers, the frame `+10` result is read only as XYZ, by
`mathfAddVector` and by the scalar `mathfRotationFromPointToPoint`. The owner never
reads frame `+1c` (the output W) and reads no VU registers, accumulator or flags.
The existing native helper computes separately rounded XYZ, preserves destination
W and leaves VU state untouched. Unknown and historical callers keep the original
ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/copsuv-aim-forward`, against parent `5135b43`
(PR #59). Parent headless/window binaries are preserved there and match PR #59's
qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 247 and 248): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Kind 247 sets the game mode to `0x14` with a live player state and a target
  coordinate system, so full entries take the first branch even without a closest
  car. Kind 248 places a car inside the laser's 600-unit range, so full entries take
  the closest-car branch; that car's target flag (`+24bc`) is asserted, and the
  no-target scenario returns normally without a transform. Producer reach is
  asserted in every case.
- Both branches continue into `LauncherSound::startLaserSound` or
  `stopLaserSound`, which call into the sound driver. Fixtures stop at those two
  calls, after the aim point has been consumed, and assert that one of them was
  reached; the laser sound and the turret matrix publication after it are not
  executed here.
- Transform outputs are checked against an independent identity or quarter-turn
  result in every reached case (the basis is the vehicle's own). The aim point
  consumed by `mathfRotationFromPointToPoint` is asserted exactly against the
  original FPU addition. The first site publishes in 512 cases, the second in 480.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  at frame `+1c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved again (vehicle tire jumps
  704 → 715, real libm errno cases 833 → 850). All scenario-defined counts and
  assertions are unchanged.
- Aggregate caller coverage is 127,488 generated sequences / 5,952 whole-memory
  comparisons. Historical forward ABI coverage is 8,640 / 540; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `5135b43`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The closest-car return `1502cc` is live: it runs
natively 1,638 times on the one-player route and 845 times on the four-player route
with no recorded difference. The player-state return `150094` is cold on these
routes and is qualified by the generated owner suite above.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames; `1502cc` runs natively 544 times in the one-player window
and 197 times in the four-player window. Reviewed captures show the intro and
credits, menus, vehicle and level selection, combat with the HUD and four
simultaneous views. Each route has five distinct captures. Stereo 48 kHz, 16-bit
WAV recordings contain nonzero audio.

Audio device underruns remain: 57.3 ms silence in one-player, 35.3 ms in
four-player and 15.2 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. The mode-`0x14` branch in actual play, full
campaign and physical controller/hardware acceptance remain open.
