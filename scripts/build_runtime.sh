#!/usr/bin/env bash
# Build and stage the native game from your own USA disc. TMB_DISC / TMB_KIT / TMB_BUILD_DIR override paths.
source "$(dirname "$0")/common.sh"
build_game "$@"
