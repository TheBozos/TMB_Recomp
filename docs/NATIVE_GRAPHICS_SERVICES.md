# Native graphics services and readback boundary

TMB pins BozoEngine main `4a8f70a`, the merge of
[engine PR #5](https://github.com/TheBozos/BozoEngine/pull/5), discussed and qualified with DC and VP2S.
The engine removes `Libraries::gs`, the privileged register bank and its runtime GIF readers. SDK DMA
configuration/completion uses native jobs and callbacks; unknown device accesses and unowned readbacks
fail explicitly. This does not claim that every TMB packet-only producer or VU calculation is removed.

TMB also retires its old `sceGsExecStoreImageOverride` and `patchFlareDepth` adapter. They scanned image
packets for BITBLTBUF/TRXPOS/TRXREG even though the default particle path already uses game screen
coordinates with `flareDepthBlock` / `Renderer::depthAt`. The retired adapter and its packet logger are
gone. Native flare sampling uses the direct renderer interface.

The old depth-off/original-particle comparison branches are not a native device backend. A request for
SDK readback through them now stops with `sceGsExecStoreImage requires a native renderer readback override`.
This was checked with `TMB_NATIVE_FLARE_DEPTH=0`; no packet/device shadow or fabricated readback is supplied.
Full native coverage of those diagnostic branches is separate work.

## Independent qualification

Artifacts are local under `D:/BozoScratch/engine-pr5-baseline`.

- Unchanged published TMB source `74fd638` was exported into a separate source/build directory and
  compiled against engine candidate `0c980b29`. Its window, headless and historical-checker tools build.
- The 9,500-frame 1P and 8,500-frame 4P routes complete with baseline-exact draw hashes/counts, sound
  hashes/sample counts, calls, EE clocks, scratchpad and thread records against engine `149bf90`.
  RAM hashes differ after the retired SDK state/packet writes; whole-RAM equality is not claimed.
- Windowed movie/menu playback completes 4,201 frames; 1P and 4P each complete 6,501. Direct captures
  show decoded movie pictures, textured 1P combat and all four snow-arena views after loading.
- The final candidate `94b688d5` has the identical runtime tree. Its compiler includes the separately
  merged guarded-adaptation and explicit-entry options. With no new options enabled, all 34 TMB
  generated files are byte-identical to the qualified build. The merged engine tree matches that head.

## Combined TMB integration

- Vehicle main `92a7cfb`, the merged engine and the packet-adapter removal build all player/check tools.
  The three native math/HUD/vehicle suites pass, together with all 13 local engine CTest checks.
- Combined 1P (9,500 frames) and 4P (8,500) reproduce every field of the independently qualified engine
  records, including RAM, draws, sound, calls and clocks. This is separate from the old-engine RAM claim.
- Another windowed 6,501-frame movie/menu/loading/4P run completes. Direct captures show all four views
  with textures, HUD and effects. Its audio log records 31.2 ms of silence; the separate movie/1P/4P
  windows record 21.5/60.4/29.2 ms. Deterministic sound is exact, without a flawless device-audio or
  subjective listening claim.

These are bounded smoke routes, not full campaign or console visual-parity acceptance. The historical
checker executable still builds; obsolete readback/device paths are not promised as replay-compatible.
TMB does not enable the new compiler `--patches` or `--entries` options in this pin change.
