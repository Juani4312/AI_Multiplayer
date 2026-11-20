#!/usr/bin/env bash
# Lógica común de los toggle.sh de cada mod (se carga con "source"; no se ejecuta sola).
#
# Cada mods/<mod>/toggle.sh define, antes de cargar esta librería:
#   MOD_DIR="$(cd "$(dirname "$0")" && pwd)"   su propia carpeta
#   MOD_NOMBRE="..."            nombre legible
#   MOD_ARCHIVOS=( "origen|destino" ... )
#                               origen: ruta dentro de la carpeta del mod; destino: ruta dentro de la carpeta del juego
#   MOD_INTERFIERE=0|1          1 = molesta al desarrollo del coop (lo apaga mods-desarrollo.sh)
#   MOD_MOTIVO="..."            por qué interfiere (o por qué no)
# y llama a:  mod_main "$@"
#
# Acciones: on (instalar), off (desinstalar), status, info, o nada (alternar).
#
# Garantías:
#   - no hace nada con el juego abierto (AI.exe);
#   - antes de instalar guarda cada original en <mod>/bak/orig/<destino> y anota su hash en <mod>/bak/manifiesto;
#     si el destino no existía, lo anota (al desinstalar se borra);
#   - verifica el hash de cada archivo copiado; si algo falla a mitad de camino, deshace lo hecho;
#   - al desinstalar solo restaura un archivo si sigue siendo el del mod (mismo hash). Si cambió por fuera (Steam
#     "verificar integridad", otro mod), no lo toca, avisa y deja el backup;
#   - el backup se borra recién cuando el original quedó restaurado y verificado.
set -Eeuo pipefail

_die() { echo "ERROR ($MOD_NOMBRE): $*" >&2; exit 1; }
_sha() { sha256sum -- "$1" | cut -d' ' -f1; }

