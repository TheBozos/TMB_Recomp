#!/usr/bin/env bash
# Recompiles every IOP module TMB loads (989SND, LIBSD, PADMAN, SIO2MAN, MTAPMAN, MCMAN, MCSERV, USBD, LGDEV) to native
# C++ in the region's generated folder, iop/ (run with PS2X_IOP_NATIVE=1). The recompiler works from each module as the
# IOP loads it (relocated at its base): PS2Recomp/tools/irx_load.py loads the disc's MOD/*.IRX offline at the bases
# recomp/tmb_iop_modules.json records (the ones the runtime's IOP gives them), the same bytes PS2X_IOP_DUMP_MODULES
# writes, then the fork's tools/iop_recomp.py runs on them. Like the rest of the generated code, the output is derived
# from the game: never committed.
#   scripts/iop_recomp.sh           offline, from game_us/MOD
#   scripts/iop_recomp.sh --boot [seconds, default 40]
#       the old way: boots the game with PS2X_IOP_DUMP_MODULES and recompiles the dumps; prints each module's base,
#       to update recomp/tmb_iop_modules.json when the runtime's IOP allocation changes
source "$(dirname "$0")/common.sh"
cd "$ROOT"
DUMPS="build/iopmods-$TMB_REGION"
rm -rf "$DUMPS" && mkdir -p "$DUMPS" "$GENERATED/iop"
if [ "${1:-}" = "--boot" ]; then
    SECONDS_TO_BOOT=${2:-40}
    EXE="$GAME_BUILD/PS2Recomp/ps2xRuntime/ps2EntryRunner"
    [ -x "$EXE" ] || EXE="$EXE.exe"
    PS2X_IOP_DUMP_MODULES="$ROOT/$DUMPS" timeout "$SECONDS_TO_BOOT" "$EXE" "$ELF" > "build/iop_dump-$TMB_REGION.log" 2>&1 || true
    ls "$DUMPS" | grep -qi '^989SND.IRX.*\.json$' || { echo "no dump of 989SND (see build/iop_dump-$TMB_REGION.log)"; exit 1; }
    python -c 'import json, sys
for p in sorted(sys.argv[1:], key=lambda p: json.load(open(p))["base"]):
    d = json.load(open(p)); print("%-14s base %#x size %#x hash %s" % (d["name"], d["base"], d["size"], d["hash"]))' "$DUMPS"/*.json
else
    python - "$GAME_DIR" "$DUMPS" <<'EOF' || exit 1
import json, os, sys
sys.path.insert(0, 'PS2Recomp/tools')
import irx_load
game, dumps = sys.argv[1], sys.argv[2]
for m in json.load(open('recomp/tmb_iop_modules.json'))['modules']:
    irx = os.path.join(game, m['file'])
    if '%016x' % irx_load.fnv1a64(open(irx, 'rb').read()) != m['hash']:
        sys.exit('%s is not the USA disc\'s' % irx)
    irx_load.write_dump(irx, int(m['base'], 16), m['name'], dumps)
EOF
fi
for json in "$DUMPS"/*.json; do
    python PS2Recomp/tools/iop_recomp.py "$json" --out "$GENERATED/iop" || exit 1
done
