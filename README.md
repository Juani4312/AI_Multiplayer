# AI_Multiplayer — coop experimental para Alien: Isolation

Mod de investigación e interoperabilidad, de uso personal, sobre una copia comprada del juego.
No contiene ni redistribuye binarios, assets ni código del juego.

**Estado (2026-11-17):** experimental. Dos jugadores, uno en Linux (Proton) y otro en Windows 10, conectados por Tailscale, ya se ven y comparten los "usar". El avatar del otro jugador es un "títere": un NPC que cada frame se pone en la posición exacta del otro, con animación de caminar o correr (probado con dos máquinas: "responsivo y exacto"). Los objetos arrojados se ven volar del otro lado y terminan donde cayó el original. Todavía no es un coop jugable: no se sincronizan la IA, el Alien, las armas de fuego ni el progreso de la misión. **Dónde quedamos y qué sigue:** sección "Estado actual y siguiente paso", al final; el historial de sesiones está en `work/HISTORIAL.md`.

| Fase | Qué hace | Estado |
|------|----------|--------|
| 1 y 2 | `winmm.dll` proxy, hook en el loop principal, lectura de estado y posición del jugador | ✅ Proton y Windows 10 nativo |
| 3 | Red UDP propia (20 paquetes/s con el estado del jugador) | ✅ por Tailscale, 0 pérdidas |
| 4 | Avatar del otro jugador: un NPC civil del nivel, sin IA, teletransportado a su posición | ✅ en los dos sentidos |
| 4b | Avatar títere: posición exacta del otro cada frame, con animación de caminar, caminar rápido y correr | ✅ dos máquinas (corrida 52); agachado: el civil no tiene la animación (corridas 65–67) |
| 5 | "Usar" compartido: botones, cajas e ítems se replican y en la otra máquina los hace el avatar | ✅ botones; los ítems quedan uno por jugador |
| 5b | Objetos arrojados y colocados (bengala, noisemaker, molotov, minas…): aparecen en la otra máquina con la misma trayectoria y quedan donde cayó el original | 🟡 vuelo ✅ con dos máquinas; corrección de dónde cae verificada en una (corridas 55–64), falta con dos |
| 6 | Disparadores de los scripts decididos por el host (zonas que arrancan diálogos, eventos…) | 🟡 por red en una máquina (corridas 72–73: el cliente aplica los del host y bloquea los suyos); falta con dos máquinas |

El detalle de cada paso está en `work/PROGRESS.md`. La evidencia de ingeniería inversa está en `work/findings/` (hallazgos 01–19).

## Build soportada

