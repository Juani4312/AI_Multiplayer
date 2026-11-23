# PROGRESS — estado para retomar

> Punto de entrada para cada sesión nueva de Claude Code. Leelo junto con `prompt.md` (instrucciones), `work/NOTES.md` (bitácora con evidencia) y `work/LOG.md` (log cronológico corto).
> Mantenelo corto: estado actual, decisiones vigentes y siguiente paso. El detalle va en NOTES.md y findings/.

_Última actualización: 2026-11-23 17:00 (corridas 170–174, locales con el peer). Menú Coop con "Opciones avanzadas" hecho. La run sin morir está en `saves/run-sin-morir-2026-11-20.tar.gz`. Resumen de las sesiones: `work/HISTORIAL.md`._

Hecho el 2026-11-20 (detalle en NOTES.md, "Variante `xinput1_3.dll`"):
- la prueba con el amigo no llegó a hacerse: su Windows 11 25H2 carga la `winmm` del sistema y el mod no arranca (papito estuvo 72 min de host sin datos);
- **variante `xinput1_3.dll` del mod** (plan B): mismos objetos, otra tabla de exports; `AIMP_PROXY_REAL` para encadenar otro mod con el mismo nombre (MovieFix); si están las dos DLL, el mod arranca una vez. `test_host_xinput` OK y **corrida 148 en el juego (Proton): carga, hooks y mando OK**;
- `runtime_setup.sh`/`mode.sh` con `--proxy xinput1_3` (aparta la `xinput1_3.dll` de MovieFix, que en Proton no se usa, y la devuelve); `aimp_check.ps1 -Proxy xinput1_3` (sin probar: no hay PowerShell acá); `INSTALAR_CON_CLAUDE.md` sección 2: primero probar la variante;
- `mod/dist` (2026-11-21, commit `4d3bab4`): `winmm.dll` `4a7bd5e1…` y `xinput1_3.dll` `ae712bf2…` (mismo protocolo que `b4de9984`; avisos sin pausa, robot, puertas y cajas por pin, avatar mujer).
- **aviso sin pausa** (`ui_toast`, cartel de consejos del HUD; caído/revivido en `death.c`): escrito, **sin probar en el juego**;
- **mods de terceros** en `<juego>/mods/` (`instructions.md` ahí; viven solo en la carpeta del juego, no en el repo): `mode.sh test` apaga los que interfieren y `play` los reactiva. SmartAI y AIMovieFix pendientes (sin README).

Hecho el 2026-11-18 a la tarde (corridas 134–147): pantalla negra al caer por un remate resuelta (`death.c`: locomoción + fundido 0 + facción del Alien mientras dura el caído), avisos con el diálogo "Warning" del juego (`ui_message`), `AIMP_TRACE_DMG`, `memsrv LOCO/ACT/WD/CALL`; REA necesita JDK 27.

Hecho en la sesión anterior (2026-11-18, corridas 123–133): dificultad del juego y del coop (`diff.c`, menú "Coop"), guardar desde el mod (`gamesave.c`), host caído: guarda el cliente (`savesync` inverso).

