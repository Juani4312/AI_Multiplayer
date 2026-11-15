# Mod AI_Multiplayer

DLL proxy `winmm.dll` para `AI.exe` (Alien: Isolation, build de Windows). Funciona en Windows 10 nativo y en Linux con Proton. Por fase:

- **fases 1 y 2**: el código del mod corre una vez por frame en el thread del juego (hook en `ModeManager::Run`, hallazgo 02). Lee la posición y el yaw del jugador, la cámara y el estado del juego (hallazgos 03 y 04). Solo lee;
- **fase 3**: red UDP propia entre dos máquinas (`AIMP_NET`), con el estado del jugador a 20 paquetes/s;
- **fase 4** (experimental, `AIMP_GHOST=3`): un NPC civil del nivel hace de avatar del otro jugador (hallazgo 12);
- **fase 5** (experimental, `AIMP_SYNC_USE=1`): los "usar" (botones, cajas, ítems) se replican a la otra máquina, donde los hace el avatar (hallazgo 13);
- **fase 4b** (experimental, `AIMP_GHOST_WALK=1`): el avatar camina y corre con su animación hacia el otro jugador en vez de teletransportarse. Verificado en una máquina (corridas 27–31); falta con dos (hallazgo 19);
- **fase 6** (experimental, `AIMP_SYNC_PINS=1`, sin probar con dos máquinas): los disparadores de zona de los scripts los decide el host. El cliente bloquea los suyos y ejecuta los del host (hallazgos 15 y 16).
- **arrojables** (experimental, `AIMP_SYNC_THROW=1`; la inyección se verificó en una máquina, falta con dos): lo que tira o coloca el jugador (bengala, noisemaker, molotov, minas…) aparece en la otra máquina con la misma posición y velocidad (hallazgo 17).

Sin opciones, el mod **solo lee**: lo único que modifica son los primeros bytes de `ModeManager::Run` (el JMP del hook), y siempre con la build verificada.
- Los trazados (`AIMP_TRACE_QUEUES`, `AIMP_TRACE_SPAWN`, `AIMP_TRACE_USE`) agregan hooks que solo miran.
- Las fases 4 y 5 sí escriben en el juego y llaman a funciones del engine: el teleport del avatar y el encolado de usos.
- Cada paso queda en el log antes de hacerlo, y un registro de excepciones (`src/crash.c`) deja la causa si el juego se cae.

## Build soportada

