#!/usr/bin/env bash
# Cambia papito entre "juego" (la partida real del usuario, sin mod) y "prueba" (un save de prueba, con el mod).
# Pedido del usuario (2026-11-17): su partida es una run sin morir; las pruebas nunca se hacen sobre ella.
#
#   mode.sh status               en qué modo está y qué save hay cargado
#   mode.sh test [save] [--cfg "AIMP_X=1;..."]
#                                pasa a prueba: guarda la partida del usuario en saves/current.tar.gz (solo si
#                                el modo actual es "juego"), carga el save de prueba (safezone por defecto; otro
#                                si hace falta Alien o enemigos) e instala el mod (pasivo, o con la cfg dada)
#   mode.sh play                 vuelve al juego: carga saves/current.tar.gz (lo que pasó en la prueba se
#                                descarta), y desinstala el mod (sin winmm.dll, sin override, sin aimp.cfg)
#
# Estado en saves/.mode (local, no se commitea): "play" o "test <save>". Sin archivo se asume "play" (la
# partida cargada es la del usuario). Con el juego cerrado. saves.sh load deja además un backup en saves/backups/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ST="$ROOT/saves/.mode"
SAVES="$ROOT/work/scripts/saves.sh"
SETUP="$ROOT/work/scripts/runtime_setup.sh"
cur() { [ -f "$ST" ] && cat "$ST" || echo play; }
if pgrep -x AI.exe >/dev/null; then echo "el juego está abierto: cerralo antes (pkill -x AI.exe)"; exit 1; fi
case "${1:-status}" in
status)
  echo "modo: $(cur)" ;;
test)
  shift
  save=safezone; cfg="AIMP_GHOST=0"
  while [ $# -gt 0 ]; do
    case "$1" in
      --cfg) cfg="$2"; shift 2 ;;
      *) save="$1"; shift ;;
    esac
  done
  [ "$save" = current ] && { echo "current es la partida del usuario: no se prueba sobre ella"; exit 1; }
  [ -f "$ROOT/saves/$save.tar.gz" ] || { echo "no existe saves/$save.tar.gz"; exit 1; }
  m=$(cur)
  if [ "$m" = play ]; then
    "$SAVES" save current
  else
    echo "ya en prueba ($m): saves/current.tar.gz no se toca"
  fi
  "$SAVES" load "$save"
  "$SETUP" install --cfg "$cfg" >/dev/null
  echo "test $save" > "$ST"
  echo "modo: prueba con $save; mod instalado ($cfg)" ;;
play)
  m=$(cur)
  if [ "$m" != play ]; then
    "$SAVES" load current
  else
    echo "ya en modo juego: la partida no se toca"
  fi
  "$SETUP" revert >/dev/null
  echo play > "$ST"
  echo "modo: juego (partida del usuario, mod desinstalado)" ;;
*)
  sed -n '2,16p' "$0"; exit 1 ;;
esac
