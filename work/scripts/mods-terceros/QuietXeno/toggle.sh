#!/usr/bin/env bash
# Quiet Xeno (v1.0). Instalación según su readme.txt: extraer DATA en la carpeta del juego, reemplazando
# DATA/SOUND/ALIEN.BNK (el readme recomienda backup del original).
# Uso: toggle.sh [on|off|status|info]   (sin argumento: alterna). Lógica común: ../_lib/toggle-lib.sh
MOD_DIR="$(cd "$(dirname "$0")" && pwd)"
MOD_NOMBRE="Quiet Xeno"
MOD_ARCHIVOS=( "DATA/SOUND/ALIEN.BNK|DATA/SOUND/ALIEN.BNK" )
MOD_INTERFIERE=0
MOD_MOTIVO="solo cambia sonidos del Alien (banco de audio): no toca lógica, IA ni UI; los dos jugadores pueden tenerlo distinto"
. "$MOD_DIR/../_lib/toggle-lib.sh"
mod_main "$@"
