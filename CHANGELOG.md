# Changelog

What changed in each release of TMB_Recomp, the PC port of Twisted Metal: Black (USA) by The Bozos. Newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/). A release is made from the section with its number: write the changes
under "Unreleased" as they land, then move them under the new version's heading when releasing.

## [Unreleased]

### Added

- **Enhanced textures in the launcher** (Settings): one button makes the AI texture pack on your PC from your own disc
  (about 15 minutes with a recent graphics card, 700 MB), with progress, Stop and Remove, and an on/off switch there and
  in F2 → Video. Real-ESRGAN now comes in the build kit (its licences in `kit\licenses\realesrgan`); nothing of the game
  is downloaded or shipped.
- **AI texture packs from your own disc:** `tools/tmb_texture_upscale.py` turns a texture dump into Real-ESRGAN
  replacements, and the game draws them from `texture_pack` in the user folder. Loading never holds up a frame; 1P
  and 4P keep 60 fps with a pack and 4x upscaling.
- `tools/tmb_texture_dump_all.py` dumps every arena and car from your own disc in headless runs (2,261 textures),
  and the dump now covers every object of the loaded levels, not only what the camera saw.
- **Texture upscaling** (off by default): the game's textures can be drawn from 2x or 4x copies with an
  edge-directed or smooth filter. Set it in F2's Video tab, or with `texture_upscale` / `texture_filter` in
  `video.ini`. It is BozoEngine's renderer option, from the Soulcalibur III port; replays are unchanged by it.
- `TMB_TEXTURE_DUMP=1|<folder>` writes each texture the renderer receives as a PNG named by its identity, for
  texture-pack work from your own disc ([`docs/TEXTURE_UPSCALING.md`](docs/TEXTURE_UPSCALING.md)).

### Changed

- Texture upscaling no longer drops frames while an arena's copies arrive (it fell to about 31 fps for some 20 s at
  4x); larger copies are sent to the GPU at most 12 MB a frame.
- BozoEngine is updated to `a36a4d2`. Native callbacks now run on the game's own thread stack instead of a fixed
  area at the top of RAM. TMB declares where its code uses the stack pointer as data (`hierTraverseAsm`,
  `sysSetSp`/`sysRestoreSp`), so a callback taken there still finds the real stack. One-player and four-player
  replays match the previous build on every frame's draws, scratchpad, calls, threads and timing. Sound differs on
  four menu frames only, with equal sample counts.

### Fixed

- The launcher's Settings no longer reset the game's frame rate and texture upscaling in `video.ini` when they saved
  the window.

## [0.5.0] - 2026-11-09

### Changed

- **The game's 3D math runs natively.** Every inventoried coordinate transform (269 forward and 134 inverse uses)
  now runs as native C++. So do the hierarchy, collision, terrain, particle, camera, weapon and HUD calculations
  built on them; none of them goes through the recompiled vector-unit helper any more. Generated tests compare
  every one against the original arithmetic. Recorded one-player and four-player replays match the previous build
  on every frame's calls, draws, sound and timing. The only differences found were in unused scratch memory.
- More of the graphics path is native: HUD and menu image builders, mesh shader parameters, screen and weather
  projection, texture inputs and upload completion. The game no longer emits per-object shader packets.
- BozoEngine is updated to `4a8f70a`.

### Fixed

- **Rare hang when leaving a menu movie** (seen once on vehicle select): a movie could freeze after a long frame, and
  the next menu choice then waited for its decoder forever. The movie player's vblank handler now runs natively.

### Developer changes

- The native-math caller suite covers about 191,000 generated sequences (9,952 whole-memory comparisons). They span
  four rounding modes, denormal flushing, RAM aliases and arbitrary W payloads, with full-owner entries for each
  call site.
- `docs/HANDOFF.md` links one qualification document per batch: test coverage, replay comparison and window runs.

### Known limits

- Only the USA disc is supported. Windows is the validated player target; the migrated Linux player build is not yet validated.
- Full campaign endings, a fresh standard-user Windows installation and four physical controllers still need player/hardware acceptance.
  Widescreen is not implemented; presentation above 60 Hz remains experimental for effects that update at game rate.

