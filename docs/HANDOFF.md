# TMB on BozoEngine — 2026-11-05

The goal is to remove the PS2Recomp fork from the project. This source tree now builds **only on BozoEngine**.
The earlier v0.3.2 release remains in Git history and on GitHub for comparison; it is not a runtime dependency.

## Architecture and build

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
- These are test candidates, not a published release. No new tag has been pushed.

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

- Clean-room rules in `BozoEngine/docs/PLAN.md` still apply: do not read the old fork, its generated code or emulator source when writing engine code.
- Push engine changes before the TMB pin; use reviewed branches/PRs and green CI. End commits with a `Co-Authored-By` line.
- Do not modify game/disc files or commit derived content. Never modify the owner's real user folder during agent runs.
- Keep scratch/builds on D:. Never edit a script while a job runs it, send input to other windows, or kill another project's processes.
- The shared benchmark mechanism is for timing runs only; builds and functional checks do not take it. Do not announce its state.

## Local evidence

Continuation scratch: `D:\BozoScratch\finish`. Source worktrees: `D:\TMB_bozo` and `D:\BozoEngine`.
Logs include `diag.log`, `ctest-final.log`, `replay/report.txt`, `pin-boot.log`, `pin-match1p.log`, `pin-stage6.log`,
`save-free.log` and the deliberately retained failing `save-fixes.log` comparison.
