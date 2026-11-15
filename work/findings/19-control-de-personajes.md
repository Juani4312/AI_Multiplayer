# 19 - Control de personajes: comandos, `READ_GAMEPAD_1` y `PLAYER_2`

- **Pregunta:** ¿cómo se mueve y se anima un personaje? Lo necesitamos para que el avatar del otro jugador camine animado (fase 4b), en vez de teletransportarse quieto, y para un Alien "títere" (etapa 3).
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA |
  |-----|----|
  | Inicialización del `Character` (crea la acción `+0x36C`, el equipo `+0x1100` y el controlador de jugador `+0x31C`; reconoce `"PLAYER_2"`) | `0x007CC400` |
  | Crea el controlador de jugador (0x4C bytes; `+0x48` activo) | `0x007B2630` → `0x0078FB90` |
  | `SwitchCommand`: `thiscall(controlador de comandos, comando)`, `ret 4` | `0x00678E20` |
  | Constructor base de los comandos `(this, tipo)` (20 callers, uno por tipo) | `0x00677A40` |
  | Comando `READ_GAMEPAD_1` (tipo 0x11): constructor | `0x0067FFB0` |
  | Lo crea y lo corre para los personajes con controlador de jugador activo (`ctrl+0x560`) | `0x00682320` |
  | `CMD_MoveTowards`, empezar: `FUN_0067e980(controlador, a2, &id[8], &destino 3x4, &parámetros, flag)` | `0x0067E980` |
  | `CMD_MoveTowards`, actualizar destino | `0x006797A0` |
  | Nombres de los tipos de comando (`char*[20]`) | `0x0163D010` |

- **Evidencia:** `work/evidence/h19/a1.txt` … `a21.txt` y `g1.txt` … `g14.txt` (Ghidra, `Dump.java`). Prólogos y la tabla de nombres leídos del archivo.
- **Confianza:** alta para las estructuras, los tipos y las llamadas (decompilación y asm). Baja para que un segundo personaje-jugador funcione: no se probó.
- **Cómo verificar en runtime:**
  - `AIMP_TRACE_CMD=1` (`mod/src/ctrl.c`): loguea cada `SwitchCommand` con el controlador, el tipo y el dueño (offset desde el `Character`);
  - `AIMP_TRACE_CTRL=1`: vuelca el controlador de jugador del jugador 0 cada vez que cambia. Con marcas F8 al caminar, correr, agacharse y girar, alcanza para mapear sus campos.
- **Verificado en runtime:** **parcial** (corrida 20, 2026-11-15, `work/evidence/runtime/run20-arrojables-control.log`; ver "Corrida 20" abajo). **Camino 1 verificado** en una máquina (corridas 26–31, `run26`…`run31`): el avatar camina y corre detrás del jugador con `MOVE_TOWARDS` en el controlador de scripts. Ver "Corridas 21–31" abajo.

## Cómo se controla un personaje

- Cada personaje lo maneja un **comando** (un objeto con el tipo en `+0x28`), que cambia con `SwitchCommand` (`"[SwitchCommand][TASK|SCRIPT] - <tipo>"`). Los 20 tipos de la tabla son:
  `NONE`, `IDLE`, `DIE`, `GOTO`, `GOTO_COVER`, `MOVE_TOWARDS`, `FOLLOW`, `PLAY_ANIMATION`, `SHOOT_AT`, `AIM_AT_CURRENT_TARGET`, `AIM_AT`, `LAUNCH_ATTACK`, `DEAD`, `PICKUP_WEAPON`, `MOVING_TO_JOB_TASK`, `SHOOT_FROM_COVER`, `SHOOT_TARGET`, **`READ_GAMEPAD_1`**, **`CLIENT_PLAYERS_LOGIC_CHARACTER_RUNNING_ON_THE_HOST`** y `TEMPLATE`.
  - `[TASK]` son los de la IA y `[SCRIPT]` los de las entidades `CMD_*` (`CMD_GoTo`, `CMD_MoveTowards`, `CMD_Follow`…), según el flag `controlador+0x0A & 4`.
  - Constructores por tipo (el argumento de `FUN_00677a40`): 1 `FUN_0067d880`/`dba0`, 2 `c730`/`cb00`, 3 `d000`/`d290`, 4 `d460`/`d6c0`, 5 `e660`/`e8a0`, 6 `cbe0`/`cf60`, 7 `df70`/`e280`, 8 `b520`/`b5e0`, 9 `e5b0`, 0xB `dea0`, **0x11 `FUN_0067ffb0`**, 0x14 `FUN_00679b90`. **El tipo 0x12 (`CLIENT_PLAYERS_LOGIC…`) no tiene constructor:** quedó afuera de retail.