| Campo | Valor |
|-------|-------|
| Ejecutable | `AI.exe` PE32 i386 |
| SHA-256 | `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Steam buildid | 4940658 |

Antes de hookear, el mod verifica el TimeDateStamp (`0x54E3199D`), el SizeOfImage (`0x01F81000`), las
RVAs de los exports `ovr_Initialize` (`0xBA83B0`) y `curl_easy_init` (`0xCEAB20`) y los 10 bytes del
prólogo de `ModeManager::Run`. Si algo no coincide, queda como proxy puro de winmm: el juego anda igual
que sin el mod y el log dice qué no coincidió.

## Compilar

Requisitos: `mingw-w64-gcc` (i686), CMake ≥ 3.20 y make. Para los tests, `wine`.

```sh
cmake -S mod -B mod/build -DCMAKE_TOOLCHAIN_FILE=$PWD/mod/cmake/toolchain-i686-w64-mingw32.cmake
cmake --build mod/build
mod/test/run_tests.sh          # tests con wine en un prefix descartable
```

Salida: `mod/build/winmm.dll` (~53 KB). Se compila **sin CRT**: el único import es `KERNEL32.dll`.

La lista de exports (`src/winmm_exports.inc`, `src/winmm.def`) la genera
`tools/gen_winmm_exports.py`. Es la unión de la winmm de Proton 9 y de los nombres de la import
library de mingw-w64, que salen de la winmm de Windows: 202 por nombre más 3 solo por ordinal. Cualquier
llamada a un export que la winmm real no tenga corta el juego con un mensaje y una línea `FATAL` en el
log, en lugar de crashear sin explicación.

## Tests (sin el juego)

`mod/test/run_tests.sh` corre con wine:

- `test_core`: el motor de hooks sobre una función con **el mismo prólogo** que `ModeManager::Run`
  (instalación, rechazo si los bytes difieren, preservación de EBX/ESI/EDI/EBP y ESP con el stack
  desalineado, remove), más el formateador contra el `snprintf` del CRT;
- `test_probe`: la sonda contra una imagen de `AI.exe` simulada, con los objetos del juego en los
  offsets de `src/game.h`;
- `test_host`: un .exe que importa `winmm.dll` como `AI.exe`. Carga el proxy, el proxy carga la real y
  las llamadas se reenvían.

## Instalar

### Linux (Proton), automatizado

`work/scripts/runtime_setup.sh install [--cfg "AIMP_X=1;AIMP_Y=2"]` copia la DLL y agrega al registro del prefix `AppDefaults\AI.exe\DllOverrides: winmm=native,builtin`, que equivale a la opción de lanzamiento pero no toca la config de Steam. Guarda un backup de `user.reg` antes de la primera edición. `runtime_setup.sh revert` deshace todo; `status` muestra el estado. Se niega a correr con el juego abierto.

### Linux (Proton), a mano

1. Copiar `mod/build/winmm.dll` a la carpeta del juego, al lado de `AI.exe`:
   `"/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation/"`.
2. En Steam: Alien: Isolation → Propiedades → Opciones de lanzamiento:
   ```
   WINEDLLOVERRIDES="winmm=n,b" %command%
   ```
   Sin esto, Wine usa su winmm interna e ignora la del juego. Se pueden agregar variables (ver abajo),
   por ejemplo `AIMP_STATUS_MS=500 WINEDLLOVERRIDES="winmm=n,b" %command%`.
3. Log: `/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer/aimp.log`
   (el de la corrida anterior queda como `aimp.log.prev`).

### Windows

1. Verificar que `winmm` **no** esté en KnownDLLs (si está, Windows ignora la copia del juego):
   ```
   reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"
   ```
2. Copiar `winmm.dll` al lado de `AI.exe`. No hace falta opción de lanzamiento.
3. Log: `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`.

### Desinstalar

Borrar `winmm.dll` de la carpeta del juego (y sacar la opción de lanzamiento). El mod no deja otros
archivos fuera de la carpeta del log.

## Variables de entorno y `aimp.cfg`

Las variables se pueden poner también en un archivo `aimp.cfg` (líneas `AIMP_X=valor`, `#` comenta), en `%LOCALAPPDATA%\AI_Multiplayer\aimp.cfg` o al lado de la DLL. Sirve en Windows, donde Steam no acepta `VAR=valor` en las opciones de lanzamiento. Si una variable está en el entorno y en el archivo, gana el entorno. Solo se aplican claves que empiezan con `AIMP_`.


