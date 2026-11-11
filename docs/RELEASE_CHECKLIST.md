# Release checklist: BozoEngine-only TMB

Use a scratch `PS2X_USER_DIR` for every automated run. Keep game data and generated code out of Git and release ZIPs.
Publication is separate from a successful build; this checklist records what was actually verified.

v0.6.0 was published on 2026-11-11 after tagged CI and the owner's release request. Its draft assets were checked
locally before publication:
- both player ZIPs pass the SHA-256 and package audits, carry an identical 7,974-file payload, and are stamped with
  the release commit and engine `a36a4d2`. Real-ESRGAN, its licences and the enhanced-texture tools are in the kit;
- a fresh install in a path with spaces and accents was built offline from the disc with only the bundled kit. The
  build was stopped and resumed, then the game played and exited normally, with menu/movie timing passing;
- on that install, the launcher's Enhanced textures dumped and upscaled 2,181 replacements in 4 min 18 s. An earlier
  draft failed here because the dump listed some textures more than once; it was rebuilt with the fix (#104);
- the v0.5.0 install was updated by its own updater, rebuilt and played; the v0.3.2 updater accepted the migration
  ZIP. Player data was kept in both update cases;
- with the bench lock, the pack at 4x held 59.1 to 59.5 minimum and 59.9 average game fps in 1P, three story arenas
  and 4P, the same as with it off (one RTX 5070 Ti).

v0.5.0 was published on 2026-11-09 after tagged CI and the owner's release request. Its draft assets were checked
locally before publication:
- both player ZIPs pass the SHA-256 and package audits, carry an identical 7,889-file payload, and are stamped with
  the release commit and engine `4a8f70a`;
- a fresh install in a path with spaces and accents was built offline from the disc with only the bundled kit. The
  build was stopped and resumed, then the game played and exited normally, with menu/movie timing passing;
- the published v0.4.0 install was updated by its own updater, rebuilt and played;
- the v0.3.2 updater accepted the migration ZIP. Player data was kept in both update cases.

v0.4.0 was published on 2026-11-06 after tagged CI, asset/hash audits and the owner's release request.
The released portable package was updated, rebuilt and played locally with a scratch user folder. Unchecked items
remain documented limitations; publication does not turn them into passing checks. See [HANDOFF.md](HANDOFF.md).

## Source and CI

- [x] TMB's only runtime submodule is `BozoEngine`, pinned to a public, reviewed commit.
- [x] No PS2Recomp gitlink, CMake target, Python import fallback, IOP recompiler or legacy runner script remains.
- [x] Engine Windows/Linux CI and TMB release-tool Windows/Linux CI pass.
- [x] Local engine instruction/runtime/library/audio tests and whole-function replay pass.
- [x] Pinned boot and 1P match agree on RAM, scratchpad, calls, threads and draws (the established final-frame exception is recorded).
- [x] Stage 6 passes its previous interrupt point. Report native sound-model differences separately from runs using recorded IOP-time replies.
- [ ] Full native story completion. Card writes and a 6,000-frame restart/readback pass; the 26,000-frame story progression does not cover the campaign ending.

## Timing and rendering

- [x] Menus: timer ticks and fields advance at their real-time rates, independently of presentation FPS.
- [x] Movies: measure decoded picture cadence and run `tools/check_timing.py` on the log.
- [ ] Human watch/listen acceptance of a full intro and story movie.
- [x] Native clock pacing stays active even with an old `TMB_NATIVE_FRAMES=0` environment setting.
- [x] Heat haze defaults on; `BOZO_HEAT_HAZE=0` restores the earlier port's rendering for comparisons.
- [x] 1P with bots, actual four-player selection/split-screen, and the effects-heavy stage hold the 60 Hz target; timing logs are retained.
- [x] Recheck the previous four-player present dip around frames 7000–7300 with captures at 6000 and 7000 (same opponent-selection screen, 60 presents/s).
- [ ] F2 Controls/Video/Cheats, mouse/keyboard launcher navigation and fullscreen switching remain usable.

## Packages

- [x] `TMB_Recomp-<version>-bozo.zip` contains one `TMB_Recomp_Bozo/` folder.
- [x] `TMB_Recomp-<version>.zip` contains the identical BozoEngine payload under `TMB_Recomp/` for the old launcher's Update path.
- [x] Both player ZIPs pass the package audit and their SHA-256 checks. Neither contains PS2Recomp or game-derived files.
- [x] The corresponding-source ZIP and licences accompany a published release.
- [x] README and release notes state Windows 10/11, AVX2 + FMA, OpenGL 3.3 and approximately 10 GB free.
- [x] Extract the player ZIP, then build from the player's USA disc with only its bundled kit, offline.
- [x] Test a path with spaces/non-ASCII, cancellation/resume, Play, and restart.
- [ ] Human F2 controls/video/cheats and fullscreen switching on the final package.
- [x] Test the migration ZIP through the previous updater; source files removed by this release disappear, saves/settings survive, rebuild starts the BozoEngine executable.
- [x] Test future updates of both folder layouts with the BozoEngine updater, including replacement of its own bundled Python DLLs.

## Clean-PC and player acceptance

- [ ] On a fresh standard-user Windows installation: SmartScreen/Mark of the Web, Defender, no MSYS2/Python/git/VC++ redist installed.
- [ ] 150%/200% display scaling and controller interaction checked on real hardware.
- [ ] Existing saves read correctly; movies, menus and matches feel right to the owner.
- [x] Record any remaining limitations explicitly in release notes before publication.

The development PC can verify an isolated source/build-kit flow, but cannot prove the fresh-OS, SmartScreen or real high-DPI checks by itself.
