#!/usr/bin/env bash
# Usage: scripts/extract_iso.sh [disc.iso|disc.bin|disc.cue]
source "$(dirname "$0")/common.sh"
if [ "$#" -gt 0 ]; then TMB_DISC="$1"; shift; fi
build_game --stop-after extract "$@"
