#!/usr/bin/env bash
# AIMovieFix 1.1 beta (emoose), la DLL que acompaña al "Cutscene Remaster" (videos 4K/60 en DATA/UI/MOVIES).
# Instalación según la página del remaster (captura del usuario, 2026-11-20): "Copy the DLL from the AIMovieFix.7z into
# your main game directory (where the main .exe file is)" → xinput1_3.dll al lado de AI.exe.
# Los videos del remaster ya estaban instalados (15 .USM del 2026-10-05) y no tienen backup: este toggle solo maneja la DLL.
# En Proton, xinput1_3.dll de la carpeta solo se carga con override (xinput1_3=n,b); MovieFix ya arranca por d3d11.dll
# (la misma DLL, que estaba en la carpeta desde antes y este toggle no toca).
# Uso: toggle.sh [on|off|status|info]   (sin argumento: alterna). Lógica común: ../_lib/toggle-lib.sh
MOD_DIR="$(cd "$(dirname "$0")" && pwd)"
MOD_NOMBRE="AIMovieFix"
MOD_ARCHIVOS=( "xinput1_3.dll|xinput1_3.dll" )
MOD_INTERFIERE=0
MOD_MOTIVO="solo choca con la variante xinput1_3 del mod coop (mismo nombre); el instalador del coop (runtime_setup.sh --proxy xinput1_3 --apartar-ajena) la aparta y la devuelve"
. "$MOD_DIR/../_lib/toggle-lib.sh"
mod_main "$@"
