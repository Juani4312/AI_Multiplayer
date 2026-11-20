#!/usr/bin/env bash
# Chequeo de la instalación del juego antes de empezar a trabajar (pedido del usuario, 2026-11-20).
# Solo lee: no cambia nada. Uso: work/scripts/check_install.sh
#
# Muestra: juego abierto, modo (juego/prueba), mod coop instalado (DLL y aimp.cfg), mods de terceros de <juego>/mods/
# (cuáles interfieren y están activos), qué partida está cargada, DLLs sueltas en la carpeta del juego y si a algún
# nivel le faltan archivos (un pack de texturas mal instalado hizo que el juego no abriera).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SETUP="$ROOT/work/scripts/runtime_setup.sh"
GAME="$("$SETUP" gamedir 2>/dev/null)"
REMOTE="$(ls -d "$HOME"/.local/share/Steam/userdata/*/214490/remote 2>/dev/null | head -1)"
avisos=()

echo "== juego: $GAME"
if pgrep -x AI.exe >/dev/null; then echo "   ABIERTO (cerrarlo antes de instalar o cambiar saves)"; avisos+=("el juego está abierto"); else echo "   cerrado"; fi

echo "== modo: $(cat "$ROOT/saves/.mode" 2>/dev/null || echo play) (mode.sh: play = partida del usuario sin el mod; test <save> = prueba)"

echo "== mod coop:"
"$SETUP" status 2>/dev/null | grep -E "^(winmm|xinput1_3)\.dll|override|aimp.cfg|^  AIMP_" | sed 's/^/   /'

echo "== mods de terceros:"
if [ -x "$GAME/mods/toggle-mod.sh" ]; then
  "$GAME/mods/toggle-mod.sh" --lista 2>/dev/null | sed 's/^/   /'
  activos=$("$GAME/mods/mods-desarrollo.sh" status 2>/dev/null | awk '/interfiere=1 activo=1/{print $1}' | tr '\n' ' ')
  if [ -n "$activos" ]; then
    echo "   ACTIVOS Y QUE INTERFIEREN CON EL COOP: $activos(mode.sh test los apaga; o mods-desarrollo.sh desactivar)"
    avisos+=("mods que interfieren activos: $activos")
  fi
else
  echo "   (no hay $GAME/mods/)"
fi

echo "== partida cargada ($REMOTE):"
if [ -z "$REMOTE" ]; then echo "   no encontré la carpeta de saves"
elif ! ls "$REMOTE"/*.ais >/dev/null 2>&1; then echo "   VACÍA (sin partidas; en el juego: Start Game)"
else
  tmp="$(mktemp -d)"; hit=""
  for f in "$ROOT"/saves/*.tar.gz; do
    rm -rf "${tmp:?}"/*; tar -C "$tmp" -xzf "$f" 2>/dev/null || continue
    if diff -rq "$tmp" "$REMOTE" >/dev/null 2>&1; then hit="$hit $(basename "$f" .tar.gz)"; fi
  done
  rm -rf "$tmp"
  if [ -n "$hit" ]; then echo "   igual a saves/:$hit"
  else echo "   no coincide con ningún saves/*.tar.gz (partida del usuario en curso: no probar sobre ella)"; fi
  case "$hit" in *run-sin-morir*) echo "   ES LA RUN SIN MORIR: no probar sobre ella"; avisos+=("cargada la run sin morir") ;; esac
fi

echo "== DLLs en la carpeta del juego:"
for d in "$GAME"/*.dll "$GAME"/*.DLL; do
  [ -f "$d" ] || continue
  n="$(basename "$d")"
  case "${n,,}" in amd_ags.dll|atiags.dll|crashreporter32.dll|steam_api.dll) continue ;; esac
  q=""; grep -q -a "AI_Multiplayer" "$d" && q=" (mod coop)"
  echo "   $n  $(sha256sum "$d" | cut -c1-12)…$q"
done

echo "== niveles (DATA/ENV/PRODUCTION):"
falta=""
for lv in "$GAME"/DATA/ENV/PRODUCTION/*/; do
  lv="${lv%/}"; n="$(basename "$lv")"
  [ "$n" = DLC ] && continue
  [ -f "$lv/WORLD/COMMANDS.PAK" ] && [ -f "$lv/RENDERABLE/LEVEL_MODELS.PAK" ] || falta="$falta $n"
done
if [ -n "$falta" ]; then echo "   FALTAN ARCHIVOS en:$falta (¿un pack reemplazó la carpeta en vez de combinarse?)"; avisos+=("niveles incompletos:$falta")
else echo "   completos ($(ls -d "$GAME"/DATA/ENV/PRODUCTION/*/ | wc -l) carpetas)"; fi
otras=$(ls -d "$GAME"/DATA/ENV/*/ 2>/dev/null | xargs -n1 -d '\n' basename | grep -vx -e GLOBAL -e PRODUCTION | tr '\n' ',' | sed 's/,$//')
[ -n "$otras" ] && echo "   otras carpetas en DATA/ENV (el juego las ignora; texturas del usuario, no tocar): $otras"

echo
if [ ${#avisos[@]} -eq 0 ]; then echo "RESUMEN: sin avisos"
else echo "RESUMEN: ${#avisos[@]} aviso(s):"; printf '  - %s\n' "${avisos[@]}"; fi
