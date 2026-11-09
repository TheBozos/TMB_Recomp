# TMB on BozoEngine — 2026-11-05

**Release update, 2026-11-06:** [v0.4.0 is published](https://github.com/TheBozos/TMB_Recomp/releases/tag/v0.4.0),
tagged at `eea175219b1e46a9c1699f640744279650c65a14`, with BozoEngine `9361960`.
The [tagged release workflow](https://github.com/TheBozos/TMB_Recomp/actions/runs/42133821321) passed and produced
both player ZIPs, their SHA-256 files, and the corresponding-source ZIP/hash. The publication notification workflow passed too.
Both downloaded player ZIPs pass the local audit/hash checks and contain the same 7,750 files; all three ZIP hashes
also match GitHub's asset digests. The published package updated the isolated accented-path portable installation,
preserved its player-data marker, reported launcher version 0.4.0 and rebuilt/played successfully.
Final release timing: EE/PC **1.0000**, **575,965 ticks/s**, **59.95 fields/s**, movies **30.00 pictures/s**.
The approved shared README format is restored. Campaign/hardware/reference-test limits below remain open.

The goal is to remove the PS2Recomp fork from the project. This source tree now builds **only on BozoEngine**.
The earlier v0.3.2 release remains in Git history and on GitHub for comparison; it is not a runtime dependency.

## Architecture and build

[`NATIVE_SPECIAL_ARCS_FORWARD.md`](NATIVE_SPECIAL_ARCS_FORWARD.md) adds the held-aim and lock-on arc special-weapon forwards.
[`NATIVE_SPECIAL_SWEEP_FORWARD.md`](NATIVE_SPECIAL_SWEEP_FORWARD.md) adds the three type-100 special-weapon forwards.
[`NATIVE_SPECIAL_VOLLEY_FORWARD.md`](NATIVE_SPECIAL_VOLLEY_FORWARD.md) adds the second batch of launcher special-weapon forwards.
[`NATIVE_SPECIAL_GUNS_FORWARD.md`](NATIVE_SPECIAL_GUNS_FORWARD.md) adds the first five launcher special-weapon forwards.
[`NATIVE_BOSS_MOVE_FORWARD.md`](NATIVE_BOSS_MOVE_FORWARD.md) adds the two FinalBoss movement forwards.
[`NATIVE_BOSS_SHOTS_FORWARD.md`](NATIVE_BOSS_SHOTS_FORWARD.md) adds the twelve FinalBoss gun-shot forwards.
[`NATIVE_PARTICLE_ARCS_FORWARD.md`](NATIVE_PARTICLE_ARCS_FORWARD.md) adds the two particle arc forwards.
[`NATIVE_AI_BEAD_FORWARD.md`](NATIVE_AI_BEAD_FORWARD.md) adds the AI hunt and bait forwards.
[`NATIVE_ZOOMIES_HAL_FORWARD.md`](NATIVE_ZOOMIES_HAL_FORWARD.md) adds the Zoomies and HalTrans weapon forwards.
[`NATIVE_COMBO_FORWARD.md`](NATIVE_COMBO_FORWARD.md) adds the five combo-launch forwards.
[`NATIVE_TIRE_ROTATION_FORWARD.md`](NATIVE_TIRE_ROTATION_FORWARD.md) adds the two tire-pair correction forwards.
[`NATIVE_MACHINE_GUN_FORWARD.md`](NATIVE_MACHINE_GUN_FORWARD.md) adds the two machine-gun muzzle forwards.
[`NATIVE_ENVHELI_PED_FORWARD.md`](NATIVE_ENVHELI_PED_FORWARD.md) adds the helicopter gun and walking pedestrian forwards.
[`NATIVE_JET_DEATH_FORWARD.md`](NATIVE_JET_DEATH_FORWARD.md) adds the six falling junk jet forwards.
[`NATIVE_BLIMP_DEATH_FORWARD.md`](NATIVE_BLIMP_DEATH_FORWARD.md) adds the sinking blimp drift and explosion forwards.
[`NATIVE_GASTANK_FORWARD.md`](NATIVE_GASTANK_FORWARD.md) adds the rolling and falling gas tank forwards.
[`NATIVE_FLYER_FORWARD.md`](NATIVE_FLYER_FORWARD.md) adds the junk jet and blimp flight forwards.
Coverage is 209/269 known forward returns (60 remain), with 134/134 known inverse returns native.

[`NATIVE_HELI_DEATH_FORWARD.md`](NATIVE_HELI_DEATH_FORWARD.md) adds the two falling-helicopter forwards.
Coverage is 150/269 known forward returns (119 remain), with 134/134 known inverse returns native.

[`NATIVE_FERRIS_SMASHER_FORWARD.md`](NATIVE_FERRIS_SMASHER_FORWARD.md) adds the falling Ferris wheel car and
smasher hit-camera forwards. Coverage is 148/269 known forward returns (121 remain), with 134/134
known inverse returns native.

[`NATIVE_ENVWEAPON_FORWARD.md`](NATIVE_ENVWEAPON_FORWARD.md) adds the two environment gun fire-point
forwards. Coverage is 146/269 known forward returns (123 remain), with 134/134 known inverse
returns native.

[`NATIVE_COPSUV_AIM_FORWARD.md`](NATIVE_COPSUV_AIM_FORWARD.md) adds the two Cop SUV laser-aim
forwards. Coverage is 144/269 known forward returns (125 remain), with 134/134 known inverse
returns native.

[`NATIVE_LAUNCHER_AIM_FORWARD.md`](NATIVE_LAUNCHER_AIM_FORWARD.md) adds the Hearse, Minion and
Tanker launcher turret-aim forwards. Coverage is 142/269 known forward returns (127 remain),
with 134/134 known inverse returns native.

[`NATIVE_RICOCHET_FORWARD.md`](NATIVE_RICOCHET_FORWARD.md) adds the two ricochet movement
forwards, before its height test and after its reflection. Coverage is 139/269 known forward
returns (130 remain), with 134/134 known inverse returns native.

[`NATIVE_WEAPON_LOCK_FORWARD.md`](NATIVE_WEAPON_LOCK_FORWARD.md) adds six weapon lock-on,
look-ahead, skill-satellite and rock forwards, with actual aim, position and height-test consumers.
Coverage is 137/269 known forward returns (132 remain), with 134/134 known
inverse returns native.

[`NATIVE_MINION_FORWARD.md`](NATIVE_MINION_FORWARD.md) adds seven Minion shield-hit
forwards, with actual distance, health, HUD and hierarchy-switch consumers.
Coverage is 131/269 known forward returns (138 remain), with 134/134 known
inverse returns native.

[`NATIVE_ATTACHMENT_FORWARD.md`](NATIVE_ATTACHMENT_FORWARD.md) adds five helicopter,
cable and weapon movement forwards. Coverage is 124/269 known forward returns
(145 remain), with 134/134 known inverse returns native.

[`NATIVE_COLLISION_FORWARD.md`](NATIVE_COLLISION_FORWARD.md) adds six vehicle-collision
forwards for velocity reconstruction, forces, impact offsets and morph hit placement.
Coverage is 119/269 known forward returns (150 remain), with 134/134 known inverses native.

Native graphics DMA/register removal: implementation and validation details are in
[`NATIVE_GRAPHICS.md`](NATIVE_GRAPHICS.md). The source uses BozoEngine `4a8f70a`, including native graphics completion
and the current shared native port services. The published v0.4.0 package remains the earlier release described above.

The player runtime's two camera matrix products now use typed native records without VU context state.
[`NATIVE_MATH.md`](NATIVE_MATH.md) records exact helper/default-run comparisons, the traced HUD position.W
RAM difference, and the vehicle/hierarchy/inline VU work that remains.

The six inline HUD point/direction blocks also use native records; [`NATIVE_HUD.md`](NATIVE_HUD.md)
records the original-block comparisons, destination-W contract and targeting-warning coverage limit.

[`NATIVE_IMAGES.md`](NATIVE_IMAGES.md) records the shared image-builder retirement contributed by DC,
the TMB resident-binding audit, exact draw/sound comparisons and movie/loading/gameplay qualification.

[`NATIVE_VEHICLE.md`](NATIVE_VEHICLE.md) covers native vehicle spatial calculations and the audited skid
direction, exact full-frame records, original-caller tests and the remaining multi-step coverage limit.

[`NATIVE_SUSPENSION_TIRE.md`](NATIVE_SUSPENSION_TIRE.md) adds the suspension ride-height
and tire-height transform leaves, with complete generated scalar consumers and tire entry calls.

[`NATIVE_PARTICLE_EMISSION.md`](NATIVE_PARTICLE_EMISSION.md) adds the emitted-direction and
attached/relative emitter transforms, with real particle metadata consumers and complete emitter-coordinate entry calls.

[`NATIVE_SHADOWS.md`](NATIVE_SHADOWS.md) adds vehicle shadow vertices and weapon shadow motion/height-sample
transforms, with complete cached-height vertex loops and actual height-query XYZ staging consumers.

[`NATIVE_TOW_TRUCK.md`](NATIVE_TOW_TRUCK.md) adds the tow-truck launcher's forward/inverse transforms,
with complete positioning, smoothing, normalization, orientation and material-publication consumers.

[`NATIVE_ATTRACTOR_GROUPS.md`](NATIVE_ATTRACTOR_GROUPS.md) adds group-selection, center and random-position
inverse transforms, with complete consumers, seeded generated producers and an audited peel-out W boundary.

[`NATIVE_PEEL_OUT.md`](NATIVE_PEEL_OUT.md) retires that peel-out forward transform,
with complete producer-to-pose checks and a fresh audit of the four opaque W fields.

[`NATIVE_CAMERA_TRANSFORMS.md`](NATIVE_CAMERA_TRANSFORMS.md) adds all eight camera-update
forward transforms, rear-view positioning and the coupled inline XYZW position publication.

[`NATIVE_WEATHER_VIEWPORT.md`](NATIVE_WEATHER_VIEWPORT.md) adds the weather viewport's
forward transform, checked through full entry, fade/fog updates, grid alignment and projection.

[`NATIVE_WEAPON_SKID_TRANSFORMS.md`](NATIVE_WEAPON_SKID_TRANSFORMS.md) adds weapon translation
and skid-mark creation/stretch offsets, with complete positioning, list and vertex consumers.

[`NATIVE_COLLISION_DRIFT_INVERSES.md`](NATIVE_COLLISION_DRIFT_INVERSES.md) adds sixteen vehicle
collision and drift inverse returns, with full XYZW checks and explicit generated-consumer boundaries.

[`NATIVE_RIGID_BODY_INVERSES.md`](NATIVE_RIGID_BODY_INVERSES.md) adds seven rigid-body inverse
returns, with complete impulse/collision entries and point-velocity consumers.

[`NATIVE_RIGID_BODY_FORWARD.md`](NATIVE_RIGID_BODY_FORWARD.md) adds ten rigid-body forward
returns with complete force, torque, impulse and integration entries plus actual
point-velocity consumers and audited destination-W lifetimes.

[`NATIVE_DRIVER_FORWARD.md`](NATIVE_DRIVER_FORWARD.md) adds seven driver path-offset,
lookahead and driving-LOS forwards. Coverage is 113/269 known forward returns
(156 remain), with 134/134 known inverse returns native. Complete generated owners
retain actual collision queries, scalar state consumers and meaningful inverse W.

[`NATIVE_FINAL_INVERSES.md`](NATIVE_FINAL_INVERSES.md) completes native coverage of all 134 known
inverse-helper returns, with force-field/health-stand entries, HUD/radar consumers and
particle visibility/sorting checks. Unknown and historical ABI support remains.

[`NATIVE_VEHICLE_LIGHT_INVERSES.md`](NATIVE_VEHICLE_LIGHT_INVERSES.md) adds six tire, rollover,
weapon-damage and object-light inverse returns, with complete generated owner entries
and real scalar state-publication consumers.

[`NATIVE_DRIVER_BOSS_INVERSES.md`](NATIVE_DRIVER_BOSS_INVERSES.md) adds five driver/boss inverse
returns and their coupled boss forward publication, with complete/bounded generated consumers
and gameplay/loading/movie qualification.

[`NATIVE_AI_INVERSES.md`](NATIVE_AI_INVERSES.md) adds five AI inverse returns,
with steering, attack-range and shot-policy consumers plus real attack-state publication.

[`NATIVE_HIERARCHY.md`](NATIVE_HIERARCHY.md) covers native collision/height hierarchy sphere tests, audited
inverse transforms and BSP plane distances, their original-caller tests and RAM boundary.

[`NATIVE_DIRECTIONS.md`](NATIVE_DIRECTIONS.md) adds three hot BSP collision direction callers to the
native record path, with generated-continuation tests, complete gameplay comparisons and stack-W audit.

[`NATIVE_DIRECTION_CONSUMERS.md`](NATIVE_DIRECTION_CONSUMERS.md) extends that scope to vehicle collision
points, tire-height corrections and light nodes, including the scalar clamps and normalization consumers.

[`NATIVE_PARTICLE_CONTINUATIONS.md`](NATIVE_PARTICLE_CONTINUATIONS.md) records the timing-sensitive
flare fallback found during that qualification and its native dispatch fix across scheduler resumes.

[`NATIVE_BSP_INVERSES.md`](NATIVE_BSP_INVERSES.md) retires the paired BSP and vehicle inverse leaves,
checks untouched VU state through their continuations, and traces the bounded contact-position W
RAM difference through the remaining terrain copy chain.

[`NATIVE_TERRAIN_CONTACTS.md`](NATIVE_TERRAIN_CONTACTS.md) replaces the 27 inline terrain, node,
point-contact and ray-result vector blocks with native XYZ calculations, with generated-block and
scalar-continuation tests and the retained fourth-component RAM boundary.

[`NATIVE_SPHERE_LOD.md`](NATIVE_SPHERE_LOD.md) adds the three sphere/LOD difference blocks and six
collision hierarchy helper returns, with complete generated sphere/LOD callers, scalar consumer
sequences and qualified gameplay/loading/movie routes. It records the remaining raw-RAM audit limit.

[`NATIVE_HIERARCHY_HELPERS.md`](NATIVE_HIERARCHY_HELPERS.md) adds twelve segment-grid, control-node,
height/BSP and line-of-sight helper returns. It records generated consumer tails, historical ABI
checks, live-route coverage and the directly traced height-BSP temporary W difference.

[`NATIVE_HIERARCHY_QUERY_RESULTS.md`](NATIVE_HIERARCHY_QUERY_RESULTS.md) adds twelve point/line,
terrain, auxiliary BSP, animated-node and lighting/attachment returns, with generated packing,
normalization and colour-consumer checks and a paired line-result W trace.

[`NATIVE_BSP_CROSSINGS.md`](NATIVE_BSP_CROSSINGS.md) adds thirteen BSP point/segment-crossing
returns, including actual point emission, duplicate/capacity checks and comparison of meaningful
inverse W when it replaces an earlier forward record.

[`NATIVE_HIERARCHY_TRAVERSAL_SETUP.md`](NATIVE_HIERARCHY_TRAVERSAL_SETUP.md) adds thirteen database
traversal, older collision-node/terrain and animated matrix-stack setup returns, with real scalar
consumer boundaries and historical ABI checks.

[`NATIVE_LEGACY_BSP_HELPERS.md`](NATIVE_LEGACY_BSP_HELPERS.md) adds fifteen older BSP setup/result
and hierarchy root/grid/traversal returns, with generated normalization, segment construction,
packing and matrix-stack publication checks.

[`NATIVE_POINT_RAY_HELPERS.md`](NATIVE_POINT_RAY_HELPERS.md) adds eleven coordinate-system/root
and point-contact/ray result transforms, with complete scalar result tails and historical ABI
checks.

[`NATIVE_TERRAIN_INPUT_HELPERS.md`](NATIVE_TERRAIN_INPUT_HELPERS.md) adds eleven terrain-node,
height-query and material-query inverse returns, with actual height scans, aggregate bounds
publication and integer-grid consumer checks.

[`NATIVE_TERRAIN_QUERY_HELPERS.md`](NATIVE_TERRAIN_QUERY_HELPERS.md) adds ten terrain LOS,
cell-collision and ray inverse returns, with translated endpoint pairs and real result
normalization/publication consumers.

[`NATIVE_CAR_PARTICLE_HELPERS.md`](NATIVE_CAR_PARTICLE_HELPERS.md) adds the fourteen older
car-debug and hierarchy particle forward returns, with actual debug-line vertex records,
view lookup and scalar position consumers.

[`NATIVE_TERRAIN_FINAL_HELPERS.md`](NATIVE_TERRAIN_FINAL_HELPERS.md) adds eight terrain debug,
edit and wave inverse returns, with actual vertex records, grid/input consumers and wave
type/bounds dispatch. It closes this narrow hierarchy/terrain two-transform-helper inventory.

[`NATIVE_SCENE.md`](NATIVE_SCENE.md) covers the subsequent renderer hierarchy producer/consumer replacement:
native scene/frame/light records, zero VU0 program calls in both qualified routes, packet-buffer-only RAM
differences at the checked gameplay snapshot, and movie/loading/1P/4P validation. Other macro math and shader
parameter packing remained at that step; the published release has not been updated by these source changes.

[`NATIVE_SHADERS.md`](NATIVE_SHADERS.md) covers removal of the mesh shader's `VuImage` and point-light packing,
named native parameters and retained state, exact complete 1P/4P recordings, and a disc-free numerical suite
added to Windows/Linux CI. Other macro math, scalar packet builders and legacy extents remain.

[`NATIVE_PROJECTION.md`](NATIVE_PROJECTION.md) covers the next audited screen/weather projection and
identity boundaries, original helper/generated-caller checks, live comparison counts, and the remaining
peeling-out W-padding chain behind the raw-RAM differences.

[`NATIVE_OBJECT_PACKETS.md`](NATIVE_OBJECT_PACKETS.md) covers retirement of the per-object shader/header
output and RGB packet readbacks. [`NATIVE_TEXTURE_INPUTS.md`](NATIVE_TEXTURE_INPUTS.md) covers the subsequent
removal of its last three texture copies and the consumer's packet reads/patches. Game allocation/usage
bookkeeping and the separate upload-link builder still execute; no packet shadow or binding is added.

[`NATIVE_TEXTURE_UPLOADS.md`](NATIVE_TEXTURE_UPLOADS.md) covers the subsequent native upload-link leaf,
preserved CPU/cursor ABI, original fallback contracts and bounded gameplay/movie validation. It emits
no upload tag at the audited calls; game texture-manager bookkeeping and other packet helpers remain.

[`NATIVE_GRAPHICS_SERVICES.md`](NATIVE_GRAPHICS_SERVICES.md) records removal of the engine GS device/GIF
reader state and TMB's old packet-based flare adapter, independent/combined tests and unsupported readback limits.

- One submodule: `BozoEngine`. No PS2Recomp gitlink, legacy CMake branch, import fallback, IOP recompilation or old runner scripts.
- `tools/tmb_build.py`: verify → extract → configure → native tools → recompile → packs → compile → stage.
- `scripts/build_runtime.sh` and the other retained shortcuts use that same pipeline.
- Native frame pacing is unconditional. A legacy `TMB_NATIVE_FRAMES=0` environment setting cannot bypass it.
- Heat haze defaults on, as requested. `BOZO_HEAT_HAZE=0` is the comparison switch.
- Existing cards/settings keep their format and `%APPDATA%\TMB_Recomp` location. Agent runs must set a scratch `PS2X_USER_DIR`.

## Validation recorded in this continuation

- Engine: all 5 local CTest checks pass, including the corrected FMAC overflow case and its new boundary cases.
- Whole-function replay: **9,232/9,232 samples, 1,518/1,518 functions**, bit-exact.
- Real-time menu/movie diagnostic: EE/PC **1.0001**, **576,005 ticks/s**, **59.97 fields/s**; movies **29.93–30.00 pictures/s**.
  The owner also said the freshly launched build no longer felt too fast. This measurement used the handoff build's existing clock fixes;
  the continuation adds a regression guard and removes the unpaced legacy frame switch.
- Native story run without a call log: **26,000 frames**, stage progression, card files written, no fault.
- Boot pinned: **MATCH**, 3,000/3,000 state frames; draws 2,999/3,000 (the established last-record exception).
- 1P pinned: **MATCH**, 16,000/16,000 state frames; draws 15,999/16,000 (the same last-record exception).
- Stage 6 pinned: the old frame-7105 interrupt mismatch is resolved. **11,000/11,000 state and draw frames match with recorded IOP-time sound replies**.
  Two events retain an ancestor pc in the reference; the checker accepts these only with an active inner call, matching function/caller,
  and an **exact cycle and checkpoint count**, and reports them separately. It does not shift their timing.
- Release-tool regressions: 12 tests pass, including accelerated menus/movies, migration layout, retired source cleanup,
  junction boundaries, configure retry and archive-pipe completion. No game data is needed by those tests.
- Engine and TMB Windows/Linux CI pass. The native package workflow's test run
  [42121468079](https://github.com/TheBozos/TMB_Recomp/actions/runs/42121468079) also passed, including its bundled-toolchain smoke check.

### Packaged Windows build and updates

- Candidate `0.4.0-test.20261105.3`, code `b9c42cb`, engine `9361960`: both player ZIPs pass the archive audit and SHA-256 checks.
  All **7,750 files** have identical contents between the two installation layouts. The corresponding-source ZIP's hash passes too.
- Built and played from `D:\BozoScratch\finish\Jérôme Jeux\TMB_Recomp_Bozo`, using only the bundled kit.
  The final packaged launcher stopped the build at 40%, resumed it, staged the native executable and launched it.
  The complete preparation took 126 seconds on this development PC; this is not a clean-OS performance promise.
- The launcher self-test completed normal Play/shutdown (exit 0), English/French settings screenshots,
  and an intentional child crash with a usable report. Failure paths now fail the self-test instead of returning a false success.
- Final packaged menu/movie log: **1.0000** EE/PC, **575,964 ticks/s**, **59.97 fields/s**, movies **29.94–30.00 pictures/s**.
  `tools/check_timing.py` passes with `TMB_NATIVE_FRAMES=0`. The fresh 2,400-frame accent-path run also passes.
- The **actual v0.3.2 updater** installed the unsuffixed migration ZIP. The subsequent new build removed the retired
  `source/PS2Recomp` marker, preserved player-data sentinels, rebuilt, and started BozoEngine.
  The new updater separately installed its `-bozo.zip` payload into an old-named `TMB_Recomp` folder and preserved its settings marker.
- This candidate-validation stage preceded publication; those test candidates were not published releases.
  The final v0.4.0 delivery is recorded at the top of this document.

### Final gameplay and profiling checks (2026-11-06)

All runs use isolated user folders, the USA disc, engine `9361960` and the packaged build's game code.
These figures measure **game-thread work**, not the interval between displayed pictures:

| Scene | Measured frames | Mean work | Mean p95 | Maximum work | Over 16.7 ms |
|---|---:|---:|---:|---:|---:|
| 1P, bots active, after 125 s | 4,458 | 3.77 ms | 4.57 ms | 8.2 ms | 0 |
| Actual four-player split-screen, after 100 s | 3,136 | 4.51 ms | 5.40 ms | 12.5 ms | 0 |
| Effects-heavy story level 20, after 125 s | 4,098 | 2.72 ms | 3.54 ms | 11.2 ms | 0 |

- 1P and level 20 each completed 12,000 frames; four-player completed 9,000. Gameplay intervals hold about 60
  updates/presents per second. Whole-run averages including loading are 59.7–59.9 updates/s. Occasional presentation
  intervals over 20 ms still occur; do not describe this as zero hitches on all hardware.
- Repeated the handoff's exact 7,500-frame four-pad command, including captures at 6,000 and 7,000. Its old script
  actually stays on opponent selection in this real-time build. The previous 42-present/s dip did not recur on that
  same screen: frames 7,000–7,300 remained at 60 presents/s.
- For the real split-screen run, omitted player 1's initial `1292:CROSS/18` from the local `p1_frames.txt` input;
  the native build has already reached the main menu there, so that old intro-skip press selects ONE PLAYER.
  Kept all other four-pad events unchanged. Captures at 6,000 and 8,000 show all four views, driving and firing.
  AI suppression was off; the four-human deathmatch itself has four human vehicles. Bot coverage comes from 1P.
- One further historical 1P timing pair (125–195 s) measured 3.77 ms native versus 5.49 ms on the previous runtime,
  with no game-work frames over budget in either. This is a limited local comparison, not a cross-hardware speedup claim.
- Completed the previously unwritten raw-profile output and fixed `hitch.py`'s imported `__file__` namespace in the local
  profiling tools. A 20-second sample contains 20,475 raw thread samples. Its game-thread pacer segmentation labels
  100 of 1,129 segments over 16.7 ms, but work samples are similar in the two groups (2.1 versus 1.8 per segment).
  The largest gaps line up with the profiler's 0.5-second thread enumeration; they cannot be counted as actual missed
  game frames. The game's own statistics in that interval show 4.51 ms mean, 8.9 ms maximum, zero over budget.
  Mesh submission and VU work remain the main CPU buckets. Mixer worker/COP2 inlining/GL batching remain optional
  optimization opportunities; none was introduced without a demonstrated frame-budget problem.
- Restarted for 6,000 headless frames with a copy of the card written by the native story run. It reached gameplay
  without a fault; the four existing card files remain intact. This is restart/readback coverage, not a full-campaign-ending claim.
- The new updater also passed in the accented `TMB_Recomp_Bozo` layout, **running from its own bundled Python**.
  It safely set aside loaded Python DLLs, installed the payload, preserved the player-data marker, and the updated Python starts.

The source migration and README review are merged through [PR #3](https://github.com/TheBozos/TMB_Recomp/pull/3).
The retired fork checkout, Git metadata and both linked worktrees are preserved independently at
`D:\TMB_Retired\2026-11-05`; they are no longer an active submodule or local submodule configuration.

## Limits of the old reference comparisons

- Native 989SND uses its own timing. Default pinned story/4P/stage runs can differ on opaque play handles or the instant a sound ends.
  Recorded sound replies let the remaining native game state/draws be checked. Those replies are a checker option and never used by the player build.
- The old pinned `save` recording still differs inside the test-only kill-enemies cheat's copied-context guest call at frame 5458.
  The native campaign run does proceed through the kills, stage transitions and real card writes. Do not describe the old pinned save recording as MATCH.
- Audio sample hashes are diagnostic, not an enforced equality claim; the existing handoff documents the few-sample timing differences.
- A fresh standard-user Windows OS, SmartScreen/Mark of the Web, physical high-DPI displays and additional human/controller acceptance need real hardware testing.

## Packaging and updates

Both player ZIPs carry **the same BozoEngine build**:

| File | Root folder | Purpose |
|---|---|---|
| `TMB_Recomp-<version>-bozo.zip` | `TMB_Recomp_Bozo/` | Separate/new installation |
| `TMB_Recomp-<version>.zip` | `TMB_Recomp/` | Existing launcher's in-place Update |

The older ZIP name does not retain the older engine. The existing updater can install the migration ZIP;
the new launcher subsequently uses `-bozo.zip` updates regardless of its installation folder name.
The corresponding-source ZIP and every SHA-256 accompany a published release.

Requirements: Windows 10 version 1903 or newer / Windows 11, **AVX2 + FMA**, OpenGL 3.3, about 10 GB free. Only the USA disc is supported.
Follow [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md). Do not publish a release tag merely because a test artifact builds.

## Working rules

- README follows the shared The Bozos project format: preserve its banner/badges, disc notice/hash, status screenshots,
  milestone/native-layer tables, goal, architecture, requirements, build/run, controls/cheats, disc facts, layout and credits.
  Updating stale facts is not permission to replace that presentation with a shorter standalone README.
- Clean-room rules in `BozoEngine/docs/PLAN.md` still apply: do not read the old fork, its generated code or emulator source when writing engine code.
- Push engine changes before the TMB pin; use reviewed branches/PRs and green CI. End commits with a `Co-Authored-By` line.
- Do not modify game/disc files or commit derived content. Never modify the owner's real user folder during agent runs.
- Keep scratch/builds on D:. Never edit a script while a job runs it, send input to other windows, or kill another project's processes.
- The shared benchmark mechanism is for timing runs only; builds and functional checks do not take it. Do not announce its state.

## Local evidence

Continuation scratch: `D:\BozoScratch\finish`. Source worktrees: `D:\TMB_bozo` and `D:\BozoEngine`.
Logs include `diag.log`, `ctest-final.log`, `replay/report.txt`, `pin-boot.log`, `pin-match1p.log`, `pin-stage6.log`,
`save-free.log` and the deliberately retained failing `save-fixes.log` comparison.
Final evidence: `final-launcher-shots/selftest.log`, `final-launcher-user/logs/TMB_Recomp.1.log`, `migration3.log`,
`native-update3.log`, `future-update3.log`, `scene-1p.log`, `four-player.log`, `scene-stage.log`, `hitch-report.txt`,
`scene-hitch.samples` (original), `scene-hitch-symbols.samples` (same samples with the Unicode module path resolved
through a hard link to the same executable), and `save-readback.log`.
