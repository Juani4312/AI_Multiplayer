# 05 - Creación de personajes (y de un segundo jugador)

- **Pregunta:** ¿Cómo crea el engine un personaje y cómo llega al vector de jugadores? ¿Se puede tener un segundo jugador?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | `CharacterManager` = `[g_root+0x08]`, constructor (0x9FF0 bytes) | `0x007D9260` | `0x3D9260` |
  | vtable de interfaz en `CharacterManager+0x04` | `0x012F68E0` | `0xEF68E0` |
  | `SpawnCharacter(request*)` = slot 1 de esa interfaz (Ghidra **no** lo desensambló) | `0x007D3B50` | `0x3D3B50` |
  | `RequestCharacter(def, params)` (`"request_character"`), llamado desde `0x7D3C33` | `0x007D0470` | `0x3D0470` |
  | `FinishSpawning()` (`"finish_spawning"`), por frame | `0x007B80C0` | `0x3B80C0` |
  | Callback de la tarea `HAVOK_POST_CHARACTER_UPDATE` (llama a `FinishSpawning` y `update3`) | `0x007CD6E0` | `0x3CD6E0` |
  | `release_player` | `0x007999B0` | `0x3999B0` |
  | `release_npc` / `release_dead_npc` / `cleanup_all_deleted_characters` | `0x007997E0` / `0x00799B50` / `0x00799CE0` | |
  | Constructor del personaje derivado (vtable final `0x012F5D80`) | `0x007CEC20` | `0x3CEC20` |
  | Constructor base `Character` (vtable `0x012F4FC8`) | `0x007B2390` | `0x3B2390` |
- **Evidencia:** `./work/evidence/h05/e1.txt` … `e12.txt` (Dump.java). `e8.txt` tiene los nombres de la API, que salen de los `strncpy` al buffer de contexto `0x016F17F8`. La llamada no registrada en `0x7D3C33` se encontró escaneando `.text` (`E8 rel32`) y se desensambló con `objdump -d --start-address=0x7d3b40`.
- **Confianza:** alta para `FinishSpawning` y el criterio jugador/NPC, los vectores y los nombres de la API. Media para `SpawnCharacter` (código no analizado por Ghidra; se leyó solo el asm parcial).
- **Cómo verificar en runtime:** con el mod, leer `[[g_root+8]+0x84]` (jugadores = 1), `+0xD4` (NPCs) y `+0x174` (pendientes) mientras se juega; el Alien y los humanos tienen que aparecer en el vector de NPCs.
- **Verificado en runtime:** no.

## Flujo de creación

```
(script/entidad CHARACTER que spawnea)
   └─> [CharacterManager+0x04]->vf[1] = SpawnCharacter(request)        0x7D3B50
         ├─ busca datos por tipo (FUN_007b1760 → bloque de 0x1D0 bytes en +0x22C)
         ├─ NETZ: [this+0x28 (= gestor+0x2C)]->FUN_007ddc60(...)       ← pide algo a la capa de red
         └─> RequestCharacter(def, params)                              0x7D0470
               ├─ toma memoria de un pool circular (DAT_016f334c[...])
               ├─ construye el personaje (FUN_007cec20 → vtable 0x12F5D80)
               ├─ estado c+0x1122 = 3 (pendiente); pendientes[+0x170][+0x174++] = c
               ├─ nombre → c+0x10 ("Character: %d" si no tiene)
               └─ c->vf[+0x14]()   (init)
(cada frame, tarea HAVOK_POST_CHARACTER_UPDATE)
   └─> FinishSpawning()                                                 0x7B80C0
         para cada pendiente c:
           if (c->def(+0x64)->m_1C == 0) → NPC:     c+0x1122 = 1; npcs[+0xD0][+0xD4++] = c
           else                          → JUGADOR: c+0x1122 = 0; jugadores[+0x80][+0x84++] = c
           c->vf[+0x10]()   (post-spawn)
```

**El criterio jugador/NPC es un flag de la definición del personaje (`def+0x1C`).** Es el mismo flag que `PlayerTorch` exige para "es un jugador" (hallazgo 03). No hay límite de un jugador en este código: el vector tiene buffer interno en `+0x38` (espacio para 18 punteros hasta `+0x80`) y contador en `+0x84`. **Hipótesis central para el coop:** spawnear un personaje con una definición de jugador (`def+0x1C != 0`) lo agrega como jugador 1, mientras `GetPlayer()` sigue devolviendo el 0 (el local).

## `CharacterManager` (layout parcial)

| Offset | Contenido |
|--------|-----------|
| `+0x00`, `+0x04`, `+0x0C`, `+0x14`, `+0x1C`, `+0x24` | vtables (herencia múltiple; principal `0x012F68E8`, interfaz de spawn `0x012F68E0` en `+0x04`) |
| `+0x28` | arg 1 del constructor (`g_root+0x4C`, objeto de `FUN_007e10d0`) |
| `+0x2C` | **NETZ** (`g_root+0x04`) |
| `+0x30` | `g_root` |
| `+0x34` | arg 5 |
| `+0x38`…`+0x7F` | buffer interno del vector de jugadores |
| **`+0x80` / `+0x84`** | **jugadores: puntero a datos / cantidad** |
| `+0xD0` / `+0xD4` | NPCs: datos / cantidad |
| `+0x170` / `+0x174` | pendientes de spawn: datos / cantidad |
| `+0x1C4` / `+0x1C8` | liberados: datos / cantidad |
| `+0x1CC` | array por slot (lo limpia `release_player`) |
| `+0x216`/`+0x218`, `+0x222`/`+0x224` | bitsets de slots, con contadores |
| `+0x22C` | bloques de 0x1D0 bytes por tipo de personaje; `FUN_007b1760` mapea tipo → índice |

