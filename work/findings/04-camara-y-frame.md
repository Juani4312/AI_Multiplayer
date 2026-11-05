# 04 - Cámara, transform del mundo y mapa de tareas del frame

- **Pregunta:** ¿Dónde está la cámara activa (posición y orientación) y cómo se arma el frame? Lo necesitamos para ubicar al otro jugador respecto de lo que ve el jugador local.
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Registro de tareas del frame (`RegisterTask(nombre, callback)` × ~90) | `0x007D6370` | `0x3D6370` |
  | `RegisterTask` | `0x00A1A220` | `0x61A220` |
  | Handles de tareas (globales consecutivos) | `0x01640F34`–`0x01641078` | `0x1240F34`– |
  | `g_root+0x58`: gestor de la cámara del jugador | — | — |
  | `PlayerCamera::Update(double dt)` (thiscall) | `0x004340D0` | `0x340D0` |
  | `PlayerCamera::PreUpdate(double dt)` | `0x00435410` | `0x35410` |
  | Cuaternión → yaw | `0x004332E0` | `0x332E0` |
  | Callback de `CAMERA_UPDATE` | `0x007B4090` | `0x3B4090` |
  | Singleton del renderer | `0x01766A00` (dato) | `0x1366A00` |
  | `Renderer::CalculatePOVs` | `0x009B30E0` | `0x5B30E0` |
  | Timer del frame (`FrameTimer::Tick`) | `0x00773050` | `0x373050` |
  | dt real del frame (double, s) | `0x016410D0` (dato) | `0x12410D0` |
  | dt escalado (double) | `0x016410C0` (dato) | `0x12410C0` |
  | Tiempo de juego acumulado (double) | `0x016410A8` (dato) | `0x12410A8` |
  | Tiempo real acumulado (double) | `0x016410B0` (dato) | `0x12410B0` |
  | Contador de frames (int) | `0x016410DC` (dato) | `0x12410DC` |
- **Evidencia:** `./work/evidence/h04/c1.txt` … `c7.txt` (salidas de `Dump.java`). `game/data/TASKS.TXT` es una copia del archivo del juego (SHA-256 `451f1fa3…a5787`, no se commitea).
- **Confianza:** alta para el registro de tareas y sus callbacks, el gestor de cámara en `g_root+0x58` y las variables del timer. Media para el layout de la cámara activa (`+0x2C` cuaternión, `+0x3C` posición); sale del uso, no de un constructor.
- **Cómo verificar en runtime:** leer `[[g_root+0x58]+0x1DC]`: `+0x3C/+0x40/+0x44` tiene que quedar cerca de la posición del jugador más la altura de los ojos, y el cuaternión en `+0x2C` tiene que tener norma 1. `DAT_016410DC` tiene que subir 1 por frame.
- **Verificado en runtime:** **sí** (Corridas del 2026-11-05 con el mod de la fase 1+2 (Proton 9.0-203); logs en `work/evidence/runtime/run1-menu.log` y `run2-juego.log`, resumen en el hallazgo 11). La cámara activa `[[g_root+0x58]+0x1DC]` tiene un cuaternión `+0x2C` de norma 1 y su posición `+0x3C` queda a 1,57 m sobre los pies del jugador (0,85 m agachado). El dt real `0x16410D0` ≈ 1/fps. **Corrección:** `0x16410DC` cuenta **frames de simulación**: +1 por frame mientras se juega y 0 en menú o pausa, donde el timer usa `FUN_00773280` (decisión en `FUN_007ddae0` por `[DAT_016f194c+4] & DAT_012f3014`). El tiempo de juego `0x16410A8` se comporta igual.

## Mapa de tareas del frame (`DATA/TASKS.TXT` + `FUN_007d6370`)

El engine arma cada frame con un **scheduler de tareas**:
- `DATA/TASKS.TXT` define el grafo: tipo, thread, llamadas por frame, prioridad, nombre y dependencias. La línea `BOOT_PC [cpx,pxl,pxl,pxl]` indica el thread principal más 3 workers. Las tareas `[c]` corren en el thread principal; las `[x]`, en cualquier worker.
- `FUN_007d6370` empareja cada nombre con su callback. Los callbacks son stubs chicos que llaman al subsistema.

Tareas relevantes para el proyecto (callback → qué llama):

| Tarea | Thread | Callback | Llama a |
|-------|--------|----------|---------|
| `PLATFORM_UPDATE` | c | `0x7CA180` | plataforma/Steam (ver hallazgo 02) |
| `CONTROLLER_UPDATE` | x | `0x783430` | input |
| `PLAYER_CAMERA_UPDATE` | c | `0x774FC0` | `[g_root+0x58]->Update(dt)` (`0x4340D0`) |
| `CAMERA_PRE_UPDATE` | c | `0x790580` | `[g_root+0x58]->PreUpdate(dt)` (`0x435410`) |
| `CAMERA_UPDATE` | c | `0x7B4090` | sistema de cámaras + caso del jugador |
| `CHARACTER_IK_AND_RAGDOLL` | c | `0x7C9FB0` | personajes |
| `ENTITY_MANAGER` | c | `0x783500` | entidades (hallazgo 05/06) |
| `ENTITY_MANAGER_TICK` | c (manual) | `0x783640` | scripts (hallazgo 06) |
| `PICKUP_MANAGER_TICK` | c | `0x7CA0B0` | items (hallazgo 07) |
| `ZONE_MANAGER` | c | `0x783590` | zonas/streaming |
| `AI_START_THINK` … `AI_END_THINK` | x | `0x7B4170` … `0x7834A0` | IA (hallazgo 08) |
| `AI_COMBAT_MANAGER_MAIN` | x | `0x7D4F30` | IA de combate |
| `AI_CHARACTER_LOGIC_ACTIONS` | c | `0x7D6330` | IA → acciones |
| **`NET_AI_UPDATE`** | x | `0x775120` | `FUN_007dfd70()` + `[0x17073FC].FUN_007dd970()` |
| **`NETZ_DISPATCH`** | x | `0x775140` | **`[g_root+0x04]->FUN_007dde20()`** |
| `HAVOK_UPDATE_WORLD` | c | `0x775180` | física |
| `CALCULATE_POVS` | x | `0x7752C0` | `[0x1766A00]->CalculatePOVs()` + `FUN_0074c870()` |
| `RENDER_MAIN` | c (manual) | — | render |
| `MAIN_FRAME_END` | c (manual) | `0x79F7C0` | fin de frame |
| `PAUSE_MODE_PROCESS` | c (manual) | `0x7905F0` | pausa |

