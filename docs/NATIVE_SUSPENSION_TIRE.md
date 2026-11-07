# Native suspension and tire-height transforms

Two hot vehicle calls of forward helper `1da298` now use the existing native XYZ
calculation. The forward allowlist grows from 66 to 68; inverse coverage stays at
82. BozoEngine remains pinned to `4a8f70a`.

| Return PC | Owner | Actual consumer |
| --- | --- | --- |
| `1888e8` | `suspCreateSuspensionMat` (`188738`) | Transformed Z contributes to suspension ride height at vehicle + `55c` |
| `18b4c0` | `tireUpdateCarHeight` (`18b420`) | Pivot reconstruction, vehicle position and coordinate-system publication, then height smoothing/clamp |

The native transform snapshots its operands, writes XYZ and preserves destination
W. It leaves VU registers, accumulator, flags and data memory untouched. Historical
hosts and unknown callers retain the original complete ABI. Suspension angle/RPH
matrix generation, tire pivot selection, scalar vector consumers, smoothing and
physics remain compiled game code; this increment replaces their transform leaves.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/suspension-tire`, compared with main
`9156063` (PR #35). Generated code, retail assets and runtime captures stay outside Git.

- Both new consumer tails run through their actual generated return. Suspension
  consumes only output Z; tire-height consumes XYZ through real subtract/add/copy
  routines, including the coordinate-system position and height update. Their
  output W words are unused by these consumers. Meaningful coordinate-system W
  remains unchanged and is compared.
- Added coverage is 1,024 sequences / 48 whole-memory comparisons, including 256
  complete tire-height entry calls / 16 whole-memory comparisons. These execute
  actual pivot selection, lowest-tire selection, input construction, transform and
  complete scalar tail. The remaining new cases enter at the transform boundary.
  Suspension angle/matrix production is outside the generated-consumer fixture.
- Four host FP modes, RAM aliases, arbitrary W payloads, signed zero/subnormal and
  integer-adjacent suspension inputs, four tire selections, and values immediately
  around the -3 height clamp are covered. CPU outside retired VU, outgoing host FP
  effects, clocks/checkpoints and consumer memory agree. Native VU stays untouched.
  Original forward W is normalized only after the complete real consumer has run.
- Aggregate consumer coverage is 61,952 sequences / 1,952 whole-memory comparisons.
  Historical forward coverage is 3,776 / 236; inverse coverage stays at 5,248 / 82.
  The hierarchy suite retains 786,432 helper cases / 384 whole-memory, 8,000 geometric
  edges, 1,024 complete recursive BSP callers / 32 whole-memory and 2,048 unknown ABI
  cases. The math suite retains 1,048,576 / 512 and 2,048 unknown ABI cases.
- Player, headless, historical checker and all native test targets rebuild. All
  eleven native suites and twelve Python tooling tests pass.

## Gameplay, loading and movies

Default and strict runs complete 9,500 one-player and 8,500 actual four-player
frames. All non-RAM frame fields match the preceding main: scratchpad, calls/counts,
draws/counts, sound/sample counts, threads and EE clock. One-player RAM hashes match
at every frame. Four-player has one RAM-hash difference, at recording frame 4702.
Full 32 MB snapshots at one-player frame 7000 and four-player frame 6000 are exact.
Strict contact, hierarchy, math and projection checks pass; their recordings and
snapshots are byte-identical to default. Default performs zero original comparisons
and zero VU0 microprogram calls; strict intentionally runs original oracles.

A focused paired replay localizes the transient difference to one stack word,
`8ff2ec`: tire-height transform output `SP+1c`. The original writes inherited
`VF16.w` (zero); native retains destination bits `0000000c`. Eight paired writes
across two logical frames identify return `18b4c0` as the writer, at matching EE
cycles. The generated tail's subtract/add/copy consumers read only XYZ. Paired
observer recordings reproduce the qualified baseline/native prefixes exactly,
and observer snapshots reproduce the corresponding one-word-difference snapshots.
The snapshot uses graphics frame 4703; the differing recording index is 4702.
This is a bounded unused-W difference, not raw-RAM identity or console parity.
No production mask or VU-state reconstruction is introduced. Observer sources are
restored, original math/hierarchy object hashes verified and clean binaries rebuilt.
All eleven suites and both complete default/strict routes pass again after restoration;
the player, headless and checker binaries contain no observer markers.

The two new native returns execute 61,829 times in one-player and 30,384 times in
four-player. Total native forward counts are 1,060,013 / 260,479. These are reached
call counts, not a measured speedup.

Fresh windowed routes complete 4,200 movie/menu frames and 6,500 frames each for
one-player and actual four-player, through clean exit with isolated scratch user
data. Reviewed captures show decoded movie pictures (frame 1000), Junkyard loading
(one-player 4500), active combat (one-player 6300), and four advancing snow-arena
views (four-player 5500/6300). All three stereo 48 kHz WAV recordings contain audio.
Device logs report 26.1 / 64.2 / 36.6 ms of silence (movie / one-player / four-player),
with underruns and dropped frames. Deterministic gameplay sound matches; flawless
device audio, subjective listening and full campaign/console parity are not claimed.

## Remaining scope

The two helper inventories now contain 68 native forward and 82 native inverse
sites. Outside those allowlists remain 201 forward and 52 inverse sites: 253 across
the game, including sites inside otherwise replaced functions. This is a structural
source inventory, not a live cost measurement, and excludes other helper families
and inline VU operations. The previously closed narrow hierarchy/terrain direct-call
inventory remains closed. Other helpers, particle/camera callers, full campaign
acceptance and console visual parity remain separate work.
