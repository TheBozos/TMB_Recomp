# Native attractor-group spatial transforms

Three inverse-transform returns now use native record calculations, extending
the [tow-truck increment](NATIVE_TOW_TRUCK.md). Forward coverage stays at 79 sites;
inverse coverage grows from 86 to 89. BozoEngine remains pinned to `4a8f70a`.

| Return PC of helper `1da1e8` | Owner | Consumers |
| --- | --- | --- |
| `108d6c` | `attractGetGroupTestPointIsIn` (`108c60`) | Local XY bounds and ordered group selection |
| `109870` | `attractGetCenterOfGroup` (`1097c8`) | Rotated half-extents plus group origin, then XYZ publication |
| `1099a0` | `attractGetRandomPositionInGroup` (`1098e8`) | Rotated random offsets plus group origin, then XYZ publication |

The existing native inverse calculation snapshots operands, computes separately
rounded XYZ and copies the first basis vector's W as data. Native calls leave VU
registers, accumulator, flags and data memory untouched. Historical hosts and
unknown callers keep the complete original ABI. Group ordering, height gates,
boundary comparisons, random generation, rotation construction and world-position
publication remain compiled game code.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/attractor-transforms`, against main
`0bf979a` (PR #39). Generated game code, retail data, recordings and captures stay
outside Git.

- Added coverage is 1,536 generated consumer sequences / 96 whole-memory
  comparisons. Aggregate coverage is 71,168 / 2,432. All final fixture memory,
  including W, is compared without a forward-W exclusion for the new cases.
  CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and memory
  agree; native VU stays untouched. Four host rounding modes, denormal controls,
  RAM aliases and arbitrary opaque W payloads are covered.
- Every point-test case runs the complete selection tail and return, including
  subsequent group iterations. Literal boundary fixtures verify the rotated
  strict bounds, signed zero and values immediately inside/outside the upper
  bound. Another 256 cases enter the actual function: empty/negative counts,
  height rejection, rotated/unrotated groups, inclusive unrotated upper bounds,
  first-match ordering and a third-group match after two rotated misses.
- Center and random-position cases run through their complete publication tails,
  retaining destination W and publishing group Z. Another 256 cases for each
  enter at the actual function entry, including zero rotation, positive/negative
  rotation and zero/reversed extents. Rotation inputs stay within thirty degrees;
  wider trigonometric argument reduction is outside these fixtures.
- Random-position entry cases execute the actual generated `mathfRandf` and
  `rand`, with guest-backed generator state restored between comparison runs.
  An explicit assertion verifies two real `rand` dispatches per entry. These
  fixtures exercise the generated producer; runtime native-library behavior is
  separately covered by the paired gameplay routes.
- Historical inverse coverage grows to 5,696 / 89 cases / whole-memory checks;
  historical forward coverage stays at 4,480 / 280. Existing hierarchy coverage
  retains 786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024
  complete BSP callers / 32 whole-memory and 2,048 unknown ABI cases.
- All eleven native suites and twelve Python tooling tests pass. Current emitted
  inverse-transform vector results are stored before MXCSR restoration; the
  original/native comparisons pass in all four caller rounding modes.

## Gameplay and RAM boundary

Default and strict builds complete 9,500 one-player and 8,500 actual four-player
frames. Strict/default recording files and full 32 MiB snapshots are identical.
Against the tow-truck baseline, every scratchpad, call, draw, sound, thread, count
and EE-clock field agrees. The entire four-player recording and both sampled full
RAM snapshots also agree. The three sites execute 14,567 / 25,280 / 3 times in
one-player respectively (39,850 total); all are cold in this four-player route.
Four-player therefore supplies general regression coverage, not site reach.
Default routes make zero original-helper comparisons and execute zero VU0
microprograms. Counts establish coverage, not a measured speedup.

One-player RAM hashes differ on 664 frames in intervals 7,621–7,629,
7,732–8,066 and 8,446–8,765. A temporary paired observer reproduces both complete
qualified recordings and every raw callback hash. Every difference is confined
to four `Vehicle+52c` peel-out vector W fields:

| RAM word | Baseline / native | Differing frames |
| --- | --- | --- |
| `26077c` | `00000000` / `02000200` | 9 |
| `26617c` | `00000000` / `02000200` | 9 |
| `268e7c` | `00000000` / `02000200` | 664 |
| `25da7c` | `00000000` / `42700000` | 23 |

Every differing word matches its last observed writer: the existing forward
return `1abd60` in `vehicleUpdatePeelingOut`. The inherited `VF16.W` supplies those
bits; XYZ is identical. Its `vehicleSetCsMat` consumer uses XYZ-only addition and
does not consume peel-out W. The complete generated coordinate-system consumer
tests now explicitly include all three observed W payloads across rounding modes
and matrix aliases, alongside the existing fuzz cases.

Replacing only these four words on a diagnostic RAM copy reproduces every
baseline frame hash, with zero unaccounted frames or writer-provenance mismatches.
No RAM mask or VU-state reconstruction ships. The three temporarily instrumented
production sources and their original object hashes were restored exactly; all
eleven rebuilt native suites pass again.

After observer removal, clean default/strict recordings and full snapshots
reproduce the qualified results byte-for-byte. Player, headless and checker
binaries contain no temporary observer markers.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, with isolated scratch
user data. Reviewed captures show decoded movie pictures (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), active one-player gameplay
(5,500 / 6,300), and four advancing snow-arena views with snow/tire effects
(four-player 5,500 / 6,300). Windowed one-player reaches the point-test and center
sites 6,613 / 11,993 times; random-position inverse is cold in that shorter route.
All three are cold in the four-player and movie routes. These captures establish
bounded functionality, not pixel-exact windowed or console parity.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
13.4 / 61.3 / 39.0 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound matches the baseline; flawless
device audio and subjective listening are not claimed.

## Remaining scope

These two helper inventories now contain 79 native forward and 89 native inverse
sites. Outside their allowlists remain 190 forward and 45 inverse sites, 235 in
total across the game. This is a structural source inventory, not a live cost
measurement, and excludes other helper families and inline VU operations. Full
campaign acceptance and console visual/audio parity remain separate work.
