# AI_Multiplayer — coop experimental para Alien: Isolation

Mod de investigación e interoperabilidad, de uso personal, sobre una copia comprada del juego.
No contiene ni redistribuye binarios, assets ni código del juego.

**Estado (2026-11-15):** experimental. Dos jugadores, uno en Linux (Proton) y otro en Windows 10, conectados por Tailscale, ya se ven y comparten los "usar". El avatar del otro jugador es un "títere": un NPC que cada frame se pone en la posición exacta del otro, con animación de caminar o correr (probado con dos máquinas: "responsivo y exacto"). Los objetos arrojados se ven volar del otro lado y terminan donde cayó el original. Todavía no es un coop jugable: no se sincronizan la IA, el Alien, las armas de fuego ni el progreso de la misión. **Dónde quedamos y qué sigue:** sección "Último trabajo y siguiente paso", al final.

| Fase | Qué hace | Estado |
|------|----------|--------|
| 1 y 2 | `winmm.dll` proxy, hook en el loop principal, lectura de estado y posición del jugador | ✅ Proton y Windows 10 nativo |
| 3 | Red UDP propia (20 paquetes/s con el estado del jugador) | ✅ por Tailscale, 0 pérdidas |
| 4 | Avatar del otro jugador: un NPC civil del nivel, sin IA, teletransportado a su posición | ✅ en los dos sentidos |
| 4b | Avatar títere: posición exacta del otro cada frame, con animación de caminar, caminar rápido y correr | ✅ dos máquinas (corrida 52); agachado: el civil no tiene la animación (corridas 65–67) |
| 5 | "Usar" compartido: botones, cajas e ítems se replican y en la otra máquina los hace el avatar | ✅ botones; los ítems quedan uno por jugador |
| 5b | Objetos arrojados y colocados (bengala, noisemaker, molotov, minas…): aparecen en la otra máquina con la misma trayectoria y quedan donde cayó el original | 🟡 vuelo ✅ con dos máquinas; corrección de dónde cae verificada en una (corridas 55–64), falta con dos |
| 6 | Disparadores de los scripts decididos por el host (zonas que arrancan diálogos, eventos…) | 🟡 código y tests; índice de entidades verificado en el juego; falta probar con dos máquinas |

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

   Recomendado: `AIMP_GHOST_WALK=1` en las dos (el avatar títere, con animación). Opcional y sin probar con dos máquinas: `AIMP_SYNC_PINS=1` en las dos (los disparadores de zona los decide el host).

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

## Último trabajo y siguiente paso

**2026-11-16 (corridas 65–68, locales):**
- **Paso del avatar:** el "se desliza" probablemente venía de que el civil animaba `WALK` (~1,2 m/s) mientras el otro caminaba a 2,46 m/s. Ahora hay tres pasos: `WALK`, `FAST_WALK` (de 1,8 a 3 m/s) y `RUN`.
- **Agacharse:** se encontró la función del juego que agacha a un NPC (`FUN_007cae20`) y la tabla de posturas del personaje. Las banderas quedan bien, pero el civil sigue parado: lo más probable es que no tenga animaciones de agachado.
- **Arrojables:** el composite se encuentra por un camino fijo de punteros, pero no siempre es el vivo; la tasa sigue en ~50–75 %. Falta leer el cuerpo físico.
- **Prueba conjunta (corrida 69):** el deslizamiento quedó aceptado (es responsivo), el agachado se aplica pero el civil no tiene la animación, y los arrojables reales no se ubicaron (0 de 5). Corridas 70–71: la bengala no vuela con `ProjectileMotion` sino con la física; lo siguiente sería leer su cuerpo de Havok.

**2026-11-15, noche (corridas 52–64):**
- **Títere con dos máquinas (corrida 52):** "funciona perfecto… responsivo y exacto el movimiento". 0,00 m en las dos máquinas, 20 paquetes/s, 0 excepciones.
- **Agacharse (corrida 53):** el comando `IDLE` con las posturas 0 a 5 no agacha al civil, ni siquiera sin reubicarlo cada frame. Ese camino queda descartado. Pistas: `crouched_or_not`, `ConditionIsCrouching`.
- **Arrojables que caen igual (corridas 54–64):** la parábola ya salía igual (`ProjectileMotion` calcula la posición en función del tiempo); la diferencia aparece en el choque y los rebotes. Ahora el que tira avisa dónde se asentó su objeto (`AIMP_EVT_THROW_REST`). El otro libera su réplica con la función del juego (`FUN_007a10f0`, la de `kill_explosive`) y crea una quieta en ese punto. Se verificó en una máquina con el peer (4 de 4 réplicas reubicadas, 144 fps).
- `mod/dist/winmm.dll` = `907e36b5…` (commit `8624e99`). En papito el mod quedó en pasivo.

**Siguiente:**
1. **Prueba con dos máquinas de los arrojables:** tirar bengalas desde cada una, contra el piso y contra paredes. Ver que la réplica salte al punto del original, y en el log `THROW fijo` de los dos lados (`work/windows/instructions.md`, sección 8b).
2. Animación del títere: el usuario lo vio "deslizarse" en la laptop. Revisar el `MOVE_TOWARDS` que anima las piernas.
3. Agacharse: buscar dónde decide la locomoción del NPC si va agachado (`crouched_or_not`, `ConditionIsCrouching`).
4. Arrojables: un camino determinista hasta el composite, o leer el parámetro de posición por ShortGuid. Hoy se encuentra por búsqueda en ~3 de 4.

