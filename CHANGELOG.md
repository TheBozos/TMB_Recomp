# Changelog

What changed in each release of TMB_Recomp, the PC port of Twisted Metal: Black (USA) by The Bozos. Newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/). A release is made from the section with its number: write the changes
under "Unreleased" as they land, then move them under the new version's heading when releasing.

## [Unreleased]

### Added

- **Smoother picture on high-refresh monitors** (F2, Video, "Frame rate": "Match the monitor"): on a 120, 144, 165
  or 240 Hz monitor the window shows that many frames a second, the ones between the game's 60 drawn in between (the
  cars, the camera, the scenery and the smoke and sparks move smoothly; the HUD stays as the game draws it). The game
  itself still runs at 60, as it always did, and this costs one game frame (about 17 ms) more input lag. Off by
  default: "60 (the game's)" stays exactly as before.

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
  what you find on the [issues page](https://github.com/boubou666/TMB_Recomp/issues).
- No widescreen yet, and no frame rate above the game's own 60.
- Only the USA disc (SCUS-97101) is supported; the European disc is not.
- The launcher and the game are not signed: Windows SmartScreen asks for "More info", then "Run anyway".
- Technical: built from TMB_Recomp's code and the PS2Recomp fork (`native/renderer`) with the build kit's GCC 16.2,
  FFmpeg 9.0.2 and Python 3.14; the default mode has no GS, VU or SPU2 emulation and no IOP hardware model.

[Unreleased]: https://github.com/boubou666/TMB_Recomp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/boubou666/TMB_Recomp/releases/tag/v0.1.0
