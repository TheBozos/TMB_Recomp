# Native shadow transforms

Six forward-transform helper returns now use native XYZ record calculations,
extending the [particle-emission increment](NATIVE_PARTICLE_EMISSION.md). Forward
coverage grows from 70 to 76 sites; inverse coverage stays at 85. BozoEngine remains
pinned to `4a8f70a`.

| Return PC of helper `1da298` | Owner | Consumer |
| --- | --- | --- |
| `15e1b8` | `shadowUpdate` (`15df38`) | Vehicle shadow world vertices, height offset and packed XYZ publication |
| `1b42c8` | `UpdateShadowTrans` (`1b41d0`) | In-place weapon motion direction, then coordinate-system position update |
| `1b430c`, `1b43a8`, `1b4434`, `1b44c0` | `UpdateShadowTrans` | Four world-space points for shadow height sampling |

The existing native calculation snapshots operands before overlapping writes,
computes separately rounded XYZ and retains destination W. Actual following world
sums, packed vertex writes and the first height-query vector copy consume XYZ only.
Later terrain-height code also copies complete vectors, retaining opaque W in
temporary endpoints and the flat-height result; its height/normal consumers use XYZ. VU
registers, accumulator, flags and data memory stay untouched. Historical hosts and
unknown callers retain the complete original ABI. Shadow selection, matrix
construction, height queries, pitch/orientation, smoothing and game checkpoints
remain compiled game code.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/shadow-transforms`, against main `0de15eb`
(PR #37). Generated game code, retail data, recordings and captures stay outside Git.

- Added coverage is 3,072 consumer sequences / 128 whole-memory comparisons.
  Aggregate coverage is 67,584 / 2,208. CPU outside retired VU, outgoing host FP
  effects, clocks/checkpoints and consumer memory agree; native VU stays untouched.
  Four host FP modes, denormal controls, RAM aliases and arbitrary opaque stack-W
  payloads are covered. Original forward W is normalized only after all actual
  consumers have run.
- Vehicle coverage includes 256 complete cached-height vertex loops / 16 extra
  whole-memory cases, with one to four vertices, actual successive helper calls,
  world sums, negative-height clamp, packed XYZ vertices and shadow-center
  publication through return. Signed zero, signed subnormals and heights around
  one are included. Shadow/matrix selection and uncached terrain queries are
  outside these loop fixtures.
- The remaining vehicle cases and every weapon transform run through the actual
  `hdHatTest` prefix: its XYZ copy, downward-ray Z adjustment and global point
  publication, stopping at its first `memset` dispatch. Terrain collision beyond
  that point is outside these bounded fixtures; no height-query stub is used.
- Another 256 weapon cases / 16 extra whole-memory enter at `1b429c`, execute the
  actual movement-Z producer, in-place transform, coordinate-system position
  update and first transformed corner through the same height-query prefix.
  The other weapon cases enter at individual helper boundaries. Coordinate-system
  position W is retained and compared exactly. Later height results, pitch and
  smoothing are not exercised by these consumer fixtures.
- Historical forward coverage grows to 4,288 / 268 cases / whole-memory checks;
  inverse coverage stays at 5,440 / 85. The hierarchy suite retains 786,432 helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete recursive BSP
  callers / 32 whole-memory and 2,048 unknown ABI cases. The math suite retains
  1,048,576 / 512 and 2,048 unknown ABI cases.
- Player, headless, historical checker and all native test targets rebuild. All
  eleven native suites and twelve Python tooling tests pass.

## Gameplay, loading and movies

The default and strict-check builds complete 9,500 one-player frames and 8,500
actual four-player frames. Strict/default recordings and full 32 MiB snapshots
are byte-exact. Against the previous increment, all scratchpad, call, draw, sound,
thread, count and EE-clock fields agree on every frame; the four-player recording
and both full snapshots also agree exactly.

One-player RAM hashes differ on 3,599 frames, first at 5,593. A temporary paired
store observer reproduces both complete qualified recordings and every raw frame
hash. All differences are confined to four opaque stack-W words: vehicle output
`8ff57c`, terrain-height temporary endpoints `8ff38c` / `8ff39c`, and the
flat-height result `8ff43c`. The latter three are actual 64-bit vector copies in
`terrainGetHAT` (`1efa38`), reached through `hdHatTest` return `1ced28` and flat
height return `1efb24`. Height calculations and the following normal copy consume
XYZ. Each differing word matches its last traced writer; replacing these four
words on a diagnostic RAM copy reproduces every baseline frame hash, with zero
unaccounted frames. No observer, RAM mask or VU-state restoration ships. All 32
generated sources and 35 production object hashes were restored exactly afterward.

The vehicle site runs 25,186 times in the one-player route and is cold in the
four-player route. The five weapon sites are cold in both; their acceptance comes
from generated-consumer comparisons, not a claim that these routes cover them.
Total native forward calls are 1,127,230 / 283,936 (one-player / four-player).
Default runs make zero original-helper comparisons and execute zero VU0
microprograms. These counts are coverage evidence, not a speedup measurement.

Fresh windowed gameplay/loading/movie checks are in progress.

## Remaining scope

The two helper inventories now have 76 native forward and 85 native inverse sites.
Outside their allowlists remain 193 forward and 49 inverse sites, 242 in total
across the game, including sites inside otherwise replaced functions. This is a
structural source inventory, not a live cost measurement, and excludes other helper
families and inline VU operations. Full campaign acceptance and console visual/audio
parity remain separate work.
