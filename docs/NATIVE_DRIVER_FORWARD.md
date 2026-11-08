# Native driver forward transforms

Seven driver forward returns use native XYZ record arithmetic after the
[rigid-body forward increment](NATIVE_RIGID_BODY_FORWARD.md). Coverage grows
from 106 to 113 of 269 known forward returns (156 remain). Known inverse coverage
stays 134 of 134; unknown/historical callers retain the original ABI.
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `126618` | `driverGetOffsetFromPoint` | 150-unit world probe, Manhattan gate, actual LOS query and path-offset publication |
| `1267f0` | `driverGetOffsetFromPoint` | Actual RPH rotation and bounded-angle producer, lateral path-offset publication |
| `1276a4` | `driverCalcLookahead` | Distance/speed prediction and translated target publication |
| `128704`, `128744` | `driverCheckDrivingLOS` | Special/ordinary world query start points |
| `1287dc`, `12880c` | `driverCheckDrivingLOS` | Special/ordinary world query endpoints, actual LOS and driver-state consumers |

Native arithmetic preserves separately rounded XYZ, destination W and the VU
registers, accumulator, flags and data memory. The original scalar driver logic,
collision queries, matrix/trigonometric/angle producers and meaningful inverse
outputs remain intact. This change extends the audited native return selection;
it does not change steering or collision policy.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/driver-forward`, against immediate parent
`94ad67d` (PR #51). Retail data, generated bodies and replay artifacts stay
outside Git.

- Added coverage is 3,584 generated sequences / 224 whole-memory comparisons:
  each return has 256 helper-boundary sequences through its actual tail and 256
  complete owner entries. Four rounding modes, FTZ/DAZ, RAM aliases, identity and
  rotated bases, opaque W payloads, zero/tiny/signed vectors and speeds, and
  lookahead outputs aliasing vehicle/target positions are covered.
- Path probes exercise active and gated entries, near/far Manhattan checks,
  actual `hdLosTest`, `terrainCollision` and `hdWalkColGrid`, clear-path flags and
  publication. Lateral entries execute real scalar RPH construction, original
  sine/cosine calls and angle wrapping, zero-width/node gates, signed offsets and
  path-node width scaling before the final vector addition.
- Lookahead entries cover missing targets, stopped targets, near-distance gates,
  moving/reversing targets, rotated bases and aliased publication. Known complete
  entries require exact translated results, alongside original/native comparison.
- Driving-LOS entries execute counter increment/wrap, actual point subtraction,
  normalization and matrix construction, speed/height adjustments, short-target
  endpoint selection, real queries and clear-state/mask publication. Existing
  inverse consumers run where enabled and their full meaningful W is compared.
- Query fixtures use actual empty terrain and endpoints outside the collision
  grid X bounds. No collision/query result is stubbed. These fixtures qualify
  real clear-path consumers; they do not claim obstructed-world coverage.
  Runtime reach is reported separately below.
- Explicit per-return and query/producer counts are required. Known-state
  assertions publish 32 probe cases, 32 lateral offsets, 32 lookahead positions
  and 64 LOS endpoint pairs. Whole-memory complete-entry samples reach active
  transforms. CPU outside retired VU, clocks/checkpoints, host FP effects, query
  endpoints and final RAM agree. Native VU remains untouched through complete
  producers and consumers.
- Opaque forward W is restricted to the actual frame's `SP+ac` (probe), `SP+5c`
  (lateral), `SP+1c` (lookahead), or `SP+1c`/`SP+2c` (LOS). Exact output counts
  and addresses are asserted separately for boundary and complete-entry frames.
  Original W stays live through all real consumers before comparison-only
  normalization. Inverses replacing forward scratch remove that exclusion and
  compare full XYZW. No runtime RAM mask or VU-bank reconstruction is added.
- Aggregate generated caller coverage is 111,616 sequences / 4,960 whole-memory
  checks. Historical forward ABI coverage is 6,656 / 416; inverse stays 8,576 /
  134. Existing hierarchy, geometric, BSP and unknown-ABI coverage remains.
  Earlier AI, vehicle/light and final inverse publication assertions stay enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass. Earlier AI,
vehicle/light and final inverse publication assertions remain enabled.

Default and strict runs complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches immediate parent `94ad67d`: raw RAM,
scratchpad, calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB
RAM images are identical. Strict/default recording files and snapshots are byte
exact; strict checks report no mismatch.

One-player reaches the new probe `126618` ten times and LOS sites `128704`,
`128744`, `1287dc`, `12880c` 198, 799, 136 and 799 times respectively. Lateral
`1267f0` and pursuit `1276a4` are cold on these replay routes and are qualified by
complete generated owners and their real scalar consumers. Four-player makes no
calls to any of these seven sites.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing driving,
AI opponents, combat/effects, four simultaneous views, decoded intro footage and
return to the menu. Each route has five distinct captures. Stereo 48 kHz,
16-bit WAV recordings contain nonzero audio.

Windowed one-player reaches the probe five times and the four LOS returns 112,
457, 93 and 457 times; lateral/lookahead remain cold. Four-player and movies make
no calls to these seven sites. Device underruns remain: 52.7 ms silence in 1P,
33.2 ms in 4P and 27.6 ms in movies. These are functional checks, not a timing or
flawless-playback claim. Full campaign, every driver/world configuration and
physical-controller/hardware acceptance remain outside this increment.
