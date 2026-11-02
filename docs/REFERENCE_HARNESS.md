# The reference harness

Phase 0 of BozoEngine (its spec: `D:\BozoEngine\docs\specs\harness.md`): TMB on its current runtime (the PS2Recomp
fork) runs deterministically and records, per game frame, what the guest computed. Two runs with the same scripted
input write the same file; a later runtime must write the same records.

## Running

```sh
PS2X_DETERMINISTIC=1 PS2X_REFERENCE=run.ref PS2X_REFERENCE_FRAMES=9000 PS2X_USER_DIR=<empty folder> \
  PS2X_PAD_SCRIPT="<frame:BUTTON/frames,...>" build/game_us/PS2Recomp/ps2xRuntime/ps2EntryRunner game_us/SCUS_971.01
tools/ref_compare.py a.ref b.ref
```

- `PS2X_DETERMINISTIC=1`: the deterministic mode (below). Off, every path is as before.
- `PS2X_REFERENCE=<file.ref>`: the recorder. Works in either mode (only deterministic runs compare equal).
- `PS2X_REFERENCE_FRAMES=<n>`: the process exits (0) once n frames are recorded.
- `PS2X_REFERENCE_CALLS=<n>` (default 120), `PS2X_REFERENCE_CALLS_FROM=<f>`: the full call lists of n frames from
  frame f, as text, in `<file>.ref.calls`.
- `PS2X_REFERENCE_DUMP=<f>[-<g>][,...]`: RAM (32 MB) then scratchpad (16 KB) of those frames in `<file>.ref.ram.<f>`.
- `PS2X_REFERENCE_MASK=<address>:<bytes>[,...]`: RAM hashed as zeros there (listed in the header). None is needed for
  TMB; `PS2X_REFERENCE_CHECK_HOST=1` scans RAM every 600 frames for words that look like host addresses (into the
  host's copy of RAM or the scratchpad, or near the runtime's own data) and logs them.
- `PS2X_REFERENCE_RUN=<name>`: the run's name in the header (default: the file's stem).
- `PS2X_USER_DIR=<folder>`: a fresh folder per run, so the memory cards start pristine and the settings default.

Pad scripts in deterministic mode are keyed on game frames: `frame:BUTTON/frames` (held 30 frames when not given);
`PS2X_PAD2_SCRIPT` .. `PS2X_PAD4_SCRIPT` alike. `tools/ref_pad_script.py` converts a timed script: run it once with
`PS2X_DETERMINISTIC=1 PS2X_PAD_CLOCK=wall PS2X_PAD_LOG=1` (the pad on the wall clock, each change logged with its
frame; that run is not deterministic), then `--log <its log> --from-log` gives pad 1's presses at the frames they
landed, and `--log <log> "<timed script>"` maps any other timed script through the same anchors.

## What deterministic mode does

The run-to-run variation found in the code (`D:\BozoEngine\docs\notes\tmb-determinism.md`) and what each became:

