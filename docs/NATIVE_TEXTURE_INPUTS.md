# Native texture consumer inputs

The subsequent [native upload-link increment](NATIVE_TEXTURE_UPLOADS.md) replaces the separate upload
leaf at its audited callers. The evidence below records the earlier texture-input increment.

The traversal's texture activation path now reads its parameters directly from the source object.
This removes the last three `pktAddVu1ObjAsm` payload stores (`1eabd0`, `1eabd4`, `1eabd8`) that the
[previous increment](NATIVE_OBJECT_PACKETS.md) retained. The complete per-object builder now emits no
output packet bytes in the native player. Its scalar source-copy loops, context results and cursor/cache
bookkeeping remain. BozoEngine stays at `4a8f70a`; the earlier player release is unchanged.

## Producer and consumer boundary

Both assembly traversals enqueue an object and a cursor for its old texture copy. Their four audited
texture-helper returns are `1d330c`, `1d3340`, `1d2d84` and `1d2db8`. At those returns, the native input
handler resolves the object and texture index from the game's actual queue and saved helper frame.
The source qword alignment matches the original builder's aligned loads.

Four reads in `texmGetTextureAddrs`/`texmActivateTexture` use the original base/filter/flag fields directly.
Six writes that only patched the retired copy or its mip table are skipped. The filtering read and
zero-texture patch are branch delay-slot sites `1f9130` and `1f9158`; the native guards run at those
instructions, preserving their enclosing branches. Unknown callers, the C builder's `1d3afc` return,
and the historical checker retain the original complete behavior.

The input handler has no binding, cached parameter image, packet-address map or register shadow.
Activation and mip helpers validate their own saved return before resolving the enclosing texture
helper's frame. Consequently, the handler can run from a suspended/resumed context without relying
on an earlier native function invocation or a live C++ local. Each read uses the owning runtime's memory.

The game's allocation, eviction, resident flags, last-used counters, loaded-texture updates and upload
cursor still execute in the original statically compiled helper chain, with unchanged checkpoints.
`pktAddVu1Tex` still builds its separate upload link. Those scalar helpers and that separate builder are
remaining work; this increment removes the per-object copy/read/patch transport, not the texture manager.
The native renderer continues to use its existing texture pack and material records.

## Validation

Evidence: `D:/BozoScratch/texture-inputs`, against main `3fd4257`, using the same USA disc, engine and
input scripts. All game runs have private user folders. These are functional checks, without a
performance or full-campaign claim.

- The new `tmb-native-texture-input-tests` executes the actual generated consumer, activation,
  allocator, eviction, last-used, mip and upload-tag helpers. 8,192 cases pass across four host FP modes,
  all four returns, object word alignments, texture formats, zero IDs, resident/new/evicted textures,
  zero through seven mip levels and filtering switches. Complete CPU/context/host control, stack,
  source data, texture-manager state and cursors match outside the sole retired 48-byte texture output.
  There are 32 complete flat-memory checks; all ten new instruction sites are reached.
- 268 cases use an inaccessible texture output pointer: the helper completes without a device access,
  with identical manager/loaded-texture state and upload cursor. 136 fallback cases retain the original
  caller/historical ABI. Another 320 reconstructed-context cases cover every site, return, object word
  alignment and two independent runtime owners, without any prior producer or scoped binding.
- The existing object-builder suite now checks all 41 output stores plus three RGB reads, with the
  complete output arena untouched. Its 8,192 cases, 32 full-memory checks and 1,158 inaccessible-output
  cases pass, including textured cases previously excluded by the retained producer.
- All nine native suites and 12 release-tool tests pass; player, headless and historical checker build.
- Default 1P (9,500 frames) and actual 4P (8,500) match main on every non-RAM field: scratchpad, draws
  and counts, sound and samples, calls, threads and EE clocks. Default VU0 program counts remain zero.
  Strict/default recordings are byte-identical. No hash mask is widened. RAM hashes differ on
  9,496 / 8,496 frames.
- The two default routes perform 526,557 / 484,986 direct texture reads and skip 398,778 / 273,524
  obsolete texture patches. Total per-object stores retired are 34,979,313 / 49,034,910; the three
  newly retired copy stores account for 4,887,132 / 7,651,320 of those.

Full 32 MB snapshots at 1P frame 7,000 and 4P frame 6,000 isolate every differing byte to these buffers,
with zero differences elsewhere:

| Half-open interval | 1P differing bytes | 4P differing bytes |
| --- | ---: | ---: |
| `564a80..5eca80` (`hierObjPkt`) | 689 | 613 |
| `5eca80..630a80` (`hierObjPkt2`) | 29,303 | 15,482 |
| `630a80..64ea80` (`hierAlphaPkt`) | 7,467 | 2,116 |
| `64ea80..66ca80` (`hierAlphaPkt2`) | 5,750 | 2,756 |

This snapshot boundary does not prove all possible campaign paths. Other packet builders/alpha links,
legacy extents, game texture-manager bookkeeping and unconverted VU macro helper callers remain.

Windowed movie/menu (4,201 frames) and 1P/4P (6,501 each) finish through loading into active arenas.
Direct captures show decoded movie action, readable loading, textured vehicles/rain/HUD in the 1P arena
and four snowy views with distinct vehicles, effects and targeting. These are visual smoke checks,
not console/pixel parity. Device logs retain dropped frames and 22.0 / 67.3 / 24.0 ms of inserted silence
in movie/1P/4P. Exact deterministic sound is not flawless-device-audio or subjective listening acceptance.
