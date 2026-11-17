# Alien compartido: diseño y lo que se sabe (2026-11-16)

Objetivo: que los dos jugadores vean **el mismo Alien** y que el Alien pueda perseguir a cualquiera de los dos.

Hoy cada máquina corre su propio Alien, y cada uno solo persigue al jugador de su máquina. En la máquina del host, el otro jugador es un NPC (el avatar).

## Lo que se sabe (estático salvo donde se indica)

- **Decisión dirigida por datos** (hallazgo 08): árboles "Legend" (`DATA/BEHAVIOR/ALIEN_*.XML`, 25 archivos) con un objetivo genérico. `ConditionTargetIsPlayer` (`0x6EFD10`) no compara con `GetPlayer()`: mira si el personaje objetivo tiene el controlador de jugador activo (`Character+0x31C`, `+0x48`).
- **Cerebro y objetivo:** cerebro de IA en `Character+0x34A8`; objetivo actual en `cerebro+0x10`; personaje objetivo en `objetivo+0x42C`, con handle débil en `+0x430` (validado con `FUN_00783b00`).
- **Registros de objetivo percibido:** los arma `FUN_006e2770(personaje, pos*, x)` (personaje en `+0x42C`, posición en `+0x3E8..+0x3F0`, `+0x488`).
- **Manejador de estímulos de la percepción:** `FUN_00730c30` thiscall(percepción, estímulo), con un `switch` por `estímulo+0xC`:
  - **0, 1 y 2** (ruido, visión, detector `"SCANNER"`) usan el personaje que emitió el estímulo (`estímulo+0x28`, handle): sirven para **cualquier personaje**, también NPCs. Las ramas comparan `[[personaje+0x64]+4]` (¿bando o tipo?) entre el que percibe y el emisor.
  - **6** está fijo en `jugadores[0]` (`[[0x16F0C88]+8]+0x80`): el otro jugador no entraría por esta vía.
- **Visibilidad** (`FUN_00725330`): recorre jugadores y NPCs. El avatar, que es un NPC, puede ser visto.
- **C++ centrado en el jugador 0:** 24 llamadas a `GetPlayer()` en la zona de IA y unas 70 inlineadas (hallazgo 08); `HasFlankedVentCloseToPlayer`; probablemente el menace manager y el Director (`FUN_006cf080`).
- **Censo (corrida 76):** en HAB_ShoppingCentre, sala del transit, no hay Alien cargado: un jugador (tipo 0), un humano armado (tipo 4) y tres civiles (tipo 6). Falta el tipo del Alien y en qué lista está (¿`CharacterManager+0x124`?).

## Plan propuesto (host con autoridad)

1. **Identificar al Alien (runtime):** `AIMP_CENSUS=1` en una partida con el Alien activo (Tech_Hub, por ejemplo; corrida 13). Hace falta que el usuario cargue esa partida: no se cargan otras partidas desde acá para no pisar el progreso.
2. **Alien títere en el cliente:** lo mismo que el avatar.
   - Cada frame, `SetTransform` a la posición y el yaw del Alien del host; los paquetes de estado suman un bloque "Alien" con posición, yaw, velocidad y estado (caminando, corriendo, en ducto).
   - IA bloqueada: filtro de `SwitchCommand` en sus dos controladores (`+0x3570`, `+0x3820`) y motivación CINEMATIC (hook de `FUN_006d3630`).
   - Animación con `MOVE_TOWARDS` por velocidad.
   - Riesgo: la locomoción del Alien es otra (cuadrúpedo, ductos). Hay que probar si el `MOVE_TOWARDS` de script la anima.
3. **El Alien del host persigue al avatar:** hoy lo percibiría como NPC (rama `ALIEN_TARGET_IS_NPC`, que mata NPCs con otra animación).
   - Opción a: marcar al avatar como "jugador": un controlador falso en `+0x31C` con `+0x48 = 1`. Riesgo alto: cámara, input y HUD miran ese campo (hallazgo 03, `GetAimPosition`).
   - Opción b: dejar que lo trate como NPC y, cuando lo ataque, mandar al cliente un evento "el Alien te atrapó" que dispare la muerte del jugador local con la cinemática de muerte del juego.
4. **Muerte y reaparición:** que una muerte en cualquiera de las dos máquinas termine la partida en las dos, o lo que el usuario prefiera.

## Preguntas abiertas

- Tipo del Alien y lista donde está (runtime).
- ¿`MOVE_TOWARDS` de script anima al Alien? ¿Y en ductos?
- Qué es `[[personaje+0x64]+4]` en la percepción (bando o tipo).
- Dónde se elige el objetivo actual (`cerebro+0x10`) entre los percibidos: no apareció un string útil (`target_select`, `TARGET_PRIORITY` no existen). Siguiente: quién escribe `cerebro+0x10`, partiendo de los nodos `IsAttackingThisTarget` y `ConditionIsEnemyOfTarget` (fábrica `FUN_0072b950`).


## Datos de runtime: save `alien` (corridas 117–118, 2026-11-17)

- **Save de prueba con el Alien:** `saves/alien.tar.gz`, inicio de la misión 6 "Outbreak" (SCI_HospitalLower). Se armó desde `airport` con Load Game → Load Mission Save → Mission 6; el juego escribió el checkpoint de entrada ("Save 2 M6 - San Cristobal Medical Facility") y "Continue" lo carga.
- **Censo:** un solo NPC, `NPCs[0] = "BackstageAlien"` (tipo 1, controlador 0), además del jugador.
- **Movimiento con el jugador quieto en (−3,4; −12,2; −3,1)** (posición `Character+0x70` cada 10 s con `memsrv`):
  - recorre el nivel siempre a y = −7,2 (5 m más arriba que el jugador: conductos o techo): (6,7; −8,4) → (−8,0; −6,5) → (11,9; 4,0) → (13,7; 11,6) → (−3,1; 11,6);
  - a los ~74 s queda **exactamente encima del jugador** (−3,4; −7,2; −3,1) y no se mueve más en 2 minutos.
- **Para el coop:** en modo "backstage", el director lo mantiene sobre el jugador 0. Con dos jugadores, el host tendría que decidir sobre cuál se pone (el más cercano o el que hace ruido) y el cliente tendría que ver un Alien títere en la misma posición. Siguiente experimento: con el avatar del otro lejos, ¿se puede llevarlo encima del avatar (objetivo en `cerebro+0x10`, percepción `FUN_00730c30`)?
