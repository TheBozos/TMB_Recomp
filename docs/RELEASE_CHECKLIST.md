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
      Start menu entry, no other folder: check `%LOCALAPPDATA%`, `%TEMP%`).

## Before publishing

- [ ] `RELEASE_NOTES.md` finished (what changed); the version tag on the release commit, pushed with it.
- [ ] `make_release.py --with-sources`: the sources zip and its `.sha256` attached to the same release (the GPL /
      LGPL corresponding source of the kit's MSYS2 binaries).
- [ ] `make_release.py --publish` (a draft), the assets and notes checked on GitHub, then published by the owner.

## Results on the development PC

RESULTS_PLACEHOLDER
