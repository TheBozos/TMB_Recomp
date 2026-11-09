# Native launcher special-weapon forward transforms (held aim and lock-on arcs)

[The weapon hit-test batch](NATIVE_WEAPON_HIT_FORWARD.md) follows with six RockHD/RicoHD sites.

Seven more `Launcher::UpdateSpecial` forward returns use native XYZ arithmetic after
the [type-100 sweep batch](NATIVE_SPECIAL_SWEEP_FORWARD.md). Known forward coverage
grows from 209 to 216 of 269 returns (53 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

All seven are in `Launcher::UpdateSpecial` (`14dae0`):

| Return PC | Producer and consumer |
| --- | --- |
| `14ed94` | Vehicle type `100` with a shot held (`+284`): the turret is turned, no animation is running, and the aim point at `+350` is smoothed toward `(0, gp-629c, 0)`. It goes through the vehicle's coordinate system into frame `+0`, plus the vehicle system's position, into the position of the object at `+34c`; the owner then returns |
| `14e2d0`, `14e320` | Vehicle type `a0` lock-on arc: a side offset in frame `+0` goes through the vehicle's coordinate system into frame `+20`, plus the vehicle position. That is the arc's first point |
| `14e498`, `14e4dc` | With a target at `+2f4`, a heading-selected table slot (frame `+40` or `+70`) goes through the target's coordinate system into frame `+30`, plus the target's position. That is the arc's second point |
| `14e6a8`, `14e6ec` | The second type `a0` block: the same heading-selected slots through the target's system into frame `+30`, plus the target's position, arcing from the vehicle position |

Every arc ends in `particleElectricArc`, which reads its two points only as XYZ.
`mathfAddVector` reads its operands only as XYZ. The owner never reads frame `+0c`,
`+2c` or `+3c`, and it reads no VU registers, accumulator or flags. The existing
native helper computes separately rounded XYZ, preserves destination W and leaves
VU state untouched. Unknown and historical callers keep the original ABI. No engine
or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/special-arcs-forward`, against parent `2d3c0f4`
(PR #79). Parent headless/window binaries are preserved there and match PR #79's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,584 generated sequences / 224 whole-memory comparisons (caller
  kinds 314 to 320): helper-boundary continuations and full entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- `14ed94` runs from the owner's start: the type dispatch, the turret step and
  bound, the node-matrix rebuild, the animation check and the aim-point smoothing,
  to the owner's real return. The arc entries start at the call-return label before
  each transform (`14e2b8`, `14e308`, `14e3f8`, `14e608`): the heading bounding and
  the window thresholds select the slot, and the fixture stops at
  `particleElectricArc` after `rand`.
- The harness requires each full entry to reach exactly its own transform. The
  vehicle and target bases are fixture-known, so every site is checked against an
  independent identity or quarter-turn result.
- Published values are asserted exactly against the original FPU arithmetic: the
  aimed object's position, and the arc end in frame `+20` or `+30` at the arc
  call. Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  (frame `+0c`, `+2c` or `+3c`) is normalized for test comparison, after every real
  consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 715 → 709,
  real libm errno cases 829 → 830). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 164,352 generated sequences / 8,256 whole-memory
  comparisons. Historical forward ABI coverage is 13,248 / 828; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `2d3c0f4`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The seven returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 31.4 ms of silence in one-player, 652.5 ms in four-player and 20.9 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The four-player window run overlapped other work on the machine (39 to 48 frames/s around frame 3,300). A back-to-back four-player rerun of new and parent builds then gave 35.1 ms and 103.1 ms of silence at 59.3 frames/s. The other special weapons in actual play, full campaign and physical controller/hardware acceptance remain open.
