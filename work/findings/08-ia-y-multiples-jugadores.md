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
