# Native inline terrain and contact calculations

The subsequent [sphere/LOD increment](NATIVE_SPHERE_LOD.md) extends this handler to thirty blocks
and replaces six additional collision hierarchy helper returns.

The player now calculates all 27 inline vector blocks in these four collision owners from native
matrix/vector records. There are no VU register loads, arithmetic, accumulator/flag updates or
register stores on these block paths. The 150 retired instruction sites include 48 arithmetic VU
operations. Other code in these functions remains compiled, including branches, allocation/result
bookkeeping, scalar query consumers and calls to separately implemented helpers.

| Owner | Blocks | Calculations |
| --- | ---: | --- |
| `hdColNode` (`1cb0d8`) | 8 | Two basis products and six point/offset sums |
| `hdColNodeCheckPt` (`1cb690`) | 8 | Two basis products, five sums and one difference |
| `hdRayTest` (`1cbd00`) | 8 | Two basis products, five differences and one sum |
| `hdTerrain` (`1cc7a0`) | 3 | Basis products with translation for terrain query points |

`tmb_contact_math.h` names each input/output record boundary. The last instruction of each block
reads its current records, snapshots aliased inputs, calculates separately rounded XYZ and writes
only those twelve bytes. An earlier block can change a later block's inputs; no result or register
image is cached across blocks, branches or scheduler continuations. Destination W remains its
existing payload. The complete CPU/VU context and host FP control state are unchanged by a block.
The historical checker still executes the original instructions. `TMB_NATIVE_CONTACT_CHECK=1`
compares native XYZ against independent original blocks extracted from fresh generated output;
those diagnostics and game instruction bodies stay in the build tree.

BozoEngine remains at the qualified `4a8f70a`. This does not remove the remaining sphere/LOD inline
math, unaudited inverse/helper entries, scalar collision algorithms or all VU emulation globally.

## Numerical and continuation validation

The local evidence directory is `D:/BozoScratch/terrain-contacts`; final expanded runs use the
`final27-` prefix. The baseline is main `75b6b35` (PR #23). Earlier unprefixed files record the
initial nineteen-block increment, before the downstream ray-result builder was included.

- `tmb-native-contact-tests` passes 884,736 comparisons against actual generated instruction blocks
  and 864 complete flat-memory checks. Cases include arbitrary/extreme number bits, signed zero,
  denormals, four host FP modes, denormal controls, record overlap, RAM aliases, low address bits
  and scratchpad. Native CPU/VU state, complete host MXCSR and destination W must remain untouched.
- Another 1,024 ordered alias cases overwrite a later point input or basis column before its
  calculation. Each reference block adopts the same explicit destination-W contract.
- 3,584 generated continuation cases and 112 full-memory checks execute seven scalar boundaries:
  terrain point setup through zero-buffer dispatch, terrain point pairs through quick-height query
  dispatch, node/point contact setup through the next inverse query, ray setup through normalization
  dispatch, and the complete ray-contact result-building tail through metadata writes and epilogue.
  CPU, clocks/checkpoints, complete outgoing FP effects and other memory match outside retired VU
  and destination-W effects. Native VU state stays unchanged through each tested continuation.
  These are bounded segments, not complete end-to-end collision-algorithm replacements.
- Player, headless and historical checker build. All eleven native suites and twelve Python
  release-tool tests pass. Generated instruction fixtures and test inputs require the local game
  ELF; they are not committed or run by the game-free GitHub CI jobs.

## Gameplay and RAM boundary

Default and strict one-player runs complete 9,500 frames; actual four-player runs complete 8,500.
Every non-RAM frame-record field matches the baseline throughout both routes: scratchpad, library
calls and counts, draws and counts, sound and sample counts, thread state and EE clock. Default
runs perform no original comparisons and no VU0 microprogram calls. Strict contact, math, hierarchy
and projection comparisons pass, reproducing both default recording files byte-for-byte.

The native blocks execute 609,504 times in one-player and 254,389 times in four-player. All 27 run
in one-player; four-player exercises 26 (the alternate point sum ending `1cb904` is absent).
These are execution counts, not a timing or FPS measurement.

Raw RAM hashes differ on 3,907 one-player frames (starting at 5593) and 3,798 four-player frames
(starting at 4702). Complete 32 MB snapshots at one-player frame 7000 and four-player frame 6000
differ by 72 bytes in 35 words / 15 bytes in six words, respectively. Every differing word is a
fourth vector component:

- One-player has 27 contact-position W words in the vehicle `b10` result arrays and one cached
  contact-position W at Vehicle+`1e8c`. Those change from inherited 60.0 to zero. The cache copy
  is the retained scalar Z/W load/store at `189f98`/`189f9c`; it copies the preceding contact record.
- Four query-vector W words at `5253bc`/`5253cc`/`5253dc`/`5253ec` and three stack-vector W words
  account for the remaining one-player differences. Four-player differs in `5253ec` and five
  stack-vector W words. Existing destination payloads survive where the old block stored VU residue.
- The terrain-to-contact copy chain documented in [the inverse increment](NATIVE_BSP_INVERSES.md)
  remains compiled. Native terrain XYZ calculations stop injecting the inherited VF16.W into that
  chain. The prior generated body-result XYZ consumer test still passes with arbitrary input W.

The snapshot classification is bounded evidence, not a claim that every raw RAM byte is identical
or that every possible campaign consumer has been exercised. No new memory-hash mask, padding
restoration, shadow VU state or generated-code instrumentation is used for this change.

Window qualification covers 4,201 movie/menu frames and 6,501 frames each for one-player and
four-player. Reviewed captures show decoded movie pictures, level loading, rainy combat and four
advancing snow-arena views. Device logs report 23.8 / 60.6 / 35.8 ms of silence (movie / one-player /
four-player), with dropped frames and underruns; deterministic sound records match.
Full-campaign acceptance, flawless device audio and console visual parity remain unclaimed.