| Variable | Default | Uso |
|----------|---------|-----|
| `AIMP_LOG` | `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` | ruta del log. En Proton se puede usar `Z:/home/...` |
| `AIMP_NOHOOK` | 0 | 1 = proxy puro, sin hooks (modo seguro) |
| `AIMP_SAMPLE_MS` | 250 | cada cuánto se toma una foto completa del estado (eventos `EV`) |
| `AIMP_STATUS_MS` | 1000 | cada cuánto se escribe el resumen (`ST`) |
| `AIMP_DUMP_KEY` | `0x78` (F9) | tecla de volcado completo de estructuras |
| `AIMP_DUMP_AT_FRAME` | 0 | volcado automático en ese frame (0 = no) |
| `AIMP_FIRST_FRAMES` | 3 | cuántos frames iniciales se loguean uno por uno |
| `AIMP_NET` | (vacío) | fase 3: `host` (escucha) o `join` (se conecta a `AIMP_NET_PEER`); vacío = red apagada |
| `AIMP_NET_PORT` | 47811 | puerto UDP del host |
| `AIMP_NET_PEER` | — | `join`: `ip:puerto` del host (p. ej. la IP de Tailscale) |
| `AIMP_NET_HZ` | 20 | paquetes de estado por segundo |
| `AIMP_MARK_KEY` | `0x77` (F8) | tecla de marca: una línea `MARCA n` con frame y posición, para cruzar acciones con el log |
| `AIMP_TRACE_QUEUES` | 0 | 1 = experimento 3.6: hookea los wrappers de encolado de las colas A y B (`FUN_00a2a290`, `FUN_00a2a810`) y registra tipo, tamaño, thread y call site de cada mensaje (líneas `TR`) |
| `AIMP_TRACE_MS` | 5000 | cada cuánto se escribe el informe del trazado |
| `AIMP_TRACE_SPAWN` | 0 | 1 = hookea el spawn de autoridad (`FUN_00797490`) y loguea cada pedido de 0x130 bytes decodificado y completo (líneas `SP`). Solo mira |
| `AIMP_GHOST` | 0 | 1 = **fase 4, experimental** (escribe en el juego): clona el pedido de spawn del jugador y mueve el clon (líneas `GHOST`). Ver abajo |
| `AIMP_GHOST_DELAY_MS` | 8000 | cuánto esperar, desde que aparece el jugador, para pedir el clon |
| `AIMP_GHOST_FROM` | −1 | clave (`+0x00`) del pedido a clonar; −1 = el último del jugador (tipo 0) |
| `AIMP_GHOST_NPC_MS` | 20000 (modo 2) / sin límite (modo 3) | cuánto tiempo el NPC hace de avatar; ≤ 0 = sin límite |
| `AIMP_GHOST_NOAUTH` | 1 en el modo 3 | el avatar pasa a no autoritativo (`bloque+0x4F = 0`): su IA deja de actuar (no huye del fuego ni se mueve solo; corrida 14). 0 = dejarle la IA |
| `AIMP_SYNC_USE` | 0 | 1 = **fase 5, experimental**: los "usar" del jugador local se mandan al otro, que los aplica con su jugador (hallazgo 13) |
| `AIMP_SYNC_USE_SKIP` | `3e3ec35d,1e38b898` | prefijos de objetivo (4 bytes en hex, como en el log) que no se mandan: lockers y terminal de guardado. `0` = mandar todo |
| `AIMP_SYNC_USE_ACTOR` | 1 | quién hace el uso remoto: 1 = el avatar (el NPC de `AIMP_GHOST=3`), si hay; 0 = el jugador local (se teletransporta a botones y lockers) |
| `AIMP_SYNC_PENDING_MS` | 60000 | cuánto se guarda un uso remoto que todavía no se puede aplicar (el receptor en otro nivel, o sin avatar todavía) |
| `AIMP_TRACE_USE` | 0 | 1 = loguea cada uso y sus dos handlers (solo mira) |
| `AIMP_TRACE_PINS` | 0 | 1 = traza de los pines de salida de CATHODE (líneas `PIN`, hallazgo 15). Solo mira. Con traza, 10 s después de cada cambio de nivel arma y verifica el índice pid → entidad (líneas `PINIX`) |
| `AIMP_PINS_FILE` | — | archivo de comandos que se relee cuando cambia: `FIRE <pid> <pin> [flags]` (reinyecta un pin) e `INDEX` (rearma el índice) |
| `AIMP_SYNC_PINS` | 0 | 1 = **fase 6, experimental** (con red): el host manda los disparos de las reglas y el cliente bloquea los suyos y aplica los del host (líneas `PINSYNC`) |
| `AIMP_SYNC_PINS_LIST` | zona | reglas `tipo:pin,...` en hex (`tipo:*` = todos los pines). Por defecto: `PlayerTriggerBox` `on_entered`/`on_exited` y `TriggerVolumeFilter` `on_event_entered`/`on_event_exited`/`triggered` |
| `AIMP_TRACE_THROW` | 0 | 1 = loguea cada objeto que tira o coloca el jugador (líneas `THROW`: plantilla, posición, velocidad, caller). Solo mira |
| `AIMP_SYNC_THROW` | 0 | 1 = **experimental** (con red): lo que tira o coloca el jugador local se manda al otro, que llama a la misma función con los mismos datos (hallazgo 17) |
| `AIMP_THROW_ECHO` | 0 | prueba local sin red: repite cada lanzamiento propio a los N ms, corrido `AIMP_THROW_ECHO_CM` cm (75) al costado. Si aparece la copia con la misma curva, la inyección funciona |
| `AIMP_GHOST_WALK` | 0 | 1 = **fase 4b, experimental** (modos 2 y 3): el avatar camina hacia el objetivo con un comando `MOVE_TOWARDS` (su propia animación) en vez de teletransportarse en cada frame; solo se teletransporta si queda lejos. Apaga `AIMP_GHOST_NOAUTH` por defecto (hallazgo 19) |
| `AIMP_GHOST_WALK_TYPE` | 1 | `move_type` cerca del objetivo (enum `MOVE`: 0 `SLOW_WALK`, 1 `WALK`, 2 `FAST_WALK`, 3 `RUN`) |
| `AIMP_GHOST_WALK_TYPE_FAR` | 3 | `move_type` si queda a más de `AIMP_GHOST_WALK_FAR_CM` del objetivo (`RUN`); igual a `AIMP_GHOST_WALK_TYPE` lo apaga |
| `AIMP_GHOST_WALK_FAR_CM` | 500 | a partir de qué distancia al objetivo corre; vuelve a caminar 250 cm más cerca |
| `AIMP_GHOST_WALK_MS` | 200 | cada cuánto se le actualiza el destino |
| `AIMP_GHOST_WALK_TP_CM` | 1200 | si queda a más de esto (o en otro piso) durante 1 s, se lo reubica con una orden nueva en el mismo frame |
| `AIMP_GHOST_WALK_PREP_MS` | 4000 | al empezar: máximo de espera a que termine el `PLAY_ANIMATION` de su IA antes de ubicarlo (mínimo 500 ms) |
| `AIMP_GHOST_WALK_FOLLOW_CM` | 200 | sin red: el avatar sigue al jugador local a esta distancia, del lado donde está; 0 = punto fijo detrás |
| `AIMP_GHOST_WALK_BEHIND_CM` | 250 | sin red: el punto detrás del jugador para ubicarlo (y para seguir, con `FOLLOW_CM=0`) |
| `AIMP_GHOST_WALK_RESTART_CM` | 0 | > 0: orden nueva si el objetivo se movió más que esto (como mucho cada `AIMP_GHOST_WALK_RESTART_MS`, 500); 0 = solo actualizaciones |
| `AIMP_GHOST_WALK_CTRL` | `0x3570` | controlador al que se le dan las órdenes: el de scripts; `0x3820` (14368) es el de tareas, que no funciona |
| `AIMP_GHOST_WALK_FACE` | 1 | con red: si el otro está quieto 1 s y el avatar se asentó, se lo gira hacia donde mira el otro (teletransporte en el lugar con ese yaw y orden nueva) |
| `AIMP_GHOST_WALK_CINEMATIC` | 1 | fuerza la motivación CINEMATIC del avatar (hook de `FUN_006d3630`): sin eso su árbol no ejecuta comandos de script |
| `AIMP_GHOST_WALK_BLOCK` | 1 | bloquea los `SwitchCommand` de la IA sobre el controlador del avatar (solo pasan el `MOVE_TOWARDS` del mod y el cambio a ninguno) |
| `AIMP_TRACE_CTRL` | 0 | 1 = vuelca el controlador de jugador del jugador 0 (`Character+0x31C`, 0x4C bytes) y su acción principal cada vez que cambian (líneas `CTRL`, como mucho cada `AIMP_TRACE_CTRL_MS` = 100 ms, hasta `AIMP_TRACE_CTRL_MAX` = 600 líneas). Solo mira (hallazgo 19) |
| `AIMP_TRACE_CMD` | 0 | 1 = loguea los `SwitchCommand` de los personajes (líneas `CMD`: controlador de comandos, tipo y dueño). Solo mira (hallazgo 19) |
| `AIMP_CRASHLOG` | 1 | registro de excepciones (VEH): código, módulo+RVA, registros y stack si el juego se cae |

