# Native HUD laser forward transforms

[The BlowDebris batch](NATIVE_BLOW_DEBRIS_FORWARD.md) follows with the debris pieces' six sites.

The two `Hud::BuildLaserStuff` forward returns use native XYZ arithmetic after the
[UpdateLauncher batch](NATIVE_UPDATE_LAUNCHER_FORWARD.md). Known forward coverage
grows from 258 to 260 of 269 returns (9 remain). Known inverse coverage stays 134 of
134; BozoEngine stays at `4a8f70a`. The nine that remain write persistent object
fields and are handled together.

`Hud::BuildLaserStuff` (`1425b8`) draws the laser sight of the HUD's vehicle (`+5e0`)
when the vehicle carries a laser (`+2438`):

| Return PC | Producer and consumer |
| --- | --- |
| `142640` | The table offset `4e6ef0` (copied to frame `+70`) goes through the vehicle's coordinate system into frame `+10`. `mathfAddVector` adds the vehicle position into frame `+90` |
| `1426cc` | `4e6f00` (frame `+80`) goes through the aim matrix rebuilt at frame `+30` into frame `+10`. It is added into frame `+90`, which `mathfTransform3dTo2d` projects |

`mathfAddVector` reads its operands as XYZ and writes XYZ, so the forward W is not
carried into frame `+9c`. The projection's input is frame `+90`, not the transform
output. The owner never loads frame `+1c`, and it reads no VU registers, accumulator
or flags. The existing native helper computes separately rounded XYZ, preserves
destination W and leaves VU state untouched. Unknown and historical callers keep the
original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/hud-laser-forward`, against parent `0fb62aa`
(PR #89). Parent headless/window binaries are preserved there and match PR #89's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 363 and 364): helper-boundary continuations and entries, across four rounding
  modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W payloads.
- `142640` runs from the owner's start: the current view, the table copies and the
  laser check. It stops at the following `boundFAngle`. `1426cc` starts at the
  call-return label after the aim matrix rebuild (`1426bc`) and stops at
  `mathfTransform3dTo2d`.
- Every basis is fixture-known, so both sites are checked against an independent
  identity or quarter-turn result. Frame `+90` is asserted exactly against the
  original FPU arithmetic. Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+1c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 711 → 707,
  real libm errno cases 810 → 859). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 186,880 generated sequences / 9,664 whole-memory
  comparisons. Historical forward ABI coverage is 16,064 / 1,004; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `0fb62aa`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. Both returns are live on the four-player route: each runs natively 88 times with no recorded difference.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 47.0 ms of silence in one-player, 28.8 ms in four-player and 20.4 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Full campaign and physical controller/hardware acceptance remain open.