**Siguiente:**
- ✅ **Menú Coop con "Opciones avanzadas"** (corridas 170–174, `menu.c` + `gfx.c`, armado con Scaleform desde el mod): selector de dificultad (Normal/Difícil/Pesadilla/Personalizada) y submenú con escala del Alien (+0..+3), humanos y androides, caer y revivir, guardado exigente, inventario y cajas compartidos; `opciones.txt`; el host las manda (`AIMP_EVT_OPTIONS` tipo 2) y el cliente las ve bloqueadas. Verificado con el peer en los dos roles. **Falta:** el efecto de "humanos y androides" (bloques por plantilla de NPC de la dificultad, `diff.c`) y caer "Normal" (desangrado + revivir en persona: el usuario tiene que elegir la duración y la tecla); con dos máquinas.
- **A futuro (pedido del usuario):** investigar el pool de botín del nivel (`AllocateGCItemsFromPool`) para un multiplicador de botín y para que las dos máquinas vean el mismo contenido en las cajas (looteo compartido).
- **con `cathode-dump`:** ponerle nombre a los pids de las trazas (robot, compuertas: `zgrep` en `work/evidence/cathode/commands/`); diferencias del `COMMANDS.PAK` modificado de `BSP_LV426_PT01` contra el de Steam (hace falta copiar el de la laptop); aviso sin pausa con `POPUP_TYPE_INGAME_LOG_TEXT` (1001, no es diálogo en `POPUPOVERLAY.GFX`) en vez del "Warning" (101), sin probar;
- ~~probar el aviso sin pausa~~ ✅ corridas 149–151 (no pausa; una clave por aviso; textos de un renglón). `ui_message` (diálogo "Warning") no tiene ese problema (corrida 157);
- **el amigo:** que su Claude Code siga `INSTALAR_CON_CLAUDE.md` sección 2 (instalar con `-Proxy xinput1_3` y `verify`); si carga, papito como host con la misma DLL de `mod/dist` (`mode.sh test safezone --cfg …`, cfg en LOG.md 2026-11-18 23:00). Si tampoco carga: el informe de ProcMon/Integridad de código;
- **prueba de a dos** (`work/windows/instructions.md` 8g, DLL `4a7bd5e1`): menú Coop y dificultad elegida por el host, host caído + terminal real de la laptop, guardado exigente, robot con marcas F8, remate de un enemigo con el modo caído (ya no tiene que quedar negro), agachado, escotilla;
- sentir la dificultad del coop jugando con el save `alien` (el efecto solo se verificó en memoria); sumar los sentidos del ALIEN (bloque por plantilla) si hace falta;
- ✅ **avatar mujer** (corridas 158–159): arreglada la preferencia (`FEMALENPC`) y, si no hay ninguna cargada, el mod hace aparecer una obrera (`AIMP_GHOST_FEMALE_SPAWN`, por defecto solo HAB_ShoppingCentre; en Airport y HospitalLower los disparadores son de encuentros ambientales inactivos en los saves de prueba, corridas 160–161: sin civiles cargados, ahí no hay avatar);
- ✅ **puertas** (corridas 162–166): el `start` del panel no alcanza; se comparte el desbloqueo del mecanismo (`AIMP_SYNC_PINS_SHARED_IDS`, `Door_Mechanism/IsLocked.true_now_false`, id `09de34ae` único en todos los niveles): verificado con el peer en los dos sentidos (palanca natural en el host → 2 eventos; cliente → puerta desbloqueada que se abre al acercarse, sin eco). Falta con dos máquinas;
- ✅ **cajas con botín** (corridas 167–169): el botín no se sortea al cargar sino al abrir, del pool de ítems del nivel (`AllocateGCItemsFromPool`), y cada máquina tiene su pool. El primero que abre una caja la sortea y para el otro queda saqueada (`AIMP_SYNC_PINS_SHARED_MAP`: `AllocateGC` → `UI_Container.emptied` de la hermana). Verificado con el peer en los dos sentidos; falta con dos máquinas. Ver el mismo contenido en las dos exigiría copiar los ítems sorteados (ingeniería inversa del pool);
- **paneles de puertas y otros "usar" del arquetipo `Use`:** recorrer en el volcado el grafo desde `used` (links, variables de entrada y salida de los composites) y ver si llega a entidades que mueven al jugador (`CMD_PlayAnimation`, `CMD_HolsterWeapon`, `VariableThePlayer`, `1st_person_animated`, `FixedCamera`: el `Door_Package` tiene de todo); los que no, a `AIMP_SYNC_PINS_SHARED`;
- interacciones del mundo: ✅ **el robot barredor** sincronizado por pin en los dos sentidos (`AIMP_SYNC_PINS_SHARED`, corridas 152–156, en una máquina con el peer); falta con dos máquinas y sumar otros "usar" del arquetipo `Use` que no muevan al jugador (paneles de puertas…). Los objetivos de los "usar" ya tienen nombre (`work/scripts/use_names.py`);
- remates de humanos y androides con el modo caído (la regla los cubre si pasan por `FUN_007d0c10`; falta verlo);
- avisos que no pausen (compañero caído, copia del save): falta un mensaje no modal (el cartel de recogidas no se dibujó; quedan subtítulos, objetivo o `PopupMessage`, que la maneja Flash);
- con el caído en la facción del Alien: ver qué hacen los humanos hostiles y los androides (prueba de a dos o un save con humanos);
- más opciones en el menú Coop (que un botón recorra varias, o habilitar los botones de Extras ocultos en PC: DESIGN_CONFIG_MENU);
- a futuro: ruido compartido con histéresis, varios Aliens, desangrado (DESIGN_DIFFICULTY); Alien compartido (DESIGN_ALIEN); revivir en checkpoints; avatar mujer.