## Fase 3: red mínima (UDP)

Protocolo propio v1 (`src/net.h`): un paquete `AimpPkt` de 76 bytes con nivel, posición, yaw, cámara, contador de simulación e id de 8 bytes del personaje, a `AIMP_NET_HZ` por segundo. Sin threads: todo corre en el hook del frame con sockets no bloqueantes; Winsock se resuelve en tiempo de ejecución. El host aprende la dirección del cliente del primer paquete. En el log, líneas `NET` (una por segundo, con lo último que llegó del otro jugador).

Detalles:
- El envío mantiene una cadencia fija: el próximo envío se cuenta desde el horario previsto, no desde el frame en que salió el anterior. Si el juego estuvo trabado (una carga), manda uno y sigue la cadencia normal, sin ráfaga.
- El socket se abre al cargar la DLL, pero se lee desde el primer frame. El primer `net_poll` descarta lo que llegó antes (`NET: descartados N paquetes que llegaron antes del primer frame`). Así no se toma un estado viejo ni se cuentan pérdidas falsas.
- Mientras un juego carga un nivel no corre el hook por frame: ese lado no manda nada. Del otro lado se ve `recibidos 0` y la edad del último estado crece. No es pérdida.
- Verificado por Tailscale entre Windows 10 nativo y Proton el 2026-11-05 (`work/NOTES.md`).

