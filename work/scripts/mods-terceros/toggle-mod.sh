#!/usr/bin/env bash
# Activa o desactiva un mod de esta carpeta usando el toggle.sh de su subcarpeta.
#   toggle-mod.sh <carpeta> [on|off|status|info]   (sin acción: alterna)
#   toggle-mod.sh --lista                          todos los mods con toggle.sh y su estado
# Ver instructions.md.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
uso() { sed -n '2,5p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }
[ $# -ge 1 ] || uso
if [ "$1" = "--lista" ]; then
  for t in "$HERE"/*/toggle.sh; do
    [ -f "$t" ] || continue
    out="$(bash "$t" status 2>&1)" || true
    echo "$(basename "$(dirname "$t")"): ${out%%$'\n'*}"
  done
  for d in "$HERE"/*/; do
    d="${d%/}"; n="$(basename "$d")"
    [ "$n" = _lib ] && continue
    [ -f "$d/toggle.sh" ] || echo "$n: sin toggle.sh (instalación pendiente: ver $n/PENDIENTE.md)"
  done
  exit 0
fi
mod="${1%/}"; shift
case "$mod" in ""|*/*|.|..|_lib) echo "ERROR: nombre de carpeta inválido: '$mod'" >&2; exit 2 ;; esac
[ -d "$HERE/$mod" ] || { echo "ERROR: no existe la carpeta $HERE/$mod" >&2; exit 1; }
if [ ! -f "$HERE/$mod/toggle.sh" ]; then
  echo "ERROR: $mod no tiene toggle.sh$( [ -f "$HERE/$mod/PENDIENTE.md" ] && echo " (instalación pendiente: ver $mod/PENDIENTE.md)" )" >&2
  exit 1
fi
case "${1:-toggle}" in on|off|status|info|toggle|activar|desactivar|estado) ;; *) uso ;; esac
exec bash "$HERE/$mod/toggle.sh" "${1:-toggle}"
