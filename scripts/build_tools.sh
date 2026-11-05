#!/usr/bin/env bash
# Prepare the disc and build BozoEngine's recompiler and externals writer.
source "$(dirname "$0")/common.sh"
build_game --stop-after tools "$@"
