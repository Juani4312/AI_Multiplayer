#!/usr/bin/env bash
# Instala o revierte el mod en Linux (Proton). Todo es reversible. Guía: INSTALACION.md.
#
#   runtime_setup.sh status
#   runtime_setup.sh install [--proxy winmm|xinput1_3] [--apartar-ajena] [--preset host|join] [--peer IP:47811]
#                    [--cfg "AIMP_X=1;AIMP_Y=2"]
#                    --proxy: por qué DLL entra el mod (winmm por defecto; xinput1_3 es la variante para cuando
#                      Windows no carga la winmm de la carpeta). Se instala una sola: la otra del mod se saca;
#                    --apartar-ajena: si ya hay una DLL ajena con ese nombre (MovieFix trae su xinput1_3.dll), la
#                      mueve al backup y "revert" la devuelve. Sin esta opción no la toca y no instala;
#                    --preset: la configuración recomendada de mod/dist/aimp-<preset>.cfg (join necesita --peer);
#                    --cfg: claves extra (o, sin --preset, toda la config). Sin ninguna de las dos, borra el aimp.cfg.
#   runtime_setup.sh gamedir    (imprime la carpeta del juego detectada)
#   runtime_setup.sh verify     (con el juego abierto o recién cerrado: ¿cargó el mod? ¿hay red?)
#   runtime_setup.sh revert
#
# Qué toca "install" (y "revert" deshace):
#   1. Copia la DLL (mod/build/<proxy>.dll si se compiló; si no, mod/dist/<proxy>.dll) a la carpeta del juego
#      (no pisa una DLL ajena; con --apartar-ajena la mueve a BACKUP).
#   2. Agrega al registro del prefix (pfx/user.reg) la sección
#        [Software\\Wine\\AppDefaults\\AI.exe\\DllOverrides]  "winmm"="native,builtin"  (o "xinput1_3")
#      Es el equivalente de WINEDLLOVERRIDES="winmm=n,b", pero solo para AI.exe y sin
#      tocar la config de Steam. Antes de la primera edición guarda user.reg en BACKUP.
#   3. Escribe aimp.cfg en %LOCALAPPDATA%\AI_Multiplayer del prefix.
# Se niega a correr si AI.exe o algún wineserver está corriendo (Wine reescribe user.reg).
#
# Rutas: se detectan con las bibliotecas de Steam (libraryfolders.vdf). Para forzarlas: AIMP_GAME_DIR (carpeta de
# Alien Isolation), AIMP_PFX (prefix de Proton del juego, appid 214490) y AIMP_DLL (la DLL a instalar).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
find_game() {
  local vdf lib
  for vdf in "$HOME/.local/share/Steam/steamapps/libraryfolders.vdf" "$HOME/.steam/steam/steamapps/libraryfolders.vdf" \
             "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"; do
    [ -f "$vdf" ] || continue
    while IFS= read -r lib; do
      [ -f "$lib/steamapps/common/Alien Isolation/AI.exe" ] && { echo "$lib"; return 0; }
    done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"/\1/p' "$vdf")
  done
  return 1
}
LIB="$(find_game || true)"
[ -n "$LIB" ] || LIB="/mnt/1,8TB/SteamLibrary"
GAME="${AIMP_GAME_DIR:-$LIB/steamapps/common/Alien Isolation}"
PFX="${AIMP_PFX:-$LIB/steamapps/compatdata/214490/pfx}"
# Backups (user.reg antes del override, DLL ajena apartada) en el proyecto (game/ no se commitea), no en la SteamLibrary
# (pedido del usuario, 2026-11-21).
BACKUP="${AIMP_BACKUP_DIR:-$ROOT/game/backup-runtime}"
PROXIES="winmm xinput1_3"
pick_dll() { # proxy
  if [ -n "${AIMP_DLL:-}" ]; then DLL="$AIMP_DLL"
  elif [ -f "$ROOT/mod/build/$1.dll" ]; then DLL="$ROOT/mod/build/$1.dll"
  else DLL="$ROOT/mod/dist/$1.dll"; fi
}
pick_dll winmm
EXPECT_SHA="dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90"
CFG_DIR="$PFX/drive_c/users/steamuser/AppData/Local/AI_Multiplayer"
MARK="AI_Multiplayer"   # texto que está dentro de nuestra DLL