**Partida real del usuario (run sin morir):** nunca se prueba sobre ella. `work/scripts/mode.sh test [save]` guarda `saves/current.tar.gz`, carga el save de prueba e instala el mod; `mode.sh play` la restaura y desinstala el mod. Al cerrar una sesión, `mode.sh play`.
Partidas para pruebas: `saves/` (`work/scripts/saves.sh list|save|load`); `safezone` = la partida de las corridas 20–82 (sin enemigos).

## Decisiones vigentes
- Build objetivo: **Windows `AI.exe` PE32 i386**, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`. El usuario juega con Proton 9.0-203 y el amigo en Windows nativo.
- Mod: DLL proxy cross-compilada con `i686-w64-mingw32` (CMake). **Proxy elegido (provisorio): `winmm.dll`** (hallazgo 01). Tiene que reexportar todas las exports de la winmm del sistema. Falta verificar KnownDLLs en el Windows del amigo.
- `AI.exe` tiene ASLR: el mod trabaja siempre con **RVAs**. Para validar la build en runtime: RVA de `ovr_Initialize` = `0xBA83B0`.
- Compatibilidad con ReShade: deseable, no obligatoria.
- **Inventario compartido (decisión del usuario, 2026-11-17):** se comparte todo. Lo que uno agarra (balas, armas, ítems de misión) lo reciben los dos, y lo que uno consume se les descuenta a los dos (`work/DESIGN_SESSION.md`).
- **Armas de a una (decisión del usuario, 2026-11-17):** cada arma tiene un dueño, el último que la equipó. Cerca, cualquiera la toma si el otro no la está usando; lejos, solo el dueño (`work/DESIGN_SESSION.md`, "Armas de a uno").
- La IA del usuario es vanilla (verificado). **Solo `BSP_LV426_PT01/WORLD/COMMANDS.PAK` está modificado** (comparado con la laptop, que tiene el de Steam; `TECH_COMMS` es el original). Antes de la fase 5 hay que restaurarlo o que el amigo use el mismo (NOTES, "COMMANDS.PAK: solo uno").

## Herramientas
- **Formatos del juego (2026-11-21):** `work/tools/` (ver su README). CathodeLib (submódulo) + `cathode-dump`: `work/tools/cathode-dump.sh` vuelca a `work/evidence/cathode/` el árbol de scripts de cada nivel **con el pid del mod** (verificado: 83–96 % de los pids de las trazas), la UI original (`UI.PAK`) y los BML de dificultad. JPEXS FFDec en `~/tools/ffdec-26.5.0` (`work/tools/get-ffdec.sh`) para el ActionScript de los `.GFX`. Requiere `dotnet-sdk`.
- Ghidra 12.1.2 en `/opt/ghidra`, mingw-w64-gcc 16.2.0, Node 24.22.1 portable en `~/tools/node-v24.22.1-linux-x64` (para REA), REA 6.8.0 compilado en `/home/juano/Documents/clones/rea`.
- **Proyecto de Ghidra persistente ya analizado:** `game/ghidra/AI_Multiplayer.gpr` (no se commitea). Con REA en modo proyecto cada consulta tarda ~10 s.
- Cómo correr REA por CLI con el proyecto:
  ```
  export PATH="$HOME/tools/node-v24.22.1-linux-x64/bin:$PATH" GHIDRA_INSTALL_DIR=/opt/ghidra GHIDRA_HEADLESS_MAXMEM=8G \
    REA_GHIDRA_PROJECT_PATH=/home/juano/Documents/Claude/AI_Multiplayer/game/ghidra/AI_Multiplayer.gpr REA_GHIDRA_PROGRAM_NAME=AI.exe
  node /home/juano/Documents/clones/rea/scripts/rea.mjs <decompile|xrefs|search|trace|function|instructions> /home/juano/Documents/Claude/AI_Multiplayer/game/AI.exe ... --provider ghidra --format json
  ```
- Scripts de Ghidra propios en `work/scripts/ghidra/` (correr con `analyzeHeadless game/ghidra AI_Multiplayer -process AI.exe -noanalysis -readOnly -scriptPath work/scripts/ghidra -postScript <Script>.java <args>`).

## Propuesta pendiente de confirmación del usuario
- **Usar el proyecto también desde MCP** (hoy el MCP de REA haría un import fresco de ~10 min en cada sesión). Requiere cambiar la registración (config del agente) y reiniciar Claude Code:
  ```
  claude mcp remove rea -s local
  claude mcp add --scope local rea --env GHIDRA_INSTALL_DIR=/opt/ghidra --env GHIDRA_HEADLESS_MAXMEM=8G \
    --env REA_GHIDRA_STARTUP_TIMEOUT_MS=900000 \
    --env REA_GHIDRA_PROJECT_PATH=/home/juano/Documents/Claude/AI_Multiplayer/game/ghidra/AI_Multiplayer.gpr \
    --env REA_GHIDRA_PROGRAM_NAME=AI.exe \
    -- /home/juano/tools/node-v24.22.1-linux-x64/bin/node /home/juano/Documents/clones/rea/scripts/rea.mjs mcp
  ```
  Contra: en modo proyecto REA es de solo lectura (sin renames). No se aplicó.

## Estado por etapa
| Paso | Estado |
|------|--------|
| §3 Identificar build | ✅ hecho |
| §4 Setup REA + checklist 4.4 | ✅ completo (Ghidra abre el binario; proyecto persistente) |
| Hallazgo 01 (inventario) | ✅ `findings/01-inventario.md` |
| Hallazgo 02 (game loop) | ✅ `findings/02-game-loop.md` (hook: `ModeManager::Run` RVA 0x3A8460) |
| Hallazgo 03 (entidad del jugador) | ✅ `findings/03-jugador.md` (`GetPlayer` RVA 0x38BAF0; pos +0x70, yaw +0x174) |
| Mod fase 1+2 (proxy winmm + hook + log de posición) | ✅ **verificado en runtime** (2026-11-05, Proton): fase 1 (código por frame) y fase 2 (posición, yaw, cámara). ✅ **También en Windows 10 nativo** (laptop de pruebas: 195 exports resueltos y 10 ausentes que el juego no usa, ASLR reubica `AI.exe` y anda igual; `work/windows/report.md`) |
| Mod fase 3 (UDP mínimo con la posición) | ✅ **verificada por Tailscale** (2026-11-05): laptop Windows 10 nativo (join) ↔ papito Proton (host). Los dos sentidos, 0 perdidos, y las posiciones recibidas coinciden con las marcas F8 del otro lado. Mejoras menores pendientes: ritmo real ~16/s en vez de 20 y backlog del socket antes del primer frame |
| Hallazgo 12 (avatar remoto: pedido de spawn y teleport) | 🟡 `findings/12-avatar-remoto.md`: flujo del spawn de autoridad (`0x797490`), layout del pedido de 0x130 bytes y teleport (`0x7C6330`). Falta runtime |
| Mod fase 4 (avatar del otro jugador) | ✅ **v1 por red OK** (corrida 9, Windows nativo ↔ Proton): en cada máquina un NPC humano del nivel sigue al jugador de la otra (`AIMP_GHOST=3`; `mod/src/ghost.c`). Clonar personajes hace caer el juego (hallazgo 12, parte 2) |
| Mod fase 5 v1 ("usar" compartido) | ✅ **botones en los dos sentidos** (corrida 12): el transit viene en las dos máquinas y el uso remoto lo hace el avatar (`AIMP_SYNC_USE_ACTOR=1`), sin mover al otro jugador. Los lockers y la terminal de guardado no se replican (`AIMP_SYNC_USE_SKIP`). `findings/13-usar-compartido.md` |
| Hallazgo 14 (armas, arrojables e ítems) | 🟡 `findings/14-armas-e-items.md`: disparar no genera un RPC, pero sus efectos sí (daño 0x18, agresión 0x0C). Bengala, nada. **Levantar un ítem es un "usar" (0x26) y ya se replica** (falta probarlo con dos máquinas) |
| Hallazgo 15 (pines de CATHODE) | ✅ `findings/15-pines-de-cathode.md`: despacho `FUN_008be0d0` (`vt+0x394`), identidad estable **pid** (ruta de composites; único y determinista entre procesos), reinyección `FIRE` verificada (se propaga como un disparo natural). Traza `AIMP_TRACE_PINS=1` |
| Hallazgo 16 (árbol de composites) | 🟡 `findings/16-arbol-de-composites.md`: hijos en `CompositeInstance+0x2C`, índice pid → entidad, mensaje de `on_entered` (entró/salió en `msg+0x20`). Código y tests listos; **falta runtime** |
| Hallazgo 20 (PLAY_ANIMATION, agachado del avatar) | ✅ `findings/20-animaciones-play-animation.md`: set `HUMAN`, tick del controlador `FUN_00679150`; `AIMP_GHOST_CROUCH=4` por defecto (corridas 83–89) |
| Hallazgo 21 (inventario) / inventario compartido | ✅ en una máquina: `findings/21-inventario.md`, `AIMP_SYNC_INV=1` (corridas 93–94). Falta con dos máquinas y "armas de a una" |
| Copia del save del host | ✅ en una máquina (corridas 102–116): `mod/src/savesync.c`, `AIMP_SAVE_SYNC=1` en el cliente; guardar juntos; game over con el checkpoint del host (buffer RVA `0x134EF80`); falta con dos máquinas (instructions 8f) |
| Muerte en coop | 🟡 `work/DESIGN_DEATH.md`: caído con `Character+0x4253` (corrida 92); falta red, revivir y el remate del Alien |
| Alien compartido | 📝 diseño en `work/DESIGN_ALIEN.md`; save `alien` (misión 6) con el Alien: tipo 1, cerebro → objetivo → jugador verificado en runtime (corridas 117–118) |
| Ítems compartidos (`AIMP_SYNC_PICKUP`) | 🟡 hook de `pickup_collected` (`FUN_007a2b30`) y aplicación en el otro con `FUN_00795ee0` por pid. Sin probar en el juego (instructions.md 8d) |
| Mod fase 6 (disparadores del host) | 🟡 `AIMP_SYNC_PINS=1` en `mod/src/pins.c`: **verificado por red en una máquina** (corridas 72–73: 9/9 pines del host aplicados, y el cliente bloquea los suyos). Falta con dos máquinas y una zona con efecto visible |
| Hallazgo 17 (arrojables y equipo) | 🟡 `findings/17-arrojables.md`: tirar = `FUN_007b5050` (pool `0x16F0DE4`, composite `Thrown\X` + `spawn_explosive`; el vuelo lo simula `ProjectileMotion` con la posición y la velocidad dadas), colocar = `FUN_007b5190`. Las armas de fuego son composites (`Weapons\…`); su disparo no se siguió |
| Hallazgo 18 (replicación de personajes de NETZ) | 🟡 `findings/18-replicacion-de-personajes.md`: replicador por personaje con 11 tipos de mensaje (acciones, animación, armas, salud), pero la capa de canales es un **stub** en retail (`FUN_00a2abb0`/`FUN_00a2ab80`). La opción C no es camino corto; la animación va a mano |
| Avatar caminando (fase 4b) | ✅ **en una máquina** (corridas 27–31): `AIMP_GHOST_WALK=1` en `mod/src/ghost.c`. `MOVE_TOWARDS` en el **controlador de scripts** (`Character+0x3570`) con la motivación CINEMATIC forzada (hook de `FUN_006d3630`); se actualiza el destino cada 200 ms; corre (`MOVE` `RUN` = 3) si queda a más de 5 m; se lo ubica cuando su IA no tiene un `PLAY_ANIMATION`. Sin red sigue al jugador a 2 m. Por red, probado en una máquina con `aimp_peer.py --replay` (corridas 32–34): sigue el recorrido del otro y, quieto, mira hacia donde mira el otro (`AIMP_GHOST_WALK_FACE`). **Falta con dos máquinas**; el v1 (teletransporte sin IA) sigue siendo el valor por defecto |
| Hallazgo 19 (control de personajes) | ✅ camino 1 verificado: `findings/19-control-de-personajes.md`. Dos controladores de comandos (scripts `+0x3570`, tareas `+0x3820`), rama CINEMATIC del árbol del civil, enum `MOVE` (VA `0x0130FD18`). El jugador se mueve con `READ_GAMEPAD_1` (controlador `Character+0x31C`); el engine reconoce un `"PLAYER_2"` (camino 2, sin probar) |
| Arrojables compartidos | ✅ **inyección verificada en una máquina** (corrida 20: la bengala se duplicó con la misma trayectoria). `AIMP_SYNC_THROW=1` en `mod/src/throw.c` (evento `AIMP_EVT_THROW`, **protocolo v2**). Falta la prueba con dos máquinas |
| Mando virtual (pruebas) | ✅ `mod/src/autopad.c`: `AIMP_AUTOPAD_FILE` (pad.txt), IAT de XINPUT1_3. En el menú, verificar cada paso con captura (corrida 17) |
| Hallazgo 11 (mensajes RPC en runtime) | ✅ `findings/11-mensajes-rpc-en-runtime.md` (patrón RPC A → B confirmado; 0x29 spawn, 0x26 interacción) |
| Hallazgo 04 (cámara y frame) | ✅ `findings/04-camara-y-frame.md` (mapa de tareas, cámara `g_root+0x58`, timer) |
| Hallazgo 10 (NETZ: red del engine) | 🟡 partes 1–7 en `findings/10-netz-red-del-engine.md` (sin transporte ni proveedor; canal por personaje; cola A en loopback; **cola B = salida a la red, descartada en retail**; registro de handlers de la cola A) |
| Hallazgo 05 (creación de personajes) | 🟡 partes 1–2 en `findings/05-creacion-de-personajes.md` (FinishSpawning, controlador de jugador, request y spawner de script) |
| Hallazgo 08 (IA y múltiples jugadores) | 🟡 parte 1 en `findings/08-ia-y-multiples-jugadores.md` (IA mixta) |
| Hallazgo 09 (niveles/checkpoints/guardado) | 🟡 parte 1 en `findings/09-niveles-checkpoints-guardado.md` |
| Hallazgo 06 (scripts/entidades) | 🟡 parte 1 en `findings/06-scripts-y-entidades.md` |
| Hallazgo 07 (items/inventario) | 🟡 parte 1 en `findings/07-items-e-inventario.md` |
| Hallazgo 08 parte 2 (árboles del Alien) | ✅ `TargetIsPlayer` mira el controlador `+0x31C`, no `GetPlayer()` |
| Hash del AI.exe del amigo | ⏳ cuando reinstale (no bloquea hasta la fase 3) |
| KnownDLLs del amigo | ⏳ `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"`. En la laptop (Windows 10 19045) winmm **no** está: es esperable lo mismo en el amigo |

