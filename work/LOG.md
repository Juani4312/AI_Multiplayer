# LOG — progreso cronológico

> Entradas cortas, con hora (UTC-3). El estado vigente está en `PROGRESS.md`, la evidencia en `NOTES.md` y `findings/`.
> El usuario pidió (2026-11-05) seguir trabajando sin frenar hasta agotar los tokens diarios: se commitea y pushea en cada bloque.

## 2026-11-05

- 01:05 Sesión nueva. REA MCP verificado (`binary_session`: Ghidra disponible). Hash de `game/AI.exe` verificado.
- 01:08 Docs de REA leídas (pe-imports, binary-diagnostics, ghidra-provider, first-query deadlines, snapshots, project reuse).
- 01:10 Inspecciones portables con REA (imports, exports, load config, certificados, recursos, entropía). Sorpresa: `AI.exe` exporta 101 funciones (libcurl + LibOVR).
- 01:11 Import + auto-análisis de Ghidra en un **proyecto persistente** `game/ghidra/AI_Multiplayer.gpr` (583 s; post-script `ReaGhidraNoReturnFix` de REA).
- 01:15 Strings: RTTI (2077 tipos, 958 `CATHODE`), 21 rutas `__FILE__` del juego, tabla de entidades de script con nodos de multiplayer (`IsMultiplayerMode`, `HostOnlyTrigger`, `SyncOnAllPlayers`, …).
- 01:22 Script de Ghidra `work/scripts/ghidra/Inventario01.java`: uso de cada import y argumentos de `LoadLibrary`/`GetProcAddress`.
- 01:24 REA CLI en modo proyecto (`REA_GHIDRA_PROJECT_PATH`): **~10 s por consulta** (vs ~10 min de import fresco).
- 01:27 Decompilación de funciones sospechosas: `IsDebuggerPresent` solo para asserts y para nombrar threads; breakpoints por hardware en código muerto; el único `VirtualProtect` parchea la DLL de la cámara del DK2. No hay anti-debug ni anti-tamper.
- 01:35 **Hallazgo 01 cerrado** → `findings/01-inventario.md`. Proxy: `winmm.dll` sigue siendo la candidata (falta verificar KnownDLLs en el Windows del amigo).
- 01:45 Herramienta `work/scripts/ghidra/Dump.java` + `dump.sh` (4 s por corrida): `fn:`, `hdr:`, `xref:`, `vt:`, `str:`, `calls:`, `asm:`, `rtti:`.
- 02:05 **Hallazgo 02 cerrado** → `findings/02-game-loop.md`. Loop principal en `FUN_007dae80` (RVA 0x3DB220); `g_root` en `0x16F0C88`; `ModeManager::Run` (RVA 0x3A8460) = un frame; update del nivel `FUN_00790000` (vtable `0x12F43E8`+0x14). ⚠️ `FUN_007cad10` tiene un chequeo de licencia Steam que crashea a propósito: no tocar. Las opciones dev (`skip_frontend`, `level`, …) están desactivadas en retail (`FUN_00d0fb70` = `return 0`).
- 02:40 **Hallazgo 03 cerrado** → `findings/03-jugador.md`. `GetPlayer()` = `FUN_0078baf0` (RVA 0x38BAF0, 180 usos) = `[[g_root+8]+0x80][0]` (¡array de jugadores con contador en +0x84!). `Character`: vtable `0x12F4FC8`, posición `+0x70/74/78` (Y arriba), yaw `+0x174` (rad, [-π,π], verificado con las constantes 1/2π y 2π de `SetTransform` `0x7C2710`). Nuevos comandos de `Dump.java`: `pre:`, `post:`, `both:`, `insn:`, `fieldw:`, `xrefw:`, `funcs:`.
- 02:50 Cambio de plan: la fase 1+2 del mod queda pendiente para una sesión con el usuario (no se implementa en esta). Se extrajo la lista de exports de la winmm de Wine 9 como referencia (`work/evidence/mod/wine9-winmm-exports.json`: 186 con nombre + ordinales 2–4). Sigo con el hallazgo 04 (cámara).
- 03:15 **Hallazgo 04 cerrado** → `findings/04-camara-y-frame.md`. `DATA/TASKS.TXT` = grafo de tareas del frame; `FUN_007d6370` registra ~90 tareas con sus callbacks. Cámara del jugador en `g_root+0x58`, cámara activa `+0x1DC` (cuaternión `+0x2C`, posición `+0x3C`). Timer: dt `0x16410D0`, frames `0x16410DC`. **NETZ**: tarea `NETZ_DISPATCH` → `[g_root+0x04]->FUN_007dde20()` y `NET_AI_UPDATE` → capa de red del engine (nuevo hallazgo 10).
- 03:45 **Hallazgo 10 (NETZ) cerrado en su primera parte** → `findings/10-netz-red-del-engine.md`. NETZ = `g_root+0x04` (ctor `0x7E1CB0`, 32 canales, dispatch por frame); **transporte `+0x69C` nunca asignado en retail** (solo el ctor lo pone en 0) → dormida. Tabla de 26 tipos de mensaje (logic/display character, triggers de entidades, física, disparos). Los mensajes "Server created/new client" son del Havok Visual Debugger (descartados). `Root::Init` (`0x7DA540`) mapea todos los campos de `g_root`; el gestor de jugadores (`+0x08`, 0x9FF0 bytes) recibe NETZ.
