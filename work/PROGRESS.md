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
| Hallazgo 10 (NETZ: red del engine) | 🟡 parte 1 en `findings/10-netz-red-del-engine.md` (capa dormida, sin transporte) |
| Hallazgo 05 (creación de personajes) | 🟡 parte 1 en `findings/05-creacion-de-personajes.md` (FinishSpawning: `def+0x1C` → vector de jugadores) |
| Hallazgo 08 (IA y múltiples jugadores) | 🟡 parte 1 en `findings/08-ia-y-multiples-jugadores.md` (IA mixta) |
| Hallazgo 09 (niveles/checkpoints/guardado) | 🟡 parte 1 en `findings/09-niveles-checkpoints-guardado.md` |
| Hallazgo 06 (scripts/entidades) | 🟡 parte 1 en `findings/06-scripts-y-entidades.md` |
| Hallazgo 07 (items/inventario) | ⏳ siguiente |
| Hash del AI.exe del amigo | ⏳ cuando reinstale (no bloquea hasta la fase 3) |
| KnownDLLs del amigo | ⏳ `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"` |

## Siguiente paso
1. Hallazgo 07 (items e inventario): `pickup_manager.cpp`, tarea `PICKUP_MANAGER_TICK` (`FUN_007ca0b0`), vftables `PickupSpawner`/`AddToInventory`/`InventoryItem` (hallazgo 06).
2. Partes 2 pendientes: 08 (objetivo del Alien), 06 (comportamiento por tipo, enumerar entidades), 05 (request de `SpawnCharacter`), 10 (productores/consumidores NETZ, flag MP), 09 (dispatcher de comandos UI).
3. Mod fase 1+2: pendiente, para hacer junto con el usuario.