## Resumen para el usuario (2026-11-05, después de las corridas en el juego)
- **Fases 1 y 2 del mod verificadas en runtime** (Proton 9.0-203): hook por frame, posición/yaw del jugador, cámara (1,57 m sobre los pies; 0,85 m agachado), nivel, personajes y NETZ. Offsets de los hallazgos 01–05, 09 y 10 confirmados (ver "Verificado en runtime" en cada uno).
- **Hallazgo 11:**
  - el patrón RPC cliente/servidor funciona en retail: cola A (local) → handler → cola B (red, descartada);
  - 0x29 = spawn (304 bytes) y 0x26 = interacción con un objeto usable (20 bytes con ids de 8 bytes del objetivo y del personaje);
  - el movimiento y la IA no pasan por estas colas.
- **2026-11-07: mod desinstalado** a pedido del usuario para jugar normal (`runtime_setup.sh revert`: sin `winmm.dll`, sin override en `user.reg`, sin `aimp.cfg`; los logs viejos quedan). Para la corrida 20 hay que reinstalar con `runtime_setup.sh install`.

## Pendientes que requieren al usuario
- ~~Corrida 20~~: hecha el 2026-11-15 (arrojables OK, controlador de comandos, locomoción; el índice de pines se llenó y ya está agrandado). El mod quedó desinstalado.
- ~~Corrida 21~~ (2026-11-15): el avatar caminó con animación, pero huía (IA). Índice de pines completo.
- ~~Corrida 22~~ (2026-11-15): raro (teletransportes que no se pegan, choques con el jugador). Corregido en código.
- ~~Corridas 23–31~~ (2026-11-15, automáticas con el mando virtual): el avatar camina y corre detrás del jugador. **El mod queda instalado en pasivo** entre corridas (pedido del usuario): para probar, `runtime_setup.sh install --cfg ...` con la config de la corrida.
- **Corrida con dos máquinas para la caminata:** `AIMP_GHOST=3;AIMP_GHOST_WALK=1` en las dos, además de la red (`AIMP_NET`…). El avatar de cada lado tiene que caminar a la posición del otro jugador y correr si queda atrás. La laptop necesita la DLL nueva de `mod/dist/`. Se puede sumar `AIMP_SYNC_THROW=1` (arrojables, protocolo v2).
- **Protocolo v2** (2026-11-15): la laptop necesita la DLL nueva de `mod/dist/` para hablar con papito.
0. ~~Tailscale para cerrar la fase 3~~: hecho con la laptop. papito es 100.126.161.126 y papulaptop, 100.126.159.45. Para el amigo:
   - instala Tailscale y se une a la red del usuario;
   - el host pone `AIMP_NET=host` y el que se une, `AIMP_NET=join` y `AIMP_NET_PEER=<IP de Tailscale del host>:47811` (en `aimp.cfg`);
   - con dos máquinas y **una sola cuenta de Steam**, una de las dos tiene que estar en modo sin conexión: Steam no deja correr el juego en dos PCs a la vez.
