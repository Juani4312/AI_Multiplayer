#!/usr/bin/env bash
# Ubica la ventana del juego. Uso: game_window.sh [workspace] [fullscreen 0/1]  (por defecto 3 1)
# Pedidos del usuario (2026-11-15): workspace 9 dividido con el chat (9 0) o workspace 3 a pantalla completa (3 1).
# Hyprland con config en lua (hl.dsp.*).
set -uo pipefail
WS="${1:-3}"; FS="${2:-1}"
for _ in $(seq 120); do hyprctl clients -j | grep -q steam_app_214490 && break; sleep 1; done
info() { hyprctl clients -j | python3 -c "import json,sys; c=[c for c in json.load(sys.stdin) if c['class']=='steam_app_214490']; print(c[0]['address'], c[0]['workspace']['id'], c[0]['fullscreen']) if c else print('- - -')"; }
read -r addr ws fs < <(info)
[ "$addr" = "-" ] && { echo "no está la ventana del juego"; exit 1; }
[ "$ws" != "$WS" ] && hyprctl dispatch "hl.dsp.window.move({ workspace = $WS, window = \"address:$addr\" })" >/dev/null
read -r addr ws fs < <(info)
if { [ "$FS" = 0 ] && [ "$fs" != 0 ]; } || { [ "$FS" != 0 ] && [ "$fs" = 0 ]; }; then
  hyprctl dispatch "hl.dsp.window.fullscreen({ window = \"address:$addr\" })" >/dev/null 2>&1
  sleep 0.5; read -r addr ws fs < <(info)
fi
echo "juego: workspace $ws, pantalla completa $fs"
