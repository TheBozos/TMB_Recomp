#!/usr/bin/env bash
# Launch the staged BozoEngine game; extra arguments go to tmb.
source "$(dirname "$0")/common.sh"
BIN="$TMB_BUILD_DIR/TMB/tmb"
[ ! -f "$BIN.exe" ] || BIN="$BIN.exe"
[ -f "$BIN" ] || { echo "Missing $BIN; run scripts/build_runtime.sh first"; exit 1; }
export PS2X_SHOW_FPS="${PS2X_SHOW_FPS:-1}"
exec "$BIN" --elf "$TMB_BUILD_DIR/TMB/SCUS_971.01" --iso "$TMB_BUILD_DIR/TMB/disc.iso" "$@"
