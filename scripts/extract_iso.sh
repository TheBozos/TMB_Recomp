#!/usr/bin/env bash
# Usage: scripts/extract_iso.sh <path-to-the-disc.iso|.bin|.cue>
# Extracts the disc into game_us/ (USA, SCUS-97101) with PS2Recomp/tools/ps2_disc.py (no bsdtar); the runtime mounts
# cdrom0: on the ELF's directory. tools/tmb_build.py does this and every other step in one command.
source "$(dirname "$0")/common.sh"
ISO="${1:-}"
[ -f "$ISO" ] || { echo "usage: $0 <iso>"; exit 1; }
mkdir -p "$GAME_DIR"
python "$ROOT/PS2Recomp/tools/ps2_disc.py" extract "$ISO" "$GAME_DIR"
[ -f "$ELF" ] || { echo "ELF not found after extraction: $ELF (is this the $TMB_REGION disc?)"; exit 1; }
echo "Extracted to $GAME_DIR"
