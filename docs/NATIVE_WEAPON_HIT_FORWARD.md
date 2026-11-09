# Native weapon hit-test forward transforms (RockHD and RicoHD)

Six weapon hit-test forward returns use native XYZ arithmetic after the
[held-aim and lock-on arc batch](NATIVE_SPECIAL_ARCS_FORWARD.md). Known forward
coverage grows from 216 to 222 of 269 returns (47 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

These are the first forwards whose results feed the hd collision system. Their
consumers were audited first:

- `hdPntSphereTest` (`1c4e70`) keeps *pointers* to its point and sphere in gp
  globals (`gp-3024`, `gp-3020`). Every reader of those globals (`hdHier`,
  `hdColGrid`, `hdColNode`, `hdTerrain`, `hdBspNode`, `terrainCollision`)
  reads the point only through XYZ helpers or as separate `x`/`y`/`z` words.
  Nothing copies the point with its W.
- `Interactives::DispatchHit` (`142d98`) only passes the point on. All twelve
  jump-table handlers ignore it, pass it on, or read it only as XYZ: the vehicle,
  fodder, destructible, final boss, loop tank and pedestrian strategy handlers,
  `vehicleReactToWeaponDamage`, `GenericTakeHit` and `DoEffect`. No object field or
  global receives its W.
- `mathfSubVector`, `mathfAddVector`, `mathfNormalize` and
  `mathfRotationFromVector` load and store only `+0/+4/+8`.
  `updateRichochetBounce` gets the coordinate system's position, whose W no
  forward writes.

| Return PC | Producer and consumer |
| --- | --- |
| `1b6cd8`, `1b6d3c` | `Weapon::RockHD` (`1b6bf0`): with `+1c` set and a nonzero fall speed at `+98`, or with `+1c` clear, a fixed offset goes through the weapon's basis at `+d0` into frame `+0`, plus its coordinate system's position. It becomes `hdPntSphereTest`'s point; on a hit, `mathfSubVector` rewrites its XYZ and `DispatchHit` gets it |
| `1b5168` | `Weapon::RicoHD` (`1b50f0`): a fixed offset through the basis into frame `+10`, plus the position; then `hdPntSphereTest`, the hit's `mathfSubVector` and `DispatchHit` |
| `1b52c0` | After a hit, the reflection rebuilds the basis; the direction at `+90` goes through it into frame `+10`, added into the coordinate system's position, which goes to `updateRichochetBounce` |
| `1b5528` | With no hit, a new offset goes through the basis into frame `+10`, plus the position, and is tested again (`hdPntSphereTest`, `mathfSubVector` and `mathfRotationFromVector`) |
| `1b5630` | The second reflection: the direction at `+90` into frame `+10`; the coordinate system's position becomes `+80` plus it |

Every result is written in place in the frame. RockHD never loads frame `+0c`, and
RicoHD never loads frame `+1c`. Neither reads VU registers, accumulator or flags.
The existing native helper computes separately rounded XYZ, preserves destination
W and leaves VU state untouched. Unknown and historical callers keep the original
ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/weapon-hit-forward`, against parent `4847649`
(PR #80). Parent headless/window binaries are preserved there and match PR #80's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons (caller
  kinds 321 to 326): helper-boundary continuations and full entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- `1b6cd8`, `1b6d3c` and `1b5168` run from the owner's start through the real
  branch selection and offset setup, and stop at `hdPntSphereTest`. The other three
  start at the call-return label before each transform (`1b52a4`, `1b5518`,
  `1b5614`). `1b52c0` stops at `updateRichochetBounce`, `1b5528` at
  `hdPntSphereTest`, and `1b5630` runs to the owner's real return.
- The harness requires each full entry to reach exactly its own transform. Every
  basis is fixture-known, so every site is checked against an independent identity
  or quarter-turn result.
- Published values are asserted exactly against the original FPU arithmetic: the
  point `hdPntSphereTest` receives (and that it receives that frame address), the
  position handed to `updateRichochetBounce`, and the returned position.
  Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+0c` or `+1c`) is normalized for test comparison, after every real
  consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 709 → 718,
  real libm errno cases 830 → 842). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 167,424 generated sequences / 8,448 whole-memory
  comparisons. Historical forward ABI coverage is 13,632 / 852; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `4847649`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. Four returns are live on the one-player route and run natively there with no recorded difference: `1b6d3c` 1,569 times, `1b5168` and `1b5528` 319 times each and `1b52c0` 5 times. `1b6cd8` and `1b5630` are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 59.3 ms of silence in one-player, 29.0 ms in four-player and 27.5 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The other special weapons in actual play, full campaign and physical controller/hardware acceptance remain open.
