# Sourced by the other scripts. Resolves the repo root and tool paths.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$HOME/.local/share/mise/shims:$PATH"

# The USA release (SCUS-97101, NTSC): its disc folder, recompiler config, generated code and build. (The European
# release is dropped; the runtime's hooks are the USA executable's addresses.)
TMB_REGION=us
GAME_DIR="$ROOT/game_us"
ELF="$GAME_DIR/SCUS_971.01"
CONFIG="$ROOT/recomp/tmb_scus97101.toml"
GENERATED="$ROOT/generated_us"
GAME_BUILD="$ROOT/build/game_us"
TOOLS_BUILD="$ROOT/build/tools"
JOBS="${JOBS:-$(nproc)}"
