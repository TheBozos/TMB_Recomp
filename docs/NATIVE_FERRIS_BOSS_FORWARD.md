# Native Ferris wheel and final boss hit-test forward transforms

Four more hit-test forward returns use native XYZ arithmetic after the
[weapon hit-test batch](NATIVE_WEAPON_HIT_FORWARD.md). Known forward coverage grows
from 222 to 226 of 269 returns (43 remain). Known inverse coverage stays 134 of 134;
BozoEngine stays at `4a8f70a`.

| Return PC | Producer and consumer |
| --- | --- |
| `12a6d8` | `FerrisWheel::UpdateFerrisWheel` (`12a580`): the wheel's offset at `+30` goes through its coordinate system (pointer at `+90`) into frame `+0`. `mathfAddVector` adds it into the system's position, which is raised by 20 and height-tested (`hdHatTest`) |
| `12a930` | A random car offset in frame `+10` goes through the system into frame `+0`, plus the position. It becomes `hdPntSphereTest`'s point against the wheel; on a hit, `mathfSubVector` rewrites its XYZ and `DispatchHit` gets it |
| `12a9e8` | The seat offset in frame `+20` the same way, after the first test's hit dispatch: `hdPntSphereTest`, then `mathfSubVector` and `DispatchHit` |
| `11201c` | `FinalBoss::CheckCollision` (`111f80`): each collision point from the table at `+710` goes through the boss's coordinate system (`+e0`) into frame `+0`, plus the position. It becomes `hdPntSphereTest`'s point; on a hit, `mathfSubVector` and `DispatchHit` |

The consumers are the ones audited for the
[weapon hit-test batch](NATIVE_WEAPON_HIT_FORWARD.md): `hdPntSphereTest` and every
reader of its globals, all twelve `DispatchHit` handlers, and the XYZ helpers. They
read the point only as XYZ, and nothing copies its W. `12a6d8`'s result reaches only
`mathfAddVector`; `hdHatTest` gets the coordinate system's position, whose W no
forward writes. Neither owner ever loads frame `+0c`, and neither reads VU
registers, accumulator or flags. The existing native helper computes separately
rounded XYZ, preserves destination W and leaves VU state untouched. Unknown and
historical callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/ferris-boss-forward`, against parent `eefb151`
(PR #81). Parent headless/window binaries are preserved there and match PR #81's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 2,048 generated sequences / 128 whole-memory comparisons (caller
  kinds 327 to 330): helper-boundary continuations and full entries, across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- `11201c` runs from the owner's start and stops at `hdPntSphereTest`. The Ferris
  wheel sites start at the call-return label before each transform (`12a6c4`,
  `12a918`, `12a9c8`); `12a6d8` stops at `hdHatTest`, the other two at
  `hdPntSphereTest`.
- The harness requires each full entry to reach exactly its own transform. Every
  basis is fixture-known, so every site is checked against an independent identity
  or quarter-turn result.
- Published values are asserted exactly against the original FPU arithmetic: the
  point each hit test receives, and that it receives that address; for `12a6d8`,
  the position after the raise by 20. Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 718 → 709,
  real libm errno cases 842 → 843). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 169,472 generated sequences / 8,576 whole-memory
  comparisons. Historical forward ABI coverage is 13,888 / 868; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `eefb151`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The four returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 44.9 ms of silence in one-player, 26.9 ms in four-player and 21.7 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The Ferris wheel and final boss in actual play, full campaign and physical controller/hardware acceptance remain open.
