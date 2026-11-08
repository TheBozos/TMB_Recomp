# Native weather viewport transform

The forward-transform return `1e8a34` in `particleUpdateRainViewport` (`1e87a0`)
now uses native record arithmetic, extending the
[camera increment](NATIVE_CAMERA_TRANSFORMS.md). Forward coverage grows from 89
to 90 sites; inverse coverage stays at 89. BozoEngine stays pinned to `4a8f70a`.

The existing native helper snapshots its operands, calculates separately rounded
XYZ and retains destination W. Its stack result is consumed as XYZ by camera
translation, weather-position publication, grid quantization and projection
setup. The complete tail never reads that temporary W. Published projection
XYZW, grid-scale W and explicit coordinate-system matrix W remain meaningful
data and are compared exactly. No register shadow or VU reconstruction is added.
Historical hosts and unknown callers retain the complete original ABI.

## Complete viewport comparisons

Evidence is under `D:/BozoScratch/weather-viewport`, against main `1067036`
(PR #42). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 512 generated sequences / 32 whole-memory comparisons.
  Aggregate coverage is 76,800 / 2,784. Four host rounding modes, denormal
  controls, all four viewport slots, RAM aliases, three seeded weather records
  and aliased coordinate-system/projection output are covered.
- 256 cases start at the transform boundary and run the entire remaining
  viewport through return. They include signed zero/subnormal inputs and
  values at and adjacent to weather-cell boundaries, preserving the original
  negative-coordinate quantization order.
- Another 256 cases enter the actual viewport function across 16 scenarios:
  disabled weather (16 early returns), fade updates enabled/disabled, null and
  supplied coordinate systems, counters 0/1/20/21/unsigned-negative values,
  timer steps 1/2, fog updates and identity/projected publication. The timer,
  fog and view accessors execute their real generated code; none is stubbed.
- Both comparison runs use the existing production-native point projection and
  weather identity helpers. Their original implementations are independently
  qualified by the projection suite. Explicit assertions check transform reach
  and the selected identity/projection branch. Whole-memory entry samples cover
  active fog updates and aliased projection output.
- CPU outside retired VU state, outgoing host FP effects, clocks/checkpoints
  and final memory agree. Native VU registers, accumulator, flags and data
  memory remain untouched through the full viewport. Only the transform's
  stack W is excluded after all real consumers execute; an assertion restricts
  the exclusion to that word in the appropriate entry/continuation frame.
- Historical forward ABI coverage grows to 5,184 / 324 cases / whole-memory
  checks; inverse stays at 5,696 / 89. Existing hierarchy coverage retains
  786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete
  BSP callers / 32 whole-memory and 2,048 unknown ABI cases.
- Player, headless runner, historical checker and all test targets build.
  All eleven native suites and twelve Python tooling tests pass. The hierarchy
  suite also passes with the explicit branch-reach and active whole-memory samples.

## Replay qualification

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames.
Every recorded field matches the immediate parent: raw RAM, scratchpad, calls,
draws, sound, threads, counts and EE clocks. Both sampled full 32 MiB RAM
snapshots match exactly. Strict/default recording files and snapshots are
byte-identical. No observer, RAM mask or VU restoration is needed.

The new site executes 5,560 / 15,192 times in one-player / four-player. Total
native forward calls become 1,163,072 / 367,262. Default routes make zero
original-helper comparisons and execute zero VU0 microprograms. These establish
coverage, not a measured speedup.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, with isolated scratch
user data. Reviewed captures show decoded movie action (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), advancing gameplay and rain
(one-player 5,500 / 6,300), and four advancing snow-arena views
(four-player 5,500 / 6,300). The new site executes 1,706 / 7,196 times in windowed
one-player / four-player and is cold in the movie route. These are bounded
functional checks, not pixel-exact windowed or console parity.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
29.1 / 50.8 / 35.6 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound is exact; flawless device audio
and subjective listening are not claimed.

## Remaining scope

The two transform inventories now contain 90 native forward and 89 native inverse
sites. Outside their allowlists remain 179 forward and 45 inverse sites, 224 in
total across the game. This structural count excludes other helper families and
inline VU operations. Other weather functions, full campaign acceptance and
console visual/audio parity remain separate work.
