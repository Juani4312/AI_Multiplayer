# PROGRESS — estado para retomar

> Punto de entrada para cada sesión nueva de Claude Code. Leelo junto con `prompt.md` (instrucciones), `work/NOTES.md` (bitácora con evidencia) y `work/LOG.md` (log cronológico corto).
> Mantenelo corto: estado actual, decisiones vigentes y siguiente paso. El detalle va en NOTES.md y findings/.

_Última actualización: 2026-11-05 (sesión autónoma: el usuario pidió seguir hasta agotar los tokens diarios)_

## Decisiones vigentes
- Build objetivo: **Windows `AI.exe` PE32 i386**, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`. El usuario juega con Proton 9.0-203 y el amigo en Windows nativo.
- Mod: DLL proxy cross-compilada con `i686-w64-mingw32` (CMake). **Proxy elegido (provisorio): `winmm.dll`** (hallazgo 01). Tiene que reexportar todas las exports de la winmm del sistema. Falta verificar KnownDLLs en el Windows del amigo.
- `AI.exe` tiene ASLR: el mod trabaja siempre con **RVAs**. Para validar la build en runtime: RVA de `ovr_Initialize` = `0xBA83B0`.
- Compatibilidad con ReShade: deseable, no obligatoria.
- La IA del usuario es vanilla (verificado). Quedan 2 COMMANDS.PAK modificados (skip de misión): antes de la fase 5 hay que restaurarlos o que el amigo use los mismos.

## Herramientas
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
| Mod fase 1+2 (proxy winmm + hook + log de posición) | ⏸️ pendiente, a hacer en una sesión con el usuario |
| Hallazgo 04 (cámara y frame) | ✅ `findings/04-camara-y-frame.md` (mapa de tareas, cámara `g_root+0x58`, timer) |
| Hallazgo 10 (NETZ: red del engine) | 🟡 partes 1–2 en `findings/10-netz-red-del-engine.md` (dormida: sin transporte ni proveedor; canal por personaje) |
| Hallazgo 05 (creación de personajes) | 🟡 partes 1–2 en `findings/05-creacion-de-personajes.md` (FinishSpawning, controlador de jugador, request y spawner de script) |
| Hallazgo 08 (IA y múltiples jugadores) | 🟡 parte 1 en `findings/08-ia-y-multiples-jugadores.md` (IA mixta) |
| Hallazgo 09 (niveles/checkpoints/guardado) | 🟡 parte 1 en `findings/09-niveles-checkpoints-guardado.md` |
| Hallazgo 06 (scripts/entidades) | 🟡 parte 1 en `findings/06-scripts-y-entidades.md` |
| Hallazgo 07 (items/inventario) | 🟡 parte 1 en `findings/07-items-e-inventario.md` |
| Hallazgo 08 parte 2 (árboles del Alien) | ✅ `TargetIsPlayer` mira el controlador `+0x31C`, no `GetPlayer()` |
| Hash del AI.exe del amigo | ⏳ cuando reinstale (no bloquea hasta la fase 3) |
| KnownDLLs del amigo | ⏳ `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"` |

## Resumen para el usuario (estado al cierre de la sesión autónoma del 2026-11-05)
- Hallazgos 01–04 completos; 05–10 con una primera parte cada uno (ver `work/findings/`).
- Descubrimientos que cambian el diseño (todos sin verificar en runtime):
  1. El engine tiene una **capa de red propia (NETZ)**, dormida: no tiene transporte (hallazgo 10).
  2. Hay un **vector de jugadores** (`[[g_root+8]+0x80]`, contador `+0x84`). Cualquier personaje con la definición de jugador entra ahí y recibe un **controlador de jugador** (hallazgos 03 y 05).
  3. La condición del Alien **`TargetIsPlayer` no depende de `GetPlayer()`** (hallazgo 08).
- Herramienta clave: `work/scripts/ghidra/dump.sh` + `Dump.java` (4 s por consulta sobre el proyecto persistente).

## Siguiente paso
1. **Con el usuario:** fase 1+2 del mod (proxy `winmm`, hook en `ModeManager::Run`, log de estado y posición). Es la forma de verificar en runtime todo lo anterior. Requiere su confirmación para compilar, colocar la DLL y ejecutar el juego.
2. Experimentos de runtime prioritarios (cuando haya mod):
   - `GetPlayer`, la posición `+0x70`, el yaw `+0x174`, la cámara `[[g_root+0x58]+0x1DC]`;
   - los contadores del `CharacterManager` (`+0x84`, `+0xD4`, `+0x174`);
   - el transporte NETZ `[[g_root+4]+0x69C]` y su máscara `+0x10/+0x14`.
3. Estático pendiente:
   - 05 parte 2: estructura del request de `SpawnCharacter` `0x7D3B50` y de dónde sale la definición del jugador.
   - 10 parte 2: productores/consumidores de NETZ.
   - 06 parte 2: comportamiento por tipo de entidad.
   - 09 parte 2: dispatcher de los comandos de UI.
   - 07 parte 2: inventario.
   - 04: matrices de vista/proyección.
