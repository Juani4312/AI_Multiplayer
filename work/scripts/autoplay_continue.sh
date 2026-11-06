#!/usr/bin/env bash
# Pruebas sin nadie al teclado: con el juego recién lanzado y el mod con AIMP_AUTOPAD_FILE apuntando a
# pad.txt del prefix, pasa las intros, elige Play Game > Alien: Isolation > Continue Game y espera a
# que aparezca el jugador 0. Uso: work/scripts/autoplay_continue.sh [hash_del_build_en_el_log]
set -uo pipefail
CFGD="/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer"
L="$CFGD/aimp.log"; PAD="$CFGD/pad.txt"; T0=$SECONDS
want="${1:-}"
wait_for() { # patrón timeout
  local end=$((SECONDS+$2))
  until grep -q "$1" "$L" 2>/dev/null; do
    [ $SECONDS -ge $end ] && { echo "timeout esperando: $1"; return 1; }
    pgrep -x AI.exe >/dev/null || { [ $((SECONDS-T0)) -gt 30 ] && { echo "AI.exe no está corriendo"; return 1; }; }
    sleep 1
  done
}
# log nuevo (el anterior pasa a .prev al abrir)
end=$((SECONDS+120))
until [ -f "$L" ] && head -1 "$L" | grep -q "AI_Multiplayer" && { [ -z "$want" ] || head -1 "$L" | grep -q "$want"; } && \
      [ "$(stat -c %Y "$L")" -ge "$(( $(date +%s) - 30 ))" ]; do
  [ $SECONDS -ge $end ] && { echo "timeout esperando el log nuevo"; exit 1; }; sleep 1
done
wait_for '\] PIN f\|nivel 0/35' 150 || exit 1
printf '%s\n' "2500-2650:A,4000-4150:START,5500-5650:A,7500-7650:A,9000-9150:A,10300-10450:DOWN,11500-11650:A" > "$PAD"
echo "menú: guion enviado ($((SECONDS-T0)) s)"
wait_for 'AUTOPAD: apareció el jugador 0' 180 || exit 1
grep -E "EV nivel actual" "$L" | tail -1 | cut -c1-160
grep -E "J0 pos" "$L" | tail -1
echo "listo en $((SECONDS-T0)) s"
