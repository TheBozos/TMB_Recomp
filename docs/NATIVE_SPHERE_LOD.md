# Native sphere/LOD and collision helper calculations

Three inline difference blocks now use native XYZ records: `hdSphereTest` at `1c6710`,
`hdLodNode` at `1c69a0`, and the sphere overload at `1cadb8`. Their instruction ranges are
`1c6714..1c6720`, `1c69bc..1c69c8`, and `1cadc4..1cadd0`. Together with the preceding
[terrain/contact increment](NATIVE_TERRAIN_CONTACTS.md), the contact handler owns thirty blocks
covering 162 instruction sites. It snapshots operands, computes XYZ once at the final store,
preserves destination W, and leaves VU state untouched. The existing scalar sphere radius tests,
LOD thresholds, selected-child metadata, return ABI and checkpoints remain compiled game code.

Six additional helper returns also use the existing audited native implementations:

| Helper | Return PCs | Consumer |
|---|---|---|
| Forward direction `1da298` | `1cb19c` | Collision node setup |
| Inverse direction `1da1e8` | `1cb1e4`, `1cb4a4`, `1cb530` | Collision node point/normal setup |
| Inverse direction `1da1e8` | `1cb770`, `1cb834` | Point-contact setup and scalar axis tests |

The inverse allowlist now has twenty entries; the forward-direction allowlist has nine.
Inverse output matches all four components, including the first basis record's W. Forward output
retains its established XYZ/preserved-destination-W contract. Historical hosts and unaudited
returns retain the original complete helper ABI. This does not replace every remaining helper
caller or all VU operations. Renderer hierarchy LOD calculation was already native in
[the scene increment](NATIVE_SCENE.md); this change handles the separate collision LOD selector.
BozoEngine remains pinned at `4a8f70a`; no engine or renderer change is included.

## Numerical and generated-caller checks

Evidence is local under `D:/BozoScratch/sphere-lod`, against main `90c9e21` (PR #24).
Generated game code, disc data, captures and RAM dumps are excluded from the commit.

- The contact suite passes 983,040 original-block comparisons and 960 complete-memory checks.
  Existing scalar contact continuations retain 3,584 cases, 112 complete-memory checks and
  1,024 ordered-alias cases.
- Another 6,144 complete generated sphere/LOD callers, including 48 complete-memory checks,
  exercise sphere class/minimum-radius branches, exact and neighbouring radius boundaries,
  LOD selection limits and metadata. Only the unused local difference W is normalized in the
  reference comparison. CPU state outside retired VU, complete outgoing host FP effects,
  clocks/checkpoints, decisions and other memory must match; native VU state must be untouched.
- Another 3,072 generated helper-consumer sequences, with 96 complete-memory checks, exercise
  the six new return sites through their following scalar/vector setup or query boundary.
  Cases include zero/copy, normalization and unit-box axis branches. Inverse XYZW is exact;
  only the existing forward destination-W contract is normalized before continuation.
  These are bounded consumer sequences, not complete collision-query executions.
- Checks cover four host rounding modes, denormal controls and RAM aliases. The hierarchy suite
  retains 786,432 helper cases, 384 complete-memory checks, 8,000 geometric edges, 1,024 recursive
  BSP callers and 2,048 unknown-caller ABI cases. Its historical-host coverage now rotates
  through all twenty inverse sites (1,280 cases, twenty complete-memory checks). Another 64
  cases/four complete-memory checks prove the new forward site's historical CPU/VU ABI.
- Player, headless and historical checker builds, all eleven native suites and twelve Python
  tooling tests pass. Temporary diagnostics were removed; restored contact/forward compiled
  objects match their saved pre-diagnostic hashes, and all three binaries contain no trace markers.

## Gameplay, loading and movies

Default and strict routes complete 9,500 one-player and 8,500 actual four-player frames.
Every non-RAM frame-record field matches the baseline throughout: scratchpad, library calls and
counts, draws and counts, sound and sample counts, thread state and EE clock. Strict contact,
hierarchy, math and projection comparisons pass and reproduce the default recording files
byte-for-byte. A clean rebuild after diagnostic removal reproduces all four qualified recording
files byte-for-byte. Default runs perform zero original comparisons and zero VU0 microprogram calls.

The three new inline blocks execute 1,283,655 / 54,166 times in one-player / four-player. All three
are reached in one-player; four-player reaches only the sphere overload. The five new inverse
sites execute 52,688 / 13,904 times; the new forward site executes 9,779 / 140 times. Thus the
six helper sites retire another 62,467 / 14,044 original helper executions. These are execution
counts, not timing or FPS measurements.

RAM hashes differ on 2,242 one-player frames (5593 through 9499) and 3,798 four-player frames
(4702 through 8499). A complete 32 MB snapshot at one-player frame 7000 is identical. At four-player
frame 6000 exactly three bytes differ, in words `8ff0dc`, `8ff1fc` and `8ff35c`: baseline zero versus
native integer bits 9. The first two are directly traced to unused sphere temporary W stores at
`1cadd0`, returning to `1cb138` and `1ca85c`. The third is a reused stack word: loading writes the
decompressor's scratch count there, and traversal also reuses it for matrix storage. Its complete
last-write divergence chain has not been established. Consequently these snapshots do not prove
every RAM difference harmless or every possible consumer covered. No new hash mask, padding
restoration, shadow VU state or generated-code instrumentation is shipped. An additional snapshot
requested at one-player frame 5593 is baseline-exact; strict/default snapshots are identical too.

Window runs complete 4,201 movie/menu frames and 6,501 frames each for one-player and four-player.
Reviewed captures show decoded movies, loading, active Junkyard combat and four snow-arena views.
Initial device logs report 447.8 / 57.7 / 39.4 ms of silence (movie / one-player / four-player).
A clean movie-only rerun without concurrent builds completes 4,201 game frames with decoded
captures and reports 22.4 ms of silence, three underruns and 1,712 dropped audio frames. This is
additional device evidence, not proof of the cause of the initial underruns. Deterministic sound
records match. Full campaign acceptance, flawless device audio and console visual parity remain
unclaimed.
