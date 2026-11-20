#!/usr/bin/env bash
# No HUD with crosshair (Nexus 7). Instalación según su README.txt: "Simply drop the UI.PAK file in Alien Isolation\DATA"
# (pisa la interfaz entera del juego; el README pide backup del original).
# Uso: toggle.sh [on|off|status|info]   (sin argumento: alterna). Lógica común: ../_lib/toggle-lib.sh
MOD_DIR="$(cd "$(dirname "$0")" && pwd)"
MOD_NOMBRE="No HUD with crosshair"
MOD_ARCHIVOS=( "UI.PAK|DATA/UI.PAK" )
MOD_INTERFIERE=1
MOD_MOTIVO="reemplaza DATA/UI.PAK (toda la UI en Flash): el mod coop usa el HUD (avisos sin pausa en el cartel de consejos), el diálogo Warning, los textos del menú y el menú Coop; sin HUD no se pueden verificar"
. "$MOD_DIR/../_lib/toggle-lib.sh"
mod_main "$@"