Prueba sin un segundo juego: `python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811` hace de otro jugador (posición falsa que da vueltas) contra el juego en modo `host`, y muestra lo que manda el juego.

## Fase 4 (experimental): avatar del otro jugador

`AIMP_GHOST=3` (`src/ghost.c`, hallazgo 12; el modo recomendado):
1. `AIMP_GHOST_DELAY_MS` (8000) después de que aparece el jugador local, elige un NPC del nivel de tipo `AIMP_GHOST_NPC_TYPE` (6 = civil "INNOCENT").
2. En cada frame lo teletransporta (`FUN_007c6330`):
   - si hay red y están en el mismo nivel, a la posición y el yaw del otro jugador;
   - si no, a 1,5 m en x del jugador local.

   También lo vuelve a poner si su IA lo corre más de 2 cm.
3. Si el NPC desaparece, elige otro. No tiene límite de tiempo.

La posición se le pasa al engine como un vector de 16 bytes alineado a 16: `SetTransform` la copia con `MOVAPS` (corrida 7).

Los modos 1 y 2 clonan el pedido de spawn del jugador para crear un personaje propio. **Hacen caer el juego** (el nivel no tiene recursos para una clave nueva), así que quedan solo para investigar. `AIMP_TRACE_SPAWN=1` loguea los pedidos de spawn.

`test_ghost` prueba todo contra stubs con los prólogos y el `ret N` reales del spawn, `SpawnCharacter` y el teleport. Verifica el balance del stack y la alineación.

## Fase 4b (experimental): avatar que camina

`AIMP_GHOST_WALK=1` (`src/ghost.c`, hallazgo 19), junto con `AIMP_GHOST=3`:
1. **Preparación:** le da un `MOVE_TOWARDS` hacia el objetivo (`FUN_0067e980`) en su **controlador de scripts** (`Character+0x3570`) desde donde está. Espera a que su controlador de tareas (`+0x3820`) no tenga un `PLAY_ANIMATION` (al menos 500 ms): teletransportarlo durante esa animación lo hacía caer del piso (corridas 26–29).
2. **Ubicación:** lo teletransporta al objetivo y le da una orden nueva en el mismo frame (sin orden nueva, la locomoción lo devuelve; corrida 22).
3. **Caminata:** cada `AIMP_GHOST_WALK_MS` (200) le actualiza el destino (`FUN_006797a0`). Camina con su propia animación (`WALK`, ~1,2 m/s). Si queda a más de 5 m del objetivo, orden nueva con `RUN` (~4,8 m/s); a menos de 2,5 m, otra vez `WALK`.
4. Si queda a más de `AIMP_GHOST_WALK_TP_CM` (12 m) u otro piso durante 1 s: teletransporte y orden nueva en el mismo frame (como mucho cada 3 s). Nunca `SwitchCommand(ctrl, NULL)`: el juego se cae (corrida 23).
5. Su árbol de comportamiento (`NPC_INNOCENT_BEHAVE.XML`) solo ejecuta comandos de script en la rama CINEMATIC: el mod fuerza esa motivación para el avatar (hook de `ConditionHasMotivation`, `FUN_006d3630`, `ret 8`). El NPC conserva su autoridad. Con `AIMP_GHOST_WALK_BLOCK=1` se bloquean los cambios de comando de su IA sobre el controlador de scripts (filtro en el hook de `SwitchCommand`, `src/ctrl.c`).

