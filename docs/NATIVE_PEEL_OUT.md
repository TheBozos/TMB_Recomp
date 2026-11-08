# Native vehicle peel-out transform

The forward-transform return `1abd60` in `vehicleUpdatePeelingOut` (`1abb18`)
now uses native record arithmetic, extending the
[attractor-group increment](NATIVE_ATTRACTOR_GROUPS.md). Forward coverage grows
from 79 to 80 sites; inverse coverage stays at 89. BozoEngine stays at `4a8f70a`.

The existing native helper snapshots its operands, computes separately rounded
XYZ and retains destination W. Its result at `Vehicle+520` feeds the XYZ-only
addition in `vehicleSetCsMat`; the coordinate-system position retains its own W.
Native calls leave VU registers, accumulator, flags and data memory untouched.
Historical hosts and unknown callers retain the complete original ABI. Peel-out
selection, timer parity, steering, phase/amplitude updates, sine, smoothing and
vehicle-pose publication remain compiled game code.

## Generated producer and consumer checks

Evidence is under `D:/BozoScratch/peel-out-transform`, against main `81d72db`
(PR #40). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 512 composed producer/consumer sequences / 32 whole-memory
  comparisons. Aggregate coverage is 71,680 / 2,464. Each case completes the
  peel-out tail and then the real generated `vehicleSetCsMat` consumer, including
  its production native matrix products and inline XYZ addition. Both reference
  and native producer runs use that production consumer; the separate vehicle
  suite qualifies the consumer against its original implementation.
- 256 cases enter the actual peel-out function across 16 scenarios: inactive
  offsets, fast/smoothed decay, snap-to-zero, speed gates, reset/reuse, timer
  parity, steering direction, phase clamps and amplitude growth. The actual
  timer, bound-angle, smoothing and sine routines execute. Trigonometric inputs
  are bounded to half a radian; wider argument reduction is outside these fixtures.
- Another 256 cases start at the helper boundary with signed zero, signed
  subnormals and positive/negative unit and fractional local offsets. Four host
  rounding modes, denormal controls, RAM aliases, coordinate-system/matrix
  aliases and arbitrary opaque W payloads are covered.
- CPU outside retired VU state, outgoing host FP effects, clocks/checkpoints and
  final memory agree. Native VU stays untouched through the complete chain.
  Only the source `Vehicle+52c` W is excluded, after all real consumers run;
  an assertion restricts that exclusion to this exact word. No published pose
  component is excluded.
- Historical forward coverage grows to 4,544 / 284 cases / whole-memory checks;
  inverse stays at 5,696 / 89. Existing hierarchy coverage retains 786,432 helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete BSP callers /
  32 whole-memory and 2,048 unknown ABI cases.
- Player, headless runner, historical checker and all test targets build.
  All eleven native suites and twelve Python tooling tests pass.

## Gameplay and RAM boundary

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Strict/default recording files and full 32 MiB snapshots are identical.
Every scratchpad, call, draw, sound, thread, count and EE-clock field matches the
immediate parent. The four-player frame records and both sampled full RAM
snapshots also match it exactly.

One-player RAM hashes differ on 664 frames, first 7,621 and last 8,765. A fresh
passive observer reproduces the production recording and checks all 9,500 frames
against the previous increment's qualified immediate-parent observer ledger.
Replacing only the following four words on a diagnostic RAM copy reproduces
every parent RAM hash, with zero unexplained differences:

| RAM word (`Vehicle+52c`) | Parent / native | Differing frames |
| --- | --- | --- |
| `26077c` | `02000200` / `00000000` | 9 |
| `26617c` | `02000200` / `00000000` | 9 |
| `268e7c` | `02000200` / `00000000` | 664 |
| `25da7c` | `42700000` / `00000000` | 23 |

These reverse the [previously traced peel-out W writes](NATIVE_ATTRACTOR_GROUPS.md):
the original producer borrowed `VF16.W`; the native producer retains destination
W. The real coordinate-system consumer uses only XYZ. Both complete frame-record
payloads now also match the older pre-attractor tow-truck baseline. No RAM mask
or VU reconstruction ships. Temporary headless observer source and its original
object were restored exactly; production binaries contain no observer markers.

The new site executes 3,890 / 7,374 times in headless one-player / four-player.
Total native forward calls become 1,141,908 / 302,704. Default routes make zero
original-helper comparisons and execute zero VU0 microprograms. These are
coverage counts, not a measured speedup.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, using isolated scratch
user data. Reviewed captures show decoded movie playback (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), advancing one-player gameplay
(5,500 / 6,300), and four advancing snow-arena views with snow/tire effects
(four-player 5,500 / 6,300). The new site executes 1,809 / 3,510 times in windowed
one-player / four-player; it is cold in the movie route. These checks establish
bounded functionality, not pixel-exact windowed or console parity.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
26.6 / 62.8 / 31.4 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound matches the parent; flawless
device audio and subjective listening are not claimed.

## Remaining scope

These two helper inventories now contain 80 native forward and 89 native inverse
sites. Outside their allowlists remain 189 forward and 45 inverse sites, 234 in
total across the game. This is a structural inventory, not a live cost
measurement, and excludes other helper families and inline VU operations.
Full campaign acceptance and console visual/audio parity remain separate work.
