# Release checklist: BozoEngine-only TMB

Use a scratch `PS2X_USER_DIR` for every automated run. Keep game data and generated code out of Git and release ZIPs.
Publication is separate from a successful build; this checklist records what was actually verified.

## Source and CI

- [x] TMB's only runtime submodule is `BozoEngine`, pinned to a public, reviewed commit.
- [x] No PS2Recomp gitlink, CMake target, Python import fallback, IOP recompiler or legacy runner script remains.
- [x] Engine Windows/Linux CI and TMB release-tool Windows/Linux CI pass.
- [x] Local engine instruction/runtime/library/audio tests and whole-function replay pass.
- [x] Pinned boot and 1P match agree on RAM, scratchpad, calls, threads and draws (the established final-frame exception is recorded).
- [x] Stage 6 passes its previous interrupt point. Report native sound-model differences separately from runs using recorded IOP-time replies.
- [ ] Native story completion, card writes and restart/readback work without a reference log.

## Timing and rendering

- [x] Menus: timer ticks and fields advance at their real-time rates, independently of presentation FPS.
- [ ] Movies: measure decoded picture cadence, then watch/listen to an intro and a story movie. Run `tools/check_timing.py` on the log.
- [x] Native clock pacing stays active even with an old `TMB_NATIVE_FRAMES=0` environment setting.
- [x] Heat haze defaults on; `BOZO_HEAT_HAZE=0` restores the earlier port's rendering for comparisons.
- [ ] 1P, four-player selection/split-screen, bots and the effects-heavy stage hold the 60 Hz target; keep timing logs.
- [x] Recheck the previous four-player present dip around frames 7000–7300 with captures at 6000 and 7000 (same opponent-selection screen, 60 presents/s).
- [ ] F2 Controls/Video/Cheats, mouse/keyboard launcher navigation and fullscreen switching remain usable.

## Packages

- [x] `TMB_Recomp-<version>-bozo.zip` contains one `TMB_Recomp_Bozo/` folder.
- [x] `TMB_Recomp-<version>.zip` contains the identical BozoEngine payload under `TMB_Recomp/` for the old launcher's Update path.
- [x] Both player ZIPs pass the package audit and their SHA-256 checks. Neither contains PS2Recomp or game-derived files.
- [ ] The corresponding-source ZIP and licences accompany a published release.
- [x] README and release notes state Windows 10/11, AVX2 + FMA, OpenGL 3.3 and approximately 10 GB free.
- [x] Extract the player ZIP, then build from the player's USA disc with only its bundled kit, offline.
- [ ] Test a path with spaces/non-ASCII, cancellation/resume, Play, F2, and restart.
- [x] Test the migration ZIP through the previous updater; source files removed by this release disappear, saves/settings survive, rebuild starts the BozoEngine executable.
- [ ] Test future updates of both folder layouts with the BozoEngine updater.

## Clean-PC and player acceptance

- [ ] On a fresh standard-user Windows installation: SmartScreen/Mark of the Web, Defender, no MSYS2/Python/git/VC++ redist installed.
- [ ] 150%/200% display scaling and controller interaction checked on real hardware.
- [ ] Existing saves read correctly; movies, menus and matches feel right to the owner.
- [ ] Record any remaining limitations explicitly in release notes before publication.

The development PC can verify an isolated source/build-kit flow, but cannot prove the fresh-OS, SmartScreen or real high-DPI checks by itself.