- **El jugador se mueve con un comando que lee el mando.** `FUN_00682320(ctrl)`: si el personaje (`ctrl+0x34`) tiene controlador de jugador activo (`Character+0x31C`, `+0x48`), crea una vez el comando `READ_GAMEPAD_1` en `ctrl+0x560` y lo ejecuta (`vt+0x10`).
- **El controlador de jugador** (`Character+0x31C`) es un struct de estado de 0x4C bytes (flags y lo que parecen ejes y botones). Lo crea la inicialización del personaje para toda definición de jugador (`THE_PLAYER`: `def+0x1C` y `+0x1F`). No guarda un índice de mando.
- **Un segundo jugador está previsto.** En `FUN_007cc400`, si el nombre del personaje empieza con `"PLAYER_2"`, `+0x2E4`, `+0x310` y `+0x314` quedan en 1; si no, se toman del pedido (`param_2+8`). **Corrección (corrida 20):** el jugador 0 también tiene 1 en `+0x2E4`. No es un índice de jugador sino la **facción**: `FUN_007c4780` la cruza con una máscara de relaciones en `[g_root+0x34]`. `PLAYER_2` queda forzado a la facción del jugador.
- **Acción principal** (`Character+0x36C`, `FUN_0064f6e0`, 0x140 bytes): la animación en curso. `+0x0C` y `+0x10` son bloques `[largo][datos]` y `+0x19` el id de la acción. Hay 12 acciones secundarias en `+0x370`. Es lo que codificaba el replicador del hallazgo 18.

## Dos caminos para animar al avatar

1. **Comandos de NPC.** Dar al avatar (un NPC) un `MOVE_TOWARDS` hacia la posición del otro (`FUN_0067e980`) y actualizarle el destino (`FUN_006797a0`). Lo movería y lo animaría su locomoción de NPC.
   - **Falta** el controlador de comandos del personaje (el `this`). `AIMP_TRACE_CMD` lo muestra en runtime.
   - Puede necesitar que el avatar tenga autoridad o IA; hoy está en NOAUTH.
2. **Un segundo personaje-jugador (`PLAYER_2`).** Spawnear un personaje con la definición `THE_PLAYER` y nombre `PLAYER_2`. Tendría su propio controlador de jugador y el índice 1. Cada frame se le escribe en el controlador la entrada que manda el otro jugador (el mismo struct de 0x4C bytes, capturado allá). La lógica de jugador del engine (`READ_GAMEPAD_1`, `ACT_PLAYER_LOCO`) lo movería con las animaciones de un jugador. La posición se corrige de vez en cuando.
   - **Riesgos:**
     - el spawn de un personaje nuevo se cayó en las corridas 6 y 8 por los recursos de la clave nueva (hallazgo 12);
     - el engine podría tratar a un segundo jugador como local: cámara, HUD, entrada;
     - no se sabe si el input del juego escribe solo el controlador del jugador 0.

## Corrida 20 (2026-11-15): lo que mostraron las trazas

- **Controlador de comandos = `Character+0x3820`**, con `ctrl+0x34` = el `Character`. Se vio en los 4 NPCs del nivel:
  - los `Character` están en un arreglo, cada 0x42D0 bytes;
  - los `SwitchCommand` vistos fueron `[TASK]` `PLAY_ANIMATION` (desde `0x27E407`) y `GOTO_COVER` (`0x27D81D`);
  - con esto ya se puede llamar a `FUN_0067e980` (`MOVE_TOWARDS`) sobre un NPC.
- **Controlador de jugador (`+0x31C`)**: no guarda la entrada cruda.
  - El bit 0 del byte 0 vale 1 cuando el jugador se mueve (`0x81`) y 0 quieto (`0x80`);
  - `+4` es un float con el rumbo, que converge al yaw (3,04; 2,21; 1,59 rad, igual que las marcas);
  - el resto son ceros y punteros.
