# Native FireSpecial forward transforms (types 20, 40, 60)

Five `Launcher::FireSpecial` forward returns use native XYZ arithmetic after the
[LoopTank batch](NATIVE_LOOPTANK_FORWARD.md). Known forward coverage grows from 238 to
243 of 269 returns (26 remain). Known inverse coverage stays 134 of 134; BozoEngine
stays at `4a8f70a`.

`Launcher::FireSpecial` (`149718`) dispatches on the vehicle type at `+20a8`:

| Return PC | Vehicle type | Producer and consumer |
| --- | --- | --- |
| `1499d4` | `20` | For each shot, the table offset at `23cfe0` goes through the vehicle's coordinate system into frame `+0`. `mathfAddVector` adds the vehicle position and stores the result in the launcher's `+620`, the weapon's start |
| `149bb8` | `40` | Each of eight table points is copied into frame `+0` with zero height. It goes through the aim matrix at frame `+50` in place; its height and its distance from frame `+30` (`mathfDist3`) aim the shot |
| `149c28` | `40` | The next table point goes through that matrix into frame `+0`, plus the vehicle position, into `+620` |
| `149d8c` | `60` | With a shot armed (`+27c`, `+374`): `23cfa0` goes through the vehicle's system, plus the vehicle position, into `+620` |
| `149e14` | `60` | `23cfb0` goes through the matrix rebuilt from the launcher's angles at frame `+50` into frame `+0`, added into `+620` |

A consumer audit covered every path from each return until the owner returns or the
slot is rewritten. It found these consumers:
- `mathfAddVector`;
- the 32-bit height load (`149bb8`);
- `mathfDist3`, which reads `+0/+4/+8` only.

The weapon constructors, `GetLockOn` and the sound calls do not receive the frame
vector. They copy `+620` with `mathfCopyVector`, and launcher `+62c` has no reader.
FireSpecial never loads frame `+0c`, never loads 64 bits at frame `+08`, and never
loads 128 bits from its frame. It reads no VU registers, accumulator or flags.

`149bb8` is in place, so the input is read whole before the output is written. The
existing native helper computes separately rounded XYZ, preserves destination W and
leaves VU state untouched. Unknown and historical callers keep the original ABI. No
engine or gameplay formula changes.

## Generated comparisons

Evidence is under `D:/BozoScratch/fire-special-forward`, against parent `3c65abe`
(PR #84). Parent headless/window binaries are preserved there and match PR #84's
qualified build. Generated bodies, retail data and runtime recordings stay outside
Git.

- Added coverage is 2,560 generated sequences / 160 whole-memory comparisons (caller
  kinds 343 to 347). They are helper-boundary continuations and entries across four
  rounding modes, FTZ/DAZ, RAM aliases, identity/quarter-turn bases and arbitrary W
  payloads.
- Entries:
  - `149d8c` runs from the owner's start through the type dispatch and the armed
    check.
  - The others start at the call-return label before their transform: `149998`,
    `149ba0` and `149c14` (all after the type's hardware timer read), and `149e00`.
- Each run stops at the first call after its result is consumed: `boundFAngle`,
  `mathfDist3` or `GetLockOn`. A transform read only by `mathfDist3` is recorded at
  that call with a zero base.
- Every basis is fixture-known, so every site is checked against an independent
  identity or quarter-turn result. Published values are asserted exactly against the
  original FPU arithmetic:
  - the launcher's `+620`;
  - the point `mathfDist3` receives, and that it receives that address.
  
  Publications: 512 for each site.
- The volley fixture from PR #78 now sets the two registers a trailing comment had
  hidden. All of its assertions still pass unchanged.
- The VU rule from the earlier weapon increments applies. Only forward scratch W at
  frame `+0c` is normalized for test comparison, after every real consumer.
- New caller kinds shift the shared random fill for later rounding modes, so two
  data-dependent counts in earlier fixtures moved (vehicle tire jumps 705 → 711,
  real libm errno cases 802 → 839). All scenario-defined counts and assertions are
  unchanged.
- Aggregate caller coverage is 178,176 generated sequences / 9,120 whole-memory
  comparisons. Historical forward ABI coverage is 14,976 / 936; inverse remains
  8,576 / 134.

## Replay and window qualification

Player, headless runner, historical checker and all eleven native test targets
build. All eleven native suites and twelve Python tooling tests pass, with every
previous publication assertion enabled.

Default and strict routes complete 9,500 one-player and 8,500 actual four-player
frames. Every recording field matches parent `3c65abe`, including the RAM hash on
every frame: scratchpad, calls, draws, sound, thread state and EE timing/counts.
Sampled 32 MiB RAM images are identical. Strict/default recordings and snapshots
are byte exact; strict comparisons report no mismatch. `149bb8` and `149c28` are live on the one-player route: each runs natively 8 times with no recorded difference. The other three are cold on these routes and are qualified by the generated owner suite above.

Fresh isolated window runs, started after the headless replays had finished,
complete 6,500 one-player, 6,500 actual four-player and 4,200 movie frames.
Reviewed captures show the intro and credits, menus, vehicle and level selection,
driving with the HUD and four simultaneous views. Each route has five distinct
captures. Stereo 48 kHz, 16-bit WAV recordings contain nonzero audio. Audio device
underruns: 52.0 ms of silence in one-player, 20.6 ms in four-player and 29.0 ms in
the movie run. These are functional checks, not a performance or flawless-playback
claim. The first window qualification's one-player run stopped at frame 4,340 on the vehicle-select screen. The main loop was waiting in `switchThread` from `updateMpegMovieDecoding` for 600 fields: the menu's background movie decoder stalled, before any gameplay (FireSpecial cannot run there). Four reruns of the same route (three with this build, one with the parent) and the fresh qualification above completed normally. The stall has not been seen before and is tracked separately; the evidence is in `fault-run/`. The FireSpecial specials in actual play, full campaign and physical controller/hardware acceptance remain open.
