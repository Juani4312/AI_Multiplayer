#!/usr/bin/env bash
# Apaga y vuelve a prender los mods que interfieren con el desarrollo del coop (AI_Multiplayer): los que en su
# toggle.sh tienen MOD_INTERFIERE=1.
#   mods-desarrollo.sh desactivar   apaga los que están activos y anota cuáles en .desactivados-para-desarrollo
#   mods-desarrollo.sh reactivar    vuelve a prender solo los que apagó "desactivar"
#   mods-desarrollo.sh status       qué interfiere, qué está activo y qué quedó anotado
# Lo llama work/scripts/mode.sh del repo (test → desactivar, play → reactivar). Ver instructions.md.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NOTA="$HERE/.desactivados-para-desarrollo"
info() { bash "$1" info | sed -n "s/^$2=//p"; }
interfieren() {
  for t in "$HERE"/*/toggle.sh; do
    [ -f "$t" ] && [ "$(info "$t" interfiere)" = 1 ] && basename "$(dirname "$t")"
  done
  return 0
}
case "${1:-status}" in
  desactivar)
    for m in $(interfieren); do
      if [ "$(info "$HERE/$m/toggle.sh" activo)" = 1 ]; then
        bash "$HERE/$m/toggle.sh" off
        grep -qx "$m" "$NOTA" 2>/dev/null || echo "$m" >> "$NOTA"
      fi
    done
    echo "mods que interfieren: apagados$( [ -s "$NOTA" ] && echo " (para reactivar: $(tr '\n' ' ' < "$NOTA"))" )" ;;
  reactivar)
    [ -s "$NOTA" ] || { echo "no hay mods apagados por mods-desarrollo.sh"; exit 0; }
    falla=0
    while IFS= read -r m; do
      [ -n "$m" ] || continue
      if [ -f "$HERE/$m/toggle.sh" ] && bash "$HERE/$m/toggle.sh" on; then :; else echo "ERROR al reactivar $m" >&2; falla=1; fi
    done < "$NOTA"
    if [ "$falla" = 0 ]; then rm -f "$NOTA"; echo "mods reactivados"; else echo "quedaron mods sin reactivar: ver $NOTA" >&2; exit 1; fi ;;
  status)
    for t in "$HERE"/*/toggle.sh; do
      [ -f "$t" ] || continue
      m="$(basename "$(dirname "$t")")"
      printf '%-20s interfiere=%s activo=%s  %s\n' "$m" "$(info "$t" interfiere)" "$(info "$t" activo)" "$(info "$t" motivo)"
    done
    [ -s "$NOTA" ] && echo "apagados por desarrollo (se reactivan con 'reactivar'): $(tr '\n' ' ' < "$NOTA")" || true ;;
  *) sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2 ;;
esac
