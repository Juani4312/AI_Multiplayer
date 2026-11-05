# Mod AI_Multiplayer

DLL proxy `winmm.dll` para `AI.exe` (Alien: Isolation, build de Windows). Funciona en Windows 10 nativo y en Linux con Proton. Por fase:

- **fases 1 y 2**: el código del mod corre una vez por frame en el thread del juego (hook en `ModeManager::Run`, hallazgo 02). Lee la posición y el yaw del jugador, la cámara y el estado del juego (hallazgos 03 y 04). Solo lee;
- **fase 3**: red UDP propia entre dos máquinas (`AIMP_NET`), con el estado del jugador a 20 paquetes/s;
- **fase 4** (experimental, `AIMP_GHOST=3`): un NPC civil del nivel hace de avatar del otro jugador (hallazgo 12);
- **fase 5** (experimental, `AIMP_SYNC_USE=1`): los "usar" (botones, cajas, ítems) se replican a la otra máquina, donde los hace el avatar (hallazgo 13).

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
| `AIMP_GHOST_NOAUTH` | 0 | 1 = experimento: el avatar pasa a no autoritativo (`bloque+0x4F = 0`), para ver si su IA se apaga |
| `AIMP_SYNC_USE` | 0 | 1 = **fase 5, experimental**: los "usar" del jugador local se mandan al otro, que los aplica con su jugador (hallazgo 13) |
| `AIMP_SYNC_USE_SKIP` | `3e3ec35d,1e38b898` | prefijos de objetivo (4 bytes en hex, como en el log) que no se mandan: lockers y terminal de guardado. `0` = mandar todo |
| `AIMP_SYNC_USE_ACTOR` | 1 | quién hace el uso remoto: 1 = el avatar (el NPC de `AIMP_GHOST=3`), si hay; 0 = el jugador local (se teletransporta a botones y lockers) |
| `AIMP_TRACE_USE` | 0 | 1 = loguea cada uso y sus dos handlers (solo mira) |
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

## Fase 5 (experimental): "usar" compartido

`AIMP_SYNC_USE=1` (`src/sync.c`, hallazgo 13), con red:
- **Envío:** un hook en el productor del "usar" (`FUN_00795d00`) manda cada uso del jugador local como evento de red. El evento se manda 3 veces y el receptor descarta los repetidos.
- **Filtro:** no se mandan los objetivos cuyo prefijo está en `AIMP_SYNC_USE_SKIP`. Por defecto son los lockers y la terminal de guardado, que son acciones de cada jugador.
- **Aplicación:** el receptor, si está en el mismo nivel, encola el RPC A 0x26 con ese objetivo. El que lo usa es el avatar (`AIMP_SYNC_USE_ACTOR=1`) o, si no hay avatar, el jugador local. El engine lo ejecuta por su camino normal: handler A → handler de sesión → `FUN_00793FB0`.
- **Resultado:** verificado con el botón del transit en los dos sentidos. El transit viene en las dos máquinas y ningún jugador se teletransporta.

`test_sync` lo prueba contra stubs del productor, los dos handlers y el wrapper de encolado. `test_net` prueba el protocolo de eventos.

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
