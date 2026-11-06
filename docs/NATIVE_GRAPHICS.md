# Native graphics submission and completion

The player no longer drives the graphics DMA/register model. This change removes that dependency from execution;
the original instructions and BozoEngine's register model remain available to the historical call-log checker.

## Submission and ownership

- Mesh, particle, font/HUD and loading-screen hooks copy their draws into the native renderer in the game's order.
- `hierCheckVifComplete` submits the current view and reports it reusable immediately. This is completion of the
  **game-side copy**, not completion of GPU rendering. No GPU fence or `glFinish` is required to release that queue.
- `hierFlushObjQ` clears the game's queued-view flags directly. The split-view flush submits its view and clears
  its pending flags directly. Neither sends a dummy transfer or waits for a fabricated DMA interrupt.
- Native VU0 routine selection happens explicitly at the three program-selection sites. No upload or channel state
  is involved. VU0 calculations themselves retain their existing arithmetic and context.
- Movie images are handed to native presentation. Their held-picture flag and decoded-picture count are released
  directly, including an empty presentation. Field parity comes from the native frame clock.

## Retired hardware output

`runtime/tmb_graphics.cpp` lists 109 instruction sites in the USA executable. `bozo:native` exports these
sites to `bozo-externals` and `bozo-recomp`. The generated code calls an inline native replacement **before** the
original memory operation; handled sites never enter the MMIO dispatcher. This also works in branch delay slots.
The native completion tests never read `CPCOND0`. There is no replacement register bank or packet-chain decoder.

The packet/data builders that feed native hooks can still run; removing their unused construction work is a separate
optimization. Native materials accompany the draws, so the player also skips `sceGsPutDrawEnv`'s register mirroring.

BozoEngine's free host faults if this port reaches a retired GIF/VIF register/FIFO, graphics DMA channel, VU1 memory,
GS privileged register, or legacy DMA-condition poll. An uncovered path therefore fails visibly instead of falling
back to the model. The checker host leaves native instruction replacements unhandled and executes the originals;
existing store patches retain their instruction PC in that path.

## Deliberately retained

- Movie **input** synchronization (channel 4 and the FFmpeg decoder's demand) is separate from graphics output.
- The game timer, scheduler, native VU0 calculations, guest memory and audio behavior are unchanged in scope.
- SDK functions can still build PS2-format structures that the game or native material hooks inspect. These are
  data compatibility, rather than transfers to a graphics device.

## Validation

Local artifacts are under `D:/BozoScratch/native-graphics` and are not committed with game data.

- The first guarded headless comparison completed 8,500 frames, including boot movies, menus, a level load and
  2,909 gameplay frame ends. There were no unknown-register accesses or retired-graphics faults.
- Batch and vertex counts match the previous runtime on all 8,500 frames. Draw hashes match on 8,450 frames.
  A diagnostic comparison of the first differing frame (6853) found the same batch order, materials and counts;
  one transform and one vertex batch differ in small floating-point values. This is **not** a bit-exact draw claim.
  Sound hashes also differ after gameplay starts; removing simulated completion events changes the scheduling trace.
- The original 9,001-frame windowed 1P smoke run completed at about 60 updates/s. Captures show the intro movie,
  menus, the loaded arena, bots, rain, weapons and explosions. The output WAV contains the native audio stream.
  Device statistics report 49.7 ms of underrun silence across the 150.7-second run; this is not an audio-perfection claim.

Final-build checks (BozoEngine `f715d57`):

- All **5 CTest checks pass**, including all **146 instruction cases**. The six new cases cover inline output,
  output in a delay slot, native completion and the checker's original-operation fallback. Address-isolation checks
  keep movie input, the game timer and VU0 data outside the retired-graphics guard.
- A fresh 8,500-frame headless run matches the first native run on every draw hash, batch/vertex count, sound hash
  and EE clock value. No retired-graphics access or unknown register is reported.
- A no-input, no-skip movie/menu run completes **4,201 frames** and returns to the main menu. Captures of the intro
  and main menu were inspected. Clock checks pass: **1.0000x**, **575,962 menu ticks/s**, **59.97 fields/s**;
  the two measured movie sequences deliver **29.9987 and 29.9948 pictures/s**.
- Actual four-player split-screen completes **9,001 frames** on the snow arena. Captures at 5,500 and 8,000 were
  inspected: four views, vehicles, HUD, pickups, snow and movement. The native audio stream is non-silent stereo
  48 kHz; device statistics report 27.7 ms of underrun silence across 150.9 seconds.
- The historical boot checker reports **MATCH** on all **3,000 state frames**, with the established final-record-only
  draw exception (2,999/3,000 draw hashes). This validates the retained diagnostic path; it is not the native player's
  execution of hardware-output instructions.
- The historical 1P checker also reports **MATCH** on all **16,000 state frames**, with 15,999/16,000 draw hashes
  matching (the same final-record exception). Its existing pinned-event allowances remain in force, and sound hashes
  are reported rather than checked. Both historical checkers and the final native run exit successfully.

Full campaign/endings and additional physical controllers/hardware remain outside this smoke-test coverage.
