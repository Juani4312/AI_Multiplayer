# 12 - Avatar del jugador remoto (fase 4): el pedido de spawn y el teleport

- **Pregunta:** ¿cómo se crea, desde el mod, un personaje que represente al otro jugador? ¿Cómo se lo mueve?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Spawn de autoridad `thiscall(this, pedido*, flag)`, `ret 8` | `0x00797490` | `0x397490` |
  | Encolar spawn desde script, sin bloques (`ret 0xC`) / con dos bloques de 24 bytes (`ret 0x14`) | `0x007A7A20` / `0x007A7A80` | `0x3A7A20` / `0x3A7A80` |
  | Inicializar pedido `thiscall(pedido, a1, a2, params*)`, `ret 0xC` | `0x00787450` | `0x387450` |
  | Copiar los primeros 0xE4 bytes de `params` al pedido | `0x00777010` | `0x377010` |
  | Observadores antes / después del spawn (`vf[1](pedido, 1/0)` de dos listas) | `0x00789F30` / `0x00789F60` | |
  | Mapa clave → bloque de 0x1D0 bytes (busca o inserta) | `0x007B1760` | `0x3B1760` |
  | Teleport `thiscall(pj, pos*, yaw, p3)`, `ret 0xC`: avisa a lo enganchado en `+0x1160/+0x1164` y llama a `SetTransform(pj, pos, yaw, p3, 0)` | `0x007C6330` | `0x3C6330` |
  | `SetTransform(pos*, yaw, p4, bool p5)`, `ret 0x10` (hallazgo 03) | `0x007C2710` | `0x3C2710` |
- **Evidencia:** `./work/evidence/h12/f1.txt` (`FUN_00777010` y el global `0x174E254`), `f2.txt` (observadores, `FUN_00795bc0`, `FUN_007b1760`), `f3.txt` (`SetTransform` completo); asm con `objdump -d -M intel --start-address=…` (en este hallazgo); payloads de 0x29 en `work/evidence/runtime/run3-interacciones.log` (líneas `TR payload A 0x0029`); prólogos leídos del archivo con un parser PE.
- **Confianza:**
  - alta: el flujo del spawn de autoridad, el layout de la cola del pedido (`+0xF0…+0x12F`) y las firmas;
  - media: el significado de los campos del encabezado (`+0x00`, `+0x28`, `+0x30`), que sale de comparar 8 payloads reales;
  - baja: que clonar un pedido produzca un personaje usable. Es lo que mide el experimento.
- **Cómo verificar en runtime:** `AIMP_TRACE_SPAWN=1` loguea cada pedido entero, decodificado. `AIMP_GHOST=1` hace el experimento de abajo.
- **Verificado en runtime:** **sí, parcial** (corridas 6 a 8 del 2026-11-05, Proton; detalle en "Parte 2"). El avatar con un NPC existente funciona. Clonar un pedido hace caer el juego.

## El spawn de autoridad (`FUN_00797490`)

```c
void __thiscall SpawnAuth(this, pedido, flag) {           // ret 8; flag no se usa
    n = sesion->m_28;                                       // FUN_00a2a490
    if (n < 1 && IsClient()) return;                       // en retail: nunca
    Observadores(DAT_016f322c, pedido, 1);                  // FUN_00789f30
    this->vf[1](pedido);                                    // SpawnCharacter (hallazgo 05)
    Observadores(DAT_016f3230, pedido, 0);                  // FUN_00789f60
    copia = alloca(0x130); memcpy(copia, pedido, 0x130);
    if (!sesion->flag_2D && n >= 1) EncolarA(DAT_016361e4 /* 0x29 */, copia, 0x130);
    else if (callback) callback(ctx, copia);
}
```

Prólogo: `55 8B EC 53 56 57 8B F9` (6 bytes sin direccionamiento relativo antes del `E8`): se puede hookear con 6 bytes robados.

## El pedido de 0x130 bytes

`FUN_007a7a20`/`a80` lo arman en el stack con `FUN_00787450(pedido, a1, params->+0x14, params)`, que:
1. copia de `params` los primeros 0xE4 bytes (`FUN_00777010`): un encabezado de 0x64 bytes y dos strings de 0x40;
2. completa la cola: `+0xF0` = a1, `+0xF4` = 0 (flag "trae bloques"), `+0xF8` = 0, `+0xFC` = `[0x174E254]`, `+0x100…+0x114` = `[0x174E254]` (6 dwords), `+0x118…+0x12C` = −1 (6 dwords).

`FUN_007a7a80` además copia dos bloques de 24 bytes de sus argumentos a `+0x100` y `+0x118`, pone `+0xF4 = 1` y `+0xF8` = su arg 2. `FUN_007a7a20` pone `+0xFC` = su arg 2. Por los valores por defecto (un global y −1), los bloques parecen ids o referencias, no transforms. `[0x174E254]` es 0 en el archivo y se inicializa en runtime.

