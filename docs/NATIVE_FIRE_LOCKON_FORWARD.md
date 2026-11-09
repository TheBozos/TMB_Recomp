# Native FireSpecial forward transforms (type a0 lock-on launch points)

Six more `Launcher::FireSpecial` forward returns use native XYZ arithmetic after the
[first FireSpecial batch](NATIVE_FIRE_SPECIAL_FORWARD.md). Known forward coverage
grows from 243 to 249 of 269 returns (20 remain). Known inverse coverage stays 134 of
134; BozoEngine stays at `4a8f70a`.

Vehicle type `a0` fires at its locked-on target (`+2f4`) in three blocks. Each block
bounds a heading (frame `+120`, `+124`, `+128`) and uses its window thresholds
(`gp-6408`, `gp-63f0`, `gp-63d8`) to pick one of four table slots, frame `+50` to
`+80`. The slot goes through the target's coordinate system into frame `+40`:

| Return PC | Block | Producer and consumer |
| --- | --- | --- |
| `14a4d4` | first | Slot `+50`, `+60` or `+70` |
| `14a518` | first | Slot `+80` |
| `14a73c` | second | Slot `+50`, `+60` or `+70` |
| `14a780` | second | Slot `+80` |
| `14a98c` | third | Slot `+50`, `+60` or `+70` |
| `14a9d0` | third | Slot `+80` |

`mathfAddVector` then adds the target's position into frame `+40`. The first two
blocks emit particles at that point (`particleCreateFx`). The third block hands it
to the take-hit info and calls `DispatchHit`.

The take-hit point pointer is dereferenced only by `mathfManhatDist` and by
`forceFieldTakeHit`'s xyz-masked VU subtraction. `DispatchHit` itself gets a
different vector. None of these consumers reads W. FireSpecial never loads frame
`+4c`, 64 bits at `+48`, or 128 bits from its frame. It reads no VU registers,
accumulator or flags. The existing native helper computes separately rounded XYZ,
preserves destination W and leaves VU state untouched. Unknown and historical
callers keep the original ABI. No engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/fire-lockon-forward`, against parent `8cd62cd`
(PR #85). Parent headless/window binaries are preserved there and match PR #85's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 3,072 generated sequences / 192 whole-memory comparisons (caller
  kinds 348 to 353). They are helper-boundary continuations and entries at the
  call-return label after each block's heading bound (`14a44c`, `14a6b4`,
  `14a900`), across four rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn
  bases and arbitrary W payloads.
- The real window comparisons select the slot from the heading. Each run stops at
  `particleCreateFx` (first two blocks) or `DispatchHit` (third block), after
  frame `+40` has been consumed.
- The harness requires each entry to reach exactly its own transform. Every basis is
  fixture-known, so every site is checked against an independent identity or
  quarter-turn result. The launch point in frame `+40` is asserted exactly against
  the original FPU arithmetic. Publications: 512 for each site.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+4c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 711 → 714,
  real libm errno cases 839 → 835). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 181,248 generated sequences / 9,312 whole-memory
  comparisons. Historical forward ABI coverage is 15,360 / 960; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `8cd62cd`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. The six returns are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 58.7 ms of silence in one-player, 35.4 ms in four-player and 23.6 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The type a0 special in actual play, full campaign and physical controller/hardware acceptance remain open.
