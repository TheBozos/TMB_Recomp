# Native object shader output

The subsequent [native texture-input increment](NATIVE_TEXTURE_INPUTS.md) removes the three texture
copies retained at this step, together with their consumer's packet reads and patches. The evidence
below records this earlier shader/header increment.

The native mesh path already submits named shader inputs from game/scene records. This increment
retires 38 scalar/header output stores in `pktAddVu1ObjAsm` (`1eaae8`), alongside the 18 VU payload
stores retired by the Scene increment. It replaces the three RGB packet readbacks at `1ead74`,
`1ead7c` and `1ead8c` with direct reads of their source game records. There is no packet image or
shadow register bank for these shader parameters. BozoEngine remains at `4a8f70a`.

The colour source is the object's aligned 128-bit colour record, with nonnegative signed RGB indices
selecting the instance's float table. Negative indices retain the object colour. The original scalar
car multiplications, flags and CPU/FPU clobbers stay in the generated builder. Its copy-source loads,
loops, checkpoints, program choice, cursor at control `+8`, and texture cache pointer at scratchpad
`+94` are retained. The historical checker host executes the original instructions.

## Texture boundary

At this step the three texture payload stores at `1eabd0`, `1eabd4` and `1eabd8` intentionally remain. After traversal,
`hier` calls `texmGetTextureAddrs` (`1f90b0`) on those copied records. That helper reads their format
and may activate a texture, update game texture-manager state and patch the copied texture record.
Removing that producer alone changes the helper's branches and subsequent draw/sound hashes.
The producer and this consumer must be replaced together; retaining an unused hardware header would
not satisfy that dependency. The new tests require these texture copies to match exactly while all
retired output bytes stay untouched.

## Validation

Evidence is under `D:/BozoScratch/object-packets`, against main `165c6f2`, using the same USA disc,
engine, build and input scripts. Every run uses a private user folder. These are functional checks
with no performance claim.

- `tmb-native-object-packet-tests` executes the actual generated builder with the existing published
  adaptations, then with this increment. All 8,192 full-builder comparisons pass across four host
  FP modes, object word alignments, signed colour indices, instance/bones, textures, matrix changes,
  car tint, fog, points, extra records and clip/program states. Complete CPU/FPU/context, MXCSR,
  cursor/cache metadata, scratchpad and memory outside retired bytes match. There are 32 complete
  flat-memory checks. All 41 new instruction sites are reached.
- Another 233 cases use an inaccessible packet output pointer with zero textures, checking that
  shader stores/RGB readbacks cannot reach a device, with unchanged tint/FPU results and extents.
  Textured cases retain their required producer and are excluded from this inaccessible-pointer test.
- All eight native suites and 12 release-tool tests pass; player, headless and historical checker build.
- Default 1P (9,500 frames) and actual 4P (8,500) match main on every draw hash/count, sound hash/sample
  count, library call field, EE clock, scratchpad and thread record. Separate strict/default recordings
  are byte-identical. Default VU0 program counts remain zero. No hash mask is widened.
- The two default routes retire 30,092,181 / 41,383,590 stores and perform 1,646,466 / 3,341,874 direct
  RGB reads. RAM hashes differ on 9,496 / 8,496 frames. Full 32 MB snapshots at 1P frame 7,000 and 4P
  frame 6,000 isolate every differing byte to the four named packet buffers below; there are zero
  differences outside these buffers at those snapshots.

| Buffer interval | 1P differing bytes | 4P differing bytes |
| --- | ---: | ---: |
| `564a80..5eca80` (`hierObjPkt`) | 995 | 590 |
| `5eca80..630a80` (`hierObjPkt2`) | 39,069 | 18,042 |
| `630a80..64ea80` (`hierAlphaPkt`) | 12,252 | 4,029 |
| `64ea80..66ca80` (`hierAlphaPkt2`) | 13,944 | 4,179 |

Intervals are half-open. The snapshot boundary is bounded evidence, not proof of all campaign consumers.

Windowed movie/menu (4,201 frames), 1P (6,502) and 4P (6,501) complete through loading into active arenas.
Direct capture inspection covers decoded movie action, readable vehicle/level selection and loading,
the rainy 1P arena with targeting/HUD/vehicles, and four independent snowy views. This is visual smoke
evidence, not console or pixel parity. Device logs retain 25.3 / 58.6 / 8.8 ms of inserted silence in
movie/1P/4P, and dropped frames in movie/1P. Exact deterministic sound does not imply flawless device
audio or subjective listening acceptance.

## Remaining scope

The texture activation/patching dependency above is addressed by the subsequent native texture-input
increment linked at the top. Alpha link records, scalar
builder bookkeeping, source copy loops, legacy packet extents, other mixed graphics builders and
unconverted VU macro helper callers remain. This is shader-output retirement, not removal of the
complete builder or all packet code. The earlier player release is unchanged. Full campaign coverage,
console visual parity and flawless device audio are not claimed.