## [0.4.0] - 2026-11-06

### Changed

- **BozoEngine is now the only recompiler and runtime.** The PS2Recomp submodule, legacy build targets and IOP
  recompilation steps are removed. TMB and BozoEngine are MIT; release packages retain every third-party licence.
- The launcher uses SDL3 and Dear ImGui; the game uses BozoEngine's native window, input, renderer, sound and movie
  services. Existing memory-card files and settings remain usable.
- Two installation ZIPs contain the same BozoEngine build: `-bozo.zip` installs beside the previous port, while the
  ZIP without that suffix preserves the old launcher's in-place update path. Both use BozoEngine.

### Fixed

- **Original heat haze enabled by default** around fire and explosions. `BOZO_HEAT_HAZE=0` disables it for comparisons
  with the previous PC build.
- Menu and movie pacing always uses the native clock. An old `TMB_NATIVE_FRAMES=0` setting can no longer select an
  unpaced path. Timing checks now measure menu ticks and movie picture cadence independently of the FPS counter.
- Match-start sound and lens-flare presentation stalls are fixed in the pinned BozoEngine revision.
- Building from folders with spaces and accents now keeps GCC's startup objects and static libraries on the
  safe path and disables unused C++ module scanning. In-place updates remove the retired runtime's source folder
  without following directory links. Launcher self-tests fail when preparing or starting the game fails.
- Native Windows executables use UTF-8 file paths. A changed toolchain invalidates stale configure checks before
  retrying, and release source exports drain tar padding so they cannot hang waiting for `git archive`.

### Developer changes

- All build/run shortcuts use the disc-to-game BozoEngine pipeline. Release audits reject the retired runtime.
- Added Windows/Linux CI and regression checks for timing, package contents and the migration ZIP layout.

### Known limits

- Only the USA disc is supported. Windows is the validated player target; the migrated Linux player build is not yet validated.
- Full campaign endings, a fresh standard-user Windows installation and four physical controllers still need player/hardware acceptance.
  Widescreen is not implemented; presentation above 60 Hz remains experimental for effects that update at game rate.
- Exact native sound-model/sample equality and the old test-only save-cheat replay remain reference-test limitations.
  Native story progression, card writes, restart/readback and actual four-player play pass; see `docs/HANDOFF.md` for the evidence.

## [0.3.2] - 2026-11-02

### Added

- **Story: start at** (F2, Cheats): a new story can begin at any of its stages, Midtown Suburbs to Warhawk's Rooftop,
  instead of the Junkyard, then goes on from there as usual. Handy to try the later stages without playing through.

### Fixed

- **Black Vista Highway** (story stage 3) had no road: most of the level's ground and walls were not drawn. Some
  pieces of three Challenge arenas (DEATH1, DEATH2, DEATH5) were missing for the same reason. "Rebuild now" after
  updating fixes them.
- **Movie sound volume**: the movies' sound now plays at the volume the game sets for it (it used a default
  volume before; a fix from the Valkyrie Profile 2 port, which shares the sound code).

### Known limits

- Story stages past the first are reachable and load correctly, but most have not been played to the end yet.
- No widescreen yet.
- Only the USA disc (SCUS-97101) is supported.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".

## [0.3.1] - 2026-11-02

### Changed

- **Faster**: the game now needs about a third less processor time per frame than in 0.3.0 (in our tests a busy
  match went from about 7.5 to 5 milliseconds of work per frame, out of the 16.7 a frame lasts at 60 frames per
  second). Older or slower PCs that dropped below 60 in big fights or in split screen should hold it much better.
  Nothing changes in how the game looks or plays.

### Known limits

- Story levels 2 to 8 are not checked to the end yet.
- No widescreen yet.
- Only the USA disc (SCUS-97101) is supported.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".

## [0.3.0] - 2026-11-02

### Added

- **The launcher updates the game itself**: when a new version is out, "Update" on the Play tab downloads it, checks
  it, installs it over this one and restarts the launcher; then "Rebuild now" prepares the game again (only what
  changed). Your saves and settings stay. Unzipping a new release by hand still works too.

