# Native rigid-body inverses

Seven rigid-body inverse-transform returns now use native record arithmetic,
extending the [collision/drift increment](NATIVE_COLLISION_DRIFT_INVERSES.md).
Inverse coverage grows from 105 to 112 sites; forward coverage stays at 95.
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Result and consumers |
| --- | --- | --- |
| `1ed6c4` | `RigidBody::applyWorldImpulse` with application point | Published angular velocity, then real point velocity |
| `1ed8ec`, `1ed92c` | Two-body `resolveCollision` | Cross/dot products, impulse magnitude and both bodies' impulses |
| `1edaec`, `1edc04` | Fixed-surface `resolveCollision` | Normal impulse, tangent normalization, friction choice and friction impulse |
| `1edd94` | Moving-surface `resolveCollision` | Relative velocity, impulse magnitude and impulse application |
| `1ee030` | `RigidBody::computeAuxiliarys` | Published angular velocity, then real point velocity |

The existing native inverse snapshots its operands, computes separately rounded
XYZ and copies the first basis column's W as data. All four output components
remain exact. Native calls leave VU registers, accumulator, flags and data memory
untouched. Historical hosts and unknown callers retain the full original ABI.
Scalar inertia/matrix production, collision decisions, cross/dot products,
friction and impulse application remain generated game code.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/rigid-body-inverses`, against main `a36c634`
(PR #45). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 3,584 sequences / 224 whole-memory comparisons. Aggregate
  coverage is 91,136 / 3,680. Four host rounding modes, denormal controls,
  cached/uncached RAM aliases and arbitrary first-basis W payloads are covered.
- Each site has 256 inverse-boundary cases through its real return and 256
  complete generated function entries. The 1,792 entries include 1,472 reached
  inverse paths and 320 separating-contact/small-speed rejection paths.
  Per-site original/native call counts distinguish those paths explicitly.
- World-impulse entries run actual momentum addition, scaling, lever-arm cross
  products and angular velocity publication. Auxiliary entries run the actual
  inertia/matrix producer. Both feed the published velocity through the real
  `getPointVelocity` function, including its lever-arm cross/add consumer.
- Two-body collision entries apply impulses to both bodies. Fixed-surface
  entries exercise normal/tangent response, zero velocity, both friction
  coefficient branches and final impulse application. Moving-surface entries
  include aliased reference velocity; world impulses include an aliased impulse
  input. Zero impulses, restitution values 0/0.5/1 and rotated coordinate bases
  are covered. These are bounded record fixtures, not every possible scene.
- All seven new consumers and complete entries run to return without service
  stubs or early checkpoint boundaries. CPU outside retired VU, outgoing host
  FP effects, clocks/checkpoints, call counts and final memory agree. Native
  VU stays untouched through the complete sequences. No new W exclusions are
  used; all four inverse-result components remain compared exactly.
- Historical inverse ABI coverage grows to 7,168 / 112 cases / whole-memory
  checks. Forward remains 5,504 / 344. Existing hierarchy coverage retains
  786,432 helper cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete
  BSP callers / 32 whole-memory and 2,048 unknown ABI cases.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Four-player recordings match the parent on every field. One-player
records match on draw, sound, calls, scratchpad, threads and timing/counts; raw
RAM hashes differ on 85 frames only. Strict/default native files and snapshots
are byte exact and strict result comparisons report no mismatch. The sampled
frame-7000 1P and frame-6000 4P RAM images match their parent byte for byte.

One-player reaches 2,221 new inverse calls. The two-body returns `1ed8ec` and
`1ed92c` stay cold in these replays and are qualified by complete-entry fixtures.
Four-player makes no calls to these seven rigid-body inverse sites.

### RAM provenance

The 1P RAM difference is bounded to opaque stack padding:

- Frames 7714-7743 differ at `8ff19c`; frames 8521-8575 at `8ff1dc`.
  Each slot holds `02000200` in the parent and zero in the native run. Dumps at
  the first affected frame in each interval identify only those two bytes.
- The inverse at `1ee030` publishes identical full XYZW, including meaningful
  basis W `02000200`. Its native implementation leaves VF16 untouched.
  The unchanged forward helper at return `1edf38` then spills stale VF16.W into
  a temporary while its actual scalar displacement consumer reads XYZ only.
- A generated-store trace identifies the first slot's later writer as
  `mathfMulMatrixTP3x3`, return `1ede74`. `createSkewSymmetric` writes only 3x3
  arithmetic fields; the matrix helper copies its residual first-column W from
  `8ff1dc` into `8ff19c`. The subsequent matrix scale/add consumes the 3x3 fields.
  This explains the padding's movement without adding a register shadow.
- Diagnostic runs hash a copied 32 MiB RAM image on all 85 affected frames after
  clearing just the identified four-byte slot for that frame. All other bytes
  match by hash. These diagnostics do not modify game RAM or production checks.
  Their original recording files match the qualified baseline/native recordings
  byte for byte; the writer trace's 7,800-frame payload also matches its baseline.
- Temporary source instrumentation was removed. Original generated/runtime
  object hashes match exactly, and the qualified headless executable was
  restored byte for byte. Raw RAM hash differences remain reported, not hidden.

Fresh isolated windowed runs complete 6,500 one-player, 6,500 four-player and
4,200 movie frames. Reviewed captures show level loading, advancing gameplay,
driving/combat/effects, four simultaneous views, decoded intro images and return
to the menu. Each route has five distinct captures. Windowed 1P reaches 853 new
inverse calls; 4P and movies make none. Stereo 48 kHz WAV captures contain nonzero
audio. Device underruns remain: 54.9 ms silence in 1P, 23.4 ms in 4P and 16.2 ms
in movies. These are functional checks, not a timing or flawless-playback claim.
Full campaign, every scene and physical controller/hardware acceptance remain
outside this increment.

## Remaining scope

Current generated source has 134 inverse-helper returns and 269 forward returns.
Of these two families, 22 inverse and 174 forward sites remain outside the native
allowlists: 196 total. This is a call-site inventory, not a count of all remaining
emulation. Remaining inverse groups include AI, driver/boss helpers, HUD/radar,
rollover, damage, lighting, a tire helper and a particle helper. Additional inline
arithmetic and forward transforms remain separate work.
