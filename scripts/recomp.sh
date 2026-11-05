#!/usr/bin/env bash
# Recompile the USA executable with BozoEngine (no IOP or VU recompiler).
source "$(dirname "$0")/common.sh"
build_game --stop-after recompile "$@"