| Offset | Contenido | Fuente |
|--------|-----------|--------|
| `+0x00` | **clave** (1…8 en los 8 spawns del nivel). `FUN_007b1760` la mapea a un bloque de 0x1D0 bytes; una clave nueva toma un bloque nuevo | payloads; hallazgo 05 |
| `+0x04…+0x0C` | 0, o valores tipo id (`639b3722`…) | payloads |
| `+0x10` | 1 en todos | payloads |
| `+0x14`, `+0x18`, `+0x20`, `+0x24` | hashes (ShortGuids). `+0x18` es igual en todos (`82fc64f8`) | payloads |
| `+0x1C` | 0 en la copia que se encola. `SpawnCharacter` la usa como def: probablemente se resuelve o se borra en el camino | payloads; hallazgo 05 |
| **`+0x28…+0x2F`** | **id de 8 bytes**. En el del jugador: `2ff722b2 b89a5172`, el formato del id de los RPCs (hallazgo 11). Los NPCs comparten el primer dword (`1c3460b3`). `FUN_00795bc0` recibe el primero | payloads |
| **`+0x30`** | **tipo de personaje**: 0 el jugador, 3 o 6 los NPCs (coincide con `def+0x04`, hallazgo 05) | payloads |
| `+0x34`, `+0x38` | enteros chicos (subtipo, índice) | payloads |
| `+0x48`, `+0x4C` | id y flag "replicado" (hallazgo 05) | hallazgo 05 |
| `+0x64` | nombre del personaje (char[0x40]) | `FUN_00777010`; spawner de script |
| `+0xA4` | composite o `"CUTSCENE"` (char[0x40]) | ídem |
| `+0xF0…+0x12F` | cola (arriba) | `FUN_00787450` |

En los 64 bytes capturados no hay floats de posición: el lugar sale del spawner del script (referenciado por los ShortGuids) o de los campos que todavía no se vieron.

## El teleport

`FUN_007c6330` es el envoltorio que usa el engine: si el personaje tiene algo enganchado (pila en `+0x1160/+0x1164`), le pasa la nueva posición, y después llama a `SetTransform(pj, pos, yaw, p3, 0)`. `SetTransform` normaliza el yaw, pone en 0 `+0x184/+0x1A0`, copia la posición y, para personajes de tipo 1, la proyecta a la navmesh. Prólogo verificado: `53 8B 5C 24 08 55 8B 6C 24 14 57 8B F9`.

## Experimento de la fase 4 (`AIMP_GHOST=1`, `mod/src/ghost.c`)

1. El hook en `FUN_00797490` guarda `this` y copia cada pedido (anillo de 32), y loguea los primeros 24 con volcado completo (`SP pedido N: …`).
2. `AIMP_GHOST_DELAY_MS` (8000) después de que aparece el jugador local, clona el último pedido de tipo 0 (o el de clave `AIMP_GHOST_FROM`). Le pone clave `0x40+n` y, en el segundo dword del id, `"AIMP"+n`. Llama a la función original con el mismo `this` (`GHOST: clonando…`).
3. Cuando aparece un personaje nuevo en los vectores del `CharacterManager`, loguea dónde quedó (jugadores o NPCs), su def y su controlador, y pone `controlador+0x48 = 0` para que no lea el input local.
4. En cada frame lo teletransporta con `FUN_007c6330`: a la posición del otro jugador si hay red y están en el mismo nivel, o a 1,5 m en x del jugador local. Solo lo hace si el objetivo se movió o pasaron 500 ms.
5. Si cambia el jugador local (nivel nuevo), vuelve a empezar. Como mucho 4 clones por sesión.

`test_ghost` prueba todo eso contra stubs con los prólogos y el `ret N` reales, incluido que el stack quede balanceado.

## Qué mirar en la primera corrida

- Los pedidos completos: nombres (`+0x64`/`+0xA4`), `+0x1C`, `+0x48`/`+0x4C` y la cola. ¿Dónde está la posición?
- `this` y su `vf[1]`: ¿es `SpawnCharacter` (RVA `0x3D3B50`)?
- Si el clon aparece: ¿en jugadores o en NPCs? ¿Se ve? (el modelo del jugador puede ser solo de primera persona) ¿La cámara o el input se van al clon?
- Si no aparece o el juego se cae: la última línea `GHOST:` del log dice en qué paso.
- Plan B, si el clon del jugador no sirve: clonar un NPC humano (`AIMP_GHOST_FROM=<clave>`, con la clave que dé el log).

---

