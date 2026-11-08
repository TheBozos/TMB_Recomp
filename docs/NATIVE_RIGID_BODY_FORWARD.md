# Native rigid-body forward transforms

Ten rigid-body forward returns now use native record arithmetic, after
[completion of the known inverse sites](NATIVE_FINAL_INVERSES.md).
Forward coverage grows from 96 to 106 of 269 returns; inverse coverage stays
134 of 134. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `1ed478` | `applyLocalForce(force)` | World-force accumulator |
| `1ed4c4`, `1ed4dc` | `applyLocalForce(force, point)` | Force and point transforms, world-force and torque accumulators |
| `1ed544`, `1ed55c` | `applyLocalTorque(force, point)` | World torque and angular-state consumers |
| `1ed708` | `applyLocalImpulse(impulse)` | Momentum and linear velocity |
| `1ed754`, `1ed76c` | `applyLocalImpulse(impulse, point)` | Linear/angular momentum, meaningful inverse result and angular velocity |
| `1edf38` | `resolveForces` | Integrated coordinate-system position |
| `1ee07c` | `computeX` | Published world position and point velocity |

Forward arithmetic is unchanged: native calls calculate separately rounded XYZ,
retain destination W and leave VU registers, accumulator, flags and data memory
untouched. Original scalar consumers use XYZ. Unknown callers and historical
hosts retain the original full ABI. No engine or integration formula is changed.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/rigid-body-forward`, against main `79acf14`
(PR #50). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 5,120 generated sequences / 320 whole-memory comparisons.
  Aggregate coverage is 108,032 / 4,736. Each return has 256 helper-boundary cases
  through its actual tail and 256 complete owner entries. Four rounding modes,
  denormal controls, RAM aliases, rotated bases, zero/nonzero vectors, signed
  forces, zero/nonzero inverse mass and source aliases with momentum/position
  records are covered. Whole-memory entry samples reach active transforms.
- Complete force/torque entries execute both transforms where present, actual
  world-point translation, world-force accumulation, point subtraction, cross
  product and torque accumulation. Impulse entries also execute actual momentum
  addition, mass scaling and the existing native inverse angular-velocity leaf.
  Explicit per-site counts and world-consumer counts are required.
- Complete `resolveForces` entries retain skew-matrix construction, scalar matrix
  operations and their store/alias order, orthonormalization, momentum updates,
  actual `computeAuxiliarys`, the final forward and coordinate-system position
  subtraction. Zero and nonzero time steps run. The original matrix/integration
  behavior is compared directly; a guessed integration equation is not substituted.
- `computeX` entries execute the actual local-to-world position publication.
  Every new case then reads the resulting body state through the real
  `getPointVelocity`, including its position subtraction, angular cross product
  and linear-velocity addition. This happens before any comparison-only W
  normalization.
- Known simple entries require exact force/torque accumulation, momentum and
  linear/angular velocity publication with identity/rotated bases. Zero-step
  integration must preserve the seeded momenta. Computed world points must equal
  known translated positions. These assertions supplement original/native
  comparisons; they do not replace the original scalar consumers.
- The only opaque forward W locations are the actual owner's stack `SP+0c`,
  `SP+1c`, integration scratch `SP+7c`, or `RigidBody+4c` for `computeX`.
  Helper-boundary and complete-entry stack bases differ by their real frame sizes;
  exact output counts and allowed addresses are asserted. Reference W remains
  intact through all real consumers, including point velocity, then is normalized
  for comparison only. Meaningful inverse W is never excluded. No runtime RAM
  mask or VU-state reconstruction is introduced.
- CPU outside retired VU, host FP effects, clocks/checkpoints, per-site forward
  counts, world/action counts and final memory agree. Native VU stays untouched
  through the complete consumers.
- Historical forward ABI coverage grows to 6,208 cases / 388 whole-memory checks;
  inverse stays at 8,576 / 134. Existing coverage retains 786,432 hierarchy helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete BSP callers /
  32 whole-memory and 2,048 unknown ABI cases. Previous AI, vehicle/light and final
  inverse publication assertions remain enabled.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass. Earlier
AI, vehicle/light and final-inverse publication assertions remain enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recording files and snapshots are byte exact;
strict comparisons report no mismatch.

One-player reaches `resolveForces` 738 times and `computeX` ten times. The eight
local force/torque/impulse sites are cold on these replay routes, and four-player
makes no calls to the ten new forward returns. Those local paths are qualified
by complete generated entries and their scalar state consumers, not claimed as
runtime route coverage.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing driving,
AI opponents, combat/effects, four simultaneous views, decoded intro images and
return to the menu. Each route has five distinct captures.

Windowed 1P reaches `resolveForces` 583 times and `computeX` four times; the other
eight new sites remain cold. Four-player and movies make no calls to these ten
sites. Stereo 48 kHz, 16-bit WAV captures contain nonzero audio. Device underruns
remain: 50.5 ms silence in 1P, 132.8 ms in 4P and 20.7 ms in movies. These runs
are functional checks, not a timing or flawless-playback claim. Full campaign,
every rigid-body configuration and physical-controller/hardware acceptance
remain outside this increment.

## Remaining scope

There are 163 forward returns outside the native allowlist, with no known static
inverse return remaining. Other inline arithmetic, remaining hierarchy/VU helpers
and historical/unknown ABI support remain separate work. This is a call-site
inventory, not a count of all remaining emulation.