- **Acción principal (`+0x36C`)**: el id (`+0x19`) sube en cada cambio de estado (quieto ↔ moverse). El bloque B (continuo) tiene dos formas:
  - quieto: 4 bytes, `04 cd 00 <rumbo>`;
  - moviéndose: 10 bytes, `0a 01 <vel. actual u16> <vel. objetivo u16> <rumbo objetivo u8> <rumbo actual u8> <u16>`. Las velocidades están en mm/s. El objetivo vale `0x0999` = 2,46 m/s caminando, `0x1133` = 4,40 m/s corriendo y `0x0533` = 1,33 m/s agachado. El último u16 es 0 en línea recta y 0x19–0x35 al girar corriendo;
  - **rumbo en u8 = yaw × 200 / 2π** (200 pasos por vuelta): 3,04 da 97, 2,215 da 70 y 1,586 da 50;
  - es el contenido de `update_action_continuous_block` (hallazgo 18): lo mínimo para reproducir la locomoción de otro.

## Corridas 21–31 (2026-11-15): el avatar camina con `MOVE_TOWARDS` (camino 1)

- **Dos controladores de comandos** consecutivos en el `Character`, de `0x2B0` bytes cada uno (`0x10` + 2 slots de `0x150`; `FUN_0067e980` construye el comando en `ctrl+0x10` o `ctrl+0x160`, cancela el pendiente con `vt+0x24` y llama a `SwitchCommand`). En los dos, `+0x00` = comando en ejecución, `+0x04` = pendiente y `+0x34` = el `Character`:
  - **`Character+0x3570` = de scripts** (`[SCRIPT]`). `ConditionHasScript` (`FUN_006eaa80`) mira sus `+0`/`+4`; `ActionScript` (`FUN_006d2830`) lo habilita al arrancar (`FUN_00679150(1)`) y ejecuta su comando. Las entidades de comandos de los scripts (`FUN_007b58b0`, `FUN_007cec20`, `FUN_007cf2e0`…) usan `LEA ECX,[X+0x3570]`.
  - **`Character+0x3820` = de tareas** (`[TASK]`): ahí la IA pone `PLAY_ANIMATION`, `GOTO_COVER`, `IDLE`. Un `MOVE_TOWARDS` ahí lo pisa la IA o no se ejecuta (corridas 21–26).
- **Árbol de comportamiento del civil** (`DATA/BEHAVIOR/NPC_INNOCENT_BEHAVE.XML`): los comandos de script solo se ejecutan en `ConditionHasMotivation(CINEMATIC_MOTIVATION) → ConditionHasScript → ActionScript`. El mod hookea `ConditionHasMotivation` (`FUN_006d3630`, thiscall(nodo, agente, x), **`ret 8`**; `nodo+0x14` = tipo, CINEMATIC = 1) y devuelve verdadero para el avatar. El agente es el `Character` (corrida 26).
- **Actualizar el destino alcanza** en el controlador de scripts (corrida 28): `FUN_006797a0` = thiscall(ctrl, a, b, destino, params, flag), **`ret 0x14`**; busca el comando por sus ids (`FUN_00677990`), `vt+0x64` solo es no nulo en `MOVE_TOWARDS` (`FUN_004a4610`) y `FUN_006776f0` escribe el destino en `cmd+0x60..+0x68` y los parámetros en `cmd+0x80..+0x9C`.
- **Enum `MOVE`** (tabla de nombres de enums en VA `0x0130FD18`): `SLOW_WALK`, `WALK`, `FAST_WALK`, `RUN`, `OBSOLETE_1`, `OBSOLETE_2`, `TELEPORT`. `params+0x00` = `WALK` (1) camina a ~1,2 m/s y `RUN` (3) corre a ~4,8 m/s (corridas 27–31).
- **Teletransporte:** sin orden nueva en el mismo frame, la locomoción lo devuelve (corrida 22). Si su IA tiene un `PLAY_ANIMATION` en el controlador de tareas, cae del piso (corridas 26–29). Por eso: primero la orden; teletransporte y orden nueva cuando ya no hay animación.
- **Nunca `SwitchCommand(ctrl, NULL)`** desde el mod: se cae leyendo `cmd+0x28` (corrida 23).
- **Comando `MOVE_TOWARDS`** (constructor `FUN_0067e660`): ids en `cmd+0x34`/`+0x3C`, tipo en `+0x28`, destino (solo la traslación) en `+0x50..+0x58`, parámetros en `+0x80` (0x1C bytes, `move_type` primero) y `never_succeed` en `+0x9C`; la actualización escribe el destino en `+0x60..+0x68`. **No guarda la rotación del destino**: para orientar al avatar quieto, el mod lo teletransporta en su lugar con el yaw del otro y le da una orden nueva en el mismo frame (corrida 34: el yaw queda).
- Resultado (corrida 31): sigue al jugador a ~2 m, sin trabarse ni pegársele; corre si queda a más de 5 m. Por red, en una máquina con `aimp_peer.py --replay` (corridas 32–34): sigue el recorrido del otro y, quieto, mira hacia donde mira el otro. Capturas en `work/evidence/runtime/shots/` (locales).

