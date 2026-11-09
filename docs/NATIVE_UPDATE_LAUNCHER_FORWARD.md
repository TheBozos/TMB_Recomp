# Native UpdateLauncher forward transforms

The three `Launcher::UpdateLauncher` forward returns use native XYZ arithmetic after
the [FireSpecial types c0/1e0/220 batch](NATIVE_FIRE_TORCH_FORWARD.md). Known forward
coverage grows from 255 to 258 of 269 returns (11 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

`Launcher::UpdateLauncher` (`147260`) has the launcher in `s1` and the vehicle in
`s3`:

| Return PC | Producer and consumer |
| --- | --- |
| `147a8c` | The aim offset in frame `+0` goes through the vehicle's coordinate system in place. `mathfAddVector` adds it to the vehicle position into the launcher's `+230`, which is height-tested (`hdHatTest`) and copied on with `mathfCopyVector` |
| `147cc8` | The current weapon's muzzle offset (`+3f0` indexed by the weapon number at `+0`) goes through the vehicle's system into frame `+0`, plus the vehicle position, into `+620`. Then `FireCurrentWeapon` fires |
| `147cf4` | For vehicle type `1c0`, that point first goes through the turret part's matrix (`+1ec8`, `+10`) in place |

The consumers are `mathfAddVector` and the next transform's input (whose W the
transform ignores). `hdHatTest`, `mathfCopyVector` and `FireCurrentWeapon` get
launcher fields, not the frame vector; launcher `+62c` has no reader. UpdateLauncher
never loads frame `+0c`, 64 bits at `+08` or 128 bits from its frame. It reads no VU
registers, accumulator or flags.

`147a8c` and `147cf4` are in place, so the input is read whole before the output is
written. The existing native helper computes separately rounded XYZ, preserves
destination W and leaves VU state untouched. Unknown and historical callers keep the
original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/update-launcher-forward`, against parent `24ef197`
(PR #87). Parent headless/window binaries are preserved there and match PR #87's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,536 generated sequences / 96 whole-memory comparisons (caller
  kinds 360 to 362). They are helper-boundary continuations and entries at the
  call-return label before each transform (`147a78`, `147ca8`, `147cc8`), across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- Each run stops after its result is consumed: at `hdHatTest` (`147a8c`) or at
  `FireCurrentWeapon`. The type-`1c0` check is real: `147cc8` runs use another type
  and skip the turret transform.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result. Published values are asserted exactly against the
  original FPU arithmetic: the launcher's `+230` (at `hdHatTest`) and `+620` (at
  `FireCurrentWeapon`). Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 708 → 711,
  real libm errno cases 831 → 810). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 185,856 generated sequences / 9,600 whole-memory
  comparisons. Historical forward ABI coverage is 15,936 / 996; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `24ef197`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. `147cc8` and `147cf4` are live on the one-player route: they run natively 17 and 6 times with no recorded difference. `147a8c` is cold on these routes and is qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 101.9 ms of silence in one-player, 33.8 ms in four-player and 19.4 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The one-player window run, made while a read-only audit was running, reported 101.9 ms of silence. A back-to-back one-player rerun of new and parent builds then gave 48.1 ms and 53.0 ms at 59.5 and 59.7 frames/s. Full campaign and physical controller/hardware acceptance remain open.
