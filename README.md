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

- `INSTALACION.md`: instalación, configuración y referencia de parámetros (para jugar).
- `mod/`: el mod. C sin CRT, CMake, cross-compilado con mingw-w64 (i686). `mod/README.md` tiene la referencia técnica: compilación, tests, variables y log.
- `mod/dist/winmm.dll`: la DLL compilada, para usar sin toolchain (por ejemplo, en Windows), con su `SHA256SUMS`.
- `saves/`: partidas guardadas para probar en zonas concretas (`safezone`: sala del transit de HAB_ShoppingCentre, sin enemigos). Se cargan con `work/scripts/saves.sh load <nombre>`.
- `work/CONTINGENCIAS.md`: limitaciones conocidas y lo que se probó sin éxito (Amanda sin modelo en tercera persona, el civil sin agachado, dónde cae el arrojable…), con lo que haría falta para resolverlas.
- `work/PROGRESS.md`: estado y siguiente paso. `work/NOTES.md` y `work/LOG.md` son la bitácora.
- `work/findings/`: un `.md` por hallazgo de RE, con direcciones, evidencia y verificación en runtime.
- `work/windows/`: instrucciones y script de PowerShell para probar en Windows (`instructions.md`, `aimp_check.ps1`). Ahí también van los reportes de la laptop de pruebas.
- `work/tools/`: herramientas de terceros que usamos para leer formatos del juego (solo lectura, nunca como fuente de offsets): **CathodeLib** de OpenCAGE (submódulo, MIT) con nuestro `cathode-dump` (scripts de cada nivel con el pid del mod, UI, BML) y **JPEXS FFDec** (ActionScript de la UI). Detalle en `work/tools/README.md`.
- `work/scripts/check_install.sh`: chequeo de la instalación antes de trabajar (modo, mod coop, mods de terceros, partida cargada, DLLs, niveles completos). Solo lee.
- `work/scripts/`: `runtime_setup.sh` (instalar y revertir el mod en Proton), `autoplay_continue.sh` (llega a "Continue Game" con el mando virtual), `aimp_peer.py` (peer de prueba del protocolo v2; con `--replay` hace de otro jugador), análisis de logs y scripts de Ghidra (`work/scripts/ghidra/`).
- `work/evidence/runtime/`: los logs de cada corrida (`runNN-*.log`) y capturas en `shots/` (locales, ignorados por git).
- `game/` y `work/evidence/`: locales, ignorados por git.

## Instalación

Todo el detalle está en **[`INSTALACION.md`](INSTALACION.md)** (empieza con una instalación rápida de 5 pasos); para que lo instale Claude Code, [`INSTALAR_CON_CLAUDE.md`](INSTALAR_CON_CLAUDE.md). La guía cubre: requisitos, instalar en Windows y en Linux (Proton), el archivo `aimp.cfg`, la red con Tailscale, las configuraciones recomendadas para host y para el que se une, cómo arrancar una partida de a dos, la referencia de todos los parámetros y problemas comunes.

Resumen: `winmm.dll` (de `mod/dist/`) al lado de `AI.exe` en las dos máquinas (en Proton, además, `WINEDLLOVERRIDES="winmm=n,b" %command%`), un `aimp.cfg` con `AIMP_NET=host` en uno y `AIMP_NET=join` + `AIMP_NET_PEER=<IP del host>:47811` en el otro, y las dos máquinas en la misma red de Tailscale. Log: `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`.

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

- **Último (2026-11-18, tarde):** con el modo caído, un remate de un enemigo (el agarre del Alien) ya no deja la pantalla negra: después de la animación, el caído recupera la imagen y el control, y el Alien lo ignora mientras está caído (se va a los conductos; al revivir lo vuelve a cazar). Avisos con el diálogo "Warning" del juego (la terminal bloqueada por el guardado exigente dice por qué). Verificado en una máquina con el save `alien`.
- **2026-11-20:** en un Windows 11 25H2 el juego no carga la `winmm.dll` del mod. Nueva variante que entra por `xinput1_3.dll` (`mod/dist/xinput1_3.dll`, `-Proxy xinput1_3` en el instalador), verificada en el juego con Proton; falta probarla en ese Windows.
- **Siguiente:** que el amigo pruebe la variante (`INSTALAR_CON_CLAUDE.md`, sección 2) y la prueba de a dos (`work/windows/instructions.md` 8g, DLL `da80783b…`).
- Detalle y pendientes: `work/PROGRESS.md`. Historial de sesiones: `work/HISTORIAL.md`.
