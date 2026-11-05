#!/usr/bin/env bash
# BozoEngine is the only runtime.
exec "$(dirname "$0")/run_native.sh" "$@"
