# Native environment gun forward transforms

The two `Launcher::UpdateEnvWeapon` (`14d3a0`) forward returns use native XYZ
arithmetic after the [Cop SUV laser-aim forwards](NATIVE_COPSUV_AIM_FORWARD.md).
Known forward coverage grows from 144 to 146 of 269 returns (123 remain). Known
inverse coverage stays 134 of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Producer and consumer |
| --- | --- |
| `14d62c` | Muzzle offset table `23d240` through the gun's basis, plus the gun position, passed to `Weapons::CreateStaticWeapon` as the shot origin (toggle at `+f0` set) |
| `14d678` | Same with the alternate muzzle table `23d250` (toggle clear) |

The game-mode jump table at `4e71c0` selects the environment-gun case. It aims
the gun at the nearest valid vehicle and fires every tenth update, alternating
muzzles. The result is the owner's frame vector at `+0`. `mathfAddVector` reads its
XYZ, and `CreateStaticWeapon` (`1b7590`) reads the position only through two
scalar `mathfCopyVector` calls; its `InitWeapon` and `activateWeaponSound` calls do
not receive it. The owner never reads frame `+0c` (the output W) and reads no VU
registers, accumulator or flags. The existing native helper computes separately
rounded XYZ, preserves destination W and leaves VU state untouched. Unknown and
historical callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/envweapon-forward`, against parent `aadc76b`
(PR #60). Parent headless/window binaries are preserved there and match PR #60's
qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons
  (caller kinds 249 and 250): helper-boundary continuations and full-owner
  entries, across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- Full entries go through the real jump table, the nearest-vehicle search over the
  game's embedded vehicle array, the aim smoothing and basis rebuild, and the fire
  counter. Each fire stops at `CreateStaticWeapon`, whose position argument is
  captured; the toggled muzzle state is asserted. The no-fire update count returns
  normally without a transform. Producer reach is asserted in every case.
- Helper-boundary outputs are checked against an independent identity or
  quarter-turn result. Full entries rebuild the gun basis from its aim angles first,
  so those are covered by the native/original comparison. The shot origin passed
  to `CreateStaticWeapon` is asserted exactly against the original FPU addition.
  Each site publishes in 480 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W
  at frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 715 → 718,
  real libm errno cases 850 → 802). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 128,512 generated sequences / 6,016 whole-memory
  comparisons. Historical forward ABI coverage is 8,768 / 548; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `aadc76b`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. Both environment-gun sites are cold on these
routes (neither level has that gun), so the generated owner suite above is their
qualification. The Cop SUV site `1502cc` still runs natively 1,638 times (one-player)
and 845 times (four-player), exact.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show the intro and credits, menus,
vehicle and level selection, driving with the HUD and four simultaneous views.
Each route has five distinct captures. Stereo 48 kHz, 16-bit WAV recordings
contain nonzero audio.

Audio device underruns remain: 56.0 ms silence in one-player, 56.3 ms in
four-player and 15.7 ms in the movie run. These are functional checks, not a
performance or flawless-playback claim. A level with the environment gun firing,
full campaign and physical controller/hardware acceptance remain open.
