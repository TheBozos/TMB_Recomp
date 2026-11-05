# Shared paths for the native BozoEngine build.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMB_BUILD_DIR="${TMB_BUILD_DIR:-$ROOT/build/native}"
TMB_DISC="${TMB_DISC:-$ROOT/Twisted Metal - Black (USA).iso}"
JOBS="${JOBS:-$(nproc)}"
build_game() {
    local kit=()
    [ -z "${TMB_KIT:-}" ] || kit=(--kit "$TMB_KIT")
    python "$ROOT/tools/tmb_build.py" --disc "$TMB_DISC" --out "$TMB_BUILD_DIR" --jobs "$JOBS" "${kit[@]}" "$@"
}