1. **Corrida del experimento de la fase 4** (ver "Siguiente paso" 2): unos 2 minutos en la máquina Linux. Puede cerrar el juego si el clon rompe algo.
   - (Opcional) corrida corta con marcas F8 para separar puerta e ítem (instrucciones en el hallazgo 11, "Pendiente").
2. Aprobar (o no) que el MCP de REA use el proyecto de Ghidra.
3. KnownDLLs en el Windows del amigo; hash de su `AI.exe` cuando reinstale; que use el mismo COMMANDS.PAK (mod de saltear la misión 9) o los dos el vanilla.

## Siguiente paso
0. **Prueba conjunta hecha (corrida 69):** deslizamiento aceptado (responsivo), agachado sin animación del civil, arrojables 0 de 5. Corridas 70–71: la bengala no usa `ProjectileMotion`; siguiente: el cuerpo de Havok de la bengala, o aceptar la diferencia (decisión del usuario).
1. **Fase 3:** ✅ cerrada (las mejoras de ritmo y backlog de `net.c` ya están hechas).
   **Fase 6:** ✅ por red en una máquina (corridas 72–73); siguiente: dos máquinas con una zona de diálogo (`AIMP_SYNC_PINS=1` en las dos).
2. **Siguiente** (a elegir con el usuario):
   - **Fase 6, scripts:** pasos 1–3 hechos (hallazgo 15). Paso 4 escrito (hallazgo 16, `AIMP_SYNC_PINS`). **Siguiente, en el juego:**
     1. **Corrida 20 (local, una máquina)**, con `AIMP_TRACE_PINS=1` y `AIMP_PINS_FILE`:
        - el `PINIX` automático (10 s después de cargar) tiene que dar "0 faltan" y "0 con otro subobjeto"; ver cuántos ms tarda;
        - los volcados `PIN mensaje del primer ...` dan el layout del mensaje (¿instigador?);
        - `FIRE <pid> 98678b2b` de un `TriggerVolumeFilter` que no disparó (pid sacado del índice o de la corrida 19): tiene que propagarse.
     2. **Corrida 21 (dos máquinas):**
        - `AIMP_SYNC_PINS=1` en las dos, más `AIMP_TRACE_PINS=1` para comparar pids;
        - el host entra a la zona del diálogo de la corrida 14: en el cliente tiene que arrancar el mismo diálogo una sola vez;
        - que el cliente, al entrar, no lo dispare.
     Detalle en `work/DESIGN_PHASE6.md`.
   - **Arrojables:** hecho en estático (hallazgo 17, `AIMP_SYNC_THROW`); falta runtime.
   - **Armas de fuego:** ubicar el disparo dentro del composite del arma (`Weapons\Pistol_Standard`…: `AIMED_WEAPON`, `BulletChamber`) o en el código que lo dispara (hallazgo 17, "El equipo del jugador").
   - **Ítems compartidos:** hoy el botín es por jugador. Si se quiere compartido, sacar el ítem del mundo en la otra máquina.
   - Hecho en la corrida 14: el avatar no autoritativo no tiene IA (`AIMP_GHOST_NOAUTH`, activado por defecto en el modo 3).
   - **Fase 4b** (animación del avatar): ✅ camino 1 (`MOVE_TOWARDS` en el controlador de scripts) en una máquina, también por red contra el peer (`--replay`), con giro hacia el yaw del otro. Siguiente: la prueba con dos máquinas. Pendiente menor: después de llegar no vuelve a arrancar hasta que el objetivo se aleja ~1–3 m. Layout de los parámetros de `MOVE_TOWARDS` ya mapeado (hallazgo 19); con `start_instantly` y la re-orden en quieto (por defecto desde la corrida 35) sigue pasos cortos de 1–1,6 m con ≤1 s de retardo. Falta una corrida de control sin el flag (NOTES, 2026-11-15 tarde). **Corrida 36 (dos máquinas):** caminata OK en las dos; arreglados el giro raro (plantado, corridas 37–38) y la muerte del avatar (invencible, `AIMP_GHOST_INVINCIBLE`); falta repetir con dos máquinas. Arrojables: caen distinto por la integración con dt del frame; sonda lista, falta la corrección. **"Dos Amandas" (corridas 40–43):** el clon `THE_PLAYER` se crea por el spawn de autoridad, sin crash y sin llevarse la cámara, pero es invisible (no hay modelo de Amanda en tercera persona). Se sigue con el NPC más responsivo (velocidad del otro, adelanto, paso por velocidad; corrida 45 OK). **Títere (`AIMP_GHOST_PUPPET=1`, por defecto, corridas 46–51):** posición exacta por frame (0,00 m) con animación de caminar/correr por velocidad; el agachado del otro se detecta, pero el NPC todavía no se agacha (`IDLE` con postura entra y no tiene efecto). **Corrida 52 (dos máquinas): títere OK** ("responsivo y exacto", 0,00 m en las dos). **Corrida 53:** el `IDLE` con posturas 0–5 no agacha al civil (descartado). Arrojables: `ProjectileMotion` es analítico en t; la diferencia nace en el choque y la física de Havok; escribir la posición con la sonda no fue concluyente. **Arrojables (corridas 54–64):** el que tira avisa dónde se asentó (`AIMP_EVT_THROW_REST`); el otro libera la réplica (`FUN_007a10f0`) y crea una quieta ahí. Verificado en una máquina con el peer; la posición propia se encuentra en ~3 de 4 lanzamientos. Siguiente: probar con dos máquinas (instructions.md 8b); revisar la animación del títere (el usuario lo vio deslizarse); agachado por la locomoción (`crouched_or_not`, `ConditionIsCrouching`). El camino 2 (`PLAYER_2`) queda como alternativa.
3. Investigar qué escribe en los canales NETZ durante el juego: posible camino para el movimiento de personajes.
4. Estático pendiente: semántica de los RPCs 0x1E/0x20/0x22/0x23/0x28 y del B 0x07; dispatcher de los comandos de UI (hallazgo 09); inventario (07).
