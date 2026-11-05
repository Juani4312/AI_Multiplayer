# 08 - IA: ¿ve a un solo jugador o a todos? (parte 1)

- **Pregunta:** Para el coop real (fase 6), ¿la IA (Alien, androides, humanos) trabaja solo con `GetPlayer()` (jugador 0) o recorre el vector de jugadores?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** ver tablas; VAs con base `0x400000`.
- **Evidencia:**
  - `./work/evidence/h03/d31.txt`: los 180 call sites de `GetPlayer`.
  - `./work/evidence/h08/a2.txt`: patrones `CMP [x+0x84]` → `MOV [x+0x80]` en todo `.text`.
  - `./work/evidence/h08/a3.txt`, `a4.txt`: decompilación.
- **Confianza:** media. El conteo es estático; los loops se confirmaron decompilando solo 3 funciones.
- **Cómo verificar en runtime:** con un segundo personaje-jugador spawneado (hallazgo 05), ver si los NPCs reaccionan a él (visibilidad/sentidos) y si el Alien lo persigue.
- **Verificado en runtime:** no.

## Código de IA ubicado (por rutas `__FILE__`)

| Archivo | Funciones que lo referencian |
|---------|------------------------------|
| `behavior_manager/menace_manager.cpp` | `FUN_006cf080` |
| `behavior_manager/alien_attack_manager.cpp` | `FUN_006e3db0` |
| `behavior_manager/visibility_manager.cpp` | `FUN_006fcf80`, `FUN_0071f070`, `FUN_0071f3f0`, `FUN_00725330` |
| `main/character_stealth_state.cpp` | `FUN_007922f0` |

Zona aproximada de `behavior_manager`: `0x6C0000`–`0x740000`.

## Resultado: la IA es "mixta"

1. **Usa solo al jugador 0 (`GetPlayer()`):** 24 de los 180 call sites (13 funciones) están en la zona de IA:
   `FUN_006b69b0`, `FUN_006bcea0`, `FUN_006be2f0`, `FUN_006cf280`, `FUN_006df340`, `FUN_006f2750`, `FUN_0070e700`, `FUN_0071b140`, `FUN_0071f3f0`, `FUN_00731be0`, `FUN_00735e50`, `FUN_0073b200`, más uno fuera de función.
   - Ejemplo: `FUN_006cf280` toma `GetPlayer()->+0x1100` (otro objeto con posición en `+0x70/+0x78` y `+0x90..+0x98`) y calcula un punto desplazado. Lo usan el visibility manager (`FUN_0071f3f0`) y `FUN_00731be0`.
   - En total el juego tiene **~70 lugares** con `GetPlayer()` inlineado (`CMP [x+0x84],0` → `[x+0x80][0]`) además de los 180 calls.
2. **Ya recorre a todos los jugadores:**
   - **`FUN_00725330` (visibility manager)** itera `jugadores + NPCs` como una lista: `i < n_jugadores ? jugadores[i] : npcs[i - n_jugadores]`. Un segundo jugador entraría en los cálculos de visibilidad. Ojo: dentro hay usos puntuales de `jugadores[0]`.
   - **`FUN_00734690`** recorre todos los jugadores, todos los NPCs, una tercera lista (`gestor+0x124`) y otra vez los jugadores.
   - Fuera de la IA hay loops parecidos (`CMP [x+0x84], reg`) en `FUN_007cc080` (`update1` del `CharacterManager`), `FUN_007d3db0`, `FUN_007d41b0` (llama a `release_player`), `FUN_007d4830`/`4a00`/`4b40`, `FUN_007b3750`, `FUN_007b88f0`, `FUN_007bafa0`, `FUN_007c2450`, `FUN_0078cc90`, `FUN_007a1230`, `FUN_00413f80`, `FUN_00414200` y `FUN_006d7040`.

## Implicaciones para el coop

- Es una buena noticia: partes del engine (visibilidad, actualización de personajes, liberación de jugadores) ya están escritas para N jugadores. Encaja con NETZ (hallazgo 10) y con los nodos de script de multiplayer (hallazgo 01).
- Para que el Alien "decida entre dos objetivos" habría que revisar los ~24 call sites de `GetPlayer()` en IA, más los inlineados. La alternativa es interceptar `GetPlayer()` en contextos de IA para devolver "el jugador relevante" (el más cercano o el que el Alien persigue).
  - Es **riesgoso**: `GetPlayer()` se usa en 180 lugares, incluidos cámara, HUD e input, que tienen que seguir viendo al jugador local.
  - Hay que hacerlo con hooks por call site, no globales.
- Falta la pieza central: la **selección de objetivo del Alien** (`alien_attack_manager.cpp`, menace manager y el Director). Queda para la parte 2.

## Preguntas abiertas (parte 2)

