# Native particles across scheduler continuations

During qualification of the vehicle/light direction changes, the 4P window stopped advancing at
frame 4915. Its UI remained responsive because the game thread had raised an uncaught C++ exception
and was waiting for the crash dialog. The crash stack reached the guarded `sceGsExecStoreImage`
from a lens-flare helper dispatched directly by the scheduler. A fresh unchanged-main 4P window
completed 6,501 frames, so the live trigger is timing-sensitive.

The original particle wrapper set `g_flaresNative` around a C++ call into generated `particleDraw`.
A checkpoint can unwind that call before the lens helper runs. When the scheduler later enters the
helper directly, the scope flag has already been cleared; the old branch executes the generated
flare code and reaches the retired SDK readback. A flag tied to that C++ stack is not a durable
native-backend selection. Host-time freshness was also unsuitable for selecting native effects
and deciding whether native depth sampling was available after a long frame.

## Change

The normal native backend now selects the existing native flare test/render routines from its
backend and explicit particle options, independently of that transient call scope. Native particle
draw selection also survives a host-time gap. Native depth availability follows the enabled native
renderer and explicit depth option; it no longer expires when the last match frame is over 100 ms
old. Actual samples still come from the previous native renderer frame via `depthAt`; missing
coverage retains the existing far-depth behavior.

No SDK image descriptor is decoded, no fake image or completion interrupt is added, and the SDK
guard remains in place. Explicit legacy particle diagnostics retain their existing compatibility
selection. BozoEngine stays at `4a8f70a`; this change is owned by TMB.

## Regression evidence and validation

Local artifacts are under `D:/BozoScratch/collision-directions`. The initial failed window log,
scratch-user crash report, successful unchanged-main comparison, and `stall-runtime-evidence.json`
are retained there. REA observation `ev_fe6c8e868562f51268869b7c3a7baf210ff7b424a388ecfcdc58767f026c3e91`
records the waiting exception/dialog threads; `ev_9dffc412844e6e3efdc03fab1756e20be96683f4c5ad5e8242e036a0a737375b`
records the selected process image/base and thread context. Their provider reports no artifact
digest or OS/architecture identity, so executable attribution additionally uses the exact owned
process path, crash report module offsets and the built executable's local symbol table. Both
observations were ephemeral and detached; no live analysis session is retained.

`tmb-native-particle-tests` enters both native flare leaves without a live `particleDraw` scope or
any recent frame marker. The old code fails by entering generated/runtime-service code. The fixed
code passes eight test/render pairs across all four views, including visible and culled points,
projection records, missing-depth behavior, visibility, complete native CPU ABI (including VU and
clocks) and untouched legacy packet output. The leaf test does not claim GPU occlusion parity.

Final gameplay/movie/loading reruns and audio limits are recorded in
[`NATIVE_DIRECTION_CONSUMERS.md`](NATIVE_DIRECTION_CONSUMERS.md). This is bounded regression coverage;
full campaign, console visual parity and explicit legacy diagnostic modes are not qualified by it.

[Native particle-emission transforms](NATIVE_PARTICLE_EMISSION.md) subsequently retire five
emitted-direction and emitter-coordinate helper returns. That increment preserves this
backend selection and adds generated emission/metadata and emitter-coordinate consumer checks.