| Campo | Valor |
|-------|-------|
| Ejecutable | `AI.exe` (Windows, PE32 i386); en Linux corre con Proton |
| SHA-256 | `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Steam buildid | 4940658 |
| PE timestamp | 0x54E3199D (2015-02-17) |

Los dos jugadores **tienen que tener exactamente este hash**, los mismos datos del juego y **la misma versión del mod**: el protocolo de red está en la v2 desde el 2026-11-15 (eventos más grandes, para los arrojables) y no habla con DLLs anteriores. Con otra build del juego, el mod queda como proxy de `winmm` y no hace nada más.

## Estructura

- `mod/`: el mod. C sin CRT, CMake, cross-compilado con mingw-w64 (i686). `mod/README.md` tiene la referencia completa: compilación, tests, variables y log.
- `mod/dist/winmm.dll`: la DLL compilada, para usar sin toolchain (por ejemplo, en Windows), con su `SHA256SUMS`.
- `saves/`: partidas guardadas para probar en zonas concretas (`safezone`: sala del transit de HAB_ShoppingCentre, sin enemigos). Se cargan con `work/scripts/saves.sh load <nombre>`.
- `work/CONTINGENCIAS.md`: limitaciones conocidas y lo que se probó sin éxito (Amanda sin modelo en tercera persona, el civil sin agachado, dónde cae el arrojable…), con lo que haría falta para resolverlas.
- `work/PROGRESS.md`: estado y siguiente paso. `work/NOTES.md` y `work/LOG.md` son la bitácora.
- `work/findings/`: un `.md` por hallazgo de RE, con direcciones, evidencia y verificación en runtime.
- `work/windows/`: instrucciones y script de PowerShell para probar en Windows (`instructions.md`, `aimp_check.ps1`). Ahí también van los reportes de la laptop de pruebas.
- `work/scripts/`: `runtime_setup.sh` (instalar y revertir el mod en Proton), `autoplay_continue.sh` (llega a "Continue Game" con el mando virtual), `aimp_peer.py` (peer de prueba del protocolo v2; con `--replay` hace de otro jugador), análisis de logs y scripts de Ghidra (`work/scripts/ghidra/`).
- `work/evidence/runtime/`: los logs de cada corrida (`runNN-*.log`) y capturas en `shots/` (locales, ignorados por git).
- `game/` y `work/evidence/`: locales, ignorados por git.

## Cómo jugar (experimental)

1. **Instalar el mod** en cada máquina: `winmm.dll` al lado de `AI.exe`, más un `aimp.cfg` (en la carpeta del juego o en `%LOCALAPPDATA%\AI_Multiplayer\`).
   - **Linux (Proton):** `work/scripts/runtime_setup.sh install --cfg "CLAVE=valor;..."` copia la DLL, configura el override `winmm=n,b` y hace backup. `revert` lo deshace. Con `--cfg "AIMP_GHOST=0"` el mod queda instalado pero pasivo: el juego anda normal.
   - **Windows:** `work\windows\aimp_check.ps1 -Action install …`, o a mano con `mod\dist\winmm.dll`. Ver `work/windows/instructions.md`.
2. **Red:** las dos máquinas en la misma red de [Tailscale](https://tailscale.com). Uno hace de host y el otro se une a su IP de Tailscale.
3. **`aimp.cfg`** (una clave por línea):

   | Quién | Claves |
   |-------|--------|
   | Host | `AIMP_NET=host`, `AIMP_GHOST=3`, `AIMP_SYNC_USE=1`, `AIMP_SYNC_THROW=1` |
   | El que se une | `AIMP_NET=join`, `AIMP_NET_PEER=<IP de Tailscale del host>:47811`, `AIMP_GHOST=3`, `AIMP_SYNC_USE=1`, `AIMP_SYNC_THROW=1` |

   Recomendado: `AIMP_GHOST_WALK=1` en las dos (el avatar títere, con animación). Para jugar la partida del host: `AIMP_SAVE_SYNC=1` en el que se une, con el juego en el menú principal (sin probar en el juego; ver "Último trabajo"). Opcional y sin probar con dos máquinas: `AIMP_SYNC_PINS=1` en las dos (los disparadores de zona los decide el host).

4. Los dos cargan la misma partida, en el mismo nivel. Unos 8 s después, cada uno ve a un civil del nivel parado donde está el otro jugador. Lo que uno aprieta (por ejemplo, el botón para llamar al transit) también pasa en la otra máquina, y lo que tira (una bengala, un noisemaker) aparece en la otra con la misma curva.

**Con una sola cuenta de Steam en dos PCs,** una de las dos tiene que estar en modo sin conexión.

Todo queda en `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` (en Proton, dentro del prefix). Si el juego se cae, el log registra la excepción, la dirección y el stack.

## Limitaciones conocidas

- El avatar es un NPC civil (no hay modelo de Amanda en tercera persona). Con `AIMP_GHOST_WALK=1` copia la posición exacta del otro cada frame y anima caminar o correr, pero **no se agacha** (el comando `IDLE` con postura no tiene efecto en el civil) y, al caminar para atrás, mira hacia donde va. En la laptop el usuario lo vio "deslizarse" en algún momento: falta revisar la animación.
- Arrojables: la réplica vuela igual; cuando el original se asienta, la réplica se borra y aparece quieta en ese punto (un "salto" corto). Del lado del que tira, la posición del objeto se encuentra en ~3 de 4 lanzamientos; si no, no se corrige.
- No se replican la IA ni el Alien, las armas de fuego (el disparo y el lanzallamas no se ven del otro lado), las puertas automáticas, lo que se elige en una terminal ni el progreso de la misión. Cada máquina corre su propia simulación.
- Los ítems que se levantan quedan uno por jugador: los dos pueden agarrar el mismo.
- Los lockers y la terminal de guardado no se replican a propósito: son acciones de cada jugador.
- Crear personajes nuevos (en vez de reusar un NPC) hace caer el juego. Ver el hallazgo 12.

## Lo que se sabe del engine (resumen)

- El engine trae restos de un modo red: colas de RPC que en retail se ejecutan en local (hallazgos 10 y 11), replicadores de personajes con 11 tipos de mensaje cuya capa de canales quedó vacía (hallazgo 18) y un comando de control `CLIENT_PLAYERS_LOGIC_CHARACTER_RUNNING_ON_THE_HOST` sin constructor (hallazgo 19).
- Los scripts de los niveles (CATHODE) se pueden observar y reinyectar pin por pin, con una identidad estable entre máquinas (hallazgos 15 y 16).
- Los objetos arrojados son composites de los scripts que el mod puede instanciar con la misma posición y velocidad (hallazgo 17). Las armas de fuego también son composites.
- A los NPCs los maneja un sistema de comandos (`GOTO`, `MOVE_TOWARDS`, `FOLLOW`…) con dos controladores por personaje: uno de tareas (su IA) y uno de scripts, que su árbol de comportamiento solo ejecuta en la rama cinemática. Al jugador lo mueve un comando que lee el mando (hallazgo 19).

## Tests

`mod/test/run_tests.sh` corre con wine, sin el juego: `test_core`, `test_probe`, `test_net`, `test_ghost`, `test_walk`, `test_sync`, `test_throw`, `test_ctrl`, `test_pins`, `test_autopad` y `test_host`. Los de las fases 4 a 6 usan stubs de código máquina con los prólogos y el `ret N` reales del juego, para validar las convenciones de llamada.

## Pruebas automáticas en una máquina

Desde el 2026-11-15 las corridas locales se hacen sin nadie al teclado (Linux, Proton, Hyprland):

1. `work/scripts/runtime_setup.sh install --cfg "...;AIMP_AUTOPAD_FILE=C:\users\steamuser\AppData\Local\AI_Multiplayer\pad.txt"` y `setsid -f steam -applaunch 214490`.
2. `work/scripts/autoplay_continue.sh menu`: espera el menú, lo navega con el mando virtual y saca una captura **solo de la ventana del juego** (con la geometría de `hyprctl`; nunca un monitor entero). Hay que ver "Continue Game" resaltado antes de `autoplay_continue.sh go`.
3. El mando virtual (`mod/src/autopad.c`) lee un guion de `pad.txt`: una línea por acción, por ejemplo `P8000-10000:LY=32000` (adelante 2 s) o `P12000-13260:RX=24000` (girar unos 180°; ~143°/s con 24000). Los tiempos `P` son relativos a la aparición del jugador. `MARK` deja una marca en el log.
4. **Otro jugador sin otra máquina:** el juego con `AIMP_NET=host` y `python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811 --replay 3 --offset 0,0,2`. El peer repite el recorrido del jugador 3 s después, corrido 2 m, así que el "otro" siempre pisa lugares transitables.
5. Al terminar: cerrar el juego (`pkill -TERM -x AI.exe`), guardar el log en `work/evidence/runtime/` y **dejar el mod instalado en pasivo** (`runtime_setup.sh install --cfg "AIMP_GHOST=0"`).

## Estado actual y siguiente paso

- **Último (2026-11-18):** menú "Coop" (antes Extras) con la dificultad del coop, que la elige el host y endurece al Alien sobre la dificultad del juego; con el host caído, el cliente guarda en una terminal y su save pasa al host. Verificado en una máquina con el peer.
- **Siguiente:** prueba de a dos (`work/windows/instructions.md` 8g, DLL `3ea5509b…`).
- Detalle y pendientes: `work/PROGRESS.md`. Historial de sesiones: `work/HISTORIAL.md`.