**Hallazgo importante para el coop:** existe una **capa de red del engine ("NETZ")**:
- tiene una tarea de dispatch por frame;
- su objeto vive en `g_root+0x04`;
- hay una tarea `NET_AI_UPDATE` dedicada a la IA.

Junto con los nodos de script de multiplayer (hallazgo 01) y el array de jugadores (hallazgo 03), esto apunta a que Cathode tenía soporte de red, quizás de un modo descartado. Se investiga en un hallazgo propio (10) antes de diseñar la red del mod.

## Cámara del jugador

- **Gestor:** `[g_root+0x58]` (`g_root` = `[0x16F0C88]`). Se actualiza en `PLAYER_CAMERA_UPDATE` y `CAMERA_PRE_UPDATE` con el dt, que vale 0 si el juego está en pausa (`[DAT_016f194c+4] & 0x179FF`).
- **Cámara activa:** `[gestor+0x1DC]`:
  - `+0x2C`: float×4, **cuaternión (x, y, z, w)**. `FUN_004332e0(cam+0x2C)` calcula los términos de la matriz de rotación y devuelve un ángulo envuelto con 2π; el resultado se guarda en `gestor+0x208`.
  - `+0x3C`, `+0x40`, `+0x44`: float×3, **posición**. `Character::GetAimPosition` (hallazgo 03) la usa en lugar de la del personaje cuando el personaje está en un estado especial.
  - `+0x157`: byte, flag que `GetAimPosition` exige.
- `gestor+0x226`: byte; si está activo, se saltea el update (¿cámara bloqueada/cinemática?).
- Hipótesis: `+0x2C` (cuaternión) seguido de `+0x3C` (posición) es un transform `{quat, vec3}` contiguo.

## Timer del frame (`FUN_00773050`)

- Modo normal: `dt = FUN_00772fa0(...)`; `dt_escalado = dt × escala` (escala = `DAT_016410c8` si `DAT_016410d8`, si no la constante `DAT_011c6ea0`); el tiempo de juego acumula `dt_escalado`; el contador de frames sube 1.
- Modo de paso fijo (`*this == 0`): `dt = DAT_01be1c6c`.
- Responde la pregunta abierta del hallazgo 02 sobre el delta time.

## Matrices de vista/proyección

No se localizaron todavía.
- El renderer (`[0x1766A00]`) tiene un array de POVs en `+0x08` (cantidad: byte `+0x88`); cada POV tiene `+0x314` (tipo), `+0x36B` (flags) y `+0x320` (puntero).
- Para un overlay del "ghost" alcanzaría con armar la vista desde la cámara activa (posición + cuaternión) y la proyección desde el FOV.
- El FOV no está ubicado; hay entidades de script `GetCurrentCameraFov`/`CameraDofController`.

## Preguntas abiertas

- ¿Dónde están la matriz de vista/proyección y el FOV del POV principal?
- ¿Qué clase es `[gestor+0x1DC]` y quién la crea?
- Confirmar en runtime el layout `{quat +0x2C, pos +0x3C}`.
- NETZ: clase, `FUN_007dde20`, quién crea `g_root+0x04` y qué transporte usa (WS2_32/WSOCK32 por ordinal, hallazgo 01). → **hallazgo 10**.

## Matrices de vista/proyección: pistas (2026-11-05, sin cerrar)

- La `tan` del CRT no está identificada por Function ID: una búsqueda por nombre (`work/evidence/h04/search_tan.json`) no la encuentra.
- La constante **π/180** (float `0x3C8EFA35`, en `0x011C2A5C`) tiene 37 usos. Los de la zona del renderer son `FUN_009c0b40`, `FUN_009da910`, `FUN_0098f220`, `FUN_00988e30`, `FUN_0097aa00`, `FUN_00a1d2c0`, `FUN_00a1d8e0` y `FUN_00a3b290` (`work/evidence/h04/c8.txt`).
  - `FUN_009c0b40` (2.776 bytes) aplica parámetros de entidad a un objeto de render (flags en `+0x2C`); no es el armado de la proyección.
- **Propuesta:** resolverlo en runtime. Alcanza con leer el FOV y la matriz del POV principal (`[0x1766A00]+0x08` → POV 0) o la cámara activa (cuaternión + posición, ya ubicados) y armar la vista/proyección del lado del mod.
