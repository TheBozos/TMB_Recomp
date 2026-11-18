# Save states

TMB uses BozoEngine's save states (`BozoEngine/docs/save-states.md`). A state holds the whole run at a frame's end.
It can be loaded back in the same process or in a new one, and the run then goes on exactly as the saved run did.

## Using them

- **Window:**
  - F5 saves in the current slot and F7 loads it.
  - F6 and Shift+F6 change the slot (0-9). F2 → Save states lists the slots.
  - The files are `states/slot<n>.state` in the user folder, about 40 MB each.
- **Command line** (`tmb` and `tmb-headless`):
  - `--save-state FILE --save-state-at N` writes a state when N frames have ended.
  - `--load-state FILE [--load-state-at N]` loads one before the first frame, or when N frames have ended.
  - Frame numbers stay absolute after a load: `--frames`, `--quit-after` and `--capture-frames` still count from the
    start of the run.
  - `tmb-headless --dump-ram FILE` writes EE RAM at the run's end.
- **Limits:**
  - A state loads only into the build that wrote it. Make the states again after a change to a section's layout.
  - A save waits while a movie plays, and says why.
  - Recorded runs (`--out`) do not follow a load.

For checks, keep a few named states, for example the title, a 1P match or a 4P match. Note the frame and pad script
that made each one, then start runs from them instead of replaying the scripts from boot.

## What is saved

The engine saves the runtime, its libraries and the host:
- EE memory, threads, the kernel, timers, pads, cards, CD and MPEG;
- the 989SND sound driver: banks, handles, its IOP memory, mixer, libsd and 989SND's own model.

Two engine changes came with this:
- **989SND unloaded banks:** the engine used to destroy a bank's header while busy voices still read its program
  records, a use-after-free. Unloaded banks are now kept, as 989SND's IOP memory is.
- **`LoadedImages::images` / `restore`:** these let a port save the mesh files it bound.

TMB adds its own section, `tmb` (`runtime/tmb_state.cpp`):

| Part | Saved | Why |
| --- | --- | --- |
| Overrides | Frame pacing parity, frame-end cycle, field carry, clock debt; the movie sound's queue | The parity is written to the game's memory; the pacing carries over between frames |
| Native hierarchy | `tmb::scene::Scene` whole: frames, lights, renderer parameters | The records the game's VU programs kept between draws |
| Meshes | The NGP files the game bound (base, size, pack file), the DMA marks' counter | Told once when the game loads a level. The images change in memory afterwards, so they cannot be bound again |
| Textures | Disc file per address, the level's file lists and set-up order, the records placed in VRAM, TBP → texture id | Told once at load time; records saved as (file, index) |
| Particles | Stripe scroll, arc VU records, texture choices, per system | Kept between frames |
| Cheats | The lock words Unlock everything opened | Put back when it goes off |

On a load, the decoded textures, the skid marks' object and the 2D menu and movie pictures are dropped. They are
caches of the old run's memory.

## How it was checked

Headless runs are deterministic. Each check saves at frame N on the 1P or 4P route (A), then:
- loads that state in a new process (B);
- loads it in the middle of a run, at a later frame (C).

All three run to the route's end.

| Route | Saved at | End-of-route EE RAM (B, C against A) | Sound after the save |
| --- | --- | --- | --- |
| 1P, menus | 3,000 | identical | byte-identical |
| 1P, match | 7,000 | identical | byte-identical |
| 4P, match | 7,000 | identical | byte-identical |

The native renderer's batch counts and EE cycles after the load match the run that never stopped.

The default replays still match the build without save states on every recorded field. That covers 1P 9,500 frames
and 4P 8,500 frames, including RAM, sound and the sampled RAM images.

A window run that loaded a match state showed the same scene, HUD and sound as the run that saved it.
