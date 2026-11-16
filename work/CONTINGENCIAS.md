# Contingencias y limitaciones conocidas

Cosas que se probaron y no salieron, o que se aceptaron así por ahora. Cada entrada dice qué pasa, por qué, la evidencia y qué haría falta para resolverlo. El detalle está en `work/NOTES.md` y en los hallazgos.

_Última actualización: 2026-11-16._

## Avatar del otro jugador

### 1. No hay "dos Amandas": falta el modelo de Ripley en tercera persona
- **Qué pasa:** se puede crear una segunda `THE_PLAYER` sin que el juego se caiga y sin que se lleve la cámara (`AIMP_GHOST_CLONE_AUTH=1`, plaza de jugador inexistente en `pedido+0x48`), pero es **invisible**.
- **Por qué:** los niveles solo traen modelos de Ripley en primera persona (`CHARACTERS\RIPLEY_FP`, `RIPLEY_NAKED_FP`, `SPACESUIT_RIPLEY_FP` en `MODELS_LEVEL.BIN`). No hay Amanda en tercera persona en ningún nivel ni en `MODELS_GLOBAL.BIN`, porque las cinemáticas son videos.
- **Evidencia:** corridas 40–43 (NOTES, "Dos Amandas"); hallazgo 12.
- **Decisión vigente:** el avatar es un NPC civil del nivel que hace de "títere" (`AIMP_GHOST_PUPPET=1`).
- **Para resolverlo:** hace falta un modelo de Amanda en tercera persona con el esqueleto de los humanos del juego. No se usan assets de terceros ni extraídos de otros juegos (regla de no redistribuir assets). Hacer uno propio es un proyecto aparte y no hay garantía de que encaje en el esqueleto ni en las animaciones.

### 2. El avatar es un civil, con las limitaciones del civil
- **Agacharse:** el civil no tiene animación de agachado.
  - Con `FUN_007cae20` el juego marca al personaje como agachado (`Character+0x2AF = 1` y postura efectiva `+0x3E3A = 1`). El usuario ve un leve movimiento de costado y nada más (corridas 65–67 y 69).
  - El comando `IDLE` con las posturas 0 a 5 tampoco lo agacha (corrida 53).
  - Para resolverlo: usar como avatar un NPC humano que sí se agache (seguridad u otros con arma), si hay en el nivel. `AIMP_GHOST_NPC_TYPE` elige el tipo.
- **Deslizamiento:** las piernas no siempre van al ritmo del desplazamiento. El títere se pone cada frame en la posición exacta del otro, y la animación sale de un `MOVE_TOWARDS` aparte.
  - Desde la corrida 65 hay tres pasos (`WALK`, `FAST_WALK`, `RUN`), pero en la prueba conjunta se siguió viendo deslizar.
  - **El usuario lo aceptó** (2026-11-16): "no me molesta siempre y cuando sea responsivo, y lo es".
  - Para resolverlo: controlar directamente la velocidad de la animación de locomoción (bloque continuo de la acción principal, hallazgo 19) en vez de usar un comando.
- **Ropa e identidad:** el avatar es el primer civil disponible del nivel. No se parece a Amanda y puede cambiar de un nivel a otro.
- **Otros NPCs y scripts:** el avatar es invencible (`AIMP_GHOST_INVINCIBLE`) y su IA está bloqueada. Si un script del nivel usa a ese civil, puede haber conflictos (sin observar todavía).

### 3. "Dos Amandas" vía `PLAYER_2` (camino 2)
- El engine reconoce un `"PLAYER_2"` (índice de jugador 1). No se probó como avatar visible porque tiene el mismo problema del modelo (punto 1). Queda como alternativa para la lógica del jugador 2 (por ejemplo, que el Alien lo persiga), no para verlo.

## Arrojables (bengala, noisemaker, molotov, minas)

### 4. La réplica no termina donde terminó la original
- **Qué pasa:** el vuelo se ve igual en las dos máquinas, pero el punto final difiere: los choques y los rebotes dependen del frame.
- **Lo intentado:**
  - Que el que tira mande dónde se asentó su objeto (`AIMP_EVT_THROW_REST`) y que el otro borre la réplica y la recree ahí. Del lado que recibe funciona (corridas 55–64).
  - El que tira no encuentra la posición de su propio objeto. La búsqueda del composite sirvió con tiros suaves de prueba (~3 de 4), pero falló **0 de 5** con tiros reales (corrida 69). El camino fijo `obj+0x24 → +0xC` no siempre da el composite vivo (corridas 66–68).
  - `ProjectileMotion` **no se usa** en la bengala tirada: los hooks de `FUN_0060b6d0` y `FUN_0060b980` no se llamaron (corridas 70–71). La bengala vuela con la física de Havok.
- **Estado:** queda como está (decisión del usuario, 2026-11-16).
- **Para resolverlo:** encontrar el cuerpo rígido de Havok de la bengala, a partir del `TemporaryEntity` del pool (`0x16F0DE4`) o del composite `Thrown\X`, y leer su transformación. Con eso, el resto ya está hecho.

### 5. El botín es por jugador
- Levantar un ítem es un "usar" (`0x26`) y se replica, pero el ítem no desaparece del mundo en la otra máquina: cada jugador tiene el suyo.
- **En curso (2026-11-16):** `AIMP_SYNC_PICKUP` saca el ítem en la otra máquina con la función del juego (`FUN_00795ee0`), buscándolo por pid. Escrito, sin probar en el juego (NOTES, "Ítems compartidos").

## Otras cosas sin sincronizar

### 6. IA, Alien y progreso
- La IA de los NPCs y del Alien corre por separado en cada máquina. El Alien persigue a cada jugador en su propia máquina.
- Los disparadores de los scripts (fase 6, `AIMP_SYNC_PINS`) están escritos y con tests, pero no se probaron con dos máquinas.
- El guardado y los checkpoints no se sincronizan. Las dos partidas tienen que estar en el mismo nivel.

### 7. Armas de fuego
- El disparo no genera un RPC, pero sus efectos sí (daño `0x18`, agresión `0x0C`). La función que decide el disparo (`FUN_007c3b10`) y la que lo ejecuta (`FUN_007c92e0`) están ubicadas (hallazgo 17), pero no se replica nada.

### 8. Red
- Una sola cuenta de Steam: una de las dos máquinas tiene que estar en modo sin conexión.
- `COMMANDS.PAK` de papito está modificado (`BSP_LV426_PT01`): el amigo tiene que usar el mismo, o los dos el original, antes de jugar esa parte.
- La red es UDP propio, sin confirmaciones: cada evento ("usar", arrojables) se manda 3 veces (`AIMP_EVT_REPEAT`) y el otro descarta los repetidos. Con mucha pérdida, un evento puede no llegar. ENet queda como opción si hace falta algo confiable (descargarlo requiere confirmación).
