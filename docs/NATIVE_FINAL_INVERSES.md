# Final native inverse-helper returns

The final six known inverse-helper returns now use native record arithmetic,
following the [vehicle/light increment](NATIVE_VEHICLE_LIGHT_INVERSES.md).
All 134 static return sites for `mathfMulVec` (`1da1e8`) are now in the native
allowlist. Forward coverage stays at 96 of 269. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Qualified consumer |
| --- | --- | --- |
| `12d0b0` | `forceFieldTakeHit` | Axis scaling, normalization, scalar response and hit-state publication |
| `136ce8` | `healthStandInit` | Four local vertices transformed and translated into a stand record |
| `13f814` | `Hud::BuildRadarStuff` | Distance-dependent alpha and real packet stores |
| `13fe54` | `Hud::BuildRadarStuff` | Nearest-target distance/ID publication and draw-argument staging |
| `14037c` | `Hud::AddRadarSignal` | Range/direction placement, height flashing, color and signal-count stores |
| `1e9778` | `particleDraw` | Camera-space visibility and sorted depth/index publication |

The inverse implementation is unchanged: separately rounded XYZ and meaningful
first-basis W remain exact. No W exclusion is added. Native leaves do not write
VU registers, accumulator, flags or data memory. Unknown returns and historical
hosts keep the original ABI; the original implementation remains for those
contracts. This completes the known static inverse call-site inventory, not all
remaining emulation or all inline VU arithmetic.

## Generated-code comparisons

Evidence is under `D:/BozoScratch/final-inverses`, against main `9298010`
(PR #49). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons.
  Aggregate coverage is 102,912 / 4,416. Each return has 256 helper-boundary cases
  through its actual consumer and 256 producer cases. Four host rounding modes,
  denormal controls, RAM aliases, rotated bases and arbitrary basis W are covered.
  Whole-memory producer samples select active transforms.
- Force-field fixtures include complete `12d038` entries and inactive/null-input
  gates. The original inline VU subtraction before the inverse is retained;
  its pre-inverse VU effects agree between original/native runs. The native leaf
  and complete scalar tail preserve that producer state. Real normalization,
  dot products, axis scaling, square-root response and hit-state publication run.
  Meaningful inverse W is checked explicitly. Hit publication is required in
  448 cases. This increment does not retire the inline subtraction itself.
- Health-stand fixtures execute the complete `136c70` entry and four-vertex loop,
  including real scalar translations, record pointers and count update. Each
  transformed vertex must match known identity/rotated positions, rather than
  merely match a second implementation. Helper-boundary cases also complete all
  four iterations. All inverse output lanes remain compared in memory.
- First radar-distance producers begin at `13f7b0`, after earlier viewport and
  vehicle-loop setup. They run visibility/class gates, actual position subtraction,
  inverse, square-root distance, scalar/double alpha interpolation and packet
  stores, stopping at the next vehicle-loop checkpoint `13f378`. Five packet
  records and exact near/interpolated/far alpha are required. Packet publication
  occurs in 480 cases. Earlier HUD setup and the enclosing loop are outside these
  bounded fixtures.
- Nearest-target producers begin at `13fd58`; actual visibility/class gates,
  position subtraction, inverse and distance-to-target stores run. The fixtures
  stop at draw helper `141af0` after real draw arguments are staged, or the next
  loop checkpoint `13fb10` when drawing is skipped. Nearest-target publication
  is required in 224 cases. The draw helper itself and full HUD loop are outside
  this fixture; they are not replaced by test stubs.
- Signal fixtures execute complete `1402a0` entries, including disabled/self-point
  gates, actual relative-position producers, inverse, distance scaling, directional
  signal trigonometry, height flashing, real color helpers and signal-count stores.
  Count publication is asserted. Near/far signals, direction-signal variants,
  zero displacement and high/low relative height are covered. The full 100-signal
  saturation/reporting path is outside these fixtures.
- Particle producers begin at the actual type traversal `1e9700`. They use real
  view translation/matrix getters and position subtraction, inverse, frustum/type/
  live-count gates, depth bias and actual insertion sorting. Two visible types
  must publish indices `[1,0]` and exact depths `[8,4]` (32 producer cases); one-
  and zero-visible cases are checked too. Getter counts must match transform
  reach. Execution stops before texture setup `1f91e8`, or synchronization
  `203c18` when none survive. Earlier FOV setup, texture/draw submission and the
  optional lens-flare call are outside these bounded fixtures.
- Per-site reach assertions distinguish active transforms and gates. CPU outside
  retired VU, host FP effects, clocks/checkpoints, helper/action counts and final
  memory agree. Native VU stays untouched through the consumers, retaining the
  force-field producer effects described above. No RAM masking or VU-state
  reconstruction is introduced.
- Historical inverse ABI coverage grows to 8,576 cases / 134 whole-memory checks;
  forward remains 5,568 / 348. Existing coverage retains 786,432 hierarchy helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete BSP callers /
  32 whole-memory and 2,048 unknown ABI cases.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass. Existing
front/rear AI publication assertions pass in 392 / 352 cases; vehicle/light
publication assertions remain enabled too.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recorded field matches the immediate parent: raw RAM, scratchpad,
calls, draws, sound, thread state and EE timing/counts. Sampled 32 MiB RAM images
are identical. Strict/default recording files and snapshots are byte exact;
strict comparisons report no mismatch.

One-player reaches four health-stand calls, 5,133 calls to each HUD distance site
and 31,216 radar-signal calls. Four-player reaches 6,248 calls to each HUD distance
site and 45,576 radar-signal calls. Force-field and particle-draw inverse returns
are cold in these replay routes; their behavior is qualified by the generated
producer/consumer fixtures above, not claimed as runtime route coverage.

Fresh isolated window runs complete 6,500 one-player, 6,500 actual four-player
and 4,200 movie frames. Reviewed captures show level loading, advancing driving,
combat/effects, AI opponents, four simultaneous views, decoded intro images and
return to the menu. Each route has five distinct captures.

Windowed 1P reaches four health-stand, 430 calls to each HUD distance return and
13,648 radar-signal calls. 4P reaches 3,296 calls to each HUD distance return and
21,588 radar-signal calls. Movies make no new inverse calls. Force-field/particle
inverse sites remain cold in these window routes. Stereo 48 kHz, 16-bit WAV
captures contain nonzero audio. Device underruns remain: 56.4 ms silence in 1P,
31.4 ms in 4P and 23.8 ms in movies. These are functional checks, not a timing or
flawless-playback claim. Full campaign, every HUD/particle/force-field state and
physical-controller/hardware acceptance remain outside this increment.

## Remaining scope

No known static inverse return remains outside the native allowlist. There are
still 173 forward returns outside its separate allowlist. Other inline arithmetic,
remaining hierarchy/VU helpers and historical/unknown ABI support are separate
work; this is not a claim that all emulation has been removed.
