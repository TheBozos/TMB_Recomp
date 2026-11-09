# Native combo-launch forward transforms

[Zoomies and HalTrans weapon forwards](NATIVE_ZOOMIES_HAL_FORWARD.md) follow with four more weapon sites.

Five combo forward returns use native XYZ arithmetic after the
[tire-pair correction forwards](NATIVE_TIRE_ROTATION_FORWARD.md). Known forward
coverage grows from 169 to 174 of 269 returns (95 remain). Known inverse coverage
stays 134 of 134; BozoEngine stays at `4a8f70a`.

All five are in `Vehicle::vehicleCheckForCombos` (`1af4c0`). Each places the launch
point for a special triggered by a button combo:

| Return PC | Combo | Producer |
| --- | --- | --- |
| `1af700` | `1` | After `Launcher::GetLockOn`: offset `(0,8,2)` through the vehicle's coordinate-system basis, in place |
| `1af790` | `0x10304` | After `GetLockOn`: offset `(0,12,5)`, the same way (`+2270` is set for the first of its four `CreateWeapon` calls) |
| `1af890` | `4`, controller count above the threshold | The stored offset at `+2620` through the vehicle basis at `+d0` (sets `+2274`) |
| `1af8fc` | `4`, otherwise | The same offset, on the single-cost branch |
| `1af980` | `5` | The same offset (sets `+2274`) |

When `+580` is 1, the combo code is the first word of the controller block at
`+2000`; when it is 2, it is the word at `+418` of the block at `+2004`. Each combo compares the counter at `+2214` with
its cost. The transform result is the frame vector at `+0`, read only by
`mathfAddVector`, which adds the vehicle position into `+2760` and writes X/Y/Z
only. `TheGame::CreateWeapon` then launches the special. Frame `+0c` is never read
in any combo case, and the W of `+2760` is not written by this path at all. The
owner reads no VU registers, accumulator or flags. The existing native helper
computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transforms are safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/combo-forward`, against parent `7627a52` (PR #70).
Parent headless/window binaries are preserved there and match PR #70's qualified
build. Generated bodies, retail data and runtime recordings stay outside Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons
  (caller kinds 274 to 278): helper-boundary continuations and full-owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Full entries run from the owner's start: the `+580` check, the combo dispatch
  and the energy-cost checks. Combos 1 and `0x10304` run the real
  `Launcher::GetLockOn`, with nothing to lock on in empty vehicle lists. Combo 4
  takes each branch by its controller count. Every launch stops at `CreateWeapon`;
  reaching it is asserted. With no combo entered, the owner adds `+20c8` into the
  counter at `+20e0` and returns without a transform.
- Both bases are fixture-known at entry as well as at the helper boundary, so every
  case is checked against an independent identity or quarter-turn result.
- The launch position in `+2760` is asserted exactly against the original FPU
  addition, together with the `+2270`/`+2274` flags. Each site publishes in 480
  cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 700 → 708,
  real libm errno cases 853 → 842). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 142,848 generated sequences / 6,912 whole-memory
  comparisons. Historical forward ABI coverage is 10,560 / 660; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `7627a52`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The combo-4 return `1af8fc`
is live: it runs natively once on the one-player route with no recorded
difference. The other four returns are cold on these routes and are qualified by
the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 50.1 ms of silence in one-player, 39.8 ms in four-player and 19.8 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The other combos in actual play, full campaign and physical
controller/hardware acceptance remain open.
