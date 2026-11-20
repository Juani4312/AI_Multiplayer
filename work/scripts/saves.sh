#!/usr/bin/env bash
# Partidas guardadas para pruebas (saves/<nombre>.tar.gz).
# El juego guarda en la carpeta de Steam Cloud: userdata/<cuenta>/214490/remote/
#   MSSV01..19.ais (slots de misión), CHSV00/01.ais (checkpoints), PROGRESSION.ais, input.cfg.
# Se guarda y se restaura la carpeta entera, para que slots y progresión queden coherentes.
#
#   saves.sh list               lista los saves del repo
#   saves.sh save <nombre>      guarda los saves actuales del juego como saves/<nombre>.tar.gz
#   saves.sh load <nombre>      reemplaza los saves del juego por saves/<nombre>.tar.gz
#                               (antes hace un backup local en saves/backups/, ignorado por git)
#   saves.sh vaciar             deja el juego sin partidas (borra los .ais; conserva input.cfg, los controles),
#                               con el mismo backup previo. En el juego: Start Game (partida nueva)
#
# Con el juego cerrado. Para `load`, mejor también con Steam cerrado: Steam Cloud sincroniza al abrir
# y cerrar el juego, y con los archivos cambiados puede preguntar qué versión conservar (elegir la local).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$ROOT/saves"
REMOTE="${AIMP_SAVE_DIR:-$(ls -d "$HOME"/.local/share/Steam/userdata/*/214490/remote 2>/dev/null | head -1)}"
[ -n "$REMOTE" ] && [ -d "$REMOTE" ] || { echo "no encontré la carpeta de saves (userdata/*/214490/remote); usar AIMP_SAVE_DIR"; exit 1; }
if pgrep -x AI.exe >/dev/null; then echo "el juego está abierto: cerralo antes"; exit 1; fi
case "${1:-list}" in
list)
  echo "saves del juego: $REMOTE"
  ls -1 "$DIR"/*.tar.gz 2>/dev/null | sed 's#.*/##; s#\.tar\.gz$##' || echo "(ninguno)" ;;
save)
  n="${2:?nombre}"
  tar -C "$REMOTE" -czf "$DIR/$n.tar.gz" .
  echo "guardado: saves/$n.tar.gz ($(du -h "$DIR/$n.tar.gz" | cut -f1))" ;;
load)
  n="${2:?nombre}"; f="$DIR/$n.tar.gz"
  [ -f "$f" ] || { echo "no existe $f"; exit 1; }
  pgrep -x steam >/dev/null && echo "aviso: Steam está abierto; si al abrir el juego pregunta por Steam Cloud, elegir los archivos locales"
  mkdir -p "$DIR/backups"
  b="$DIR/backups/antes-de-$n-$(date +%Y%m%d-%H%M%S).tar.gz"
  tar -C "$REMOTE" -czf "$b" .
  echo "backup del estado actual: ${b#"$ROOT"/}"
  find "$REMOTE" -maxdepth 1 -type f \( -name '*.ais' -o -name 'input.cfg' \) -delete
  tar -C "$REMOTE" -xzf "$f"
  echo "cargado: $n (en el juego: Continue Game)" ;;
vaciar)
  pgrep -x steam >/dev/null && echo "aviso: Steam está abierto; si al abrir el juego pregunta por Steam Cloud, elegir los archivos locales"
  mkdir -p "$DIR/backups"
  b="$DIR/backups/antes-de-vaciar-$(date +%Y%m%d-%H%M%S).tar.gz"
  tar -C "$REMOTE" -czf "$b" .
  echo "backup del estado actual: ${b#"$ROOT"/}"
  find "$REMOTE" -maxdepth 1 -type f -name '*.ais' -delete
  echo "vacío: sin partidas (queda $(ls "$REMOTE" | tr '\n' ' '))" ;;
*)
  sed -n '2,18p' "$0"; exit 1 ;;
esac