# Parte 2 - Lo que mostraron las corridas 6 a 8

Evidencia: `work/evidence/runtime/run6-ghost.log`, `run7-ghost-npc.log`, `run8-ghost-npc-ok.log` (gitignored); `work/evidence/h12/f4.txt`–`f8.txt`; resumen en `work/NOTES.md` (corridas 6, 7 y 8).

## Correcciones a la parte 1

| Qué | Antes | En runtime |
|-----|-------|------------|
| `+0xA4` | composite o `"CUTSCENE"` | **nombre de la definición** del personaje: `THE_PLAYER`, `INNOCENT`, `ALIEN` (`CHR_INFO/ATTRIBUTES`). `+0x64` (nombre) viene vacío |
| posición | "no hay floats en los primeros 64 bytes" | **`+0x50…+0x5B` = posición** y **`+0x60` = yaw**. Jugador: (4,55, 13,62, 33,59), −π: el punto de guardado |
| `+0xF0` | — | repite la clave en el pedido de autoridad (0x130 bytes) |
| tipos | 0 jugador; 3/6 NPCs | 0 `THE_PLAYER`, 1 `ALIEN`, 6 `INNOCENT` (el 3 apareció en otro nivel) |
| `this` del spawn de autoridad | interfaz de spawn del `CharacterManager` | **`CM+0x10`** (vtable `0xEF6900`): su `vf[1]` es `ret 4` y su `vf[2]` es `mov al,1; ret 4` |

## Cómo se crea un personaje de verdad

```
script spawner (FUN_00490e30)
  └─ FUN_007a7a20/a80 → FUN_00797490(CM+0x10, pedido 0x130)   // vf[1] vacío: solo replica
        └─ encola A 0x29 → handler FUN_00414d10                 // observadores vacíos, vf[2] vacío
              └─ encola B 0x2C (0x130)
… (B → sesión, sin mapear)
sesión tipo 0x2E (0xF0 bytes) → FUN_00414a70(ctx = CM+0x04)
  └─ copia el mensaje a un buffer alineado a 16
  └─ ctx->vf[1](copia) = SpawnCharacter (0x7D3B50)              // crea el personaje
        └─ RequestCharacter (0x7D0470) → … → FinishSpawning (por frame)
```

- Las cuatro listas de observadores (`0x16F322C`/`30`, `0x16A6728`/`2C`) están **vacías** en runtime y nadie las escribe en el binario.
- `SpawnCharacter` recibe un mensaje de **0xF0 bytes**, con las mismas claves, ids, tipos y posiciones que los pedidos de autoridad. Los primeros 0xE4 bytes no son idénticos.

## Teleport: el vector tiene que estar alineado a 16

`SetTransform` copia la posición con `MOVAPS` (`FUN_00401410`). Con un `float[3]` sin alinear el juego se cae, y Windows lo reporta como acceso en `0xffffffff` (#GP; corrida 7). El mod usa un `float[4]` estático alineado a 16 que conserva el cuarto componente del personaje (`+0x7C`). **Regla general: todo vector que se le pase al engine, alineado a 16.**

## Resultados

| Corrida | Qué se probó | Resultado |
|---------|--------------|-----------|
| 6 | clon del pedido del jugador por el spawn de autoridad | el juego se cae en el frame siguiente (todavía sin registro de excepciones) |
| 7 | NPC existente + teleport | crash en el primer teleport: `MOVAPS` desalineado (error del mod) |
| 8 | NPC existente + teleport alineado | **OK 20 s**: el INNOCENT "CV5" sigue al jugador a 1,5 m, sin errores; el usuario lo vio |
| 8 | clon por `SpawnCharacter` (clave 64) | crash en `RequestCharacter` → `FUN_006ad740`: el recurso que se busca por la clave nueva es NULL (`FUN_00a2bc70` sin chequeo) |
| 9 | **avatar por red**, las dos máquinas con `AIMP_GHOST=3` (laptop Windows en join, papito Proton en host) | **OK en los dos sentidos**: en cada máquina un INNOCENT siguió al jugador de la otra (media 0,18 m del objetivo, 0 perdidos, 0 excepciones). El usuario: "nos vemos mutuamente" |

## Decisión (fase 4 v1)

El avatar del otro jugador es **un NPC humano del nivel** (`AIMP_GHOST=3`). Se lo teletransporta en cada frame a la posición y el yaw recibidos por red, sin límite de tiempo, y si desaparece se elige otro.

Pendiente:
- ~~prueba por red con la laptop~~: hecha (corrida 9);
- después, que no se note la IA del NPC (animación, que no se escape ni ataque);
- crear un personaje nuevo de verdad: reutilizar la clave de uno liberado, o preparar sus recursos.
