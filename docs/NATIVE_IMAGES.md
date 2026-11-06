# Native image-builder completion

BozoEngine's native graphics mode returns zero from `sceGsSetDefLoadImage` and
`sceGsSetDefStoreImage` without constructing unused image packets or reading the output pointer.
Generic SDK callers keep their existing output ABI. The shared implementation is
[`64f6b22`, engine PR #4](https://github.com/TheBozos/BozoEngine/pull/4), contributed by the DC port
after discussion with TMB and VP2S. The first adoption pinned engine main `149bf90`, whose source tree
exactly matches that tested implementation. The later graphics-services pin and packet-reader retirement
are recorded in [`NATIVE_GRAPHICS_SERVICES.md`](NATIVE_GRAPHICS_SERVICES.md). Neither adds image readback support.

TMB's `tmb_textures.cpp::sceGsSetDefLoadImageOverride` still binds resident RTX records and advances
the ordinal before calling the SDK builder. The shared guard runs after those game-owned updates.
Movies use decoded native pictures through `loadImageOverride`; they do not consume the skipped packet.
No TMB texture-manager layout was copied from DC.

## Validation

Local artifacts are under `D:/BozoScratch/native-images`, compared with the retained HUD-main build
at `5b5cd57` (engine `5bb4efb`).

- Nine engine CTest checks pass, including native/generic image builders and inaccessible-output guards.
- The existing HUD and camera math suites pass on the new engine head.
- A 9,500-frame 1P run and an 8,500-frame 4P run have exact baseline draw hashes, batch/vertex counts,
  sound hashes/sample counts, calls, EE clocks, scratchpad and thread records.
- Both routes reproduce all six baseline texture-initialization diagnostic lines exactly, including
  resident texture ordinals and file identities. No texture-number mismatch was observed.
- RAM hashes differ because the SDK packet writes are absent. Whole-RAM equality is not claimed.
- Windowed default runs complete 4,201 movie/menu frames, 6,501 1P frames and 6,501 4P frames.
  Direct captures show the opening movie, active 1P combat and all four snow-arena views after loading,
  with resident textures and HUD present. The window logs record brief audio underruns; deterministic
  sound comparisons are exact, but this is not a flawless device-audio or subjective listening claim.
- All 12 release-tool tests and Windows/Linux engine CI pass.

The change retires two packet serializers in the default native path. Other game-owned unused builders,
compatibility data and VU0 math remain separate work. It does not claim full campaign or console parity.
