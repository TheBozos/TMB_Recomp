# Native traversal and older collision setup transforms

Thirteen more hierarchy returns use the existing native record calculations: nine inverse and
four forward sites bring the allowlists to forty-two and thirty-seven entries respectively.
This extends the [BSP crossing increment](NATIVE_BSP_CROSSINGS.md). BozoEngine remains pinned at
`4a8f70a`; no engine or renderer change is included.

| Helper | Return PCs | Owner |
|---|---|---|
| Inverse `1da1e8` | `1c0684`, `1c0884`, `1c0a70` | Database traversal `1c0300` |
| Inverse `1da1e8` | `1c5d0c` | Older collision grid `1c5470` |
| Inverse `1da1e8` | `1c639c` | Older hierarchy traversal `1c60e8` |
| Inverse `1da1e8` | `1c6bf4`, `1c6c94`, `1c6f04` | Older collision node `1c6ad0` |
| Inverse `1da1e8` | `1c7620` | Older animated node `1c73a0` |
| Forward `1da298` | `1c6bac`, `1c6c54` | Older collision node `1c6ad0` |
| Forward `1da298` | `1c7134`, `1c7160` | Older terrain setup `1c70c0` |

Native leaves snapshot inputs before overlapping writes, calculate separately rounded XYZ and
leave VU registers, accumulator, flags and data memory untouched. Inverse XYZW is exact, including
meaningful basis[0].W; forward XYZ is exact while existing destination W is preserved. Unknown
callers and historical hosts retain the full original helper ABI. Scalar matrix products,
normalization, callbacks, query dispatch, clocks and checkpoints remain compiled game code.
Other helper returns and inline VU operations remain; this is not complete VU removal.

## Generated consumers and numerical checks

Local evidence is under `D:/BozoScratch/hierarchy-traversal-setup`, against main `a29c09f` (PR #28).
Generated game code, disc data, captures, recordings and RAM dumps are excluded from the commit.

- The consumer harness grows from 18,944 to 25,600 sequences and from 592 to 800 whole-memory
  comparisons (6,656 / 208 added). Four host FP modes, denormal controls, RAM aliases and arbitrary
  W payloads are covered. CPU outside retired VU, outgoing host FP effects, clocks/checkpoints and
  other memory match; native VU remains untouched.
- Database continuations run actual matrix copying/products into the indirect callback boundary,
  or scalar vector addition into the modeled-effect creation boundary. Tests stop before callback
  or particle dispatch; they do not claim complete traversal or modeled-particle producer coverage.
- Grid/traversal tails retain capacity-counter updates at 0 and 1199 and compare the next scalar
  matrix-product arguments. Animated-node tails run that product, index publication and return,
  including capacity 0, 1199 and 1200. General child traversal is outside these fixtures.
- Collision-node sequences run world/local additions, paired transforms, point-mode selection,
  zero/copy/normalization and unit-box decisions through the first plane-dot dispatch. Exact and
  adjacent unit-axis values are included. Terrain sequences run both world-point sums through
  the next zeroing-call boundary. Complete collision/terrain queries are outside these fixtures.
- Reference forward W and VU effects stay live throughout real consumers. Only final unused
  forward-output W fields are normalized afterward; any later inverse overwrite retires that
  exclusion so meaningful inverse W remains compared exactly.
- Historical forward checks grow to 1,792 cases / 112 whole-memory comparisons (256 / 16 added).
  Historical inverse checks cover all forty-two returns: 2,688 cases / 42 whole-memory comparisons.
  The hierarchy suite retains 786,432 helper cases, 384 whole-memory checks, 8,000 geometric edges,
  1,024 recursive BSP callers / 32 whole-memory checks and 2,048 unknown-caller ABI cases.
- The math suite retains 1,048,576 helper cases, 512 whole-memory checks and 2,048 unknown-caller
  ABI cases, including overlap, alignment, aliases and scratchpad coverage.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames. Strict
contact, hierarchy, math and projection checks pass; strict/default recordings and snapshots are
byte-identical. Every non-RAM frame-record field matches baseline throughout: scratchpad,
calls/counts, draws/counts, sound/sample counts, thread state and EE clock. The entire four-player
recording is also baseline-byte-identical, including RAM. Default runs perform zero original
comparisons and zero VU0 microprogram calls.

All thirteen added sites execute in one-player: 20,086 inverse and 11,521 forward calls. Four-player
reaches the first two database traversal inverses, 820 calls combined; the other sites have
generated-consumer and historical-ABI coverage. This retires another 31,607 / 820 original helper
executions in one-player / four-player. These are counts, not timing or FPS measurements.

One-player RAM hashes differ on 2,058 frames, first at 7003 and last at 9499. The complete 32 MB
snapshot at frame 7000 is baseline-identical, as is four-player at frame 6000. A paired late
snapshot at frame 9499 differs by ten bytes in four W words: `23deac`, `23defc`, `23e38c`, `23e3dc`.
Temporary write observers identify scalar `mathfMulMatrix3x3` (`1da428`, return `1ee020`) copying
SP+`0c` (`8ff0cc`) into the first W of the two matrices, then original inverse calls (`1ee030`
and `1ed6c4`) copying basis[0].W into their result W. The matrix owner
`RigidBody::computeAuxiliarys` initializes only three components in each temporary row; the
scalar product copies the untouched W independently of its numerical 3x3 calculation. Baseline
words are `42700000` / `c4f00000`; native words are `02000200` / zero. These are inherited
temporary-padding differences, not changed XYZ products. The original inverses preserve their
full ABI and copy those W values exactly. This is a bounded source/snapshot/write-chain audit,
not a classification of every RAM difference or possible campaign consumer.

Paired observer recordings and snapshots reproduce both late qualified runs exactly. Observers
were removed, restored sources forced to rebuild, the generated object matches its original
hash, and all three executables contain no observer markers. No padding restoration, hash mask
or shadow VU state is shipped. All eleven native suites pass again, and clean default replays
reproduce both complete qualified recordings and snapshots byte-for-byte.

Fresh window routes complete 4,201 movie/menu frames and 6,501 each for one-player and actual
four-player. Reviewed captures show decoded movie pictures, Junkyard loading and active rainy
combat, and four advancing snow-arena views. Device logs report 19.2 / 52.0 / 28.4 ms of silence
(movie / one-player / four-player), with underruns and dropped frames. Deterministic sound records
match; flawless device audio, full campaign acceptance and console visual parity remain unclaimed.
