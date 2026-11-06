# Native texture upload-link completion

`pktAddVu1Tex` (`1ea5c8`) now uses a native leaf at its two audited activation/mip returns, `1f947c`
and `1f977c`. Texture content is already supplied by the native pack/material path. The native leaf
advances control `+8` by 16 bytes and preserves the original CPU caller results; it reads no upload
descriptor and emits or dispatches no hardware tag. Its three former output stores are gone from
this native path. No graphics register bank, transfer queue or synthetic interrupt is added.

The original leaf has no checkpoints. Its complete context ABI is retained: low 64-bit results in
registers 2/3/5/6/7, the original full-register clear of register 3, return PC, and untouched CPU/FPU/VU
state, clocks and checkpoint count. Native completion preserves the game's legacy extent metadata;
it is not a GPU fence or a new rendering operation.

Unknown callers and the historical checker execute the original complete leaf. Deliberate overlap
between the output writes and the 20-byte control record also retains the original behavior, because
the old writes can change subsequent cursor loads. The guard compares physical flat-memory spans,
including RAM aliases. Ordinary disjoint records need no mapped output pointer.

## Validation

Evidence: `D:/BozoScratch/texture-uploads`, against main `6341d9b`, with the same USA disc, engine and
input scripts. All game runs use private user directories. BozoEngine stays at `4a8f70a`; the earlier
player release is unchanged. These are functional checks, with no performance claim.

- The expanded texture-input suite checks 4,096 original/native leaf cases in four FP modes, five
  RAM views, scratchpad, word-aligned controls/cursors and arbitrary descriptor arguments.
  Complete CPU/FPU/VU/context, host FP control, clocks/checkpoints, cursor and other memory match, with
  the native output arena untouched. There are 32 complete flat-memory checks.
- 128 inaccessible-output cases complete with exact pointer-relative CPU results and no device
  access. 128 unknown/historical cases retain the original output/context behavior. Another 16 cases
  compare complete flat memory and context for overlapping control/output records through RAM aliases.
- The actual consumer/activation/allocator/mip chain still passes 8,192 cases, 32 full-memory checks,
  268 inaccessible texture-output cases, 136 input fallback cases and 320 reconstructed-context/owner
  cases. Its only memory exclusions are the explicitly retired 48-byte object texture output and
  the exact number of 16-byte upload links formerly appended at the upload cursor.
- All nine native suites and 12 release-tool tests pass; player, headless and historical checker build.
- Complete 1P (9,500 frames) and actual 4P (8,500) recordings match main on every non-RAM field:
  scratchpad, draws/counts, sound/samples, library calls, threads and EE clocks. Separate strict/default
  recordings are byte-identical. Default VU0 program counts remain zero. No hash mask is widened.
- The two routes execute 36 / 13 native upload leaves, retiring 108 / 39 tag stores. This is a small
  execution count, not evidence of a meaningful performance improvement.

RAM hashes differ on 1P frames `5593..9499` (3,907 frames), and on seven 4P frames: `4704`, `4731`,
`4793..4795`, `5747`, `5865`. The checked 32 MB snapshots isolate ten differing bytes in `hierAlphaPkt`
at 1P frame 7,000, five in that same buffer at 4P frame 4,794, and zero differences at 4P frame 6,000.
There are no differing bytes outside that packet buffer in these snapshots. Old untouched tag bytes
are not cleared or restored artificially. The snapshots are bounded evidence, not a campaign-wide proof.

Windowed movie/menu (4,201 frames) and 1P/4P (6,501 each) finish through loading into active arenas.
Direct captures show movie action, readable loading, rainy combat with targeting/health/HUD/fire/effects,
and four snowy views with distinct vehicles. These are visual smoke checks, not console/pixel parity.
Device logs retain dropped frames and 29.2 / 54.9 / 25.1 ms of inserted silence in movie/1P/4P; exact
deterministic sound does not imply flawless device audio or subjective listening acceptance.

## Remaining scope

Game texture allocation/eviction/last-used bookkeeping, per-object source-copy loops and legacy
extents remain. Placeholder tags, alpha-link builders and other mixed packet helpers remain separate
work, along with unconverted VU macro helper callers. This increment replaces the audited upload-link
leaf rather than claiming complete packet/emulation removal or console visual parity.