is_ours() { [ -f "$1" ] && grep -q -a "$MARK" "$1"; }

guard_idle() {
  if pgrep -x -i 'AI.exe' >/dev/null || pgrep -x wineserver >/dev/null; then
    echo "ERROR: AI.exe o un wineserver está corriendo; cerrá el juego primero." >&2
    exit 2
  fi
}

reg_edit() { # add <proxy> | remove | check
  python3 -I - "$PFX/user.reg" "$1" "${2:-winmm}" <<'EOF'
import sys, time, os
path, mode, proxy = sys.argv[1], sys.argv[2], sys.argv[3]
KEY = "[Software\\\\Wine\\\\AppDefaults\\\\AI.exe\\\\DllOverrides]"
OURS = ['"winmm"="native,builtin"', '"xinput1_3"="native,builtin"']  # lo que puede haber puesto este script
WANT = '"%s"="native,builtin"' % proxy
data = open(path, encoding="utf-8", newline="").read()
lines = data.split("\n")
idx = [i for i, l in enumerate(lines) if l.startswith(KEY)]
def section():
    i = idx[0]
    j = i + 1
    while j < len(lines) and not lines[j].startswith("["):
        j += 1
    return i, j, [l for l in lines[i + 1:j] if l and not l.startswith("#time=")]
if mode == "check":
    print("presente: " + " ".join(section()[2]) if idx else "ausente")
    sys.exit(0)
if idx:
    i, j, body = section()
    if [l for l in body if l not in OURS]:
        print("ERROR: la sección tiene otros valores, no la toco:", body); sys.exit(3)
    if mode == "add" and body == [WANT]:
        print("ya estaba"); sys.exit(0)
    del lines[i:j]
elif mode == "remove":
    print("no estaba"); sys.exit(0)
if mode == "add":
    now = int(time.time())
    ft = (now + 11644473600) * 10**7
    block = [f"{KEY} {now}", f"#time={ft:x}", WANT, ""]
    # Insertar antes de la primera sección AppDefaults (orden alfabético: "AI.exe" va primero).
    pos = next((i for i, l in enumerate(lines) if l.startswith("[Software\\\\Wine\\\\AppDefaults\\\\")), len(lines))
    lines[pos:pos] = block
tmp = path + ".aimp-tmp"
open(tmp, "w", encoding="utf-8", newline="").write("\n".join(lines))
os.replace(tmp, path)
print("ok:", mode, proxy if mode == "add" else "")
EOF
}

status() {
  echo "AI.exe: $(sha256sum "$GAME/AI.exe" | cut -c1-16)… $( [ "$(sha256sum "$GAME/AI.exe" | cut -d' ' -f1)" = "$EXPECT_SHA" ] && echo '(build esperada)' || echo '(OTRA BUILD)')"
  local px
  for px in $PROXIES; do
    pick_dll "$px"
    if [ -f "$GAME/$px.dll" ]; then
      echo "$px.dll en el juego: $(sha256sum "$GAME/$px.dll" | cut -c1-16)… $(is_ours "$GAME/$px.dll" && echo '(es el mod)' || echo '(AJENO)')"
      if is_ours "$GAME/$px.dll"; then
        [ -f "$DLL" ] && cmp -s "$DLL" "$GAME/$px.dll" && echo "  = ${DLL#"$ROOT"/} actual" || echo "  != ${DLL#"$ROOT"/} actual"
      fi
    else
      echo "$px.dll en el juego: no"
    fi
    if [ -f "$BACKUP/$px.dll.ajena" ]; then echo "  apartada (la devuelve revert): $BACKUP/$px.dll.ajena"; fi
  done
  echo "override en user.reg: $(reg_edit check)"
  if [ -f "$CFG_DIR/aimp.cfg" ]; then echo "aimp.cfg:"; sed 's/^/  /' "$CFG_DIR/aimp.cfg"; else echo "aimp.cfg: no"; fi
  ls -la "$CFG_DIR"/aimp.log* 2>/dev/null || echo "logs: ninguno todavía"
}