1. ¿Dónde se elige el objetivo del Alien y cómo se representa (puntero a `Character`, id)? Empezar por `FUN_006e3db0` (alien_attack_manager) y el menace manager (`FUN_006cf080`).
2. ¿Qué es `Character+0x1100`? Hipótesis: un objeto de "punto de interés" o de predicción del jugador.
3. Los inlineados de `GetPlayer()` en IA: lista completa a partir de `a2.txt` (los de la zona `0x68`–`0x74`).
4. ¿La lista `gestor+0x124` es otra categoría de personajes (¿Aliens?)?

---

# Parte 2 - Árboles de comportamiento y `TargetIsPlayer`

- **Evidencia:** `./work/evidence/h08/a5.txt` … `a12.txt`; copias de `DATA/BEHAVIOR/ALIEN_*.XML` en `game/data/BEHAVIOR/` (25 archivos, no se commitean); desensamblado con `objdump` de `0x6EFD00`–`0x6EFD86` (código que Ghidra no definió como función).
- **Confianza:** media-alta. La función de evaluación se identificó por la vtable pegada a la string `"CONDITION_TARGET_IS_PLAYER"`; no se vio una llamada directa.

## Cómo decide el Alien

- La decisión es **dirigida por datos**: árboles de comportamiento "Legend" (`DATA/BEHAVIOR/*.XML`, 86 archivos; 25 del Alien, como `ALIEN_ATTACK*.XML`, `ALIEN_BACKSTAGE_STALK.XML`, `ALIEN_IN_VENT*.XML`, `ALIEN_TARGET_IS_NPC.XML`).
- Los árboles trabajan con un **objetivo genérico**:
  - Condiciones: `ConditionIsEnemyOfTarget` (9 usos), `ConditionHasValidRouteToTarget` (5), `IsAttackingThisTarget` (2), `ConditionLogicCharacterFlags`.
  - Ramas propias del jugador: `ConditionTargetIsPlayer` (5) y `ConditionHasFlankedVentCloseToPlayer` (2).
- La fábrica de nodos es `FUN_0072b950`: una cadena de `strcmp(nombre_clase)` → constructor. Por ejemplo, `ConditionTargetIsPlayer` → `FUN_007165f0`, que crea el nodo `FUN_006f7ac0` (vtable `0x012E7CE8`) y parsea los conectores `ConditionTrue`/`ConditionFalse`.

## `CONDITION_TARGET_IS_PLAYER` no compara con `GetPlayer()`

```asm
; 0x6EFD10 (evaluación; su vtable termina en 0x012E80C8, justo antes de la string "CONDITION_TARGET_IS_PLAYER")
mov edi, [arg+0x34A8]        ; cerebro de IA del personaje que evalúa
mov esi, [edi+0x10]          ; objetivo actual
...valida el objetivo con un handle débil ([esi+0x42C] / [esi+0x430], FUN_00783b00)...
mov eax, [[edi+0x10]+0x42C]  ; personaje objetivo
mov eax, [eax+0x31C]         ; componente/controlador de jugador
cmp byte [eax+0x48], 0       ; ¿activo?
→ devuelve 1 si existe y está activo
```

- `Character+0x31C` es el mismo campo que usa `GetAimPosition` (hallazgo 03) para apuntar desde la cámara cuando el personaje es jugador. Hipótesis: es el **controlador de jugador**, que solo tienen los personajes manejados por una persona.
- **Consecuencia para el coop (fase 6):** si el segundo jugador tiene un `Character` con controlador de jugador activo (`+0x31C`, `+0x48`), los árboles del Alien lo tratan como "jugador" cuando sea su objetivo. Lo que queda centrado en el jugador 0 está en C++: los 24 `GetPlayer()` de IA, `HasFlankedVentCloseToPlayer` y probablemente el menace manager/Director.
- Composición de la tarea `AI_COMBAT_MANAGER_MAIN` (`FUN_007d4f30`), por clasificar en detalle:

  | Función | Trato de los jugadores |
  |---------|------------------------|
  | `FUN_006f5310` | sin clasificar |
  | `FUN_0071b140` | jugador 0 |
  | `FUN_0072b1b0(dt)` | sin clasificar |
  | `FUN_007d3db0` | todos los jugadores |
  | `FUN_00734690` | todos los jugadores + NPCs |
  | `FUN_00682d90` | si `jugador0+0x3AD0` |
  | `FUN_007c2450` | todos los jugadores |
  | `FUN_00735e50` | jugador 0 |
  | `FUN_00466530`, `FUN_007aec30` | sin clasificar |

## Preguntas abiertas (parte 3)

1. ¿Cómo se elige el objetivo (`[cerebro+0x10]`)? ¿Desde la percepción de todos los personajes del visibility manager?
2. ¿Qué es `Character+0x34A8` (cerebro de IA) y quién crea el controlador `+0x31C` (solo para jugadores)?
3. Menace manager/Director: ¿mide la amenaza solo respecto del jugador 0?
