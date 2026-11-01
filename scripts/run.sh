#!/usr/bin/env bash
# Launches the recompiled game on the PS2 path (VU1 and the GS simulated), for comparison: the runner's default is
# the native port (scripts/run_native.sh), TMB_NATIVE=0 turns it off. Extra args are passed to ps2EntryRunner.
source "$(dirname "$0")/common.sh"
BIN="$GAME_BUILD/PS2Recomp/ps2xRuntime/ps2EntryRunner"
[ -x "$BIN" ] || { echo "missing $BIN, run scripts/build_runtime.sh first"; exit 1; }
export TMB_NATIVE="${TMB_NATIVE:-0}"
exec "$BIN" "$ELF" "$@"
