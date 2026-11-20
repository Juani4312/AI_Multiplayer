# mod/dist

Lo necesario para instalar el mod sin compilar nada (guía: [`INSTALACION.md`](../../INSTALACION.md)).

| Archivo | Qué es |
|---------|--------|
| `winmm.dll` | el mod compilado (no es un archivo del juego). Va al lado de `AI.exe` |
| `xinput1_3.dll` | el mismo mod, variante que entra por `xinput1_3.dll`: solo si Windows no carga la `winmm.dll` (ver `INSTALACION.md`). Se usa una u otra |
| `SHA256SUMS` | hashes de las dos DLL, para verificarlas |
| `aimp-host.cfg` | configuración recomendada para el host; se copia como `aimp.cfg` |
| `aimp-join.cfg` | configuración recomendada para el que se une (cambiar la IP de `AIMP_NET_PEER`); se copia como `aimp.cfg` |

Los scripts de instalación (`work/windows/aimp_check.ps1 -Preset …`, `work/scripts/runtime_setup.sh --preset …`) usan estos dos `.cfg`.

Para desarrolladores: las DLL se regeneran con `cmake --build mod/build`, copiando `mod/build/winmm.dll` y `mod/build/xinput1_3.dll` acá y `sha256sum winmm.dll xinput1_3.dll > SHA256SUMS`. Las dos salen del mismo commit y hablan el mismo protocolo. El commit de origen es el que las actualizó (`git log -- mod/dist/`).
