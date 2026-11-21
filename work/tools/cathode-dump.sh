#!/usr/bin/env bash
# Compila cathode-dump (si hace falta) y vuelca, solo leyendo la instalación del juego:
#   - el árbol de scripts de cada nivel (COMMANDS.PAK) con el pid del mod -> commands/<NIVEL>.tsv.gz
#   - la UI original (UI.PAK; si NoHUD está activo, la copia de su bak/orig)  -> ui/
#   - los BML de configuración (dificultad, constantes, ítems)                -> bml/*.xml
# Salida en work/evidence/cathode/ (ignorada por git). Uso: work/tools/cathode-dump.sh [NIVEL ...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME="${AIMP_GAME_DIR:-/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation}"
OUT="$ROOT/work/evidence/cathode"
BIN="$ROOT/work/tools/cathode-dump/bin/out/cathode-dump"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
[ -x "$BIN" ] || dotnet build -c Release -o "$ROOT/work/tools/cathode-dump/bin/out" "$ROOT/work/tools/cathode-dump/cathode-dump.csproj" >/dev/null
mkdir -p "$OUT/commands" "$OUT/bml"

levels=("$@")
if [ ${#levels[@]} -eq 0 ]; then
  while IFS= read -r p; do levels+=("$(basename "$(dirname "$(dirname "$p")")")"); done \
    < <(find "$GAME/DATA/ENV/PRODUCTION" -maxdepth 3 -path '*/WORLD/COMMANDS.PAK' | sort)
fi
for L in "${levels[@]}"; do
  pak="$GAME/DATA/ENV/PRODUCTION/$L/WORLD/COMMANDS.PAK"
  [ -f "$pak" ] || { echo "$L: sin COMMANDS.PAK"; continue; }
  tmp="$OUT/commands/$L.tsv"
  "$BIN" commands "$pak" "$tmp" 2>&1 | tail -1 | sed "s/^/$L: /"
  gzip -f "$tmp"
done

if [ $# -eq 0 ]; then
  ui="$GAME/DATA/UI.PAK"
  [ -f "$GAME/mods/NoHUD-crosshair/bak/orig/DATA/UI.PAK" ] && ui="$GAME/mods/NoHUD-crosshair/bak/orig/DATA/UI.PAK"
  "$BIN" pak2 "$ui" "$OUT/ui" 2>&1 | tail -1
  for b in DIFFICULTYSETTINGS/DIFFICULTYSETTINGS DIFFICULTYSETTINGS/NOVICE DIFFICULTYSETTINGS/EASY DIFFICULTYSETTINGS/MEDIUM DIFFICULTYSETTINGS/HARD DIFFICULTYSETTINGS/IRON GLOBALCONSTANTS GBL_ITEM COVERANDTRAVERSALRULES; do
    "$BIN" bml "$GAME/DATA/$b.BML" "$OUT/bml/$(basename "$b").xml" && echo "bml: $b"
  done
fi