Con red, el objetivo es la posición del otro jugador y, quieto, mira hacia donde mira el otro (`AIMP_GHOST_WALK_FACE`). Sin red, sigue al jugador local a `AIMP_GHOST_WALK_FOLLOW_CM` (200), del lado donde está; la primera vez y si quedó lejos, se lo ubica `AIMP_GHOST_WALK_BEHIND_CM` (250) detrás.

**Probar la red en una sola máquina:** el juego con `AIMP_NET=host` y `python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811 --replay 3 --offset 0,0,2`. El peer hace de otro jugador que repite el recorrido del jugador 3 s después (corridas 32–34).

**Estado:** verificado en una máquina con el mando virtual (corridas 27–34), también por red contra el peer: sigue caminando y corriendo, sin trabarse ni pegársele, y quieto mira hacia donde mira el otro. Falta la prueba con dos máquinas. Después de llegar, no vuelve a arrancar hasta que el objetivo se aleja ~1–3 m.

`test_walk` lo prueba contra stubs con los prólogos y el `ret N` reales de los dos comandos (`ret 0x14`), de `SwitchCommand` y del nodo de motivación (`ret 8`): preparación con un `PLAY_ANIMATION` en curso, seguimiento a distancia, cambio a `RUN` y vuelta a `WALK`, reubicación y filtro.

## Fase 5 (experimental): "usar" compartido

`AIMP_SYNC_USE=1` (`src/sync.c`, hallazgo 13), con red:
- **Envío:** un hook en el productor del "usar" (`FUN_00795d00`) manda cada uso del jugador local como evento de red. El evento se manda 3 veces y el receptor descarta los repetidos.
- **Filtro:** no se mandan los objetivos cuyo prefijo está en `AIMP_SYNC_USE_SKIP`. Por defecto son los lockers y la terminal de guardado, que son acciones de cada jugador.
- **Aplicación:** el receptor, si está en el mismo nivel, encola el RPC A 0x26 con ese objetivo. El que lo usa es el avatar (`AIMP_SYNC_USE_ACTOR=1`) o, si no hay avatar, el jugador local. El engine lo ejecuta por su camino normal: handler A → handler de sesión → `FUN_00793FB0`.
- **Resultado:** verificado con el botón del transit en los dos sentidos. El transit viene en las dos máquinas y ningún jugador se teletransporta.

`test_sync` lo prueba contra stubs del productor, los dos handlers y el wrapper de encolado. `test_net` prueba el protocolo de eventos.

## Fase 6 (experimental): disparadores decididos por el host

`AIMP_SYNC_PINS=1` (`src/pins.c`, hallazgos 15 y 16), con red:
- **Host:** cada disparo natural de un pin que cumple una regla (`AIMP_SYNC_PINS_LIST`) sale como evento `AIMP_EVT_PIN` con el `pid` de la entidad (la ruta de composites, igual en las dos máquinas con los mismos datos), el pin, el tipo, el id y los flags del mensaje.
- **Cliente:** todo disparo que cumple una regla se bloquea (no se llama al original), salvo el que inyecta el mod. Al llegar un evento del mismo nivel, busca la entidad por `pid` en un índice armado recorriendo el árbol de composites, y dispara el pin con una plantilla de mensaje. La plantilla es el mensaje de un disparo natural visto: del mismo pid y pin, del mismo tipo y pin, del mismo pin o del mismo tipo. Le aplica la clase del host (entró o salió).
- Los eventos de otro nivel o de entidades que todavía no están quedan pendientes, en orden, hasta `AIMP_SYNC_PENDING_MS`.

