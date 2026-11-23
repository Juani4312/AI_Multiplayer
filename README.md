# AI_Multiplayer — coop experimental para Alien: Isolation

Mod de investigación e interoperabilidad, de uso personal, sobre una copia comprada del juego.
No contiene ni redistribuye binarios, assets ni código del juego.

**Estado (2026-11-21):** experimental. Dos jugadores, uno en Linux (Proton) y otro en Windows, conectados por Tailscale, juegan la misma partida: se ven, comparten los "usar", el inventario y la munición, el cliente arranca con la partida del host y, si uno cae, el otro lo revive desde una terminal de guardado. El avatar del otro jugador es un "títere": un NPC (una mujer, donde se puede) que cada frame se pone en la posición exacta del otro, con animación de caminar, correr y agacharse. Todavía no es un coop completo: la IA y el Alien corren por separado en cada máquina, las armas de fuego no se ven del otro lado y solo algunas interacciones del mundo se sincronizan. **Dónde quedamos y qué sigue:** sección "Estado actual y siguiente paso", al final.

| Parte | Qué hace | Estado |
|-------|----------|--------|
| Proxy y hook | `winmm.dll` (o `xinput1_3.dll`) proxy, hook en el loop principal, lectura del estado y la posición del jugador | ✅ Proton, Windows 10; la variante `xinput1_3` falta en Windows 11 |
| Red | UDP propio (20 paquetes/s con el estado del jugador, eventos repetidos) | ✅ por Tailscale, 0 pérdidas |
| Avatar | NPC del nivel sin IA, títere de la posición exacta del otro: camina, corre, se agacha (pose), mira hacia donde mira el otro. Es una mujer si hay una en el nivel o si el mod puede hacer aparecer una | ✅ dos máquinas; avatar mujer en una (corridas 158–159) |
| "Usar" compartido | botones, cajas e ítems: el uso del otro lo repite el avatar | ✅ botones del transit con dos máquinas |
| Pines compartidos | lo que el avatar no puede usar se replica reinyectando un pin de los scripts, en los dos sentidos: el robot barredor (el ícono del "usar"), **las puertas** (el desbloqueo del mecanismo, cualquiera sea la forma de abrirla; del otro lado queda desbloqueada y se abre al acercarse) y **las cajas con botín** (el primero que la abre la sortea; para el otro queda saqueada) | ✅ en una máquina con el peer (corridas 152–156, 163–169) |
| Inventario compartido | lo que uno agarra lo reciben los dos; lo que uno gasta se descuenta a los dos | ✅ dos máquinas (corrida 119) |
| Copia del save | el cliente arranca con la partida del host; guardar juntos; game over con el checkpoint del host | ✅ dos máquinas (corrida 119) |
| Muerte en coop | el que cae queda caído (invencible, el Alien lo ignora); el compañero lo revive con una terminal de guardado; si caen los dos, game over | ✅ en una máquina con el peer |
| Dificultad y menú Coop | menú "Coop" del juego (en Extras): dificultad del coop y "Opciones avanzadas" (Alien, caer y revivir, guardado exigente, inventario y cajas compartidos), que decide el host | ✅ en una máquina |
| Avisos | avisos sin pausa (cartel de consejos del HUD) y con el diálogo "Warning" del juego | ✅ en una máquina (corridas 149–151) |
| Arrojables | bengala, noisemaker, molotov, minas: aparecen en la otra máquina con la misma trayectoria y quedan donde cayó el original | 🟡 vuelo ✅ con dos máquinas; dónde cae, verificado en una |
| Disparadores de los scripts | zonas y eventos de la misión los decide el host | ✅ objetivos con dos máquinas (corrida 119); algunos pines de composites compartidos no se encuentran |

El detalle de cada paso está en `work/PROGRESS.md`. La evidencia de ingeniería inversa está en `work/findings/` (hallazgos 01–21).

## Para Claude Code

- **Para instalar el mod y jugar** (por ejemplo, en la PC de un amigo): seguí [`INSTALAR_CON_CLAUDE.md`](INSTALAR_CON_CLAUDE.md).
- **En `papulaptop`** (la laptop Windows de pruebas): tu tarea está en [`work/windows/instructions.md`](work/windows/instructions.md).

