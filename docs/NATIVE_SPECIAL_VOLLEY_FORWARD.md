# Native launcher special-weapon forward transforms (second batch)

Five more `Launcher::UpdateSpecial` forward returns use native XYZ arithmetic after
the [first special-weapon batch](NATIVE_SPECIAL_GUNS_FORWARD.md). Known forward
coverage grows from 201 to 206 of 269 returns (63 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Vehicle type | Producer and consumer |
| --- | --- | --- |
| `14ee10` | `1c0` | A volley shot's table offset (`23d150`, with the launcher's `+4` set) through the vehicle's coordinate system into frame `+0` |
| `14ee2c` | `1c0` | The same with table `23d0b0` (`+4` clear) |
| `14ee48` | `1c0` | That frame vector, in place, through the basis of the vehicle part at `+1ec8`, added to the vehicle position into the launcher's `+620`; then `Weapons::CreateWeapon` |
| `14ef50` | `140` | A constant offset (`gp-6298`) written into `+620`, transformed in place through the vehicle's coordinate system, plus the vehicle position; then `Weapons::CreateWeapon` |
| `14ef80` | `140` | The launcher's `+400` offset through the vehicle's coordinate system into `+620`, plus the vehicle position |

The type `1c0` volley fires ten shots once its countdown runs out. Each shot chains
two transforms in the frame vector at `+0` and then adds the vehicle position. The
frame vector is read only by the second transform (X/Y/Z input) and
`mathfAddVector`, and frame `+0c` is never read in the owner.

The type `140` sites write `+620`, which is the vehicle's `+2760` launch point,
since the launcher sits at `vehicle+2140`. There the transform's W persists. No code
in the game reads `vehicle+276c` (or a 64-bit or 128-bit block covering it) at a
fixed offset. `Weapons::CreateWeapon` copies the launch point with
`mathfCopyVector` (X/Y/Z only), and the owner hands `+620` only to
`mathfAddVector`.

The owner reads no VU registers, accumulator or flags. The existing native helper
computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transforms are safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/special-volley-forward`, against parent
`c4a4fac` (PR #77). Parent headless/window binaries are preserved there and match
PR #77's qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons
  (caller kinds 306 to 310): helper-boundary continuations and owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Type `140` entries run from the owner's start through the type dispatch,
  countdown and flag. A launcher still counting down returns without a transform.
- Type `1c0` entries start right after the volley's `timerGetCurTics` call. That
  call reads the hardware timer, which the harness does not model. From there each
  entry runs the first shot of the volley, both transforms and the addition, and
  stops at `Weapons::CreateWeapon`.
- The volley's two transforms chain without an addition between them. For these
  kinds, the test records the first transform's output as the second transform's
  input, and checks both.
- Every basis is fixture-known (the vehicle's coordinate system and the part's
  basis), so every case is checked against an independent identity or quarter-turn
  result. The final launch point in `+620` is asserted exactly against the original
  FPU addition. Publications: 640/640/1,536 and 480/480.
- The VU rule from the earlier weapon increments applies. Only the forward W (frame
  `+0c`, or `+62c` for type `140`) is normalized for test comparison, after every
  real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 702 → 715,
  real libm errno cases 801 → 835). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 159,232 generated sequences / 7,936 whole-memory
  comparisons. Historical forward ABI coverage is 12,608 / 788; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `c4a4fac`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The volley returns `14ee2c` and `14ee48` are live: each runs natively 20 times on the one-player route with no recorded difference. The other three are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 56.7 ms of silence in one-player, 32.5 ms in four-player and 18.3 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The other special weapons in actual play, full campaign and physical controller/hardware acceptance remain open.
