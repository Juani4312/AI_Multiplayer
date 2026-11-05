# Opciones de arquitectura para el coop (borrador para decidir con el usuario)

> Basado en los hallazgos 01–10 del 2026-11-05. **Nada está verificado en runtime**: cada opción depende de los experimentos de `RUNTIME_PLAN.md`.
> Las fases del prompt (§6) no cambian: 1 código cargado → 2 leer posición → 3 UDP → 4 representar al otro → 5 eventos → 6 IA.

## Lo que sabemos del engine (resumen)

| Pieza | Estado | Hallazgo |
|-------|--------|----------|
| Loop por frame y punto de enganche (`ModeManager::Run`) | ubicado | 02 |
| Jugador local: `GetPlayer()`, posición `+0x70`, yaw `+0x174` | ubicado | 03 |
| Vector de jugadores (N), criterio por definición (`def+0x1C/+0x1F`), controlador de jugador | ubicado | 03, 05 |
| Spawn de personajes (request de 0x130 bytes, `SpawnCharacter`) | ubicado | 05 |
| Árboles del Alien: `TargetIsPlayer` mira el controlador, no `GetPlayer()` | ubicado | 08 |
| IA mixta: visibilidad recorre todos; 24 `GetPlayer()` en IA | ubicado | 08 |
| NETZ: sin transporte ni proveedor (todo es autoridad local) | ubicado | 10 |
| Cola RPC (`g_root+0x50`): 70+ productores "ejecutar local + encolar", flush y despacho en *loopback* por frame | ubicado | 10 |
| ShortGuid nombre → id (SHA-1) | **verificado offline** | 06 |
| Los nodos de script de multiplayer no se usan en ningún nivel | **verificado offline** | 06 |

## Opción A: replicación manual ("clásica")

- Cada juego corre su mundo. El mod manda y recibe estado explícito: posición, yaw y animación del jugador, más eventos elegidos (puertas, items) identificados por ShortGuid.
- **Representar al otro:**
  - (a) spawnear un personaje con la definición de un NPC humano y moverlo por transform (`SetTransform`, hallazgo 03), sin IA si se puede; o
  - (b) spawnear un personaje-jugador con el controlador desactivado (`+0x31C+0x48 = 0`).
- **A favor:** control total; se puede avanzar por fases como pide el prompt.
- **En contra:** hay que identificar y sincronizar cada sistema a mano; la IA del Alien en dos mundos distintos diverge.
- **Experimentos que la habilitan:** bloques 0–2 de `RUNTIME_PLAN.md`, y en la fase 4 probar spawnear un NPC o jugador extra.

## Opción B: replicar la cola RPC del engine

- El host captura cada frame el buffer de la cola A en el flush (`0xA2A3F0`), o instala un **sumidero** propio en `[g_root+0x50]+0x00`, que es la interfaz prevista (`vf[1](&buffer) → bool`). Manda el buffer al cliente.
- El cliente lo inyecta en su cola y sus handlers lo aplican.
- **A favor:** reutiliza la serialización y el despacho que el engine ya tiene: spawns, scripts, física, IA, personajes. Potencialmente es la vía más corta al "mundo compartido".
- **En contra y riesgos:**
  1. el cliente también produce sus propios eventos, lo que genera duplicados;
  2. las referencias a entidades dentro de los payloads tienen que coincidir entre máquinas: mismo nivel, mismos datos y mismo orden de spawn;
  3. no se conoce la semántica de cada tipo;
  4. puede ser frágil ante desincronizaciones.
- **Experimento que la decide:** bloque 3 de `RUNTIME_PLAN.md`: ¿la cola crece cada frame en una partida normal? ¿Qué tipos aparecen al abrir puertas o agarrar items?

## Opción C: revivir el modo cliente/host original

- Instalar un proveedor en la sesión (`[DAT_01be1bd4]+0x00`, con `vf[1]` = "¿soy autoridad?") y el flag `+0x2C`, para que el engine trabaje en su modo cliente/host. Sumar un transporte NETZ (`[g_root+0x04]+0x69C`, `vf[1]` = enviar) y un sumidero de cola.
- **A favor:** si el modo original está completo, el engine haría casi todo: personajes "logic/display", autoridad, triggers de cliente.
- **En contra y riesgos:**
  1. no se sabe si el código de cliente está completo en retail; hay indicios de stubs y de código no alcanzado;
  2. los niveles no usan los nodos MP (verificado), así que los scripts no esperan esa lógica;
  3. es el más difícil de depurar y puede romper sistemas (UI, guardado).
- **Experimento previo:** entender la interfaz del proveedor (quién llama `vf[1]`, qué más espera) y probar, en un nivel simple, solo el cambio de `IsClient()` en una de las dos instancias.

## Recomendación (provisional)

1. Fases 1–3 iguales para todas las opciones: hook por frame, lectura de estado y UDP.
2. Ejecutar el bloque 3 de `RUNTIME_PLAN.md` (solo lectura) **antes** de la fase 4. Si la cola RPC se mueve como se espera, prototipar B para eventos y mantener A para el transform de los jugadores (modelo híbrido).
3. Reservar C como investigación aparte. No es camino crítico.

## Restricciones que no cambian

- La misma build (hash) en las dos máquinas: los ids de tipo y los offsets dependen del binario.
- Los mismos datos de nivel: hay que resolver los `COMMANDS.PAK` modificados del usuario antes de la fase 5 (ver `PROGRESS.md`).
- No tocar el chequeo de licencia de Steam (`FUN_007cad10`, hallazgo 02).
