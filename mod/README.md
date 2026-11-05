# Mod AI_Multiplayer (fase 1+2)

DLL proxy `winmm.dll` para `AI.exe` (Alien: Isolation, build de Windows). Esta versión no agrega
multiplayer todavía. Sirve para:

- **fase 1**: probar que el código del mod corre una vez por frame en el thread del juego (hook en
  `ModeManager::Run`, hallazgo 02);
- **fase 2**: leer y loguear la posición y el yaw del jugador, la cámara y el estado del juego
  (hallazgos 03 y 04);
- verificar en runtime, **solo leyendo**, los offsets de los hallazgos 02–10 (`work/RUNTIME_PLAN.md`,
  bloques 0 a 3): loop, nivel, personajes, cámara, NETZ, sesión de red y cola RPC A.

El mod **nunca escribe** en estructuras del juego. Lo único que modifica son los primeros 6 bytes de
`ModeManager::Run` (el JMP del hook) y, solo con `AIMP_TRACE_QUEUES=1`, los de los dos wrappers de encolado. Siempre con la build verificada.

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

### Linux (Proton)

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

## Variables de entorno

| Variable | Default | Uso |
|----------|---------|-----|
| `AIMP_LOG` | `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` | ruta del log. En Proton se puede usar `Z:/home/...` |
| `AIMP_NOHOOK` | 0 | 1 = proxy puro, sin hooks (modo seguro) |
| `AIMP_SAMPLE_MS` | 250 | cada cuánto se toma una foto completa del estado (eventos `EV`) |
| `AIMP_STATUS_MS` | 1000 | cada cuánto se escribe el resumen (`ST`) |
| `AIMP_DUMP_KEY` | `0x78` (F9) | tecla de volcado completo de estructuras |
| `AIMP_DUMP_AT_FRAME` | 0 | volcado automático en ese frame (0 = no) |
| `AIMP_FIRST_FRAMES` | 3 | cuántos frames iniciales se loguean uno por uno |
| `AIMP_TRACE_QUEUES` | 0 | 1 = experimento 3.6: hookea los wrappers de encolado de las colas A y B (`FUN_00a2a290`, `FUN_00a2a810`) y registra tipo, tamaño, thread y call site de cada mensaje (líneas `TR`) |
| `AIMP_TRACE_MS` | 5000 | cada cuánto se escribe el informe del trazado |

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
5. Revisar `aimp.log`. El análisis de lo esperado está en `work/RUNTIME_PLAN.md`.
6. Si la primera corrida anduvo, segunda corrida con `AIMP_TRACE_QUEUES=1 WINEDLLOVERRIDES="winmm=n,b" %command%`: repetir lo mismo y anotar la hora aproximada de cada acción (abrir una puerta, agarrar un item, ver al Alien), para cruzarla con los tipos de mensaje del log.

Si el juego no arranca: sacar la DLL (o usar `AIMP_NOHOOK=1`) y revisar el log. Si ni siquiera hay log,
el problema es la carga del proxy (opción de lanzamiento, KnownDLLs).
