#!/usr/bin/env bash
# Instala o revierte el mod en la instalación del usuario (Proton). Todo es reversible.
#
#   runtime_setup.sh status
#   runtime_setup.sh install [--cfg "AIMP_X=1;AIMP_Y=2"]   (sin --cfg: borra el aimp.cfg)
#   runtime_setup.sh revert
#
# Qué toca "install" (y "revert" deshace):
#   1. Copia mod/build/winmm.dll a la carpeta del juego (no pisa un winmm.dll ajeno).
#   2. Agrega al registro del prefix (pfx/user.reg) la sección
#        [Software\\Wine\\AppDefaults\\AI.exe\\DllOverrides]  "winmm"="native,builtin"
#      Es el equivalente de WINEDLLOVERRIDES="winmm=n,b", pero solo para AI.exe y sin
#      tocar la config de Steam. Antes de la primera edición guarda user.reg en BACKUP.
#   3. Opcional: escribe aimp.cfg en %LOCALAPPDATA%\AI_Multiplayer del prefix.
# Se niega a correr si AI.exe o algún wineserver está corriendo (Wine reescribe user.reg).
set -euo pipefail

GAME="/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation"
PFX="/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490/pfx"
BACKUP="/mnt/1,8TB/SteamLibrary/AlienIsolation_backup_AI_Multiplayer/runtime"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DLL="$ROOT/mod/build/winmm.dll"
EXPECT_SHA="dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90"
CFG_DIR="$PFX/drive_c/users/steamuser/AppData/Local/AI_Multiplayer"
MARK="AI_Multiplayer"   # texto que está dentro de nuestra DLL

is_ours() { [ -f "$1" ] && grep -q -a "$MARK" "$1"; }

guard_idle() {
  if pgrep -f -i 'AI\.exe' >/dev/null || pgrep -x wineserver >/dev/null; then
    echo "ERROR: AI.exe o un wineserver está corriendo; cerrá el juego primero." >&2
    exit 2
  fi
}

reg_edit() { # add|remove|check
  python3 -I - "$PFX/user.reg" "$1" <<'EOF'
import sys, time, os
path, mode = sys.argv[1], sys.argv[2]
KEY = "[Software\\\\Wine\\\\AppDefaults\\\\AI.exe\\\\DllOverrides]"
data = open(path, encoding="utf-8", newline="").read()
lines = data.split("\n")
idx = [i for i, l in enumerate(lines) if l.startswith(KEY)]
if mode == "check":
    print("presente" if idx else "ausente")
    sys.exit(0)
if mode == "add":
    if idx:
        print("ya estaba"); sys.exit(0)
    now = int(time.time())
    ft = (now + 11644473600) * 10**7
    block = [f"{KEY} {now}", f"#time={ft:x}", '"winmm"="native,builtin"', ""]
    # Insertar antes de la primera sección AppDefaults (orden alfabético: "AI.exe" va primero).
    pos = next((i for i, l in enumerate(lines) if l.startswith("[Software\\\\Wine\\\\AppDefaults\\\\")), len(lines))
    lines[pos:pos] = block
elif mode == "remove":
    if not idx:
        print("no estaba"); sys.exit(0)
    i = idx[0]
    j = i + 1
    while j < len(lines) and not lines[j].startswith("["):
        j += 1
    body = [l for l in lines[i + 1:j] if l and not l.startswith("#time=")]
    if body != ['"winmm"="native,builtin"']:
        print("ERROR: la sección tiene otros valores, no la toco:", body); sys.exit(3)
    del lines[i:j]
tmp = path + ".aimp-tmp"
open(tmp, "w", encoding="utf-8", newline="").write("\n".join(lines))
os.replace(tmp, path)
print("ok:", mode)
EOF
}

status() {
  echo "AI.exe: $(sha256sum "$GAME/AI.exe" | cut -c1-16)… $( [ "$(sha256sum "$GAME/AI.exe" | cut -d' ' -f1)" = "$EXPECT_SHA" ] && echo '(build esperada)' || echo '(OTRA BUILD)')"
  if [ -f "$GAME/winmm.dll" ]; then
    echo "winmm.dll en el juego: $(sha256sum "$GAME/winmm.dll" | cut -c1-16)… $(is_ours "$GAME/winmm.dll" && echo '(es el mod)' || echo '(AJENO)')"
    [ -f "$DLL" ] && cmp -s "$DLL" "$GAME/winmm.dll" && echo "  = mod/build/winmm.dll actual" || echo "  != mod/build/winmm.dll actual"
  else
    echo "winmm.dll en el juego: no"
  fi
  echo "override en user.reg: $(reg_edit check)"
  if [ -f "$CFG_DIR/aimp.cfg" ]; then echo "aimp.cfg:"; sed 's/^/  /' "$CFG_DIR/aimp.cfg"; else echo "aimp.cfg: no"; fi
  ls -la "$CFG_DIR"/aimp.log* 2>/dev/null || echo "logs: ninguno todavía"
}

case "${1:-status}" in
  status) status ;;
  install)
    guard_idle
    [ "$(sha256sum "$GAME/AI.exe" | cut -d' ' -f1)" = "$EXPECT_SHA" ] || { echo "ERROR: AI.exe no es la build esperada" >&2; exit 2; }
    [ -f "$DLL" ] || { echo "ERROR: falta $DLL (compilar primero)" >&2; exit 2; }
    if [ -f "$GAME/winmm.dll" ] && ! is_ours "$GAME/winmm.dll"; then
      echo "ERROR: ya hay un winmm.dll ajeno en la carpeta del juego; no lo piso" >&2; exit 2
    fi
    mkdir -p "$BACKUP"
    if ! ls "$BACKUP"/user.reg.* >/dev/null 2>&1; then
      ts=$(date +%Y%m%d-%H%M%S)
      cp -p "$PFX/user.reg" "$BACKUP/user.reg.$ts"
      printf 'Backup de pfx/user.reg del prefix 214490 antes de agregar el override de winmm (%s).\nRevertir: work/scripts/runtime_setup.sh revert (saca solo la sección agregada).\n' "$ts" > "$BACKUP/LEEME.txt"
      echo "backup: $BACKUP/user.reg.$ts"
    fi
    cp "$DLL" "$GAME/winmm.dll"
    echo "copiado: $GAME/winmm.dll"
    reg_edit add
    mkdir -p "$CFG_DIR"
    if [ "${2:-}" = "--cfg" ] && [ -n "${3:-}" ]; then
      echo "$3" | tr ';' '\n' > "$CFG_DIR/aimp.cfg"; echo "aimp.cfg escrito"
    else
      rm -f "$CFG_DIR/aimp.cfg"
    fi
    status ;;
  revert)
    guard_idle
    if [ -f "$GAME/winmm.dll" ]; then
      if is_ours "$GAME/winmm.dll"; then rm -f "$GAME/winmm.dll"; echo "borrado: $GAME/winmm.dll"
      else echo "AVISO: el winmm.dll del juego no es el mod; no lo toco"; fi
    fi
    reg_edit remove
    rm -f "$CFG_DIR/aimp.cfg"
    echo "(los logs en $CFG_DIR quedan; borrarlos a mano si se quiere)"
    status ;;
  *) echo "uso: $0 status|install [--cfg \"A=1;B=2\"]|revert" >&2; exit 1 ;;
esac
