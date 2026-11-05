#!/usr/bin/env bash
# Corre los tests del mod con wine, en un prefix descartable (no toca ~/.wine ni el
# prefix de Proton del juego).
# Uso: mod/test/run_tests.sh [dir_de_build]   (por defecto mod/build)
# Variables: AIMP_TEST_WINEPREFIX (prefix a usar), AIMP_TEST_TMP (dir para logs).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$(cd "${1:-$HERE/../build}" && pwd)"
TMP="${AIMP_TEST_TMP:-${TMPDIR:-/tmp}/aimp-tests}"
mkdir -p "$TMP"
export WINEPREFIX="${AIMP_TEST_WINEPREFIX:-$TMP/wineprefix}"
export WINEDEBUG=-all
# Sin ventanas y sin instaladores de Mono/Gecko.
unset DISPLAY WAYLAND_DISPLAY
run() { # nombre exe [overrides]
  local name="$1" exe="$2" ovr="${3:-}"
  echo "== $name"
  ( cd "$BUILD/test" && WINEDLLOVERRIDES="mscoree=;mshtml=${ovr:+;$ovr}" AIMP_LOG="Z:$TMP/$name.log" \
      timeout 180 wine "$exe" ) 2>&1 | grep -v '^wine: created the configuration directory'
  local rc=${PIPESTATUS[0]}
  echo "== $name: rc=$rc"
  return "$rc"
}
fail=0
cp "$BUILD/winmm.dll" "$BUILD/test/winmm.dll"
run test_core test_core.exe || fail=1
run test_probe test_probe.exe || fail=1
run test_net test_net.exe || fail=1
run test_ghost test_ghost.exe || fail=1
run test_host test_host.exe "winmm=n,b" || fail=1
echo "-- log del proxy en test_host ($TMP/test_host.log):"
cat "$TMP/test_host.log"
if [ "$fail" -eq 0 ]; then echo "RESULTADO: todos los tests OK"; else echo "RESULTADO: HAY FALLAS"; fi
exit "$fail"
