# Native FinalBoss gun-shot forward transforms

[FinalBoss movement forwards](NATIVE_BOSS_MOVE_FORWARD.md) follow with the boss's two movement sites.

Twelve FinalBoss forward returns use native XYZ arithmetic after the
[particle arc forwards](NATIVE_PARTICLE_ARCS_FORWARD.md). Known forward coverage
grows from 182 to 194 of 269 returns (75 remain). Known inverse coverage stays 134
of 134; BozoEngine stays at `4a8f70a`.

All twelve are in `FinalBoss::UpdateBoss` (`10f7c0`) and end a gun shot the same
way. A muzzle offset goes through the boss's coordinate system into the frame
vector at `+10`, and `mathfAddVector` adds the boss position. Some states then aim
at the target with `mathfRotationFromPointToPoint`. With shots enabled
(`gp-4538`), `Weapons::CreateStaticWeapon` is called with that frame vector as the
shot origin.

| Return PCs | Muzzle offset |
| --- | --- |
| `110114`, `110880` | The game's table at `235070` |
| `1101b0`, `11091c`, `110d9c` | The game's table at `235090` |
| `10febc`, `1103e4`, `1104e4`, `110a0c`, `110adc`, `110e8c`, `110f74` | A vector just built with `mathfSetFVector` |

The frame vector is read only by `mathfAddVector`, by `CreateStaticWeapon`, and,
with shots disabled, by `playHelicopterMachineGunSound`. Two earlier increments
cover these: the environment-gun increment showed that `CreateStaticWeapon` reads
only X/Y/Z, and the sound helper passes the point only to `calcDualMonoMixVolPan`,
which the [sinking blimp increment](NATIVE_BLIMP_DEATH_FORWARD.md) showed reads only
X/Y/Z.
Frame `+1c` is never read. The owner reads no VU registers, accumulator or flags.
The existing native helper computes separately rounded XYZ, preserves destination
W and leaves VU state untouched. It reads its input before writing, so the in-place
transforms are safe. Unknown and historical callers keep the original ABI. No
engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/boss-shots-forward`, against parent `1f22318`
(PR #74). Parent headless/window binaries are preserved there and match PR #74's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 6,144 generated sequences / 384 whole-memory comparisons
  (caller kinds 287 to 298): helper-boundary continuations and owner entries,
  across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases
  and arbitrary W payloads.
- The attack states pick their shots through random rolls and timers, so owner
  entries start at the call-return label just before each shot. The frame is set
  as the owner's prologue leaves it, as in the boss fixtures that test the inverse
  transforms. Each entry is one of:
  - right after the offset's `mathfSetFVector`;
  - after the roll that gates the shot, entered with roll result 0;
  - after the burst-interval check, entered with remainder 0.

  From there each entry runs the transform, the position addition, any aiming, and
  the call to `CreateStaticWeapon`, where the fixture stops; reaching it is
  asserted.
- The boss basis is fixture-known in every case, so every output is checked against
  an independent identity or quarter-turn result.
- The shot origin is asserted exactly against the original FPU addition, both in
  the frame and as the origin `CreateStaticWeapon` receives. Each site publishes in
  512 cases.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+1c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 704 → 709,
  real libm errno cases 838 → 859). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 153,088 generated sequences / 7,552 whole-memory
  comparisons. Historical forward ABI coverage is 11,840 / 740; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `1f22318`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. None of the twelve boss returns runs on these routes, so they are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 52.0 ms of silence in one-player, 501.9 ms in four-player and 19.4 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. In the qualification four-player window run, the frame-1,000 capture was black and the run logged 501.9 ms of silence, from a stall early in the intro. Two back-to-back short reruns (new build, then parent) captured frames 900 to 1,100. Both show the intro at frame 1,000 and about 20 ms of silence each, so the stall was the machine, not this change. The boss fight in actual play, full campaign and physical controller/hardware acceptance remain open.