_init() {
  [ -n "${MOD_DIR:-}" ] && [ -d "$MOD_DIR" ] || _die "toggle.sh tiene que definir MOD_DIR (su propia carpeta)"
  MOD_DIR="$(cd "$MOD_DIR" && pwd)"
  GAME_DIR="$(cd "$MOD_DIR/../.." && pwd)"
  BAK="$MOD_DIR/bak"
  MANI="$BAK/manifiesto"      # destino<TAB>existía(0/1)<TAB>sha original<TAB>sha del mod
  [ -f "$GAME_DIR/AI.exe" ] || _die "no encuentro AI.exe en $GAME_DIR (la carpeta del mod tiene que estar en <juego>/mods/)"
  [ "${#MOD_ARCHIVOS[@]}" -gt 0 ] || _die "MOD_ARCHIVOS está vacío"
  local par src dst
  for par in "${MOD_ARCHIVOS[@]}"; do
    src="${par%%|*}"; dst="${par#*|}"
    [ -n "$src" ] && [ -n "$dst" ] && [ "$src" != "$par" ] || _die "entrada mal formada en MOD_ARCHIVOS: '$par'"
    case "$dst" in /*|*..*) _die "destino inválido (tiene que ser relativo al juego, sin ..): '$dst'" ;; esac
    case "$src" in /*|*..*) _die "origen inválido (relativo a la carpeta del mod, sin ..): '$src'" ;; esac
  done
}

_activo() { [ -f "$MANI" ]; }

_juego_cerrado() {
  if pgrep -x AI.exe >/dev/null 2>&1 || pgrep -x -i 'AI.exe' >/dev/null 2>&1; then
    _die "el juego está abierto: cerralo antes"
  fi
}

_status() {
  local par src dst g s0 sm estado
  echo "$MOD_NOMBRE: $(_activo && echo ACTIVO || echo inactivo)$( [ "$MOD_INTERFIERE" = 1 ] && echo ' — interfiere con el desarrollo del coop' )"
  for par in "${MOD_ARCHIVOS[@]}"; do
    src="${par%%|*}"; dst="${par#*|}"; g="$GAME_DIR/$dst"
    if [ ! -e "$g" ]; then estado="no existe"
    else
      sm=$(_sha "$MOD_DIR/$src")
      s0=$(_activo && awk -F'\t' -v d="$dst" '$1==d{print $3}' "$MANI" || true)
      case "$(_sha "$g")" in
        "$sm") estado="la del mod" ;;
        *) if [ -n "$s0" ] && [ "$(_sha "$g")" = "$s0" ]; then estado="la original"; else estado="otra (original o de otro mod)"; fi ;;
      esac
    fi
    echo "  $dst: $estado"
  done
}

_on() {
  _juego_cerrado
  _activo && { echo "$MOD_NOMBRE ya está activo (nada que hacer)"; return 0; }
  [ -d "$BAK/orig" ] && [ -n "$(find "$BAK/orig" -type f -print -quit 2>/dev/null)" ] && \
    _die "quedó un backup de una desinstalación incompleta en $BAK/orig: revisalo a mano antes de seguir"
  local par src dst g tmp hechos=() existia s0 sm
  for par in "${MOD_ARCHIVOS[@]}"; do
    src="${par%%|*}"; [ -f "$MOD_DIR/$src" ] || _die "falta $MOD_DIR/$src"
  done
  mkdir -p "$BAK/orig"
  tmp="$MANI.nuevo"; : > "$tmp"
  _deshacer() {
    echo "  algo falló: deshago lo instalado" >&2
    local l d e
    for l in "${hechos[@]}"; do
      d="${l%%|*}"; e="${l#*|}"
      if [ "$e" = 1 ]; then cp -p -- "$BAK/orig/$d" "$GAME_DIR/$d" && rm -f -- "$BAK/orig/$d"
      else rm -f -- "$GAME_DIR/$d"; fi
    done
    rm -f -- "$tmp"
    find "$BAK/orig" -depth -type d -empty -delete 2>/dev/null || true
  }
  trap '_deshacer' ERR
  for par in "${MOD_ARCHIVOS[@]}"; do
    src="${par%%|*}"; dst="${par#*|}"; g="$GAME_DIR/$dst"
    sm=$(_sha "$MOD_DIR/$src")
    if [ -e "$g" ]; then
      existia=1; s0=$(_sha "$g")
      [ "$s0" = "$sm" ] && echo "  aviso: $dst ya era idéntico al del mod"
      mkdir -p -- "$(dirname -- "$BAK/orig/$dst")"
      cp -p -- "$g" "$BAK/orig/$dst"
      [ "$(_sha "$BAK/orig/$dst")" = "$s0" ] || false
    else
      existia=0; s0="-"
      mkdir -p -- "$(dirname -- "$g")"
    fi
    hechos+=("$dst|$existia")
    cp -- "$MOD_DIR/$src" "$g.mod-tmp"
    mv -f -- "$g.mod-tmp" "$g"
    [ "$(_sha "$g")" = "$sm" ] || false
    printf '%s\t%s\t%s\t%s\n' "$dst" "$existia" "$s0" "$sm" >> "$tmp"
    echo "  instalado: $dst$( [ "$existia" = 1 ] && echo ' (original guardado en bak/orig)' )"
  done
  trap - ERR
  mv -f -- "$tmp" "$MANI"
  echo "$(date '+%F %T') on" >> "$BAK/historial"
  echo "$MOD_NOMBRE: ACTIVADO"
}

_off() {
  _juego_cerrado
  _activo || { echo "$MOD_NOMBRE ya está inactivo (nada que hacer)"; return 0; }
  local dst existia s0 sm g actual problemas=0
  # Primero se verifica todo; después se restaura (así no queda a medias por un archivo cambiado).
  while IFS=$'\t' read -r dst existia s0 sm; do
    g="$GAME_DIR/$dst"
    if [ ! -e "$g" ]; then
      echo "  aviso: $dst ya no existe en el juego"; continue
    fi
    actual=$(_sha "$g")
    if [ "$actual" != "$sm" ] && [ "$actual" != "$s0" ]; then
      echo "  PROBLEMA: $dst cambió por fuera (no es ni el del mod ni el original): no lo toco" >&2
      problemas=1
    fi
    if [ "$existia" = 1 ] && [ "$(_sha "$BAK/orig/$dst" 2>/dev/null || echo x)" != "$s0" ]; then
      echo "  PROBLEMA: el backup de $dst falta o está dañado ($BAK/orig/$dst)" >&2
      problemas=1
    fi
  done < "$MANI"
  [ "$problemas" = 0 ] || _die "no se desinstaló nada. Opciones: restaurar a mano desde $BAK/orig, o Steam → Verificar integridad, y borrar $MANI"
  while IFS=$'\t' read -r dst existia s0 sm; do
    g="$GAME_DIR/$dst"
    if [ "$existia" = 1 ]; then
      cp -p -- "$BAK/orig/$dst" "$g.orig-tmp"
      mv -f -- "$g.orig-tmp" "$g"
      [ "$(_sha "$g")" = "$s0" ] || _die "la restauración de $dst no verificó; el backup sigue en $BAK/orig"
      rm -f -- "$BAK/orig/$dst"
      echo "  restaurado: $dst"
    else
      rm -f -- "$g"
      echo "  borrado: $dst (no existía antes del mod)"
    fi
  done < "$MANI"
  find "$BAK/orig" -depth -type d -empty -delete 2>/dev/null || true
  rm -f -- "$MANI"
  echo "$(date '+%F %T') off" >> "$BAK/historial"
  echo "$MOD_NOMBRE: DESACTIVADO"
}

mod_main() {
  _init
  case "${1:-toggle}" in
    on|activar) _on ;;
    off|desactivar) _off ;;
    status|estado) _status ;;
    info) echo "interfiere=$MOD_INTERFIERE"; echo "activo=$(_activo && echo 1 || echo 0)"; echo "motivo=$MOD_MOTIVO" ;;
    toggle) if _activo; then _off; else _on; fi ;;
    *) echo "uso: $0 [on|off|status|info]   (sin argumento: alterna)" >&2; exit 2 ;;
  esac
}
