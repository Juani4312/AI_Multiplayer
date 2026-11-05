#!/usr/bin/env bash
# Corre Dump.java sobre el proyecto persistente (solo lectura).
# Uso: work/scripts/ghidra/dump.sh <salida.txt> <maxLineas> <cmd> [<cmd> ...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$1"; shift
GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-8G}" /opt/ghidra/support/analyzeHeadless \
  "$ROOT/game/ghidra" AI_Multiplayer -process AI.exe -noanalysis -readOnly \
  -scriptPath "$ROOT/work/scripts/ghidra" -postScript Dump.java "$OUT" "$@" \
  > "$OUT.log" 2>&1 || { echo "analyzeHeadless falló, ver $OUT.log"; exit 1; }
grep -q 'REPORT: Post-analysis succeeded\|REPORT: Analysis succeeded\|Execute script: Dump.java' "$OUT.log" || true
