#!/usr/bin/env bash
# Pruebas sin nadie al teclado (mod con AIMP_AUTOPAD_FILE = pad.txt del prefix).
#   autoplay_continue.sh menu    -> espera el frontend, manda A,A,A,DOWN y saca captura de la ventana del juego.
#                                   Desde el título queda "Continue Game" resaltado. Desde cualquier
#                                   estado anterior esa secuencia NO llega a confirmar una partida nueva.
#   autoplay_continue.sh go      -> manda A (Continue) y espera al jugador 0.
# Corrida 17: un guion a ciegas con más A llegó al diálogo "Start new game". Verificar la captura
# antes de "go".
set -uo pipefail
CFGD="/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer"
L="$CFGD/aimp.log"; PAD="$CFGD/pad.txt"; SHOT="${AIMP_SHOT:-/tmp/aimp_menu.png}"
w() { local end=$((SECONDS+$1)); until [ $SECONDS -ge $end ]; do sleep 0.5; done; }
# Captura solo la ventana del juego (Hyprland), nunca un monitor entero: en el otro puede haber cosas privadas.
shot_game() {
  local g
  g=$(hyprctl clients -j 2>/dev/null | python3 -c "import json,sys; c=[c for c in json.load(sys.stdin) if c['class']=='steam_app_214490']; print('%d,%d %dx%d'%(c[0]['at'][0],c[0]['at'][1],c[0]['size'][0],c[0]['size'][1])) if c else None" 2>/dev/null)
  [ -n "$g" ] && [ "$g" != "None" ] || { echo "no se encontró la ventana del juego; sin captura"; return 1; }
  grim -g "$g" -s 0.5 "$1"
}
case "${1:-menu}" in
menu)
  end=$((SECONDS+40))
  until [ "$(stat -c %Y "$L" 2>/dev/null || echo 0)" -ge "$(( $(date +%s) - 20 ))" ] || [ $SECONDS -ge $end ]; do sleep 1; done
  end=$((SECONDS+180))
  until grep -q '\] PIN f\|nivel 0/35' "$L" 2>/dev/null; do
    [ $SECONDS -ge $end ] && { echo "timeout esperando el frontend"; exit 1; }; sleep 1
  done
  w 7
  printf '%s\n' "0-150:A,2000-2150:A,3500-3650:A,5000-5150:DOWN" > "$PAD"; w 7
  shot_game "$SHOT" && echo "captura: $SHOT (verificar 'Continue Game' resaltado antes de 'go')" ;;
go)
  printf '%s\n' "0-150:A" > "$PAD"
  end=$((SECONDS+180)); n0=$(grep -c "apareció el jugador 0" "$L")
  until [ "$(grep -c "apareció el jugador 0" "$L")" -gt "$n0" ]; do
    [ $SECONDS -ge $end ] && { echo "timeout esperando al jugador"; exit 1; }; sleep 1
  done
  grep -E "EV nivel actual" "$L" | tail -1 | cut -c1-140; grep "J0 pos" "$L" | tail -1 ;;
esac
