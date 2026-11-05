<div align="center">

<img src="docs/branding/tmb_banner.png" alt="Twisted Metal: Black, native PC port by The Bozos" width="100%">

# TMB_Recomp

**A native Linux and Windows port of *Twisted Metal: Black* (USA `SCUS-97101`), made by statically recompiling the game's PlayStation 2 code to C++ on BozoEngine.**<br>
The game's own MIPS R5900 code runs as a PC program. Its meshes, textures, menus, effects, sound and movies use native
PC services. BozoEngine replaces the PS2Recomp fork completely; the player build has no IOP, SPU2, GS, VIF or VU interpreter.

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-1b8cff)
![Compiler](https://img.shields.io/badge/compiler-GCC%20%7C%20Clang-1b8cff)
![Based on](https://img.shields.io/badge/based%20on-BozoEngine-17a35b)
![Status](https://img.shields.io/badge/status-playable%2C%20full%20speed%2C%20native-17a35b)

</div>

> [!IMPORTANT]
> No game files are included: no ELF, IRX modules, data archives, or code generated from them.
> You need **your own copy** of the game: the USA release, PS2 ID **`SCUS-97101`**. The port is built against its
> executable:
>
> | File | SHA-1 |
> |---|---|
> | `SCUS_971.01` | `3ff17f4897552e355d0bb8c3abfa5125fe3ff2ef` |

## For players (Windows)

You build the game yourself, on your PC, from **your own disc**: nothing of the game is in the download.

**Download status:** [v0.3.2](https://github.com/TheBozos/TMB_Recomp/releases/tag/v0.3.2) is still the published release
and uses the previous runtime. This repository now builds on BozoEngine only. The native migration's candidate ZIPs
are available as artifacts from the successful [Release workflow test run](https://github.com/TheBozos/TMB_Recomp/actions/runs/42121468079).
The steps below describe those native packages and the forthcoming release.

1. Download `TMB_Recomp-<version>-bozo.zip` and its `.sha256` for a new/separate installation. Check its SHA-256 before
   running anything (`Get-FileHash .\TMB_Recomp-<version>-bozo.zip` in PowerShell).
2. Unzip it anywhere with about 10 GB free (into `D:\Games`, say: the ZIP holds a `TMB_Recomp_Bozo` folder), but not in
   `Program Files`. Keep its folders together: `TMB Launcher.exe`, `source\` (our code), `kit\` (the compiler and tools),
   `README.txt` (these steps, English and French), `CHANGELOG.md`, `LICENSES\`.
3. Double-click **TMB Launcher.exe**. It is not signed, so Windows may show *"Windows protected your PC"*:
   click **More info**, then **Run anyway**.
4. Click **Select your disc image…** and pick your USA disc image: a `.iso`, or a `.bin` with its `.cue`.
   The European disc and other games are refused with an explanation.
5. Click **Prepare the game**. The included tools build it locally, without downloads. It takes a few minutes the
   first time; you can stop it and resume where it left off.
6. Click **Play**. **F2** opens controls, video and cheats. Memory cards, settings and logs live in
   `%APPDATA%\TMB_Recomp` (the launcher's Maintenance tab opens it).

Requires **Windows 10 version 1903 or newer / Windows 11, an AVX2 and FMA capable CPU, and OpenGL 3.3**.
Original heat haze around fire and explosions is enabled by default. `BOZO_HEAT_HAZE=0` disables it for comparisons
with the older PC port. Menus and movies use their native timing, independently of the presentation frame rate.

For an existing v0.3.2 installation, use its **Update** button or extract the unsuffixed `TMB_Recomp-<version>.zip`
over its `TMB_Recomp/` folder. That migration ZIP contains **the same BozoEngine build**, not the old runtime.
After an update, **Rebuild now** prepares the new code; removed release files are cleaned up, while saves stay.
Subsequent native updates use `-bozo.zip` regardless of the installation folder's name.

The launcher checks for updates at start unless disabled in Settings. Update verifies the ZIP's SHA-256, installs
the new source/tools and restarts. Manual extraction over the matching folder works too. Delete the installation
folder to uninstall; delete `%APPDATA%\TMB_Recomp` separately to remove saves/settings. The two layouts and previous
release share that user folder by default; use a scratch `PS2X_USER_DIR` for comparisons.

## Status

Picking the work up elsewhere: [`docs/HANDOFF.md`](docs/HANDOFF.md) (current state, measurements and remaining checks).

**Playable, at full speed, as a native port targeting NTSC's 60 updates per second.** The BozoEngine build boots,
plays movies with sound, runs menus and level loading, and plays one-player, story and four-player split-screen
matches: driving, weapons, bots, HUD, music and effects. Native card writes and a restart with those cards pass.
The latest measured gameplay holds about 60 updates/presents per second on the development PC; game-thread work
averages 3.8 ms in 1P, 4.5 ms in four-player and 2.7 ms on the effects-heavy stage 20. These are local measurements,
not hardware-independent performance guarantees.

Menu/movie checks measure a 1.0000× game clock, about 576,000 menu timer ticks/s and 29.94–30.00 decoded movie
pictures/s. The legacy `TMB_NATIVE_FRAMES=0` setting can no longer bypass native pacing. The previous match-start
sound freeze and lens-flare presentation stall are fixed. Occasional shorter presentation hitches remain.

Still being checked: full story endings, every arena on the new runtime, some effects against the original picture,
and fresh-OS / physical-controller acceptance. The older pinned save-cheat recording and exact native sound-model
equality remain diagnostic limitations; the player build never uses recorded sound replies. The handoff distinguishes
those comparisons from actual native play. [docs/RECOMP_NOTES.md](docs/RECOMP_NOTES.md) retains the port's bring-up history.

<p align="center">
  <img src="docs/screenshots/match_explosion.jpg" width="49%" alt="An explosion in front of the player's truck in Junkyard, its sparks drawn natively">
  <img src="docs/screenshots/match_night.jpg" width="49%" alt="The player's truck at night in the rain, its weapon glowing, the HUD at twice the PS2's resolution">
  <img src="docs/screenshots/intro_movie.jpg" width="49%" alt="The intro movie">
  <img src="docs/screenshots/story_menu.jpg" width="49%" alt="The game-mode menu">
</p>
<p align="center"><sub>The USA native PC port at twice the PS2's resolution. These repository screenshots predate the BozoEngine migration; current validation is in docs/HANDOFF.md.</sub>
</p>

| Milestone | |
|---|:---:|
| Map the USA ELF's own symbols and recompile its functions to C++ | ✅ |
| Link the generated code with BozoEngine; remove the PS2Recomp submodule and build dependency | ✅ |
| Boot: native scheduler, Sony SDK services and reads from the owner's disc | ✅ |
| USA release (SCUS-97101, NTSC 60 Hz) | ✅ |
| Controller input; buttons and stick directions rebindable to keys, controllers and mouse (F2, `input.ini`) | ✅ |
| Movies (PSS, MPEG-2 decoded with FFmpeg) and movie sound | ✅ |
| Title screen, menus, level select and loading | ✅ |
| Native memory-card writes and restart/readback (`mc0`, `mc1` in the user folder) | ✅ |
| Matches: HUD, cars, textured 3D, driving, weapons and bots | ✅ |
| Deaths, respawns, game over and replay | ✅ |
| Story progression and card writes without a reference recording | ✅ |
| Full speed: native rendering and frame pacing on the PC's clock | ✅ |
| Native 989SND driver/mixer; music, effects and movie audio | ✅ |
| Game geometry/effects drawn from the game's own data; no GS or VIF/GIF decoder in play | ✅ |
| VU work translated to native C++; no VU interpreter or sandbox | ✅ |
| Original heat haze enabled by default | ✅ |
| Multi-player split-screen, 2 to 4 players; frame-keyed scripts for unattended checks | ✅ |
| Settings (F2): controls, video, cheats; windowed / fullscreen / borderless | ✅ |
| Higher internal resolution (2× by default; Video's render scale) | ✅ |
| Interpolated presentation above 60 Hz; rain, snow, flares and some trails still update at game rate | 🧪 |
| Every Challenge arena rechecked on BozoEngine | 🔧 |
| Every story level to the endings | 🔧 |
| Fresh standard-user Windows installation and four physical controllers | 🔧 |
| Linux player build validated on the new runtime | ⏳ |
| Widescreen | ⏳ |

**Native port**: the shared goal of The Bozos' ports is to draw and play sound directly from the game's own data,
with PC timing and services. In this project BozoEngine now supplies the runtime, without the PS2Recomp fork.

| Layer | |
|---|:---:|
| EE functions statically recompiled to C++, with native kernel/library calls | ✅ |
| IOP-facing services handled by native drivers; no IOP interpreter or recompiled IRX dependency in the player | ✅ |
| 989SND sound banks played by the native mixer; no SPU2 emulator | ✅ |
| Frame loop and sound on the PC's clock | ✅ |
| OpenGL renderer on the GPU; no paraLLEl-GS or GS renderer | ✅ |
| No GS memory, GIF packet decoder or VIF front end in the player build | ✅ |
| VU0 calculations and VU1 shading/effects implemented as native code | ✅ |
| Textures and meshes converted offline into native packs | ✅ |
| 3D objects drawn at the game's hooks, using its state and order | ✅ |
| Particles, lens flares, rain and other effects drawn from the game's lists | ✅ |
| Font, HUD, overlays, loading and movies drawn natively | ✅ |
| Existing memory-card and settings file formats retained | ✅ |

✅ works · 🧪 experimental · 🔧 in progress · ⏳ not reached yet

[docs/HANDOFF.md](docs/HANDOFF.md) records current evidence and limits. [docs/RECOMP_NOTES.md](docs/RECOMP_NOTES.md)
records the earlier bring-up and fixes; its historical fork diagnostics are not build instructions for this tree.

## Goal

A fully playable *Twisted Metal: Black* as a true PC program, from the intro to the endings: no PS2 hardware simulated
where the PC can do the job itself. Accuracy first; then widescreen and frame rates above the PS2's.

## How it works

The game's R5900 functions become C++ and run on the host CPU. BozoEngine supplies native scheduling, library
services, rendering, input and sound; TMB's hooks supply the game-specific data and behavior.

| Part of the game | On the PS2 | In the port |
|---|---|---|
| Frame timing | vblank fields and hardware timers | native frame pacing and field counts from the PC clock; presentation can interpolate between game updates |
| Game code (EE, MIPS R5900) | the EE CPU | `bozo-recomp` generates C++ and an address-indexed function table from the owner's ELF |
| Kernel and Sony libraries | EE software and IOP services | BozoEngine's native scheduler and library implementations |
| Sound driver and mixing | 989SND, LIBSD and SPU2 | native 989SND bank/voice handling and mixer; no IOP or SPU2 emulator |
| 3D meshes | VU1 programs, then GS draws | offline native mesh packs, submitted at the game's per-object hooks in its own order (`runtime/tmb_meshes.cpp`) |
| Shading, skinning and clipping | VU calculations | native C++ calculations and OpenGL rendering |
| Particles, rain, flares, trails and heat haze | VU1 / GS effects | the game's particle/flare lists and native rendering (`runtime/tmb_particles.cpp`); heat haze defaults on |
| Textures | GS texture memory | offline packs keyed by the game's texture IDs (`tools/tmb_texture_pack.py`, `runtime/tmb_textures.cpp`) |
| Font, HUD, overlays and loading | game packet builders, GS | native builders reading the game's variables (`runtime/tmb_2d.cpp`) |
| Movies | IPU / GS plus sound stream | FFmpeg decoding, native picture presentation and audio |
| Controllers and cards | libpad / libmc, IOP devices | SDL3 input and native card files, preserving the port's settings/save formats |

```text
Your USA disc
    ├─ SCUS_971.01 ── bozo-recomp ── generated C++ ─┐
    └─ textures/models ── native asset packs ────────┤
                                                   ▼
                         TMB's native hooks + BozoEngine
                         OpenGL · SDL3 · native mixer · FFmpeg
                                                   │
                                                tmb.exe
```

The executable retains its symbols, so this port does not need a Ghidra function-map pass.
`runtime/tmb_overrides.cpp` contains game-specific bindings; `bozo/` contains the real-time host, headless host and
checker. Generic recompiler/runtime work lives in the [BozoEngine](https://github.com/TheBozos/BozoEngine) submodule.
Its [clean-room specifications and validation](https://github.com/TheBozos/BozoEngine/tree/main/docs) describe the engine.
PS2Recomp remains credited in the project's history and available in the v0.3.2 release, but is no longer a dependency.

## Requirements

The player package brings its own build tools. To build from the repository:

- Windows 10 version 1903 or newer / Windows 11; Windows is the validated player target. Linux portability is not yet
  validated for this migration.
- An **AVX2 and FMA capable CPU**, OpenGL 3.3, about 10 GB free for preparation, and your own USA disc.
- CMake 3.21+, Ninja, Python 3 and a C++20 compiler. The matching kit's GCC is the validated Windows configuration;
  MSVC is not supported for this port.
- SDL3 and Dear ImGui (fetched at pinned hashes, or supplied by the kit), plus FFmpeg development libraries
  (`libavcodec`, `libavutil`; `BOZO_FFMPEG_ROOT` for the static kit).
- Optional zlib for BozoEngine's recording/checking tools. Player builds use `BOZO_BUILD_TOOLS=OFF`.

## Build and run

### One command, from your disc (Windows)

Clone the repository with its BozoEngine submodule, then use the same pipeline as the launcher:

```bat
git clone --recurse-submodules https://github.com/TheBozos/TMB_Recomp.git
cd TMB_Recomp
python tools\tmb_build.py --disc "D:\Discs\Twisted Metal - Black (USA).iso" --out D:\TMB-build --kit D:\tmb_kit_bozo\kit
"D:\TMB-build\TMB\Play TMB.cmd"
```

- The pipeline is `verify → extract → configure → tools → recompile → packs → compile → stage`. It checks the USA ELF's
  SHA-1 and handles ISO or BIN/CUE without a separate disc-extraction program.
- The staged folder is `<out>/TMB`: `tmb.exe`, `SCUS_971.01`, the native packs, `disc.iso` and `Play TMB.cmd`.
  A same-drive ISO is hard-linked; another drive or a raw BIN/CUE is copied/converted. The original image is never modified.
- `--game <dir>` stages elsewhere; `--check` only verifies the disc. `--stop-after <step>` stops at a step;
  `--redo <step>` repeats it and the steps after it. Finished steps with matching inputs are skipped.
- `--toolchain <dir>` chooses the compiler/tools; `--engine <dir>` selects another BozoEngine tree. `--no-kit`
  uses installed tools instead of an automatically discovered kit. `--fetch-cache <dir>` supplies cached SDL3/ImGui sources.
- Python tools use the standard library. The build reports `PROGRESS`, `PERCENT`, `SKIP`, `DONE`, `ERROR` and `RESULT`
  lines to the launcher; detailed step output lives in `<out>/logs/`.
- Spaces and accented paths are supported. On Windows the builder uses temporary ASCII junctions for compiler paths
  and reports real paths back to the launcher. A changed toolchain clears stale configure checks before retrying.

### The build kit: nothing installed (Windows)

Use the **matching BozoEngine kit (v2)**. The published v0.3.2 kit contains the earlier runtime's tools and is incompatible.
The package includes its kit; maintainers can assemble one from the pinned source/tool versions:

```bat
python tools\make_build_kit.py --out D:\tmb_kit_bozo\kit --zip D:\tmb_kit_bozo\kit.zip
rem with this repository in TMB_Recomp and the kit next to it:
kit\python\python.exe -I -X utf8 TMB_Recomp\tools\tmb_build.py --disc "D:\Discs\Twisted Metal - Black (USA).iso" --out D:\TMB-build --kit kit
```

| Kit folder | Contents |
|---|---|
| `ucrt64/` | pinned MSYS2 UCRT64 GCC, binutils, mingw-w64 headers/CRT, CMake, Ninja, and their required DLLs |
| `ffmpeg/` | pinned FFmpeg MPEG-2 decoding libraries, built static (LGPL) |
| `deps/` | pinned SDL3 and Dear ImGui sources, without `.git` |
| `python/` | pinned Windows embeddable Python |
| `sources/`, `licenses/`, `kit_manifest.json` | source archives, licences, component versions/origins/hashes |

The builder finds `kit/` in the installation or beside the source folder; `--kit` overrides that.
The validated packaged build works offline with only this kit, including an accented/spaced installation path.
The staged player is statically linked and needs no compiler DLLs beside it. The kit itself is build output, never
committed; release archives include third-party licences and the corresponding-source offer.

### The launcher (Windows)

`launcher/` is **TMB Launcher.exe**, a separate CMake project using SDL3, Dear ImGui and OpenGL. It is linked static
and uses only Windows system DLLs. English and French are supported.

```bat
cmake -S launcher -B D:\TMB-launcher-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build D:\TMB-launcher-build
rem offline: -DFETCHCONTENT_SOURCE_DIR_SDL3=<kit>/deps/sdl3-src
rem          -DFETCHCONTENT_SOURCE_DIR_IMGUI=<kit>/deps/imgui-src
rem          -DLAUNCHER_FONT_DIR=<folder containing Exo2[wght].ttf and OFL.txt>
```

- It expects the release layout: `TMB Launcher.exe`, `source/`, `kit/`. It creates `build/` for preparation and `game/`
  for the staged game. `--release <dir>` points at another installation.
- Preparation uses bundled Python and `source/tools/tmb_build.py`, in a Windows job object. Stop ends that build tree;
  the next preparation resumes it. Errors show the step's log and a **Copy details** button.
- Play starts `game/tmb.exe` with the staged ELF and disc, hides the launcher, then returns when the game exits.
  A crash report is offered if one appeared in the user folder.
- Window settings live in `video.ini`; disc choice, language, background motion and build state live in `launcher.ini`
  in the user folder. The launcher's Settings tab controls the starting window; F2 in the game controls the game settings.
- The team's RecompWatcher styling remains: PS2 BIOS blue, glowing boot towers, glass cards, Exo 2 (SIL OFL), and TMB's
  flaming-clown icon (`launcher/tmb.ico`, `runtime/app/tmb.ico`). No game art is used for the icon.
- Update checks use GitHub's latest-release API. The native updater fetches `TMB_Recomp-<version>-bozo.zip` and its
  SHA-256, verifies names/version, stages the full archive and installs it. Loaded files are set aside as `*.tmb-old`.
  The old launcher's unsuffixed download is provided separately as the migration ZIP. Both layouts retain their user data.
- `--selftest <iso> --shots <dir> [--wrong <image>] [--cancel] [--play-seconds s] [--crash-seconds s]` exercises the flow
  and records screenshots/logs; a failed build or game start fails the test. Always set a scratch `PS2X_USER_DIR`.
  `--screens`, `--ui-scale`, `--update-url`, `--download-prefix` and `--update-now` support UI/update test scenarios.

### The release (maintainers)

`tools/make_release.py` exports **committed files**, builds the launcher with the kit, audits the payload and writes hashes:

```bat
python tools\make_release.py --version <version> --kit D:\tmb_kit_bozo\kit --out D:\TMB-release --with-sources
python tools\make_release.py --verify-zip D:\TMB-release\TMB_Recomp-<version>-bozo.zip
```

| Output (in `--out`) | Contents |
|---|---|
| `TMB_Recomp-<version>-bozo.zip` | `TMB_Recomp_Bozo/`: launcher, TMB/BozoEngine source, matching kit, player README, changelog and licences |
| `TMB_Recomp-<version>.zip` | the identical BozoEngine payload under `TMB_Recomp/`, for the previous launcher's Update path |
| `<zip>.sha256` | SHA-256 in `sha256sum` format |
| `TMB_Recomp-<version>-bozo-sources.zip` | `--with-sources`: the kit's MSYS2 source packages and FFmpeg source, attached to a published release |
| `RELEASE_NOTES.md` | the version's changelog section, download/migration instructions, hashes, sizes, component versions and source offer |

The version comes from a tag or `--version`. A real release requires its `## [x.y.z]` changelog section;
`--allow-missing-changelog` is for test builds only. Source/export/archive audits reject the retired runtime and
game-derived files. `--publish` creates a **draft** with `gh`; `--dry-run` prints the command. `--publish-only` uses
existing output. The commit must already be on GitHub; the script never pushes code or publishes the draft itself.
Follow [docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md) and include the corresponding sources and licences.

### Making a release

Releases are built by `.github/workflows/release.yml` when the owner asks. An ordinary push does not publish one.

1. Move `CHANGELOG.md`'s Unreleased entries into `## [<version>] - <date>`, update its links, and merge through a PR
   with green checks. Engine changes must already be public before updating the submodule pin.
2. Tag that merged commit (`v<version>`) and push the tag, or run the workflow against the already pushed tag.
3. The workflow checks the changelog, assembles the pinned kit, makes both player ZIPs and corresponding sources,
   verifies hashes/contents and runs the bundled-toolchain/launcher checks, then creates a **draft** release.
4. Download and test the draft using the release checklist, including migration and fresh-Windows/player acceptance.
5. Publish the reviewed draft on GitHub. Publication can notify RecompWatcher when its dispatch secret is configured.

A manual workflow without a tag produces test artifacts, not a release. The kit's MSYS2 packages are pinned in
`tools/build_kit_packages.json`; `make_build_kit.py --install-packages` installs them. When upgrading the kit, update
the pins, rebuild and test before committing. No game files or generated game code belong in the archives.

### Step by step (development)

All retained shortcuts use the same BozoEngine pipeline:

```bash
export TMB_DISC="D:/Discs/Twisted Metal - Black (USA).iso"
export TMB_KIT="D:/tmb_kit_bozo/kit"
export TMB_BUILD_DIR="D:/TMB-build"
bash scripts/extract_iso.sh       # verify and extract the disc
bash scripts/build_tools.sh       # configure and build native tools
bash scripts/recomp.sh            # generate native C++
bash scripts/build_runtime.sh     # build and stage the native game
bash scripts/run_native.sh        # run it; run.sh uses the same native path
```

For an already extracted ELF and a development toolchain:

```bash
cmake -S . -B D:/TMB-dev -G Ninja \
  -DTMB_ELF="D:/TMB-build/disc/SCUS_971.01" \
  -DTMB_ISO="D:/Discs/Twisted Metal - Black (USA).iso" \
  -DBOZO_BUILD_TOOLS=OFF -DBOZO_FFMPEG_ROOT=D:/tmb_kit_bozo/kit/ffmpeg
cmake --build D:/TMB-dev -j 8
D:/TMB-dev/tmb.exe --iso "D:/Discs/Twisted Metal - Black (USA).iso"
```

`BOZO_ENGINE_DIR` selects another engine tree. `TMB_WINDOW=OFF` builds headless tools only.
`BOZO_BUILD_TOOLS=OFF` removes the optional zlib-dependent recording/checking tools from a player build.

### Windows

For development, use MSYS2 **UCRT64** with GCC, CMake, Ninja, Python and FFmpeg (or the matching kit).
MSVC is not a supported configuration. In a UCRT64 shell, put the compiler on PATH:

```bash
export PATH=/c/msys64/ucrt64/bin:/usr/bin:$PATH
```

Then follow the commands above. SDL3 and ImGui are fetched at pinned hashes when no kit/cache is supplied.
The player uses OpenGL 3.3; it does not require Vulkan or the former runtime's DLL-staging scripts.

### As a PC game (no scripts, no variables)

Double-click the launcher's **Play**, or the staged `Play TMB.cmd`. The game is `tmb.exe`; it always uses BozoEngine's
native renderer, sound and frame pacing. **F3** toggles the FPS counter (`PS2X_SHOW_FPS=1` enables it at launch).

- **Game data:** the staged ELF, native packs and `disc.iso` stay together. The launcher and `Play TMB.cmd` supply
  their paths; `tmb.exe --elf <path> --iso <path>` selects them explicitly.
- **Window:** TMB's flaming-clown icon, no Windows console window in a player build; redirected output still goes to the log.
- **User folder:** `%APPDATA%\TMB_Recomp` on Windows; `$XDG_DATA_HOME/tmb_recomp` (`~/.local/share/tmb_recomp`)
  is the source port's Linux location. `PS2X_USER_DIR=<dir>` isolates another installation/test.

  | What | Where in the user folder |
  |---|---|
  | Memory cards | `mc0/`, `mc1/` |
  | Controls and video | `input.ini`, `video.ini`, `imgui.ini` |
  | Launcher settings | `launcher.ini` |
  | Logs | `logs/TMB_Recomp.log`, previous runs as `.1.log`, `.2.log` |
  | Crash reports | `crashes/crash-<date>-<time>.txt` |

  Existing native card/settings formats and the user-folder identity are retained. Sharing that folder makes changes
  visible to both the old and new builds; use separate scratch user folders for comparisons.
- **Crashes:** Windows reports include the exception, module/address, stack and recent log. The launcher offers the
  report after the game exits. `PS2X_TEST_CRASH=<s>` and `PS2X_NO_DIALOGS=1` support isolated unattended checks.

Player 1 uses the keyboard and first controller by default; up to four players are rebindable in F2. Keyboard defaults
use physical US positions:

| PS2 button | Keyboard |
|---|---|
| D-pad / left stick | Arrow keys or W A S D |
| Cross / Circle / Square / Triangle | X or Space / C or Esc / Z or Numpad 0 / V or Numpad 1 |
| L1 / R1 / L2 / R2 | Q / E / Left Shift / Right Shift |
| Start / Select | Enter / Tab |
| L3 / R3 | F / G |
| Right stick | I J K L |

| Key | What it does |
|---|---|
| F2 | Settings: Controls (per-player buttons, sticks, triggers, mouse and sensitivity), Video (resolution, window mode, render scale and presentation frame rate), Cheats. Controls save to `input.ini`; video saves to `video.ini`. |
| F3 | Toggle the frame-rate counter. |
| F11, Alt+Enter | Switch between the window and the last fullscreen mode. |
| F8 | Capture consecutive presents (the sequence settings below). |
| F9 | Start/stop a capture burst, ten captures per second. |

The picture stays 4:3 inside the window. Video can present at game rate, a fixed higher rate, or the monitor's rate
with interpolation; the game itself keeps its 60 Hz timing. Interpolation adds about one game frame of latency and
remains experimental for effects that only update at game rate.

### Cheats

The Cheats tab of the settings window (F2) switches them while playing, each at once, in a match (one player or split
screen); the variables give their state at launch (`runtime/tmb_cheats.cpp`).

| Cheat | | Variable |
|---|---|---|
| Unlock everything | every level (Freeway, Prison Passage, Skyscrapers, Minion's Stadium...), every vehicle (Yellow Jacket, Axel, Warthog, Manslaughter, Minion...) and every movie, opened in the menus; switched off, put back; saving while it is on keeps them on the card | `TMB_UNLOCK_ALL=1` |
| Story: start at | a new story begins at the chosen level instead of the Junkyard (every stage: Midtown Suburbs or Expressway 104, Downtown or Black Vista Highway, Minion's Stadium, Prison Passage, Blackfield Asylum, Abandoned Skyscrapers, Warhawk's Rooftop; and the other arenas), then goes on as the game chains them; for testing the later stages without playing through | `TMB_STORY_LEVEL=<level number>` (the game's level table: 1 Junkyard, 0 Suburbs, 2 Expressway, 5 Downtown, 6 Black Vista, 14 Stadium, 3 Prison, 16 Asylum, 4 Skyscrapers, 13 their second part, 20 Warhawk's Rooftop) |
| Infinite health | per player: the car's health held at its maximum (the game's own, as its health bar shows it) | `TMB_PLAYER_GOD=<players>` |
| Infinite ammo | per player: pickup weapons and the special are not spent (the HUD's counts stay) | `TMB_INFINITE_AMMO=<players>` |
| Infinite energy | per player: the energy bar (freeze, mines, shield...) held full | `TMB_INFINITE_ENERGY=<players>` |
| No AI | the computer's cars do nothing | `TMB_NO_AI=1` |
| Weak enemies | the computer's cars held at most this much health (a slider, 1 to 300) | `TMB_ENEMY_HEALTH_CAP=<x>` |

`<players>`: digits 1 to 4 (`12`: players 1 and 2), any other value but `0` every player.

<details>
<summary><b>Runtime and diagnostic options</b></summary>

| Variable / argument | Effect |
|---|---|
| `PS2X_USER_DIR=<dir>` | Separate cards, settings, logs and crash reports; required for agent/unattended tests |
| `PS2X_INPUT_CONFIG=<file>`, `PS2X_VIDEO_CONFIG=<file>` | Other controls/video configuration files |
| `PS2X_WINDOW_SIZE=<w>x<h>`, `PS2X_WINDOW_MODE=windowed\|fullscreen\|borderless`, `PS2X_RENDER_SCALE=<x>` | Starting window mode, size and internal render scale, over `video.ini` |
| `PS2X_FRAME_RATE=game\|monitor\|<n>` | Presentation at game rate, monitor rate or a fixed rate; higher rates interpolate without speeding up the game |
| `PS2X_SHOW_FPS=1` | Show FPS at launch; F3 toggles it |
| `PS2X_FRAME_STATS=1` | Game-thread work measurements independently of the cap |
| `TMB_TRACE_CLOCK=1 BOZO_TIMING_LOG=1` | EE/PC ratio, menu timer/field rates and decoded movie cadence |
| `python tools/check_timing.py <log>` | Check those timing observations; missing data or sustained speed errors fail |
| `BOZO_HEAT_HAZE=0` | Disable default-on heat haze for comparison with the earlier PC port |
| `--pad-script [n=]<file>` | Frame-keyed scripted input for player n (1–4); repeat for multiple players |
| `--quit-after <frames>` | End an unattended windowed run |
| `--capture-dir <dir> --capture-frames n,n` | Capture chosen game frames |
| `PS2X_CAPTURE_WINDOW_DIR=<dir>` | F8/F9 capture destination |
| `PS2X_CAPTURE_SEQUENCE=<n>`, `PS2X_CAPTURE_SEQUENCE_AT=<s>[,<s>...]` | Consecutive presents at F8 or the chosen times, with interpolation timing metadata |
| `PS2X_CAPTURE_BURST_AT=<s>[,<s>...]` | Start/stop an F9-style burst at the chosen times |
| `PS2X_CAPTURE_DIR=<dir>`, `PS2X_CAPTURE_INTERVAL=<s>` | Periodic picture captures |
| `--audio-stats`, `--audio-latency <ms>` | Audio queue/underrun diagnostics and queue target |
| `--wav <file>`, `PS2X_AUDIO_RECORD=<file>` | Record the native mixer output |
| `--no-sound` | Disable device playback for a diagnostic run |
| `PS2X_SETTINGS_OPEN=Controls\|Video\|Cheats` | Open F2 directly on a tab for UI tests |
| `PS2X_LOG_FILE=<file>\|0`, `PS2X_NO_DIALOGS=1`, `PS2X_TEST_CRASH=<s>` | Another log/none, suppress unattended dialogs, or intentionally test crash handling |
| `TMB_CHEATS_LOG=1`, `TMB_CHEATS_TOGGLE=<s>` | Cheat diagnostics and periodic switching for tests |
| `TMB_KILL_ENEMIES=<s>`, `TMB_PLAYER_HEALTH_CAP=<x>` | Test-only story progression or player-health cap; see the handoff's save-cheat comparison limit |

The former GS/IOP/VIF interpreter switches are not alternate player engines in this tree. Native pacing remains
active even if an old environment contains `TMB_NATIVE_FRAMES=0`.

The native instruction, runtime, library and mixer tests run in UCRT64 with:

```bash
cmake -S BozoEngine -B D:/TMB-engine-tests -G Ninja -DBOZO_BUILD_TOOLS=OFF
cmake --build D:/TMB-engine-tests -j 8
ctest --test-dir D:/TMB-engine-tests --output-on-failure
python -m unittest discover -s tests -v
```

The checker compares native calls, RAM, scheduling and draws against local recordings; the headless host also runs
without a reference. Current results and limits are in [docs/HANDOFF.md](docs/HANDOFF.md). Reference recordings,
captures, generated game code and disc data are local-only and never release contents.

</details>

## Disc facts

| | |
|---|---|
| Boot ELF | `SCUS_971.01` (USA, May 2001), entry `0x100008`, `.text` at `0x100000` (1.2 MB), not stripped |
| Video mode | NTSC |
| IOP modules | `MOD/*.IRX`: 989SND, LGDEV, LIBSD, MCMAN, MCSERV, MTAPMAN, PADMAN, SIO2MAN, SNDBGET, USBD, USBKEYBD, USBMLOAD |
| IOP image | `IOPRP213.IMG` |
| Data | `LVL/`, `PCAR/`, `ACAR/`, `PTS/`, `RST/`, `SND/`, `VAG/`, `SHELL/` (per-language UI), `MOV/*.PSS` |

## Repository layout

| Path | Contents |
|---|---|
| `BozoEngine/` | the native recompiler/runtime submodule (MIT) |
| `bozo/` | TMB's windowed host, headless host and recorded-run checker |
| `cmake/bozo.cmake` | the native game build and generated-code integration |
| `recomp/` | game-specific recompiler declarations |
| `runtime/` | TMB's own drawing hooks, bindings and cheats |
| `scripts/` | shortcuts for the common disc-to-game pipeline and native run |
| `tools/` | disc verification/extraction, build and release tools, native pack converters and analysis helpers |
| `launcher/` | SDL3/ImGui launcher: disc selection, preparation, updates and Play |
| `tests/` | game-free release/build/timing regression checks |
| `docs/` | bring-up history, current handoff, release checklist and branding/screenshots |
| `.github/workflows/` | Windows/Linux tooling checks; tagged draft releases and manual test artifacts |
| `CHANGELOG.md`, `LICENSE` | release changes and TMB's MIT licence; third-party components retain their own licences |

Everything derived from the game (disc folders/images, generated C++, build output and reference captures) stays
local and git-ignored. No game executable, disc data or generated game code is distributed in player/source archives.

## Credits

- **The Bozos**: boubou666 and Arzaroth, who make this port and BozoEngine
  ([RecompWatcher](https://thebozos.github.io/RecompWatcher/) shows our projects).
- [BozoEngine](https://github.com/TheBozos/BozoEngine), The Bozos' MIT recompiler and native runtime, now the port's only engine.
- [PS2Recomp](https://github.com/ran-j/PS2Recomp) by ran-j and contributors (GPL-3.0), on which the earlier port was built;
  it remains credited in the project's history.
- The other recompilations that shared work with the earlier port: *Valkyrie Profile 2: Silmeria*, *Dark Cloud*,
  *Soulcalibur III*, *Drakengard*, *The Lord of the Rings: The Third Age*, *Jade Cocoon 2* and the other team projects.
- *Twisted Metal: Black* © Sony Interactive Entertainment, developed by Incognito Entertainment. This is an unofficial
  fan project, not affiliated with or endorsed by them.