## Parámetros de `MOVE_TOWARDS` (2026-11-15, estático)

La entidad de script `CMD_MoveTowards` arma los 0x1C bytes de parámetros en `FUN_0052d600` (llamada desde `FUN_0054b540`/`FUN_0054b5d0`). Cada campo sale de un getter (`FUN_008e23f0`..`FUN_008e2660`) que busca el parámetro por el ShortGuid de su nombre. Los ShortGuid son globals en `0x01750950..0x01750988`, inicializados en `0x011586xx..0x011587xx` con `push "<nombre>"; mov ecx,<global>; call FUN_008bf7f0`. Los bool valen 0 por defecto (`mov byte [ebp-1],0` en cada getter) y `arrived_radius`, 0,6 (`DAT_011c6824`). El `FUN_0067e980` que usa el mod pone 0,1 (`DAT_011bb6ac`).

| Offset | Campo | Offset | Campo |
|---|---|---|---|
| `+0x00` | `move_type` (u32, enum `MOVE`) | `+0x0A` | `DestinationIsBackstage` |
| `+0x04` | `disallow_traversal` | `+0x0B` | `allow_to_use_vents` |
| `+0x05` | `enable_lookaround` | `+0x0C` | `always_stop_at_radius` |
| `+0x06` | `use_stopping_anim` | `+0x0D` | `stop_at_radius_if_lined_up` |
| `+0x07` | `continue_from_previous_move` | `+0x0E` | `maintain_current_facing` |
| `+0x08` | `should_be_aiming` | `+0x0F` | `start_instantly` |
| `+0x09` | `use_current_target_as_aim` | `+0x10` | `AimTarget` (8 bytes, referencia a entidad) |
| | | `+0x18` | `arrived_radius` (float, m) |

En el mod: `AIMP_GHOST_WALK_FLAGS` (bit i = byte `+0x04+i`; 2048 = `start_instantly`, 8 = `continue_from_previous_move`) y `AIMP_GHOST_WALK_RADIUS_CM` (10). `AIMP_GHOST_WALK_REARM_CM` le da una orden nueva si está quieto 1 s a más de esa distancia del objetivo (como mucho cada 2 s). **Corrida 35:** con `start_instantly` (2048) y la re-orden a 100 cm, el avatar siguió 5 pasos cortos de 1,1–1,6 m (3 arrancó solo y 2 con la re-orden), con ≤1 s de retardo; desde entonces son los valores por defecto. Falta una corrida de control sin el flag.

Evidencia: `FUN_0052d600` decompilado y `objdump` de los getters y de los inicializadores (comandos en `work/NOTES.md`, 2026-11-15).

## Preguntas abiertas

1. ~~¿Dónde vive el controlador de comandos?~~ En `Character+0x3820` (corrida 20); el de scripts, en `Character+0x3570` (corrida 26).
2. Layout del controlador de jugador: bit de movimiento y rumbo (corrida 20). La entrada cruda está en otro lado: ¿dónde? ¿Quién la escribe para el jugador 0?
3. ¿Qué hace el resto de la vtable de `READ_GAMEPAD_1` (`0x012B19F0`: `+0x50` `FUN_00682830`, `+0x58` `FUN_00680990`, `+0x60` `FUN_00682b50`, `+0x68` `FUN_006816a0`, `+0x78` `FUN_00682560`, `+0x80` `FUN_00682b10`)?
4. ¿Cómo se spawnea un personaje con la definición del jugador sin el crash de recursos (reutilizar los recursos del jugador 0)?
5. ~~¿Qué controla el `+0x18` de los parámetros de `MOVE_TOWARDS`?~~ Es `arrived_radius` (ver "Parámetros de `MOVE_TOWARDS`"). Sigue abierto por qué con el objetivo a menos de ~2,8 m el avatar no arranca (corrida 31). Con `start_instantly` arranca con 1–1,6 m (corrida 35; falta el control).
6. ~~¿Se puede hacer que el avatar mire hacia donde mira el otro cuando está quieto?~~ Sí, con teletransporte en el lugar más una orden nueva (corrida 34).