# install [--preset host|join] [--peer IP:PUERTO] [--cfg "A=1;B=2"]: vale la primera aparición de cada clave, así que
# las de --cfg reemplazan a las del preset.
write_cfg() {
  local preset="" peer="" extra="" out="$CFG_DIR/aimp.cfg"
  shift
  while [ $# -gt 0 ]; do
    case "$1" in
      --preset) preset="$2"; shift 2 ;;
      --peer) peer="$2"; shift 2 ;;
      --cfg) extra="$2"; shift 2 ;;
      *) echo "ERROR: opción desconocida $1" >&2; exit 1 ;;
    esac
  done
  if [ -z "$preset" ] && [ -z "$extra" ]; then rm -f "$out"; echo "aimp.cfg: ninguno (mod sin configuración)"; return; fi
  local tmp; tmp="$(mktemp)"
  if [ -n "$extra" ]; then echo "$extra" | tr ';' '\n' | grep -v '^$' > "$tmp"; fi
  if [ -n "$preset" ]; then
    local pf="$ROOT/mod/dist/aimp-$preset.cfg"
    [ -f "$pf" ] || { echo "ERROR: no existe $pf (presets: host, join)" >&2; rm -f "$tmp"; exit 1; }
    [ "$preset" != join ] || [ -n "$peer" ] || { echo "ERROR: --preset join necesita --peer IP_DEL_HOST:47811" >&2; rm -f "$tmp"; exit 1; }
    while IFS= read -r line; do
      case "$line" in
        AIMP_NET_PEER=*) [ -n "$peer" ] && line="AIMP_NET_PEER=$peer" ;;
      esac
      local key="${line%%=*}"
      if [[ "$line" == AIMP_* ]] && grep -q "^$key=" "$tmp"; then continue; fi
      echo "$line" >> "$tmp"
    done < "$pf"
  fi
  mv "$tmp" "$out"
  echo "aimp.cfg escrito ($( [ -n "$preset" ] && echo "preset $preset" || echo "a mano")):"; grep -v '^#' "$out" | sed 's/^/  /'
}

verify() {
  local log="$CFG_DIR/aimp.log"
  if [ ! -f "$log" ]; then
    echo "NO hay $log: el mod no cargó. Revisar que winmm.dll esté al lado de AI.exe y el override ('$0 status')."
    echo "Si se lanzó con opciones propias, que no pisen WINEDLLOVERRIDES (o agregar winmm=n,b)."
    return 1
  fi
  echo "log: $log ($(stat -c '%y' "$log" | cut -c1-19))"
  head -6 "$log"
  echo "..."
  grep -E 'build|FATAL|ERROR|NET |SESSION|SAVESYNC' "$log" | tail -10 || true
  if grep -q 'build NO reconocida' "$log"; then echo "=> build del juego no reconocida: el mod queda inactivo (verificar integridad en Steam)";
  elif grep -qE 'recibidos [1-9]' "$log"; then echo "=> el mod cargó y hay datos del otro jugador";
  else echo "=> el mod cargó; todavía sin datos del otro jugador (¿el otro tiene el juego abierto? ¿IP/Tailscale?)"; fi
}

