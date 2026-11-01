# Handoff (2026-11-01)

Where the work stands and how to pick it up on another machine. Details and history are in `RECOMP_NOTES.md`.

## Goal

The game smooth (its own 60 fps, the USA release only: the European one is dropped) and fully playable as a true PC
program: no PS2 hardware simulated where the PC can do the job itself. Locked runs on 2026-11-01: a 1P match
57.7-60.4 fps, 4P split screen 55.8-60.4. Earlier:

| Release | Path | fps mean | slowest tenth |
|---|---|---|---|
| USA (`SCUS-97101`, NTSC) | native (`scripts/run_native.sh`, 2x resolution) | 58.9 | 58.5 |

Windows, Ryzen 7 7800X3D, RTX 5070 Ti; scripted play through the menus into the first arena (`tools/play_script.py`).

## Setting up a new machine

1. Install the requirements in the README (CMake, Ninja, GCC 13+, FFmpeg dev libs, X11/Wayland + OpenGL headers,
   bsdtar, an AVX2 CPU). The native port draws with OpenGL 3.3. A Vulkan driver is only needed for the PS2 path's
   paraLLEl-GS (`scripts/run.sh`), fetched by CMake with its shaders precompiled.
2. `git clone --recurse-submodules https://github.com/boubou666/TMB_Recomp.git`. `PS2Recomp` is the fork
   `boubou666/PS2Recomp`; TMB pins a commit of its `native/renderer` branch (the native work, merged into `main` as it
   settles).
3. The game data is not in the repository (never commit it). Put your own ISO at the repo root, then follow "Build and
   run" in the README: `extract_iso.sh`, `build_tools.sh`, `recomp.sh`, `build_runtime.sh`, optionally
   `iop_recomp.sh` (the USA release: `game_us/`, `generated_us/`, `build/game_us`; the European disc's executable
   `SCES_503.60` in `game/` too, for the region map the hooks' addresses go through).
4. On Windows: MSYS2 UCRT64 with its GCC, as in the README's Windows section; `scripts/stage_dlls.sh` after a build
   to start the runner outside the MSYS2 shell. No `perf`: use the fork's sampling profiler (`PS2X_PROFILE=<s>`).
5. The first build compiles about 3500 generated files. A change to `ps2_runtime.h`, `ps2_memory.h` or
   `gs_frontend.h` rebuilds all of them.

## Current state

- **Playable.** Boot, movies (the USA release's story cinematics too), menus, loading, matches (driving, weapons, AI,
  sound, HUD, split screen), deaths, respawns, game over, saving and loading. Story mode: level 1 of 8 checked.
- **The native path** (`TMB_NATIVE=1`, `scripts/run_native.sh`):
  - frames on the PC's clock (`timer1HandlerNative`, the fork's `FramePacer`; `TMB_NATIVE_FRAMES=0` for the vblanks);
  - 3D from an offline mesh pack at the game's per-object hook, in the game's order, clipped to its own clip volume
    (`runtime/tmb_meshes.cpp`); effects from the particle lists (`runtime/tmb_particles.cpp`); 2D builders rewritten
    (`runtime/tmb_2d.cpp`); textures from an offline pack (`runtime/tmb_textures.cpp`); VU0 programs in C++
    (`runtime/tmb_vu0.cpp`); no VU1 or VU0 interpreter;
  - no GS front end: no GS object, memory or thread; the DMA kicks to GIF / VIF0 / VIF1 complete unread; the native
    renderer presents the frame at 2x (`PS2X_GS_SCALE`);
  - sound: the recompiled 989snd's libsd calls on the fork's native mixer, the SPU2 absent (`TMB_NATIVE_AUDIO=0`
    brings it back);
  - still the fork's model: the IOP kernel layer (interrupts, timers, SIF) under the recompiled IOP modules.
- **Controls.** Rebindable to keys, controllers and the mouse: F2 in the game, saved to `input.ini`.
- **Known gaps.** Not checked yet: every arena and story level to the endings. Widescreen and frame rates above the
  PS2's are not started (the game's update steps in whole fields).

## Measuring

- **Check runs.** A pad script reaching the match about 150 s in: `PS2X_PAD_SCRIPT="$(tools/play_script.py 230)"`,
  with `TMB_TRACE_FRAMES=1` (frame split and CPU per host thread every 100 frames; under native frames also the game's
  fields per real second, 60 when it runs at its own speed). `PS2X_CAPTURE_DIR` saves the presented picture
  every few seconds, `PS2X_CAPTURE_WINDOW_DIR` the window as shown, `PS2X_WINDOW_SIZE=1280x960` a bigger window.
- **Timeline.** `TMB_TRACE_TIMELINE=<file>`, then `tools/timeline.py <file> [frame] [count]`.
- **What is still read or emulated.** `TMB_TRACE_PACKETS=1`: per 250 frames, packets read, chains walked, VU0
  interpreter runs (all 0 by default); `[audio] native mixer` lines every 10 s with the SPU2 counters (all 0).
- **Live diagnosis.** `TMB_LIVE_TOGGLES=<file>` (words read every 60 frames: nodepth, nofog, flat, noguard, dump...)
  and F9 bursts of window captures: let the owner drive to a spot scripts can't reach.
- **Comparison modes.** `TMB_NATIVE_VIF=1` (+ `TMB_NATIVE_MESHES_CHECK=1`), `TMB_NATIVE_MESHES=0 TMB_NATIVE_CHECK=<n>`
  (VU1's own output beside the translation), `PS2X_VU0_HLE_CHECK=1`, `PS2X_AUDIO_RECORD=<wav>` on either sound path.
- **Shared machine.** Only timed runs go through the bench lock (`tools/bench_lock.sh`); builds and visual checks run
  freely. Discard the first run after a build on Windows (Defender scans the new files).

## Next steps

1. The IOP kernel layer (interrupts, timers, SIF DMA) under the recompiled IOP modules: native mechanisms at their API.
2. Every arena and story level to the endings, fixing what shows up (the owner's reports first).
3. Widescreen; frame rates above the PS2's (interpolating between game updates).

## Working conventions

- Commit as `Thomas Martin <304581853+boubou666@users.noreply.github.com>`, never a personal e-mail. End commits with
  the `Co-Authored-By` line used in the history.
- Fixes to the runtime, IOP or recompiler go into the fork `boubou666/PS2Recomp`, not into workarounds here. Push the
  fork first, then bump the submodule here.
- Run the fork tests before pushing (`ps2x_tests`, 610+ cases), and a TMB check run on the USA release.
- Push with Git for Windows (`/c/Program Files/Git/cmd/git.exe`): MSYS2's git hangs without credentials.
- Never edit a script a running or queued job will execute; clear a stale bench lock entry if a job dies holding it.
