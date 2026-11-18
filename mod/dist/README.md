# mod/dist

Lo necesario para instalar el mod sin compilar nada (guía: [`INSTALACION.md`](../../INSTALACION.md)).

| Archivo | Qué es |
|---------|--------|
| `winmm.dll` | el mod compilado (no es un archivo del juego). Va al lado de `AI.exe` |
| `SHA256SUMS` | hash de `winmm.dll`, para verificarla |
| `aimp-host.cfg` | configuración recomendada para el host; se copia como `aimp.cfg` |
| `aimp-join.cfg` | configuración recomendada para el que se une (cambiar la IP de `AIMP_NET_PEER`); se copia como `aimp.cfg` |

Los scripts de instalación (`work/windows/aimp_check.ps1 -Preset …`, `work/scripts/runtime_setup.sh --preset …`) usan estos dos `.cfg`.

Para desarrolladores: la DLL se regenera con `cmake --build mod/build`, copiando `mod/build/winmm.dll` acá y `sha256sum winmm.dll > SHA256SUMS`. El commit de origen es el que la actualizó (`git log -- mod/dist/winmm.dll`).