**2026-11-15 (sesión larga, corridas 20 a 34):**
- **Arrojables** (hallazgo 17, corrida 20): la bengala, el noisemaker, etc. se reinyectan con la misma trayectoria (`AIMP_SYNC_THROW=1`, protocolo v2). Verificado en una máquina.
- **Avatar que camina y corre** (hallazgo 19, corridas 21 a 34):
  - Cada personaje tiene **dos controladores de comandos**: el de tareas (`Character+0x3820`, donde manda su IA) y el de scripts (`Character+0x3570`). El árbol de comportamiento del civil solo ejecuta los comandos de script en la rama cinemática.
  - El mod le da un `MOVE_TOWARDS` en el controlador de scripts, fuerza la motivación cinemática del avatar (hook de `FUN_006d3630`, `ret 8`) y le actualiza el destino cada 200 ms.
  - **Corre** si queda a más de 5 m. El enum `MOVE` (`SLOW_WALK`, `WALK`, `FAST_WALK`, `RUN`…) salió de una tabla de nombres del propio `AI.exe`.
  - Antes de ubicarlo espera a que termine el `PLAY_ANIMATION` de su IA (si no, se caía del piso). Sin red sigue al jugador a 2 m; por red va a la posición del otro y, quieto, mira hacia donde mira el otro.
  - Lecciones: nunca `SwitchCommand(ctrl, NULL)` (se cae) y verificar siempre el `ret N` en el asm antes de hookear (un `ret 8` declarado como `ret 4` hizo caer el juego).
- **Herramientas:** corridas automáticas con el mando virtual y capturas de la ventana del juego; `aimp_peer.py` en v2 con `--replay` y `--offset`.
- `mod/dist/winmm.dll` está actualizada (commit `85c21ff`). En papito el mod quedó instalado en pasivo (`AIMP_GHOST=0`).

**Siguiente (mañana):**
1. **Prueba con dos máquinas** (papito host + laptop join). La laptop necesita la DLL nueva de `mod/dist/`, porque el protocolo cambió a v2. Config de las dos: `AIMP_GHOST=3`, `AIMP_GHOST_WALK=1`, `AIMP_SYNC_USE=1`, `AIMP_SYNC_THROW=1`, más `AIMP_NET=host` en papito y `AIMP_NET=join` con `AIMP_NET_PEER=<IP de Tailscale de papito>:47811` en la laptop. Ver que cada avatar camine y corra hacia el otro jugador, que gire cuando el otro está quieto, y que las bengalas y los noisemakers aparezcan del otro lado.
2. Si anda, dejar `AIMP_GHOST_WALK=1` por defecto con `AIMP_GHOST=3`.
3. Después, en estático: el disparo de las armas de fuego (hallazgo 17, "El equipo del jugador"), y más adelante el Alien.
4. Pendientes menores: `AIMP_SYNC_PINS` con dos máquinas; una corrida de control para el arranque corto.

**2026-11-15, noche (corridas 46–51), el títere:** el avatar ya no decide nada por su cuenta. Cada frame se pone en la posición exacta del otro jugador (0,00 m de diferencia en la mayoría de las muestras), con la animación de caminar o correr según la velocidad del otro. Que se agache cuando el otro se agacha todavía no funciona. Falta probarlo con dos máquinas: la DLL nueva está en `mod/dist/`.

**2026-11-15, noche (corridas 39–45), "dos Amandas":** se puede crear una segunda Amanda (`THE_PLAYER`) desde el mod, clonando el pedido de spawn de autoridad del jugador, sin que se caiga el juego y sin que se lleve la cámara. Pero **es invisible**: el juego solo trae modelos de Amanda en primera persona (las cinemáticas son videos). Por eso el avatar sigue siendo un NPC, ahora más responsivo:
- calcula la velocidad del otro y apunta adonde va a estar;
- corre cuando el otro corre;
- detecta cuándo el otro se agacha. Falta que el NPC se agache: se haría con el comando `IDLE` y `desired_stance`.

**2026-11-15, tarde (corridas 36–38):** primera prueba de la caminata con dos máquinas. El avatar camina y sigue natural en las dos. Hay que tener el mismo guardado: el mod no sincroniza el nivel, así que la laptop tiene que bajar la nube de Steam antes de pasar a sin conexión. Arreglos después de la prueba:
- el avatar ya no gira raro ni camina cuando el otro solo mueve la cámara: queda "plantado" hasta que el otro se mueve;
- el avatar es **invencible**, con el mismo flag que `CHR_SetInvincibility`, así matarlo no da game over por civiles;
- los arrojables caen distinto porque el engine integra el vuelo con el tiempo de cada frame. Ya se sabe dónde está su posición (sonda); falta corregir la réplica.

**2026-11-15, tarde (corrida 35):** el avatar ya vuelve a arrancar con movimientos cortos del otro (1–1,6 m, ≤1 s de retardo). Salió del layout de los parámetros de `MOVE_TOWARDS` (hallazgo 19): el flag `start_instantly`, más una orden nueva si queda quieto a más de 1 m. Los dos están activados por defecto y `mod/dist/winmm.dll` está actualizada (`a88bc1a`).

El detalle completo está en `work/PROGRESS.md` (estado), `work/NOTES.md` y `work/LOG.md` (bitácora) y `work/findings/19-control-de-personajes.md`.
