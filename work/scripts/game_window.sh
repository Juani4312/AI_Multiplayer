#!/usr/bin/env bash
# Ubica la ventana del juego en el workspace 9 (pantalla dividida con el chat) y le saca la pantalla completa.
# Pedido del usuario (2026-11-15). Hyprland con config en lua (hl.dsp.*).
set -uo pipefail
for _ in $(seq 120); do hyprctl clients -j | grep -q steam_app_214490 && break; sleep 1; done
info() { hyprctl clients -j | python3 -c "import json,sys; c=[c for c in json.load(sys.stdin) if c['class']=='steam_app_214490']; print(c[0]['address'], c[0]['workspace']['id'], c[0]['fullscreen']) if c else print('- - -')"; }
read -r addr ws fs < <(info)
[ "$addr" = "-" ] && { echo "no está la ventana del juego"; exit 1; }
[ "$ws" != 9 ] && hyprctl dispatch "hl.dsp.window.move({ workspace = 9, window = \"address:$addr\" })" >/dev/null
read -r addr ws fs < <(info)
if [ "$fs" != 0 ]; then
  hyprctl dispatch "hl.dsp.window.fullscreen({ window = \"address:$addr\" })" >/dev/null 2>&1
  sleep 0.5; read -r addr ws fs < <(info)
fi
echo "juego: workspace $ws, pantalla completa $fs"
