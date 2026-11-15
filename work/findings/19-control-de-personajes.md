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
- **Verificado en runtime:** no.

## Cómo se controla un personaje

- Cada personaje lo maneja un **comando** (un objeto con el tipo en `+0x28`), que cambia con `SwitchCommand` (`"[SwitchCommand][TASK|SCRIPT] - <tipo>"`). Los 20 tipos de la tabla son:
  `NONE`, `IDLE`, `DIE`, `GOTO`, `GOTO_COVER`, `MOVE_TOWARDS`, `FOLLOW`, `PLAY_ANIMATION`, `SHOOT_AT`, `AIM_AT_CURRENT_TARGET`, `AIM_AT`, `LAUNCH_ATTACK`, `DEAD`, `PICKUP_WEAPON`, `MOVING_TO_JOB_TASK`, `SHOOT_FROM_COVER`, `SHOOT_TARGET`, **`READ_GAMEPAD_1`**, **`CLIENT_PLAYERS_LOGIC_CHARACTER_RUNNING_ON_THE_HOST`** y `TEMPLATE`.
  - `[TASK]` son los de la IA y `[SCRIPT]` los de las entidades `CMD_*` (`CMD_GoTo`, `CMD_MoveTowards`, `CMD_Follow`…), según el flag `controlador+0x0A & 4`.
  - Constructores por tipo (el argumento de `FUN_00677a40`): 1 `FUN_0067d880`/`dba0`, 2 `c730`/`cb00`, 3 `d000`/`d290`, 4 `d460`/`d6c0`, 5 `e660`/`e8a0`, 6 `cbe0`/`cf60`, 7 `df70`/`e280`, 8 `b520`/`b5e0`, 9 `e5b0`, 0xB `dea0`, **0x11 `FUN_0067ffb0`**, 0x14 `FUN_00679b90`. **El tipo 0x12 (`CLIENT_PLAYERS_LOGIC…`) no tiene constructor:** quedó afuera de retail.
- **El jugador se mueve con un comando que lee el mando.** `FUN_00682320(ctrl)`: si el personaje (`ctrl+0x34`) tiene controlador de jugador activo (`Character+0x31C`, `+0x48`), crea una vez el comando `READ_GAMEPAD_1` en `ctrl+0x560` y lo ejecuta (`vt+0x10`).
- **El controlador de jugador** (`Character+0x31C`) es un struct de estado de 0x4C bytes (flags y lo que parecen ejes y botones). Lo crea la inicialización del personaje para toda definición de jugador (`THE_PLAYER`: `def+0x1C` y `+0x1F`). No guarda un índice de mando.
- **Un segundo jugador está previsto.** En `FUN_007cc400`, si el nombre del personaje empieza con `"PLAYER_2"`, el personaje queda con índice de jugador 1 (`+0x2E4`, `+0x310` y `+0x314`); si no, toma el índice de la definición (`param_2+8`). Hay máscaras por índice en `[g_root+0x34]` (`+0x2AE`).
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

## Preguntas abiertas

1. ¿Dónde vive el controlador de comandos respecto del `Character`? (`AIMP_TRACE_CMD`)
2. Layout del controlador de jugador de 0x4C bytes: ¿quién lo escribe cada frame? ¿Solo el del jugador 0? (`AIMP_TRACE_CTRL` y un hook de escritura)
3. ¿Qué hace el resto de la vtable de `READ_GAMEPAD_1` (`0x012B19F0`: `+0x50` `FUN_00682830`, `+0x58` `FUN_00680990`, `+0x60` `FUN_00682b50`, `+0x68` `FUN_006816a0`, `+0x78` `FUN_00682560`, `+0x80` `FUN_00682b10`)?
4. ¿Cómo se spawnea un personaje con la definición del jugador sin el crash de recursos (reutilizar los recursos del jugador 0)?
