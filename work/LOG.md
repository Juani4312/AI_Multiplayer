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