| # | Source | Deterministic mode |
|---|---|---|
| 1 | The frame step from host time (`FramePacer::frameEnd`), the EE cycles charged at the frame end from it, TMB's field carry | `FramePacer` returns exactly one period of the cap (1/60 s); TMB charges exactly `294912000 / field rate` EE cycles less what the frame accounted (`nativeFrameEnd`), so one field per frame, no carry. The window is still held to 60 fps (pacing only) |
| 2 | `sceCdReadClock` from the host's date (TMB seeds its RNG from it) | 2001-06-18 12:00:00 (`ps2x::det::fixedDate`); OSD time zone UTC |
| 3 | The IOP on the host clock (`PS2X_IOP_WALLCLOCK`, which TMB sets), the IOP thread | Ignored: the IOP advances only by the EE cycles accounted, on the EE thread (`PS2X_IOP_THREAD` ignored) |
| 4 | The native mixer rendered on the audio device thread | Mixed on the IOP's clock: 768 IOP cycles per 48 kHz sample, brought up to the current IOP cycle before every libsd call and at the end of every IOP slice (`NativeMixer::advanceTo`), so envelopes, ended voices, input positions and the input's half interrupts change only at IOP points. The device only drains a 16384-sample queue (silence when dry, oldest dropped when full). The emulated SPU2 is never on the wall clock |
| 5 | Pad scripts in seconds; host keyboard / gamepads; players from plugged gamepads | Scripts in game frames (the frame count only moves at frame ends: latched per frame); no host input (`PS2X_PAD_NO_HOST` forced, `PS2X_PAD_INPUT` / `PS2X_PAD_RECORD` off); players 2-4 present only by their scripts; a pad port with no player behind it (port 2 while the multitap is on port 1) reads at rest instead of falling back to the host's keyboard and first gamepad (found by the first proof runs: its sticks read 0x7f or 0x80 with the window's focus) |
| 6 | EE wall-clock catch-up (`PS2X_EE_CATCHUP`, on by default) and host deadlines | Off; every scheduled event is due by its cycle alone; vblank holds ignored; an idle wait still sleeps (pacing) but always charges exactly the cycles to the next event; `waitHostDeadline` (other ports' frame waits) completes at the next dispatch point |
| 7 | Movies: picture clock from host time, host-deadline waits, decode time charged to the EE | Frame-clocked: pictures paced by the vblank count (which follows EE cycles), no host deadline, decode time not charged. A movie still plays whole (it ends no game frame, so the pad script's frame clock stands still through it) |
| 8 | Lens flare depth read back from the GPU into guest RAM; `inMatch` from a 100 ms host-time window | Flare samples all far (0); `inMatch` / the menus' window measured in game frames (frame count / 60), so a frame's kind follows the kinds of the frames before it |
| 9 | Memory card contents and dates; file dates | A fresh `PS2X_USER_DIR` per run (pristine card); every card and file date is the fixed date |
| 10 | Settings window actions (Cheats tab) | The tab shows nothing but a note; only the environment's cheats apply (from launch). `TMB_CHEATS_TOGGLE` off |

Beyond the list: the MPEG decode charge and `waitHostDeadline` (above), the OSD time zone, file `getstat` dates.

Left as they are (checked: no guest-visible effect, or not used by TMB's native port): the GS front end and VIF1
thread (absent in the native port), the CD read-ahead threads (the guest waits for them synchronously), the renderer's
present thread (reads published frames only), host-side statistics and traces.

## The record file

Little-endian, packed. Hashes: XXH64 (seed 0) for `ram`, `scratchpad`, `calls`, `draws`, `threads`; the sound is
FNV-1a 64. No hash ever takes a host pointer: textures are identified by their content.

Header (240 bytes, then `maskCount` pairs of u32 {address, bytes}):

| Offset | Type | Field |
|---|---|---|
| 0 | char[8] | magic `PS2XREF1` |
| 8 | u32 | version (1) |
| 12 | u32 | headerBytes (240 + 8 x maskCount: where the records start) |
| 16 | u32 | recordBytes (80) |
| 20 | u32 | hash (1: XXH64 seed 0, sound FNV-1a 64) |
| 24 | u32 | ramBytes (33554432) |
| 28 | u32 | scratchpadBytes (16384) |
| 32 | u64 | elfHash: XXH64 of the ELF file |
| 40 | u32 | flags: bit 0 deterministic mode |
| 44 | u32 | maskCount |
| 48 | char[64] | run name |
| 112 | char[64] | build ("TMB_Recomp <commit> fork <commit>", as configured) |
| 176 | char[64] | the ELF's file name |

Record (80 bytes), one per game frame. A game frame ends where the game hands its picture over
(`EeScheduler::presentGuestFrame`: TMB's `timer1Handler` / `sceGsSyncV`, menus and loading screens included; movies
end none). Frame 0 is everything up to the first one.

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | frame |
| 4 | i32 | thread: the guest thread running at the frame end |
| 8 | u64 | ram: XXH64 of the 32 MB of EE RAM (masked ranges as zeros) at the frame end |
| 16 | u64 | scratchpad: XXH64 of the 16 KB scratchpad |
| 24 | u64 | calls: XXH64 of the frame's calls, each 7 u32 {id, a0, a1, a2, a3, v0, flags}, in call order |
| 32 | u64 | draws: the frame's published draw lists (below) |
| 40 | u64 | sound: FNV-1a 64 of the stereo int16 samples mixed during the frame (0 when none) |
| 48 | u64 | threads: XXH64 of i32 {running thread, then the ready threads in run order} |
| 56 | u64 | eeCycle: the EE cycle count at the frame end |
| 64 | u32 | callCount |
| 68 | u32 | drawBatches |
| 72 | u32 | drawVertices |
| 76 | u32 | soundSamples (800 a frame once the mix runs at one field per frame) |

Calls: the dispatcher's calls (JAL / JALR) to the game's library and kernel stubs, the 362 addresses of the generated
code's stub list (`ps2_recompiled_stubs.h`, compiled into `build/.../tmb_call_names.cpp` at configure time), and raw
`syscall`s. `id` is FNV-1a 32 of the stub's name with the top bit clear, or `0x80000000 | number` for a syscall;
`a0`-`a3` the argument registers' low words; `v0` the result's low word; `flags` 1 when the call blocked (its result
comes later), 2 when the frame ended inside it (`sceGsSyncV`, TMB's frame end). A tail jump compiled to a direct call
is not seen. The `.calls` side file lists them as text: name, id, a0-a3, result.

Draws: per published list (kind 1 the 3D frame at `Renderer::endFrame`, 2 the 2D overlay at `publishOverlay`, 3 a
movie picture at `setBackdrop`) h = XXH64(kind), then per batch h = XXH64(words, h) of 8 u64 {tex0, texa, region,
the flags packed (textured, blend << 8, alphaRef << 16, alphaTest << 24, depthTest << 25, depthWrite << 26, clampU
<< 27, clampV << 28, fog << 29, sampleTarget << 30, fromGsContext << 31, alphaBlend << 32, context << 40), fogColour,
texture identity (XXH64 of its RGBA texels seeded with XXH64 of {width, height}; 0 for none), vertex count, 0}, then
each GS write {address, value} of a GS-context batch, the matrix (16 floats), the clip (4 floats) and the vertices
(32 bytes each: x, y, z, u, v, rgba, fog, q). The frame's `draws` folds each list's h in order: d = XXH64({d, h}).

## Comparing

`tools/ref_compare.py a.ref b.ref [--ignore field,...]` prints both headers, then either `identical: N frames
compared` or the first frame where any field differs, every field that differs there with both values, and the first
frame each field differs at. With RAM dumps of that frame from both runs (`PS2X_REFERENCE_DUMP=<frame>`), the first
differing 32-byte block (address, both runs' bytes); with both runs' call lists of the first frame whose calls differ
(`PS2X_REFERENCE_CALLS_FROM=<frame> PS2X_REFERENCE_CALLS=1`), the two lists side by side from their first difference.
Exit status 0 when identical, 1 when not.
