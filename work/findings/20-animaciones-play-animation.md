# 20 - Animaciones: el comando `PLAY_ANIMATION` y el agachado del avatar

- **Pregunta:** ¿cómo se le hace hacer una animación concreta a un NPC? Hace falta para que el avatar (un civil) se vea agachado cuando el otro jugador se agacha. El civil no tiene ciclo de caminar agachado, y las banderas de agachado (`FUN_007cae20`) y el `IDLE` con postura no lo agachan (corridas 53, 65–67 y 78).
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA |
  |-----|----|
  | `PLAY_ANIMATION`, empezar: `thiscall(controlador, a[8]*, b[8]*, start_instantly, params[0x58]*)`, `ret 0x10` | `0x0067E3B0` |
  | Constructor del comando (tipo 7): copia los parámetros a `cmd+0x50` y `start_instantly` a `cmd+0xAC` bit 0 | `0x0067DF70` |
  | `vt+0x48` del comando ("¿puede arrancar ya?"): verdadero si `cmd+0xAC` bit 0 | `0x0067E250` |
  | Entidad `CMD_PlayAnimation`, al dispararse | `0x00530E60` |
  | Arma los parámetros desde la entidad (`AnimationSet` y `Animation` en FNV-1a 32) | `0x0052FA10` |
  | Getters de los parámetros: `0x008E1B80 + 0x30*i` lee el parámetro i (ShortGuids en `0x0175087C + 4*i`, desde `AnimationSet`) | `0x008E1B80`… |
  | Tick del controlador de comandos: promueve el pendiente si su `vt+0x48` lo deja y actualiza el que corre, `thiscall(ctrl, char)`, `ret 4` | `0x00679150` |
  | Cambio de comando efectivo (`[CommandSwitched]`) | `0x00679000` |
  | Tabla de textos del juego: hash → texto (`cdecl`, `"BAD_STRING_ENTRY"` si no está) | `0x00A35B90` |

- **Evidencia:** `work/evidence/h20/p1.txt` … `p11.txt` (Ghidra, `Dump.java`; local), `objdump` de los getters y del constructor (comandos en NOTES, 2026-11-17). Runtime: `work/evidence/runtime/run83…run89*.log` y capturas `shots/run87-pose-arrodillado.png`, `run88-pose-cower.png`, `run89-pose-idle-crouch.png` (locales).
- **Confianza:** alta para la llamada, el layout de los campos usados y el tick (verificados en runtime). Media para los bits de flags que no se usan.
- **Verificado en runtime:** sí (corridas 83–89, una máquina, con `aimp_peer.py --replay`).

## Layout de los parámetros (0x58 bytes)

| Offset | Campo (default del getter) |
|---|---|
| `+0x00..+0x1F` | cuatro referencias a entidades de 8 bytes (marcadores; 0 = ninguna) |
| `+0x20` | float (de una referencia) |
| `+0x24` | `AnimationSet`: FNV-1a 32 del texto |
| `+0x28` | `Animation`: FNV-1a 32 del texto |
| `+0x2C` / `+0x30` | `StartFrame` / `EndFrame` (−1) |
| `+0x34` | `PlayCount` (1) |
| `+0x38` | `PlaySpeed` (1,0) |
| `+0x3C` | `ConvergenceTime` (1,0) |
| `+0x40` | `BlendInTime` (0,3) |
| `+0x44` | `ExertionFactor` (0) |
| `+0x48` | float (−1,0, `DAT_011bb280`) |
| `+0x4C` | `ExitConvergenceTime` (1,0) |
| `+0x54` bits | 3 `AllowGravity` (1), 4 `AllowCollision` (1), 5 `AllowInterruption`, 6 `RemoveMotion`, 7 `LocationConvergence` |
| `+0x55` bits | 0 `OrientationConvergence`, 1 `FullCinematic`, 2 `RagdollEnabled`, 3 `NoIK`, 4 `NoFootIK`, 5 `NoLayers`, 6 `PlayerAnimDrivenView`, 7 `GaitSyncStart` |
| `+0x56` bits | 1 `Mirror`, 3 lo pone `FUN_00579b50` al validar el set, 4 `UseExitConvergence`, 6 `InitiallyBackstage`, 7 `IsCrouchedAnim`. En la IA vale siempre al menos `0x0D` |
| `+0x57` bits | 0 `Death_by_ragdoll_only`, 1 `DisableGunLayer` |