case "${1:-status}" in
  status) status ;;
  gamedir) echo "$GAME" ;;
  install)
    guard_idle
    proxy=winmm; apartar=0; rest=()
    shift
    while [ $# -gt 0 ]; do
      case "$1" in
        --proxy) proxy="$2"; shift 2 ;;
        --apartar-ajena) apartar=1; shift ;;
        *) rest+=("$1"); shift ;;
      esac
    done
    case " $PROXIES " in *" $proxy "*) ;; *) echo "ERROR: --proxy tiene que ser uno de: $PROXIES" >&2; exit 1 ;; esac
    pick_dll "$proxy"
    [ "$(sha256sum "$GAME/AI.exe" | cut -d' ' -f1)" = "$EXPECT_SHA" ] || { echo "ERROR: AI.exe no es la build esperada" >&2; exit 2; }
    [ -f "$DLL" ] || { echo "ERROR: falta $DLL" >&2; exit 2; }
    [ -d "$PFX" ] || { echo "ERROR: no está el prefix $PFX: abrí el juego una vez con Proton (o definí AIMP_PFX)" >&2; exit 2; }
    if [ -f "$GAME/$proxy.dll" ] && ! is_ours "$GAME/$proxy.dll"; then
      if [ "$apartar" = 1 ]; then
        mkdir -p "$BACKUP"
        if [ -f "$BACKUP/$proxy.dll.ajena" ]; then echo "ERROR: ya hay una $proxy.dll apartada en $BACKUP; no la piso" >&2; exit 2; fi
        mv "$GAME/$proxy.dll" "$BACKUP/$proxy.dll.ajena"
        echo "apartada: $GAME/$proxy.dll -> $BACKUP/$proxy.dll.ajena (revert la devuelve)"
      else
        echo "ERROR: ya hay un $proxy.dll ajeno en la carpeta del juego; no lo piso (--apartar-ajena lo mueve al backup)" >&2; exit 2
      fi
    fi
    for px in $PROXIES; do # una sola variante del mod a la vez
      if [ "$px" != "$proxy" ] && is_ours "$GAME/$px.dll"; then rm -f "$GAME/$px.dll"; echo "borrado: $GAME/$px.dll (otra variante del mod)"; fi
    done
    mkdir -p "$BACKUP"
    if ! ls "$BACKUP"/user.reg.* >/dev/null 2>&1; then
      ts=$(date +%Y%m%d-%H%M%S)
      cp -p "$PFX/user.reg" "$BACKUP/user.reg.$ts"
      printf 'Backup de pfx/user.reg del prefix 214490 antes de agregar el override de winmm (%s).\nRevertir: work/scripts/runtime_setup.sh revert (saca solo la sección agregada).\n' "$ts" > "$BACKUP/LEEME.txt"
      echo "backup: $BACKUP/user.reg.$ts"
    fi
    cp "$DLL" "$GAME/$proxy.dll"
    echo "copiado: ${DLL#"$ROOT"/} -> $GAME/$proxy.dll"
    reg_edit add "$proxy"
    mkdir -p "$CFG_DIR"
    write_cfg install "${rest[@]}"
    status
    echo "siguiente: abrir el juego desde Steam (no hace falta opción de lanzamiento) y, en el menú, '$0 verify'" ;;
  verify) verify ;;
  revert)
    guard_idle
    for px in $PROXIES; do
      if [ -f "$GAME/$px.dll" ]; then
        if is_ours "$GAME/$px.dll"; then rm -f "$GAME/$px.dll"; echo "borrado: $GAME/$px.dll"
        elif [ "$px" = winmm ]; then echo "AVISO: el winmm.dll del juego no es el mod; no lo toco"; fi
      fi
      if [ -f "$BACKUP/$px.dll.ajena" ]; then
        if [ -f "$GAME/$px.dll" ]; then echo "AVISO: no devuelvo $BACKUP/$px.dll.ajena: ya hay un $px.dll en el juego"
        else mv "$BACKUP/$px.dll.ajena" "$GAME/$px.dll"; echo "devuelta: $GAME/$px.dll (la ajena que se había apartado)"; fi
      fi
    done
    reg_edit remove
    rm -f "$CFG_DIR/aimp.cfg"
    echo "(los logs en $CFG_DIR quedan; borrarlos a mano si se quiere)"
    status ;;
  *) echo "uso: $0 status | install [--proxy winmm|xinput1_3] [--apartar-ajena] [--preset host|join] [--peer IP:47811] [--cfg \"A=1;B=2\"] | verify | revert" >&2; exit 1 ;;
esac
