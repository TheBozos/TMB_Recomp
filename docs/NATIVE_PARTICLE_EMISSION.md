# Native particle-emission transforms

Five particle-emission helper returns now use native record calculations, extending
the [suspension/tire increment](NATIVE_SUSPENSION_TIRE.md). Forward coverage grows
from 68 to 70 sites and inverse coverage from 82 to 85. BozoEngine stays at `4a8f70a`.

| Helper / return PC | Owner | Consumer |
| --- | --- | --- |
| Forward `1da298` / `1e4818` | `particleEmit` (`1e4400`) | In-place direction used for emitted position and velocity XYZ |
| Forward `1da298` / `1e42d8` | `utilUpdateEmitterMatrices` (`1e4298`) | Attached emitter offset, followed by scalar world/relative position construction |
| Inverse `1da1e8` / `1e435c`, `1e43b4` | `utilUpdateEmitterMatrices` | Attached and unattached relative emitter positions |
| Inverse `1da1e8` / `1e43c8` | `utilUpdateEmitterMatrices` | Relative emitter direction |

The existing forward calculation captures inputs before writes, computes XYZ and
retains destination W. Its stack W outputs are unused by the real consumers. The
inverse preserves exact XYZW, including meaningful basis[0].W. Neither native path
transports VU registers, accumulator, flags or data memory. Historical hosts and
unknown callers retain the complete original ABI.

Scalar matrix composition, attachment dispatch, RNG and range selection, particle
position/velocity publication, bounds, lifetime and metadata remain compiled game
code. The Burning Man emission variant and emitter movement contain no direct calls
to these two transform helpers or inline VU arithmetic in the audited generated
functions; they are not rewritten by this increment.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/particle-emission`, against main `017908b`
(PR #36). Generated game code, retail data, snapshots and captures stay outside Git.

- Added coverage is 2,560 consumer sequences / 128 whole-memory comparisons.
  Aggregate coverage reaches 64,512 / 2,080. Four host FP modes, denormal controls,
  RAM aliases, in-place direction outputs and arbitrary meaningful inverse-W
  payloads are covered. CPU outside retired VU, outgoing host FP effects,
  clocks/checkpoints and consumer memory match; native VU stays untouched.
- Emission runs its complete real scalar tail through return. First and later
  particle slots, types 1/2/3/7/8/9, fixed and random lifetime ranges, bounds
  initialization/extension, spawn-time conversion and per-particle metadata are
  exercised. Position/velocity fourth components are real metadata and compared
  exactly. Only the unused forward-transform stack W is normalized, after all
  actual consumers finish.
- Of the emission cases, 256 enter at `1e4714`, before the actual position/velocity
  range RNG branches, and execute the in-place transform through return. Random
  normalized-direction generation, timer-driven entry and additional loop
  iterations are outside this bounded fixture; gameplay exercises those paths.
- Another 512 cases execute the complete emitter-coordinate function from entry,
  including all four attached/unattached and relative/world combinations, actual
  scalar matrix composition, both position paths and direction publication. The
  other new cases enter at individual helper boundaries and complete their tails.
- Historical forward coverage grows to 3,904 / 244 cases / whole-memory checks;
  inverse coverage grows to 5,440 / 85. The hierarchy suite retains 786,432 helper
  cases / 384 whole-memory, 8,000 geometric edges, 1,024 complete recursive BSP
  callers / 32 whole-memory and 2,048 unknown ABI cases. The math suite retains
  1,048,576 / 512 and 2,048 unknown ABI cases.
- Player, headless, historical checker and native test targets rebuild. All eleven
  native suites and twelve Python tooling tests pass. The final meaningful-W and
  in-place consumer additions also pass the rebuilt hierarchy suite.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. All non-RAM frame fields match the preceding main: scratchpad, calls/counts,
draws/counts, sound/sample counts, threads and EE clock. The entire four-player
recording matches baseline. One-player has one RAM-hash difference, at frame 8457;
all other RAM hashes match. Full 32 MB snapshots at one-player frame 7000 and
four-player frame 6000 are exact. Strict contact, hierarchy, math and projection
checks pass; strict/default recordings and snapshots are identical. Default runs
perform zero original comparisons and zero VU0 programs; strict intentionally
executes original oracles.

An initial graphics-frame snapshot misses the transient hash point. A paired
headless frame-boundary observer captures frame 8457 exactly: each dump's hash
matches its qualified frame record, and each complete observer record prefix
reproduces the qualified baseline/native route. The only changed word is
`8ff38c`, attached-emitter output `SP+5c`: original writes inherited `VF16.w`
bits `02000200`, native retains zero. The actual scalar matrix composition is
independent of that temporary W; subsequent vector consumers read XYZ only.

Across 1,754 paired helper events near that frame, 12 writes at `1e42d8` show this
difference. Another 39 at `1e4818` show emission output `SP+4c` (`8ff43c`) retaining
`3f800000` from the real normalized-direction producer, where original writes
`02000200`. Its position/velocity and metadata consumers also read only XYZ.
That second temporary does not survive to the captured frame hash. No production
mask or VU-state reconstruction is introduced. This is bounded unused-W evidence,
not raw-RAM identity or console parity. Temporary runtime/headless sources are
restored and all three original object hashes verified.

Forward returns `1e42d8` / `1e4818` execute 9,209 / 32,822 times in one-player and
13,467 / 9,990 times in four-player: 42,031 / 23,457 new native executions overall.
Total native forward counts become 1,102,044 / 283,936. The three new inverse sites
remain cold in these routes; they have generated-consumer and complete emitter
entry coverage. These are reached call counts, not measured speedups.

After observer removal, all eleven rebuilt suites pass again. Clean default/strict
replays reproduce the same complete results above, and the player, headless and
checker binaries contain no observer markers.

Fresh configured 4,200-frame movie/menu and 6,500-frame one-player / actual
four-player windowed routes complete through clean exit, with isolated scratch
user data. Reviewed captures show decoded movie pictures (1000), return to menu
(1800), Junkyard loading (one-player 4500), active one-player gameplay (5500/6300)
and four advancing snow-arena views with snow/tire effects (four-player 5500/6300).
The windowed one-player / four-player runs reach 12,807 / 11,678 new native forward
calls. All three stereo 48 kHz WAV recordings contain audio. Device logs report
20.2 / 59.6 / 27.8 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound matches; flawless device audio,
subjective listening and full campaign/console parity are not claimed.

## Remaining scope

These two helper inventories now have 70 native forward and 85 native inverse
sites. Outside their allowlists remain 199 forward and 49 inverse sites, 248 in
total across the game, including sites inside otherwise replaced functions. This
is a structural source inventory, not a live cost measurement, and excludes other
helper families and inline VU operations. Full campaign acceptance and console
visual/audio parity remain separate work.