## Instalación

Todo el detalle está en **[`INSTALACION.md`](INSTALACION.md)** (empieza con una instalación rápida de 5 pasos); para que lo instale Claude Code, [`INSTALAR_CON_CLAUDE.md`](INSTALAR_CON_CLAUDE.md). La guía cubre: requisitos, instalar en Windows y en Linux (Proton), el archivo `aimp.cfg`, la red con Tailscale, las configuraciones recomendadas para host y para el que se une, cómo arrancar una partida de a dos, la referencia de todos los parámetros y problemas comunes.

Resumen: `winmm.dll` (de `mod/dist/`) al lado de `AI.exe` en las dos máquinas (si Windows no la carga, la variante `xinput1_3.dll`; en Proton, `work/scripts/runtime_setup.sh install --preset host|join`), un `aimp.cfg` con `AIMP_NET=host` en uno y `AIMP_NET=join` + `AIMP_NET_PEER=<IP del host>:47811` en el otro (`mod/dist/aimp-host.cfg` y `aimp-join.cfg`), y las dos máquinas en la misma red de Tailscale. Log: `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`.

## Build soportada

| Campo | Valor |
|-------|-------|
| Ejecutable | `AI.exe` (Windows, PE32 i386); en Linux corre con Proton |
| SHA-256 | `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Steam buildid | 4940658 |
| PE timestamp | 0x54E3199D (2015-02-17) |

Los dos jugadores **tienen que tener exactamente este hash**, los mismos datos del juego y **la misma versión del mod**: el protocolo de red está en la v2 desde el 2026-11-15 y no habla con DLLs anteriores. Con otra build del juego, el mod queda como proxy y no hace nada más.

## Estructura

- `INSTALACION.md`: instalación, configuración y referencia de parámetros (para jugar). `INSTALAR_CON_CLAUDE.md`: lo mismo, como procedimiento para Claude Code.
- `mod/`: el mod. C sin CRT, CMake, cross-compilado con mingw-w64 (i686). `mod/README.md` tiene la referencia técnica: compilación, tests, variables y log.
- `mod/dist/`: las DLL compiladas (`winmm.dll` y la variante `xinput1_3.dll`) con su `SHA256SUMS` y las configuraciones recomendadas, para usar sin toolchain.
- `work/PROGRESS.md`: estado y siguiente paso. `work/NOTES.md` y `work/LOG.md` son la bitácora; `work/HISTORIAL.md`, el resumen de cada sesión.
- `work/CONTINGENCIAS.md`: limitaciones conocidas y lo que se probó sin éxito, con lo que haría falta para resolverlas.
- `work/DESIGN_*.md`: diseño de cada parte (sesión compartida, muerte, dificultad, Alien, menú de opciones, fase 6).
- `work/findings/`: un `.md` por hallazgo de ingeniería inversa, con direcciones, evidencia y verificación en runtime.
- `work/windows/`: instrucciones y script de PowerShell para instalar y probar en Windows (`instructions.md`, `aimp_check.ps1`) y los reportes de la laptop de pruebas.
- `work/scripts/`: `runtime_setup.sh` (instalar y revertir el mod en Proton), `aimp_peer.py` (peer de prueba del protocolo: hace de otro jugador), `memsrv.py` (lector de memoria y comandos de prueba del mod), análisis de logs y trazas, `use_names.py` y `female_spawns.py` (nombres y disparadores sacados de los scripts del juego) y scripts de Ghidra (`work/scripts/ghidra/`).
- `work/tools/`: `cathode-dump` (volcados de los formatos del juego con CathodeLib) y la descarga de JPEXS FFDec. Ver "Herramientas y librerías de terceros".

## Herramientas y librerías de terceros

El mod (`mod/`, la DLL) no incluye código de terceros: es C propio, sin CRT. Lo de abajo se usa para desarrollar o para leer los datos del juego, y no se redistribuye nada del juego.

| Qué | Para qué | Cómo se obtiene | Licencia |
|-----|----------|-----------------|----------|
| [CathodeLib](https://github.com/OpenCAGE/CathodeLib) (OpenCAGE, Matt Filer) | leer los formatos de CATHODE: scripts de los niveles (`COMMANDS.PAK`), personajes, UI, BML. La usa `work/tools/cathode-dump` | submódulo `work/tools/third_party/CathodeLib` (`git submodule update --init --recursive`) | MIT; sus dependencias (AlienBML, DotRecast, NanoRT…) en su `THIRD-PARTY-NOTICES.md` |
| [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler) 26.5.0 | leer el ActionScript de la UI del juego (`.GFX`) | `work/tools/get-ffdec.sh` lo baja y verifica el SHA-256 | GPL-3 |
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) y REA | ingeniería inversa de `AI.exe` (hallazgos en `work/findings/`) | se instalan aparte | Apache 2.0 (Ghidra) |
| mingw-w64 (GCC), CMake, Wine | compilar la DLL y correr los tests | paquetes del sistema | GPL / BSD / LGPL |
| .NET SDK | compilar `cathode-dump` | paquete del sistema | MIT |
| Tailscale | red entre los dos jugadores | se instala aparte (opcional) | — |

Las herramientas sirven para orientarse y ponerle nombre a lo que ve el mod; los offsets salen siempre de Ghidra/REA o del runtime. Detalle y verificaciones: `work/tools/README.md`. Los mods de terceros del juego (UI, IA, audio) no son parte del proyecto: para probar el coop se apagan los que tocan la UI o la IA.

## Limitaciones conocidas

- No hay modelo de Amanda en tercera persona: el avatar es un NPC del nivel (una obrera donde se puede). No camina agachado (se desliza en la pose) y a veces los pies no siguen el ritmo del desplazamiento.
- Arrojables: la réplica vuela igual; cuando el original se asienta, la réplica se borra y aparece quieta en ese punto (un "salto" corto).
- La IA de los NPCs y el Alien corren por separado en cada máquina: el Alien persigue a cada jugador en su propia máquina.
- Las armas de fuego no se ven del otro lado (el disparo ni el lanzallamas), ni lo que se elige en una terminal. De las interacciones del mundo, se sincronizan los botones del transit, los disparadores de zona, el robot barredor y el desbloqueo de las puertas.
- Los ítems sueltos quedan en el mundo de cada máquina: con el inventario compartido, lo que uno levanta lo reciben los dos, pero el otro todavía ve el ítem. Las cajas con botín, en cambio, las sortea el primero que las abre y para el otro quedan cerradas (la tapa no se ve abierta).
- Los lockers y la terminal de guardado no se replican a propósito: son acciones de cada jugador.
- Crear personajes nuevos (en vez de reusar un NPC) hace caer el juego. Ver el hallazgo 12.

Detalle y lo que haría falta para resolverlas: `work/CONTINGENCIAS.md`.

## Lo que se sabe del engine (resumen)

- El engine trae restos de un modo red: colas de RPC que en retail se ejecutan en local (hallazgos 10 y 11), replicadores de personajes con 11 tipos de mensaje cuya capa de canales quedó vacía (hallazgo 18) y un comando de control `CLIENT_PLAYERS_LOGIC_CHARACTER_RUNNING_ON_THE_HOST` sin constructor (hallazgo 19).
- Los scripts de los niveles (CATHODE) se pueden observar y reinyectar pin por pin, con una identidad estable entre máquinas, el pid (hallazgos 15 y 16). El pid se reproduce desde los archivos con CathodeLib en el 83–96 % de los casos.
- Los ids de 8 bytes que usa el juego para objetos y personajes (por ejemplo, en el RPC del "usar") son el `EntityHandle` de CathodeLib: el id de la entidad y el de la instancia de su composite. Con eso, cada objeto y cada personaje de un log tiene nombre.
- Los objetos arrojados son composites de los scripts que el mod puede instanciar con la misma posición y velocidad (hallazgo 17). Las armas de fuego también son composites.
- A los NPCs los maneja un sistema de comandos (`GOTO`, `MOVE_TOWARDS`, `FOLLOW`…) con dos controladores por personaje: uno de tareas (su IA) y uno de scripts, que su árbol de comportamiento solo ejecuta en la rama cinemática. Al jugador lo mueve un comando que lee el mando (hallazgo 19).

## Tests

`mod/test/run_tests.sh` corre con Wine, sin el juego: `test_core`, `test_probe`, `test_net`, `test_savesync`, `test_ghost`, `test_walk`, `test_sync`, `test_throw`, `test_ctrl`, `test_pins`, `test_autopad`, `test_host` y `test_host_xinput`. Los de las partes que tocan el juego usan stubs de código máquina con los prólogos y el `ret N` reales, para validar las convenciones de llamada.

## Pruebas en una máquina

Las corridas se pueden hacer sin nadie al teclado y sin otra máquina (Linux, Proton):

1. Instalar con la config de la prueba: `work/scripts/runtime_setup.sh install --cfg "...;AIMP_AUTOPAD_FILE=C:\users\steamuser\AppData\Local\AI_Multiplayer\pad.txt"` y abrir el juego (`steam -applaunch 214490`).
2. **Mando virtual** (`mod/src/autopad.c`): lee un guion de `pad.txt`, una línea por acción, por ejemplo `P8000-10000:LY=32000` (adelante 2 s), `P12000-13260:RX=24000` (girar unos 180°) o `0-150:A` (apretar A ya). Los tiempos con `P` son relativos a la aparición del jugador. Sirve también para recorrer el menú paso a paso, con una captura en cada uno.
3. **Otro jugador:** el juego con `AIMP_NET=host` y `python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811 --replay 3 --offset 0,0,2`: el peer repite el recorrido del jugador 3 s después, corrido 2 m. Con `--event T:tipo:hex` manda eventos (caído, pin, "usar"…) y con `--record-events` graba los que manda el juego. Al revés (`--host`), el juego hace de cliente.
4. **Comandos de prueba** (`AIMP_MEMSRV_FILE` + `AIMP_MEMSRV_TEST=1`, con `work/scripts/memsrv.py`): teletransportar al jugador (`TP`), avisos (`TIP`, `MSG`), matar (`KILL`, para el modo caído), guardar (`SAVE`). Con `AIMP_PINS_FILE`, `FIRE <pid> <pin>` reinyecta un pin de los scripts.
5. Al terminar: cerrar el juego (`pkill -x AI.exe`) y guardar el log.

## Estado actual y siguiente paso

- **Último (2026-11-21):** avisos sin pausa verificados; el robot barredor, **las puertas** y **las cajas con botín** se sincronizan por pines de los scripts en los dos sentidos (verificado en una máquina con el peer); el avatar es una mujer donde se puede. Herramientas nuevas: CathodeLib con `cathode-dump`, que le pone nombre a los pids, objetos y personajes de los logs. Diseño del menú Coop con "Opciones avanzadas" (se puede armar desde el mod con Scaleform) y de caer y revivir (Normal: desangrado y revivir en persona; Difícil: solo terminal).
- **2026-11-20:** en un Windows 11 25H2 el juego no carga la `winmm.dll` del mod. Variante que entra por `xinput1_3.dll` (`mod/dist/xinput1_3.dll`, `-Proxy xinput1_3` en el instalador), verificada en el juego con Proton; falta probarla en ese Windows.
- **2026-11-23:** el menú "Coop" tiene el selector de dificultad y el submenú **"Opciones avanzadas"** (escala del Alien, caer y revivir, guardado exigente, inventario y cajas compartidos), armado por el mod con los componentes del propio juego. Las opciones las decide el host; el que se une las ve bloqueadas. Verificado en una máquina con el peer.
- **Siguiente:** que el amigo pruebe la variante (`INSTALAR_CON_CLAUDE.md`, sección 2) y la prueba de a dos (`work/windows/instructions.md` 8g, DLL `1e065146…`): menú Coop, robot, puertas, cajas, avatar mujer, avisos, modo caído.
- Detalle y pendientes: `work/PROGRESS.md`. Historial de sesiones: `work/HISTORIAL.md`.
