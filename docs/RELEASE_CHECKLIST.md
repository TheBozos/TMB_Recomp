# Release checklist: a clean Windows PC before each release

Run `tools/make_release.py` (README, "The release"), then check the zip on a Windows 10 or 11 PC that has never seen
the project: a fresh VM or a spare PC, nothing installed (no MSYS2, git, Python, Visual C++ redistributables), a
standard user account, Defender on. Tick each line, write what differed in "Notes", and keep the filled copy with the
release (or in the release's issue).

Release: `TMB_Recomp-<version>.zip`, SHA-256 `<from the .sha256>`, commit `<commit>`.

## Download and unzip

- [ ] The release page's SHA-256 equals `Get-FileHash .\TMB_Recomp-<version>.zip` (and `certutil -hashfile <zip>
      SHA256`).
- [ ] Unzip with Explorer's "Extract all" (the zip carries the Mark of the Web from the browser).
- [ ] SmartScreen: double-click `TMB Launcher.exe`: "Windows protected your PC", **More info**, **Run anyway**; it starts.
      (Once per exe; the downloaded zip's files carry the mark, the game built on the PC does not.)
- [ ] Defender: no detection while unzipping, at the launcher's start, during the build (it scans the compiler's
      output; note how much it slows the compile: Task Manager, "Antimalware Service Executable"), at Play.
- [ ] A path with spaces and non-ASCII characters: `C:\Users\Jérôme\Jeux\TMB_Recomp-<version>\` (a user name with an
      accent puts the user folder `%APPDATA%\TMB_Recomp` under it too).
- [ ] Not in `C:\Program Files`: unzip there, start the launcher: preparing fails to write (the README says not to);
      note the message the player gets.

## The disc

- [ ] The USA ISO on another drive than the release (or on a USB disk): accepted; `game\disc.iso` is a copy (3.8 GB
      more on the release's drive; the launcher asks for about 10 GB free), not a hard link.
- [ ] The USA ISO on the same drive: `game\disc.iso` a hard link (no extra space).
- [ ] A `.bin` + `.cue` (MODE2/2352): accepted, converted to `game\disc.iso` (a copy).
- [ ] The European disc: refused, "This is the European disc. Only the USA disc (SCUS-97101) is supported."
- [ ] Another PS2 game, a non-disc file: refused with a plain message.

## Preparing

- [ ] No internet (cable out / VM network off) from the launcher's start to Play: the build needs nothing from the
      network; the update check stays silent.
- [ ] Timing on a 4-core / 8 GB PC (and on the 8-core reference): each step's time from `build\logs` or the launcher,
      the total "Prepared in". The PC stays usable meanwhile.
- [ ] Stop at the compile, close the launcher (it asks first), start it again: "Continue preparing" resumes; "Prepared
      in" is the sum of both runs.
- [ ] Free space: the launcher warns under 10 GB.

## Playing

- [ ] Play: the boot movies, the main menu, a match; the launcher hides and comes back when the game closes.
- [ ] F2 settings in the game; Settings' window mode / size in the launcher used at the next start.
- [ ] A crash (`PS2X_TEST_CRASH=25`, or `--selftest ... --crash-seconds 25`): the launcher offers the crash report.
- [ ] High DPI: Windows' scaling at 150 % and 200 % (Settings, Display): the launcher readable, nothing clipped on
      each tab; the game's window and F2 panel usable.

## Updating and removing

- [ ] With an older release published: "Version X is available" on the Play tab, "Open the release page" opens it.
- [ ] Settings, "Check for updates at start: off": no request at start (a proxy log, or offline: nothing changes).
- [ ] Unzip the new release over the old one: the Play tab says the new version is installed and offers **Rebuild
      now**; after it, the notice is gone, the memory cards (`%APPDATA%\TMB_Recomp\mc0`, `mc1`) and settings are
      unchanged, the game plays.
- [ ] Uninstall: delete the release folder, then `%APPDATA%\TMB_Recomp`: nothing else was written (no registry, no
      Start menu entry, no other folder: check `%LOCALAPPDATA%`, `%TEMP%`; `%ProgramData%\TMB_Recomp\paths` empty or
      absent: the junctions of a non-ASCII path are removed after each build).

## Before publishing

- [ ] `RELEASE_NOTES.md` finished (what changed); the version tag on the release commit, pushed with it.
- [ ] `make_release.py --with-sources`: the sources zip and its `.sha256` attached to the same release (the GPL /
      LGPL corresponding source of the kit's MSYS2 binaries).
- [ ] `make_release.py --publish` (a draft), the assets and notes checked on GitHub, then published by the owner.

## Results on the development PC

2026-11-01, Windows 10 Home 22H2 (16 threads, 1920 x 1080 at 100 %), not a clean PC: what could be checked here.
Release `TMB_Recomp-0.1.0-test.zip` (commit ac63080, fork 1466e9d, SHA-256
`8fc2cb3713f171d9493646587c83c21944221868d5e719da0410bfa0aaaeb214`), then `0.1.1-test` (commit 29179f4) over it.

| Check | Result |
|---|---|
| SHA-256 | `Get-FileHash` equals the `.sha256` file |
| Unzip | PowerShell's `Expand-Archive` (no Mark of the Web: SmartScreen not exercised) into `D:\tmb_rel_test\Jérôme Jeux\` (spaces and accents), 96 s |
| Non-ASCII path with spaces | **Failed three times, fixed**: ps2_recomp could not open the ELF (UTF-8 manifest, fork), windres's preprocessor (a space in the path: bare `gcc`), ld (junctions with plain names). Then: built and played from there |
| European disc | Refused: "This is the European disc. Only the USA disc (SCUS-97101) is supported." (launcher screenshot; `tmb_build --check` exit 1) |
| `.bin` + `.cue` | A MODE2/2352 image made from the ISO (4.36 GB) in `D:\tmb_rel_test\bin cue Jérôme\`: `--check` OK; extract 30.9 s, `disc.iso` a copy (1 link) byte-identical to the ISO. The copy path is the one an ISO on another drive takes; no second drive here (C: has 12 GB free) |
| ISO on the same drive | `game\disc.iso` a hard link |
| Preparing, Stop / resume | From an empty folder 186.6 s (extract 3.4, recompile 5.2, packs 33.4, iop 2.8, configure 5.3, compile 20 + 114.4 after a stop at 40 %, stage 0.2); "Prepared in 3:07", the sum of both runs |
| 4-core / 8 GB | Not available here (16 threads) |
| No internet | Not unplugged. The update check pointed at a closed port: silent, no notice; the real API (no release published yet, 404): silent. The build's network use is none by construction (FetchContent fully disconnected, the kit's sources) |
| Defender | On, no detection while unzipping, building or playing; scan time not measured |
| Play | Boot movies and the main menu (window captures), `GS scale 1` from video.ini, closed with code 0, the launcher back |
| Crash report | `PS2X_TEST_CRASH=25`: exit 0xC0000005, the report offered |
| High DPI | No 150 % / 200 % display here: `--ui-scale 1.5` and `2`. The layout did not fit a 1920 x 1080 screen at 150 % (1680 x 1140): the scale is now lowered to fit (1.32), every tab readable, nothing clipped; the Maintenance rows were made fixed-height (six rows overflowed the card at 1x). A real high-DPI monitor is still to check |
| Update notice | A local server answering as the API with `v0.2.0`: "Version 0.2.0 is available", the link `.../releases/tag/v0.2.0`, "update available" in the footer and Maintenance |
| Rebuild after an update | 0.1.1-test unzipped over 0.1.0-test (`-Force`): "Version 0.1.1-test is installed", Rebuild now; 31.8 s (recompile, packs, iop, configure again; nothing to compile); `built_from.ini` updated; the memory card file and video.ini byte-identical before and after; played again |
| Uninstall | Not run here; the launcher writes only the release folder and the user folder (and, for a non-ASCII path, junctions under `%ProgramData%\TMB_Recomp\paths` removed after each build) |
| Program Files | Not run (needs administrator rights to unzip there) |
