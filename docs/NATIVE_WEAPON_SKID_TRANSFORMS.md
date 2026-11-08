# Native weapon translation and skid-mark transforms

Five forward-transform returns now use native record arithmetic, extending the
[weather viewport increment](NATIVE_WEATHER_VIEWPORT.md). Forward coverage grows
from 90 to 95 sites; inverse coverage stays at 89. BozoEngine stays at `4a8f70a`.

| Return PC | Owner | Consumers |
| --- | --- | --- |
| `1b28cc` | `Weapon::UpdateTranslation` | Weapon-local offset followed by position addition |
| `1b28e8` | `Weapon::UpdateTranslation` | Coordinate-system offset followed by position addition |
| `168ca0` | `skidmarkAddSkidMark` | Half-width offsets, four vertices, material/colour and mark metadata |
| `1691b0` | `skidmarkStretchOldSkidMark` | Stationary left/right endpoint adjustment and completion flags |
| `169378` | `skidmarkStretchOldSkidMark` | Moving forward/reverse vertex pairs and orientation history |

The earlier creation return `168b30` was already native. The existing forward
helper snapshots operands, calculates separately rounded XYZ and retains
destination W. Historical hosts and unknown callers retain the complete original
ABI. All five new results are consumed as XYZ; native calls leave VU registers,
accumulator, flags and data memory untouched. Smoothing, timers, material gates,
list operations, travel-direction checks and vertex publication stay compiled
game code. No register shadow or VU reconstruction is added.

## Complete generated consumers and entries

Evidence is under `D:/BozoScratch/weapon-skid-transforms`, against main `c5888d2`
(PR #43). Generated code, retail data, recordings and captures stay outside Git.

- Added coverage is 2,560 sequences / 160 whole-memory comparisons. Aggregate
  coverage is 79,360 / 2,944. Four host rounding modes, denormal controls, RAM
  aliases, all four tires/mark slots and arbitrary position/vertex W are covered.
- Each site has 256 helper-boundary cases through the complete consumer/return,
  plus 256 actual function entries: 512 generic weapon updates, 256 skid
  creation updates and 512 stationary/moving stretch updates in total.
- Weapon entries exercise both basis-selection modes, zero/nonzero prior
  translation, real smoothing and timer-scaled offsets. Generic types 0–3 and
  the terrain-adjustment-disabled tail are covered. Special weapon update
  routines and the optional height-query branches are outside these fixtures.
- Creation entries cover disabled generation, invalid material, tire rejection,
  overload rejection, both travel directions, removal from the free list and
  insertion into the used list, and reuse with an empty free list. Actual
  material, timer and intrusive-list routines run without stubs. RGB packing,
  mark-index publication and all four vertex records are compared.
- Stretch entries cover material/direction rejection, distance and accumulated
  angle gates, retained short marks, both moving directions, stationary
  left/right updates and completion flags. The actual software-double
  conversion/multiply/conversion width producer executes before moving offsets.
  Explicit per-site reach assertions distinguish active paths from early exits.
- CPU outside retired VU, outgoing host FP effects, clocks/checkpoints, call
  counts and final memory agree. Native VU stays untouched through the complete
  consumers. W exclusions occur only after all real consumers run and are
  restricted to the audited stack offset records and creation's tire-position
  W. Published coordinate-system and vertex W remain compared exactly.
- Whole-memory entry samples cover active weapon translations, new mark creation,
  stationary stretching and moving vertex updates. Historical forward ABI
  coverage grows to 5,504 / 344 cases / whole-memory checks; inverse stays at
  5,696 / 89. Existing hierarchy coverage retains 786,432 helper cases / 384
  whole-memory, 8,000 geometric edges, 1,024 complete BSP callers / 32 whole-memory
  and 2,048 unknown ABI cases.
- Player, headless runner, historical checker and all test targets build.
  All eleven native suites and twelve Python tooling tests pass.

## Replay qualification

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames.
Every recorded field matches the immediate parent: raw RAM, scratchpad, calls,
draws, sound, threads, counts and EE clocks. Both sampled full 32 MiB RAM snapshots
match exactly. Strict/default recording files and snapshots are byte-identical.
No observer, RAM mask or VU restoration is needed.

| New return | One-player calls | Four-player calls |
| --- | ---: | ---: |
| `1b28e8` | 5,648 | 0 |
| `168ca0` | 1,522 | 1,449 |
| `169378` | 2,578 | 6,914 |
| Total | 9,748 | 8,363 |

Weapon-local `1b28cc` and stationary `1691b0` are cold in these routes and are
qualified by the generated comparisons. Total native forward calls become
1,172,820 / 375,625. Default routes make zero original-helper comparisons and
execute zero VU0 microprograms. Counts establish reach, not a measured speedup.

Fresh 6,500-frame one-player and actual four-player windowed routes and a
4,200-frame movie/menu route complete through clean exit, with isolated scratch
user data. Reviewed captures show decoded movie action (1,000), return to menu
(1,800), Junkyard loading (one-player 4,500), movement and active combat with
weapon effects (one-player 5,500 / 6,300), and four advancing snow-arena views
with skid effects (four-player 5,500 / 6,300). These are bounded functional
checks, not pixel-exact windowed or console parity.

Windowed one-player reaches coordinate-system weapon translation 1,718 times,
skid creation 450 times and moving stretch 778 times: 2,946 new native calls.
Four-player reaches creation/stretch 825 / 3,195 times: 4,020 new calls. The two
fixture-qualified cold sites remain cold here; all five are cold in the movie route.

All three stereo 48 kHz WAV recordings contain audio. Device logs report
25.6 / 53.4 / 31.2 ms of silence (movie / one-player / four-player), with underruns
and dropped frames. Deterministic gameplay sound is exact; flawless device audio
and subjective listening are not claimed.

## Remaining scope

The two inventories now contain 95 native forward and 89 native inverse sites.
Outside their allowlists remain 174 forward and 45 inverse sites, 219 in total
across the game. This structural count excludes other helper families and inline
VU operations. Full campaign, every weapon/special attack and console visual/audio
parity remain separate work.