Estados de `Character+0x1122`: `0` jugador, `1` NPC, `3` pendiente, `4` liberado. `Character+0x1120` (short) es el índice dentro del vector en el que está.

## Notas

- `SpawnCharacter` (`0x7D3B50`) y parte de su zona (`0x7D3C00`–`0x7D3DAF`) **no están desensambladas** en el proyecto de Ghidra: el auto-análisis no llegó porque solo se alcanzan por una vtable. Para estudiarlas hay que crear la función, en una corrida no read-only, o usar objdump. Lo mismo puede pasar con otras funciones que solo se llaman por vtable: **una búsqueda vacía de xrefs no prueba que algo no se use**.
- `RequestCharacter` deja su nombre en el buffer de contexto `0x016F17F8` (lo usa el crash handler). Su nombre coincide con los mensajes NETZ `request_logic_character`/`request_display_character` (hallazgo 10).
- Corrección al hallazgo 03: la vtable de un personaje en runtime es `0x012F5D80`, no `0x012F4FC8`.

## Preguntas abiertas

1. ¿Quién arma el `request` para `SpawnCharacter` y qué contiene? En asm se ven `+0x1C` = def → `bloque+0x64`, `+0x48` = id, `+0x4C` = flag y `+0x28`.
2. ¿Qué hace `NETZ->FUN_007ddc60` durante el spawn? (¿asigna un id de red?)
3. ¿Qué sistemas usan solo `GetPlayer()` (índice 0) y cuáles recorren el vector de jugadores? Es clave para la IA (hallazgo 08): si el Alien solo mira el índice 0, el segundo jugador sería invisible para la IA.
4. ¿De dónde sale la definición del jugador (`def` con `+0x1C != 0`)? ¿`CHR_INFO/ATTRIBUTES/THE_PLAYER.BML`?

---

## Parte 2 (adelanto) - El controlador de jugador

- `Character::Setup` (`FUN_007cc400`, slot 2 de las vtables `0x012F4FC8`/`0x012F5D80`, llamado al crear el personaje vía `FUN_007cf2e0` desde `RequestCharacter`) hace, en `0x7CC55E`:
  ```c
  if (def->m_1F && def->m_1C && pj->m_31C == NULL)
      CreatePlayerController(pj, 1);      // FUN_007b2630
  ```
- `CreatePlayerController` (`0x7B2630`) reserva 0x4C bytes, los construye con `FUN_0078fb90(personaje)`, pone `controlador+0x48 = 1` (activo) y guarda `personaje+0x31C = controlador`.
- **Un personaje con la definición del jugador (`def+0x1C` y `def+0x1F`) recibe su propio controlador de jugador**, entra al vector de jugadores (`FinishSpawning`) y el Alien lo trata como "jugador" (`CONDITION_TARGET_IS_PLAYER` mira `+0x31C`/`+0x48`, hallazgo 08).
- **Riesgo para el diseño (sin verificar):** el controlador probablemente lee el **input local**. Un segundo personaje-jugador en la misma máquina podría moverse con el mismo teclado/mando. Para el avatar del jugador remoto habría que:
  - (a) desactivar su controlador (`+0x48 = 0`), sabiendo que eso también lo saca de `TargetIsPlayer`; o
  - (b) alimentar su controlador con el input recibido por red; o
  - (c) moverlo por transform (`SetTransform`, hallazgo 03) sin controlador.

  NETZ (hallazgo 10) resolvía esto con personajes "logic" (en el host) y "display" (en los clientes).

## Parte 2 (cont.) - De dónde sale el request

- `SpawnCharacter` (`0x7D3B50`) ya es función en el proyecto (después de `SeedVtables`, hallazgo 10). Decompilado en `work/evidence/h05/e13.txt`. Campos del `request` que usa:

  | Campo | Uso |
  |-------|-----|
  | `+0x00` | tipo/clase → `FUN_007b1760` → bloque de 0x1D0 bytes |
  | `+0x1C` | **definición** (`def`), se copia al bloque `+0x64` |
  | `+0x28` | se pasa a `FUN_00795bc0` |
  | `+0x48` | id (se busca en `[root+0xAC0]`, entradas de 0x158) |
  | `+0x4C` | flag "replicado según tabla" |

- **Spawner de personajes de script** (`FUN_00490e30`, 1.798 bytes; lo llama `FUN_004917a0`; mensaje `"Failed to find some valid components for a character spawner - please check you script."`):
  - arma el request en el stack con:
    - el nombre del personaje (0x40, de `obj->vf[+0x138]()`);
    - el nombre de composite o `"CUTSCENE"` (0x40);
    - tres vectores (`FUN_00853dd0`), probablemente posición/rotación/escala;
  - lo encola en el `CharacterManager` con `FUN_007a7a20` o `FUN_007a7a80` (usa `[g_root+8]+0x9D30`).
- Hipótesis para la fase 4 (ghost): la forma "limpia" de crear el avatar remoto sería reutilizar este camino (spawner de script / cola del `CharacterManager`) con la definición del jugador o la de un NPC humano. Falta saber qué es la cola `+0x9D30` y la firma de `FUN_007a7a20`/`a80`.
