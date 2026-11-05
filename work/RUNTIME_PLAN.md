# Plan de verificación en runtime

> Para la próxima sesión con el usuario, cuando exista la fase 1 del mod (código cargado y callback por frame en el thread principal).
> Todas las direcciones son **VA con base `0x400000`**: en runtime usar `base + (VA − 0x400000)`.
> Ejecutar el juego requiere la confirmación del usuario (prompt §2).

## 0. Guardas antes de leer nada (hallazgos 01 y 02)

| Chequeo | Esperado |
|---------|----------|
| TimeDateStamp del header COFF | `0x54E3199D` |
| RVA de `ovr_Initialize` (export) | `0xBA83B0` |
| Bytes en `0x7A8460` (`ModeManager::Run`) | `51 53 55 56 8B F1 83 7E 4C FF` |

Si algo no coincide: no tocar nada y loguear.

## 1. Loop y estado (hallazgos 02 y 04)

| Qué | Cómo | Esperado |
|-----|------|----------|
| Contador de frames | `*(int*)0x16410DC` | +1 por frame |
| dt real / escalado | `*(double*)0x16410D0` / `*(double*)0x16410C0` | ~0,016 a 60 fps; 0 en pausa (el escalado) |
| `g_root` | `*(void**)0x16F0C88` | ≠ NULL después del init |
| Estado del `ModeManager` | `[[g_root+0x3C]+0x4C]` | 5 mientras se juega |
| Modo actual / índice | `[[g_root+0x3C]+0x40]` / `+0x44` | cambia al cargar un nivel |
| Nivel actual | `L = *(void**)0x1BE1C4C`; índice `[L+0x2BC8]`; nombre en `L + índice·0x46` | nombre legible (hallazgo 09) |

## 2. Jugador y cámara (hallazgos 03, 04 y 05)

| Qué | Cómo | Esperado |
|-----|------|----------|
| Gestor de personajes | `cm = [g_root+0x08]` | — |
| Contadores | jugadores `[cm+0x84]`, NPCs `[cm+0xD4]`, pendientes `[cm+0x174]` | 1 jugador; NPCs varía |
| Jugador local | `pj = [[cm+0x80]+0]` | vtable `*pj == 0x12F5D80` |
| Posición | `pj+0x70/+0x74/+0x78` (float) | cambia al caminar; Y casi constante en el piso |
| Yaw | `pj+0x174` (float) | en [−π, π]; cambia al girar |
| Controlador de jugador | `[pj+0x31C]` ≠ NULL y `byte [ctrl+0x48] == 1` | (hallazgos 05 y 08) |
| Definición | `d = [pj+0x64]`; `byte [d+0x1C]`, `byte [d+0x1F]`, `[d+0x04]` | 1, 1, tipo del jugador |
| Cámara activa | `cam = [[g_root+0x58]+0x1DC]`; cuaternión `cam+0x2C..0x38`, posición `cam+0x3C..0x44` | norma del cuaternión ≈ 1; posición ≈ jugador + ~1,6 m en Y |

## 3. Red del engine (hallazgo 10): los experimentos más importantes

| # | Qué | Cómo | Pregunta |
|---|-----|------|----------|
| 3.1 | Transporte NETZ | `[[g_root+0x04]+0x69C]` | ¿Siempre 0? (esperado: sí) |
| 3.2 | Máscara de canales NETZ | `[[g_root+0x04]+0x10]`, `+0x14` | ¿Cambia al spawnear NPCs? |
| 3.3 | Proveedor de sesión | `[*(void**)0x1BE1BD4 + 0x00]` | ¿Siempre 0? (esperado: sí) |
| 3.4 | Cola RPC (A) | `q = [g_root+0x50]`; tamaño `[q+0x34]` justo antes del flush (`0xA2A3F0`) | ¿Crece cada frame? ¿Cuántos bytes? |
| 3.5 | Catálogo de handlers | tabla de la cola A: `[[g_root+0x50]+0x08]` (`{fn, ctx}` por tipo); tabla de la sesión: `[[g_root+0x54]+0x08]`, cantidad en `+0x0C` (los 8 endpoints de la lista `0x1BE1BD0`) | listar `(tipo, fn, ctx)` de las dos al cargar un nivel |
| 3.6 | Tráfico por tipo | contar los `u16` de cada mensaje dentro del buffer `[q+0x30]` (formato `[u16 tipo][payload]`; el largo lo define cada handler) | ¿Qué tipos aparecen al abrir una puerta, agarrar un item o con el Alien activo? |

Si en 3.4 la cola crece, la hipótesis "replicar el buffer de la cola del host al cliente" (hallazgo 10, parte 4) queda viva. Es el camino más corto al mundo compartido.

## 4. IA (hallazgo 08)

| Qué | Cómo | Pregunta |
|-----|------|----------|
| Objetivo del Alien | `[[alien+0x34A8]+0x10]` (objetivo actual), con `alien` = el NPC Alien en `[cm+0xD0]` | ¿Apunta al jugador cuando lo persigue? |

## 5. Orden sugerido

1. Bloques 0–2: confirman los offsets base. Con eso la fase 2 del mod queda cerrada.
2. Bloque 3, solo lectura y logging: decide la arquitectura de red del coop.
3. Bloque 4: preparación de la fase 6.