El hash es el de `FUN_0052fa10`: `h = 0x811C9DC5; h = (h ^ (int)(signed char)c) * 0x01000193`, y 0 para el texto vacío.

## Qué animaciones hay y cuáles usa la IA

- **Set de los humanos: `"HUMAN"`** (`0x1B92511A`; la tabla de textos del juego lo nombra). Las tareas de los civiles (composites `AYZ\ANIMATION\TASKS\...` con `CMD_PlayAnimation`) usan ese set: `jb_kn_enter`, `jb_kn_idlebase01`, `jb_scav_general_1m_*`, `jb_search_l_and_r_*` (corrida 83; los nombres salen de comparar el hash con los textos de `GLOBAL/ANIMATION.PAK`).
- Los humanos **no tienen ciclo de caminar agachado** (`HUMAN\BASE\MALE\LOCOMOTION\CROUCH\DEFAULT` solo trae transiciones). Sí hay poses quietas, y las tres probadas andan con el set `HUMAN` aunque no se estén usando en la zona:
  - `idle_crouch` (`HUMAN\IDLE_CROUCH_1`): agachado bajo, con una mano en el piso. **La que queda por defecto.**
  - `cower_low_idle`: acurrucado, cubriéndose la cabeza.
  - `jb_kn_idlebase01`: arrodillado sobre una rodilla.

## El comando queda pendiente si nadie hace el tick

- `SwitchCommand` (`FUN_00678e20`) solo deja el comando en `ctrl+4` (pendiente), pone `ctrl+8 = 1` y le pide al que corre que termine (`exec+0x2C = 1`).
- El paso a ejecución lo hace el tick `FUN_00679150`: si `pendiente->vt+0x48(personaje)` es verdadero, marca el que corre como terminado (`+0x2C = 4`) y llama a `FUN_00679000`, que cambia `ctrl+0 = pendiente` y arranca el nuevo (`vt+0x38`, `vt+0x1C`).
- Al tick lo llama `ActionScript` desde el árbol de comportamiento. **Con el avatar títere el árbol deja de correr** a los ~2 s (el hook de motivación cinemática deja de llamarse; corridas 85–86). Por eso el `PLAY_ANIMATION` quedaba pendiente para siempre, aunque la animación estuviera cargada.
- **Solución:** el mod llama al tick (`FUN_00679150(ctrl, 1)`) justo después de dar la orden (`AIMP_GHOST_CROUCH_TICK=1`, por defecto). Corrida 87: el comando pasa a ejecución enseguida y el avatar se arrodilla.

## En el mod

`mod/src/anim.c`:
- `anim_play` arma los parámetros y llama a `FUN_0067e3b0`;
- `anim_ctrl_tick` hace el tick;
- `anim_hash` y `anim_name` traducen hashes y nombres;
- `AIMP_TRACE_ANIM=1` traza cada `PLAY_ANIMATION`.

En `ghost.c`, `AIMP_GHOST_CROUCH=4` (por defecto):
- cuando el otro se agacha, el títere recibe la pose y se sigue moviendo en ella (se desliza);
- cuando el otro se para, vuelve el `MOVE_TOWARDS`.

Opciones:
- `AIMP_GHOST_CROUCH_ANIM`: una animación o varias separadas por coma, que se alternan;
- `AIMP_GHOST_CROUCH_SET`;
- `AIMP_GHOST_CROUCH_FLAGS`;
- `AIMP_GHOST_CROUCH_BLEND_MS`;
- `AIMP_GHOST_CROUCH_DROP_CM`: baja el modelo, 0 por defecto.

## Preguntas abiertas

1. ¿Por qué deja de correr el árbol del avatar? Probablemente porque `ActionScript` termina con el `MOVE_TOWARDS` que nunca acaba. No molesta mientras el mod haga el tick.
2. ¿Sirve `PLAY_ANIMATION` para otras cosas del avatar, como mirar, usar o morir? Con la tabla de textos y la traza se pueden sacar los nombres de lo que usa la IA.
