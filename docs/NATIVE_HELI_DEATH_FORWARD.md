# Native falling helicopter forward transforms

[Junk jet and blimp flight forwards](NATIVE_FLYER_FORWARD.md) follow with two more environment sites.

The two helicopter death forward returns use native XYZ arithmetic after the
[Ferris wheel fall and smasher camera forwards](NATIVE_FERRIS_SMASHER_FORWARD.md).
Known forward coverage grows from 148 to 150 of 269 returns (119 remain). Known
inverse coverage stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `12a180` | `EnvHeli::UpdateDeath` (`12a0f0`) | Smoothed fall velocity through the craft's stored basis at `+40`, added to its coordinate-system position |
| `1392e4` | `Helicopter::UpdateDeath` (`139250`) | The same with the basis at `+50`; at death frame 60 it then bursts into debris |

While the death timer runs (under 120 frames), each owner sinks and rolls the
craft (rebuilding the coordinate system's matrix with `mathfRotMatrixRPH`), smooths
the fall speed and moves the craft by the transformed velocity. The result is the
owner's frame vector at `+0`, read only by `mathfAddVector`. Neither owner touches
its frame after that, so `+0c` is never read, and neither reads VU registers, the
accumulator or flags. The existing native helper computes separately rounded XYZ,
preserves destination W and leaves VU state untouched. Unknown and historical
callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/heli-death-forward`, against parent `9daca2b`
(PR #62). Parent headless/window binaries are preserved there and match PR #62's
qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 253 and 254): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Full entries run the sink, roll, matrix rebuild, fall-speed smoothing,
  transform and position update. The timeout scenario (timer reaching 120) takes
  no transform and stops at the original `BlowDebris` (EnvHeli) or `Regen`
  (Helicopter). The Helicopter's frame-60 scenario stops at `BlowDebris` after the
  position update, and reaching it is asserted. `BlowDebris` and `Regen` have their
  own transforms and effects and are not claimed here. Producer reach is asserted
  in every case.
- Transform outputs are checked against an independent identity or quarter-turn
  result in every reached case. The published position is asserted exactly
  against the original FPU addition. Each site publishes in 480 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  at frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 715 → 709,
  real libm errno cases 830 → 827). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 130,560 generated sequences / 6,144 whole-memory
  comparisons. Historical forward ABI coverage is 9,024 / 564; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `9daca2b`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. Both new sites are cold on these routes, so the
generated owner suite above is their qualification. The Cop SUV site `1502cc`
still runs natively 1,638 times (one-player) and 845 times (four-player), exact.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. The first four-player window run, made while both headless
replays were running, drifted from its frame-scripted pad input and stayed on the
opponent-selection menu. That run is kept under `first-four-player` in the evidence
folder. A solo rerun of the same route reached the four-way match as usual, with
`1502cc` running natively 197 times. Reviewed captures show the intro and credits,
menus, vehicle and level selection, driving with the HUD and four simultaneous
views. Each route has five distinct captures. Stereo 48 kHz, 16-bit WAV recordings
contain nonzero audio.

Audio device underruns remain: 58.5 ms silence in one-player, 39.4 ms in the
four-player rerun and 21.5 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A helicopter destroyed in actual play, full
campaign and physical controller/hardware acceptance remain open.
