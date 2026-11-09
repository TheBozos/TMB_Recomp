# Native particle arc forward transforms

[FinalBoss gun-shot forwards](NATIVE_BOSS_SHOTS_FORWARD.md) follow with twelve boss sites.

Two particle forward returns use native XYZ arithmetic after the
[AI hunt and bait forwards](NATIVE_AI_BEAD_FORWARD.md). Known forward coverage grows
from 180 to 182 of 269 returns (87 remain). Known inverse coverage stays 134 of 134;
BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Producer and consumer |
| --- | --- | --- |
| `1e7e08` | `particleFlatArcs` (`1e7c00`) | A random flat offset (scaled by 20, Z 0) through a basis built from the arc's Z direction, into the particle's point array, plus the start point; the point's W is then rewritten with a fresh random value |
| `1e77e8` | `particleElectricArc` (`1e74c0`) | A random offset on a ring through the A→B basis into the frame vector at `+0`, scaled and added to A for each segment end |

Both owners take a free particle from the game's particle list. They build a basis
with `utilGenerateMatrixFromZvector`, fill the particle's point array and hand the
particle to `1e22f0`.

- **`particleFlatArcs`** throws ten points. Each transform writes the point slot;
  `mathfAddVector` adds the start point (X/Y/Z only), and the owner then stores a new
  W into the same slot before anything reads it. The transform's W can therefore
  never be observed.
- **`particleElectricArc`** writes the frame vector at `+0`, read only by
  `mathfScaleVector`. Frame `+0c` is never read, and the segment points' W values are
  written explicitly.

Neither owner reads VU registers, the accumulator or flags. The existing native
helper computes separately rounded XYZ, preserves destination W and leaves VU state
untouched. Unknown and historical callers keep the original ABI. No engine or
gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/particle-arcs-forward`, against parent `c265cfd`
(PR #73). Parent headless/window binaries are preserved there and match PR #73's
qualified build. Generated bodies, retail data and runtime recordings stay
outside Git.

- Added coverage is 1,024 generated sequences / 64 whole-memory comparisons (caller
  kinds 285 and 286): helper-boundary continuations and full-owner entries, across
  four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and
  arbitrary W payloads.
- Full entries run from the owner's start:
  - the real free-list allocation, the basis construction, the random-vector and
    seed helpers, and `sinf`/`cosf` with `sqrt` for the electric ring;
  - every point of the particle.

  They stop at `1e22f0`; reaching it is asserted. No free particle returns without
  a transform, and an electric arc with no segments skips its loop.
- `particleFlatArcs` rewrites each point's W itself, so its cases compare memory
  exactly, with no W exclusion at all. Its full entries check all ten points in
  order.
- Helper-boundary FlatArcs outputs are checked against an independent identity or
  quarter-turn result. ElectricArc scales its result before the addition, so its
  published segment end is asserted against the original FPU arithmetic instead.
  The sites publish in 2,496 and 448 cases.
- The VU rule from the earlier weapon increments applies. Only ElectricArc's forward
  scratch W at frame `+0c` is normalized for test comparison, after every real
  consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 706 → 704,
  real libm errno cases 853 → 838). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 146,944 generated sequences / 7,168 whole-memory
  comparisons. Historical forward ABI coverage is 11,072 / 692; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. The ElectricArc return `1e77e8` is live: it runs natively 640 times on the
one-player route and 864 times on the four-player route. Strict mode compares every
native XYZ against the original leaf and reports no mismatch. Strict/default
recordings and snapshots are byte exact. The FlatArcs return `1e7e08` is cold on
these routes and is qualified by the generated owner suite above.

**One recorded difference, proven to be a dead W slot.** This applies the rule
accepted for the [machine-gun increment](NATIVE_MACHINE_GUN_FORWARD.md). Against
parent `c265cfd`, the four-player recording matches on every field. The one-player
recording matches on every field except the end-of-frame RAM hash, which differs on
391 of 9,500 frames (from frame 5,655), and the 32 MiB snapshot is identical.

- A RAM dump right after frame 5,655 shows exactly one differing word, `0x8ff36c`:
  the parent has `00000000`, the native build `ffffffff`.
- A temporary logging build showed ElectricArc's output at `sp = 0x8ff360`, so the
  word is its dead frame `+0c` W slot.
- The same temporary build wrote a NaN pattern into that slot after every
  ElectricArc transform. Over the full one-player route only the RAM hash differed
  from the parent: calls, draws, sound, thread state, EE timing and every count
  matched.

The experiment was not committed; the qualified binary was restored afterwards.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 53.1 ms of silence in one-player, 41.5 ms in four-player and 15.9 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. Flat arcs in actual play, full campaign and physical controller/hardware
acceptance remain open.
