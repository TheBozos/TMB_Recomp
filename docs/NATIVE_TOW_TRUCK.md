# Native tow-truck launcher transforms

Four transform returns in `UpdateTowTruck` (`150b60`) now use native record
calculations, extending the [shadow increment](NATIVE_SHADOWS.md). Forward
coverage grows from 76 to 79 sites and inverse coverage from 85 to 86.
BozoEngine remains pinned to `4a8f70a`.

| Return PC | Helper | Following consumers |
| --- | --- | --- |
| `150bc8` | Forward `1da298` | Base-position XYZ addition and publication |
| `150cc0` | Forward `1da298` | Offset addition and three-axis position smoothing |
| `150d2c` | Forward `1da298` | Vehicle-space conversion and attachment-position publication |
| `150d98` | Inverse `1da1e8` | Local-direction subtraction, normalization and orientation construction |

The existing native forward calculation snapshots operands, computes separately
rounded XYZ and retains destination W. The inverse snapshots its operands and
publishes the first basis vector's W as data. The shared forward temporary is
consumed as XYZ and then completely replaced by the real inverse. Native paths
leave VU registers, accumulator, flags and data memory untouched. Historical
hosts and unknown callers keep the complete original ABI. Launcher selection,
tilt/clamping, rotation construction, smoothing, normalization, material selection
and game checkpoints remain compiled game code.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/tow-truck-transforms`, against main
`f1118f4` (PR #38). Generated game code, retail data, recordings and captures stay
outside Git.

- Added coverage is 2,048 complete generated consumer sequences / 128 whole-memory
  comparisons. Aggregate coverage is 69,632 / 2,336. The tow-truck cases compare
  all final memory, including stack W, without any forward-W exclusion: an
  explicit assertion checks that the inverse has overwritten the shared temporary.
- Every new site runs through the complete scalar tail and return: vector
  addition/subtraction, smoothing, normalized direction, orientation matrix,
  attachment position, material-pointer selection and visibility-byte writes.
  Zero-length and vertical directions exercise the actual normalization and
  orientation fallback branches. Consumers and trigonometry run their actual
  generated code. The visibility byte starts nonzero, making clearing and
  retention observable, including the disabled early returns.
- 512 of those cases enter at the actual function entry: 256 complete
  updates, 192 updates reusing the existing launcher position and 64 disabled
  early returns. The complete updates include all four transform calls. Another
  256 start at the real tilt/clamp and rotation producer; 256 start at the final
  smoothed-Z producer. Rotation inputs stay within half a radian; wider argument
  reduction is outside these consumer fixtures.
- Four host FP modes, denormal controls, RAM aliases and arbitrary opaque W
  payloads are covered. CPU outside retired VU state, outgoing host FP effects,
  clocks/checkpoints and memory agree. Native VU stays untouched through the
  complete consumers. Inverse W is compared exactly as meaningful data.
- Historical forward coverage grows to 4,480 / 280 cases / whole-memory checks;
  historical inverse coverage grows to 5,504 / 86. Existing hierarchy coverage
  retains 786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024
  complete recursive BSP callers / 32 whole-memory and 2,048 unknown ABI cases.
- Player, headless runner, historical checker and all native test targets build.
  All eleven native suites and twelve Python tooling tests pass. The hierarchy
  suite passes again with the explicit no-W-exclusion assertion enabled.

## Gameplay, loading and movies

Default and strict-check builds complete 9,500 one-player frames and 8,500 actual
four-player frames. Every recorded frame field agrees with the shadow baseline,
including raw RAM, scratchpad, calls, draws, sound, threads, counts and EE clocks.
Both full 32 MiB snapshots also agree. Strict/default recording files and snapshots
are byte-identical. No observer, RAM mask or VU-state restoration is needed.

Each of the four new sites executes 3,596 times in one-player and 3,798 times in
four-player: 14,384 / 15,192 new native executions in total. Total native forward
calls become 1,138,018 / 295,330. Default routes make zero original-helper
comparisons and execute zero VU0 microprograms. These are coverage counts, not a
measured speedup.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, using isolated scratch
user data. Reviewed captures show decoded movie playback (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), active one-player gameplay with the
tow-truck launcher (5,500 / 6,300), and four advancing snow-arena views with
snow/tire effects and the attached tow-truck launcher (four-player 5,500 / 6,300).
Each new site runs 1,706 / 1,799 times in windowed one-player / four-player:
6,824 / 7,196 new native executions in total. These captures establish bounded
functional coverage, not pixel-exact windowed or console parity.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
21.0 / 54.5 / 8.0 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound matches the shadow baseline;
flawless device audio and subjective listening are not claimed. Production player,
headless and checker binaries contain no prior temporary observer markers.

## Remaining scope

These two helper inventories now contain 79 native forward and 86 native inverse
sites. Outside their allowlists remain 190 forward and 48 inverse sites, 238 in
total across the game. This is a structural source inventory, not a live cost
measurement, and excludes other helper families and inline VU operations. Full
campaign acceptance and console visual/audio parity remain separate work.
