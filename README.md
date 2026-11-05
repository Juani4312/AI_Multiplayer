# AI_Multiplayer — coop experimental para Alien: Isolation

Mod de investigación e interoperabilidad, de uso personal, sobre una copia comprada del juego.
No contiene ni redistribuye binarios, assets ni código del juego.

**Estado (2026-11-05):** experimental. Dos jugadores, uno en Linux (Proton) y otro en Windows 10, ya se ven y comparten algunas interacciones, conectados por Tailscale. Todavía no es un coop jugable: no se sincronizan la IA, el Alien, las armas ni el progreso.

| Fase | Qué hace | Estado |
|------|----------|--------|
| 1 y 2 | `winmm.dll` proxy, hook en el loop principal, lectura de estado y posición del jugador | ✅ Proton y Windows 10 nativo |
| 3 | Red UDP propia (20 paquetes/s con el estado del jugador) | ✅ por Tailscale, 0 pérdidas |
| 4 | Avatar del otro jugador: un NPC civil del nivel sigue su posición | ✅ en los dos sentidos |
| 5 | Mundo compartido: los "usar" (botones, cajas, ítems) se replican y en la otra máquina los hace el avatar | 🟡 botones verificados; armas, ítems e IA pendientes |

El detalle de cada paso está en `work/PROGRESS.md`. La evidencia de ingeniería inversa está en `work/findings/` (hallazgos 01–13).

## Build soportada

| Campo | Valor |
|-------|-------|
| Ejecutable | `AI.exe` (Windows, PE32 i386); en Linux corre con Proton |
| SHA-256 | `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Steam buildid | 4940658 |
| PE timestamp | 0x54E3199D (2015-02-17) |

Los dos jugadores **tienen que tener exactamente este hash** y los mismos datos del juego. Con otra build, el mod queda como proxy de `winmm` y no hace nada más.

## Estructura

- `mod/`: el mod. C sin CRT, CMake, cross-compilado con mingw-w64 (i686). `mod/README.md` tiene la referencia completa: compilación, tests, variables y log.
- `mod/dist/winmm.dll`: la DLL compilada, para usar sin toolchain (por ejemplo, en Windows), con su `SHA256SUMS`.
- `work/PROGRESS.md`: estado y siguiente paso. `work/NOTES.md` y `work/LOG.md` son la bitácora.
- `work/findings/`: un `.md` por hallazgo de RE, con direcciones, evidencia y verificación en runtime.
- `work/windows/`: instrucciones y script de PowerShell para probar en Windows (`instructions.md`, `aimp_check.ps1`). Ahí también van los reportes de la laptop de pruebas.
- `work/scripts/`: `runtime_setup.sh` (instalar y revertir el mod en Proton), `aimp_peer.py` (peer de prueba del protocolo), análisis de logs.
- `game/` y `work/evidence/`: locales, ignorados por git.

## Cómo jugar (experimental)

1. **Instalar el mod** en cada máquina: `winmm.dll` al lado de `AI.exe`, más un `aimp.cfg` (en la carpeta del juego o en `%LOCALAPPDATA%\AI_Multiplayer\`).
   - **Linux (Proton):** `work/scripts/runtime_setup.sh install --cfg "CLAVE=valor;..."` copia la DLL, configura el override `winmm=n,b` y hace backup. `revert` lo deshace.
   - **Windows:** `work\windows\aimp_check.ps1 -Action install …`, o a mano con `mod\dist\winmm.dll`. Ver `work/windows/instructions.md`.
2. **Red:** las dos máquinas en la misma red de [Tailscale](https://tailscale.com). Uno hace de host y el otro se une a su IP de Tailscale.
3. **`aimp.cfg`** (una clave por línea):

   | Quién | Claves |
   |-------|--------|
   | Host | `AIMP_NET=host`, `AIMP_GHOST=3`, `AIMP_SYNC_USE=1` |
   | El que se une | `AIMP_NET=join`, `AIMP_NET_PEER=<IP de Tailscale del host>:47811`, `AIMP_GHOST=3`, `AIMP_SYNC_USE=1` |

4. Los dos cargan la misma partida, en el mismo nivel. Unos 8 s después, cada uno ve a un civil del nivel parado donde está el otro jugador. Lo que uno aprieta (por ejemplo, el botón para llamar al transit) también pasa en la otra máquina.

**Con una sola cuenta de Steam en dos PCs,** una de las dos tiene que estar en modo sin conexión.

Todo queda en `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` (en Proton, dentro del prefix). Si el juego se cae, el log registra la excepción, la dirección y el stack.

## Limitaciones conocidas

- El avatar es un NPC civil: conserva su IA (puede querer huir) y no anima la caminata del otro jugador.
- No se replican la IA ni el Alien, las armas (el lanzallamas no se ve del otro lado), los ítems, las puertas automáticas, lo que se elige en una terminal ni el progreso de la misión. Cada máquina corre su propia simulación.
- Los lockers y la terminal de guardado no se replican a propósito: son acciones de cada jugador.
- Crear personajes nuevos (en vez de reusar un NPC) hace caer el juego. Ver el hallazgo 12.

## Tests

`mod/test/run_tests.sh` corre con wine, sin el juego: `test_core`, `test_probe`, `test_net`, `test_ghost`, `test_sync` y `test_host`. Los de la fase 4 y 5 usan stubs de código máquina con los prólogos y el `ret N` reales del juego, para validar las convenciones de llamada.