### Changed

- **Smoother frame times**: the game does less work each frame (about 2 ms less per frame in our tests), so slower PCs
  drop fewer frames in busy fights. The lens flares no longer read the whole picture back from the graphics card
  each frame.

### Known limits

- Players on 0.1.0 or 0.2.0 update to this version by hand one last time (unzip it over the old one, then Rebuild);
  from 0.3.0 on, the launcher's Update button does it.
- Story levels 2 to 8 are not checked to the end yet.
- No widescreen yet.
- Only the USA disc (SCUS-97101) is supported.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".

## [0.2.0] - 2026-11-02

### Added

- **Smoother picture on high-refresh monitors, experimental** (F2, Video, "Frame rate": "Match the monitor"): on a 120, 144, 165
  or 240 Hz monitor the window shows that many frames a second, the ones between the game's 60 drawn in between (the
  cars, the camera, the scenery and the smoke and sparks move smoothly; the HUD stays as the game draws it). The game
  itself still runs at 60, as it always did, and this costs one game frame (about 17 ms) more input lag. Off by
  default: "60 (the game's)" stays exactly as before. Experimental: a few effects (rain, snow, lens flares, some
  trails) still move at 60 against the smoother rest.

### Changed

- **New icon** for the launcher and the game window: The Bozos' clown, with its head on fire.
- The project moved to The Bozos' GitHub organisation (github.com/TheBozos/TMB_Recomp). The launcher's update check
  and links follow; the 0.1.0 launcher still finds new releases through GitHub's redirect.

### Known limits

- Story levels 2 to 8 are not checked to the end yet.
- No widescreen yet.
- Only the USA disc (SCUS-97101) is supported.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".

## [0.1.0] - 2026-11-01

The first release. You build the game on your own PC from your own USA disc; nothing of the game is in the download.

### Added

- **The launcher** (TMB Launcher.exe, by The Bozos): pick your disc image, "Prepare the game" (a few minutes, once,
  and it can be stopped and resumed), then Play. It refuses the European disc, other games and damaged copies with a
  message saying so, and it tells you when a newer release is out. Everything it needs to build the game (compiler,
  tools) is in the download: nothing else to install.
- **The whole game runs as a native PC program** at its own 60 frames per second, drawn at twice the PS2's resolution
  by default (the render scale goes higher). No PS2 emulation in the normal mode: the 3D, effects, HUD, menus, movies
  and sound are made by PC code from the game's own data.
- **Sound and music**, including the movies' sound.
- **Every Challenge arena** has been played.
- **Split screen for 2 to 4 players**, each on their own controller.
- **Memory card saves**: save, load and overwrite, in `%APPDATA%\TMB_Recomp` (kept when you update).
- **Settings window (F2 in the game)**: controls (every button and stick rebindable to keys, controllers or the
  mouse), video (resolution, windowed / full screen / borderless, render scale) and cheats (infinite health, ammo and
  energy, no AI, weak enemies, and "Unlock everything": every level, vehicle and movie).

### Known limits

- Story mode: level 1 of 8 checked to the end; levels 2 to 8 and the endings are not fully checked yet. Please report
  what you find on the [issues page](https://github.com/TheBozos/TMB_Recomp/issues).
- No widescreen yet, and no frame rate above the game's own 60.
- Only the USA disc (SCUS-97101) is supported; the European disc is not.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".
- Technical: built from TMB_Recomp's code and the PS2Recomp fork (`native/renderer`) with the build kit's GCC 16.2,
  FFmpeg 9.0.2 and Python 3.14; the default mode has no GS, VU or SPU2 emulation and no IOP hardware model.

[Unreleased]: https://github.com/TheBozos/TMB_Recomp/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/TheBozos/TMB_Recomp/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/TheBozos/TMB_Recomp/compare/v0.3.2...v0.4.0
[0.3.2]: https://github.com/TheBozos/TMB_Recomp/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/TheBozos/TMB_Recomp/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/TheBozos/TMB_Recomp/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/TheBozos/TMB_Recomp/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/TheBozos/TMB_Recomp/releases/tag/v0.1.0
