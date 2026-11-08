# Native helicopter gun and walking pedestrian forward transforms

[Machine-gun muzzle forwards](NATIVE_MACHINE_GUN_FORWARD.md) follow with the two weapon-initialization sites.

Two single-site forward returns use native XYZ arithmetic after the
[falling junk jet forwards](NATIVE_JET_DEATH_FORWARD.md). Known forward coverage
grows from 163 to 165 of 269 returns (104 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `129c48` | `EnvHeli::UpdateEnvHeli` (`1297e8`) | The gun's muzzle offset (game table `4e50d0`) through the helicopter's basis, in place, plus its position, passed to `Weapons::CreateStaticWeapon` as the shot origin |
| `10cc7c` | `AttractPed::updateTrans` (`10cbf0`) | A walking pedestrian's velocity, scaled by 60/update rate, through its basis, added to its position |

In attack mode, the environment helicopter takes the closest live car from the
game's vehicle array (`Launcher::GetClosestCar`). It turns toward the car with
`mathfRotationFromPointToPoint`, smoothing, and `UpdateRot` (`mathfRotMatrixH`,
`mathfRotMatrixRPH`). On every 30th update with the aim inside its tolerance, it
fires from the transformed muzzle point. The result is the frame vector at `+10`,
read only by `mathfAddVector` and by `CreateStaticWeapon`. The environment-gun
increment already established that `CreateStaticWeapon` reads only X/Y/Z. Frame
`+1c` is never read.

The attract-mode pedestrian, when its coordinate system is on, moves by the
transformed velocity. The probe is lifted by 5 for `hdHatTest`, then the height is
settled and the walk animation speed set. The result is the frame vector at `+10`.
It is read only by `mathfAddVector`. `hdHatTest` receives the same pointer as an
output and only writes it (`mathfCopyVector`, `mathfSetFVector`). Frame `+1c` is
never read.

Neither owner reads VU registers, the accumulator or flags. The existing native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. It reads its input before writing, so the in-place transform is safe.
Unknown and historical callers keep the original ABI. No engine or gameplay formula
changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/envheli-ped-forward`, against parent `ba9f83e`
(PR #67). Parent headless/window binaries are preserved there and match PR #67's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 268 and 269): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Helicopter entries cover the following, and reaching each stop is asserted:
  - Car selection, the aim and the matrix rebuild, then the firing update, which
    stops at `CreateStaticWeapon`.
  - A non-firing update, which stops at `UpdateTrans` (it has its own native
    transform).
  - A non-attacking mode, which stops at its sound update.
- Pedestrian entries cover the update-rate scaling, the transform, the probe lift,
  `hdHatTest` on the empty grid and both height-settling branches. They stop at
  `getDbId`, ahead of the animation-speed tables. A pedestrian whose coordinate
  system is off drifts without a transform and stops at its grid reparent.
- Helper-boundary outputs are checked against an independent identity or
  quarter-turn result. Full entries rebuild or scale first, so those are covered by
  the native/original comparison.
- Published values are asserted exactly against the original FPU addition:
  - the shot origin passed to `CreateStaticWeapon`;
  - the pedestrian's X/Y position, and the point `hdHatTest` received.

  The helicopter site publishes in 448 cases, the pedestrian in 480.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+1c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 704 → 705,
  real libm errno cases 813 → 828). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 138,240 generated sequences / 6,624 whole-memory
  comparisons. Historical forward ABI coverage is 9,984 / 624; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `ba9f83e`: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recordings and snapshots are byte exact; strict
comparisons report no mismatch. The pedestrian return `10cc7c` is live: it runs
natively 390 times on the one-player route (default and strict) with no recorded
difference. The helicopter gun return `129c48` is cold on these routes and is
qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. In the qualification movie run, the frame-1,000 capture landed in a fade
and was nearly black. Two back-to-back short reruns (new build, then parent)
captured frames 900 to 1,100. Both show the same fade at 900 and the intro at
1,000, with comparable brightness. The black frame was real-time movie timing,
not this change. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio.
Audio device underruns: 55.8 ms of silence in one-player, 32.9 ms in four-player
and 37.7 ms in the movie run. These are functional checks, not a performance or
flawless-playback claim. The attacking helicopter in actual play, full campaign and
physical controller/hardware acceptance remain open.