`test_pins` lo prueba con un árbol falso: índice, FIRE sin disparo previo, bloqueo, aplicación, bloqueo del disparo en cadena y envío del host.

## Arrojables (experimental): lo que tira o coloca el jugador

`AIMP_SYNC_THROW=1` (`src/throw.c`, hallazgo 17), con red:
- **Envío:** hooks en `FUN_007b5050` (tirar: el evento de animación `spawn_projectile`) y `FUN_007b5190` (colocar: minas y soltar el ítem de la mano). Cada llamada sale como `AIMP_EVT_THROW` (112 bytes): la matriz de la mano, la velocidad, `time_left` y la plantilla (`Thrown\Flare`…).
- **Aplicación:** el receptor, si está en el mismo nivel, llama a la misma función por el trampolín (no vuelve a mandarse). El engine instancia el composite `REQUIRED_ASSETS\THROWN\…` y `ProjectileMotion` simula el mismo vuelo.
- **Protocolo v2:** los eventos pasaron de 52 a 112 bytes de datos. Las dos máquinas tienen que tener la misma versión del mod.
- **Corrida 20 (una máquina):** con `AIMP_TRACE_THROW=1;AIMP_THROW_ECHO=1500`, una bengala real se duplicó 1,5 s después, 75 cm al costado, con la misma trayectoria. Falta la prueba con dos máquinas.

`test_throw` lo prueba contra stubs con el prólogo SEH y los `ret` reales de las dos funciones.

## Qué hay en el log

- Encabezado: versión, commit, plataforma (Windows/Wine), winmm real cargada, exports resueltos y
  ausentes, verificación de la build, hook instalado.
- `FASE 1: primer frame dentro de ModeManager::Run` + lista de módulos cargados (muestra ReShade,
  MovieFix, overlay de Steam, etc.).
- `EV ...`: cambios de estado del `ModeManager` (por frame), de nivel, de personajes, del jugador, de la
  cámara y de la red. Cualquier cosa marcada `INESPERADO` contradice el análisis estático.
- `ST ...`: resumen periódico con fps, tiempo de `Run`, estado, nivel, personajes, **posición y yaw del
  jugador**, cámara (posición, cuaternión, diferencia con el jugador), NETZ, sesión y tamaño de la cola A.
- `RPC ...`: catálogo de endpoints de la cola A y de la sesión (tipo → handler), una vez por sesión de juego.
- `TR ...` (solo con `AIMP_TRACE_QUEUES=1`): tipos nuevos con su call site, informe por período y totales al salir.
- `==== volcado ====`: estructuras en crudo (dwords + float) del root, `ModeManager`,
  `CharacterManager` (con la lista de jugadores y NPCs), jugador, definición, controlador, cámara, NETZ,
  cola A y sesión. Se hace automáticamente cuando aparece el jugador y 5 s después, y con F9.

## Primera prueba (checklist para la sesión con el usuario)

1. Copiar la DLL y poner la opción de lanzamiento (ver arriba).
2. Arrancar el juego. En el menú principal, esperar unos 10 s.
3. Cargar una partida. Ya en el juego: quedarse quieto 5 s, caminar en línea recta unos 5 s, girar
   despacio 360°, agacharse, abrir una puerta y apretar F9 en dos lugares distintos.
4. Salir al menú y cerrar el juego.
5. Revisar `aimp.log`: `python3 -I work/scripts/analyze_aimp_log.py <aimp.log>` cruza el log con cada chequeo de `work/RUNTIME_PLAN.md` y marca OK / FALLA / sin datos.
6. Si la primera corrida anduvo, segunda corrida con `AIMP_TRACE_QUEUES=1 WINEDLLOVERRIDES="winmm=n,b" %command%`: repetir lo mismo y anotar la hora aproximada de cada acción (abrir una puerta, agarrar un item, ver al Alien), para cruzarla con los tipos de mensaje del log.

Si el juego no arranca: sacar la DLL (o usar `AIMP_NOHOOK=1`) y revisar el log. Si ni siquiera hay log,
el problema es la carga del proxy (opción de lanzamiento, KnownDLLs).
