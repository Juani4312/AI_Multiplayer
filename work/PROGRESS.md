# PROGRESS — estado para retomar

> Punto de entrada para cada sesión nueva de Claude Code. Leelo junto con `prompt.md` (instrucciones) y `work/NOTES.md` (bitácora con evidencia).
> Mantenelo corto: estado actual, decisiones vigentes y siguiente paso. El detalle va en NOTES.md y findings/.

_Última actualización: 2026-11-05_

## Decisiones vigentes
- Build objetivo: **Windows `AI.exe` PE32 i386**, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`. El usuario juega con Proton 9.0-203 y el amigo en Windows nativo.
- Mod: DLL proxy cross-compilada con `i686-w64-mingw32` (CMake). No usar `dinput8` (no está importada). `dxgi`/`d3d11`/`xinput1_3` están ocupadas por ReShade/MovieFix. Candidata principal: `winmm.dll` (a decidir en la fase 1).
- Compatibilidad con ReShade: deseable, no obligatoria.
- Los mods de datos del usuario (IA/BEHAVIOR, COMMANDS.PAK) no afectan el análisis estático. Antes de la fase 5, los dos jugadores tienen que tener los mismos datos de gameplay (ver NOTES.md).

## Estado por etapa
| Paso | Estado |
|------|--------|
| §3 Identificar build | ✅ hecho |
| Repo GitHub privado | ✅ `github.com/Juani4312/AI_Multiplayer` (gh logueado como Juani4312) |
| Ghidra 12.1.2 (`/opt/ghidra`, pacman) + JDK 21 | ✅ instalados |
| mingw-w64-gcc 16.2.0 (`i686-w64-mingw32-g++`) | ✅ instalado |
| REA `npm ci` | ✅ (deps en `/home/juano/Documents/clones/rea/node_modules`; esbuild postinstall bloqueado por npm 12 allowScripts) |
| REA `npm run build:cached` | ❌ **falla con Node 26.12.1**: hace falta Node 24.22.1 |
| Registrar REA como MCP (scope local) | ⏳ pendiente (después del build) |
| Checklist 4.4 | parcial (ver NOTES.md) |
| Hallazgo 01 (inventario del binario) | ⏳ no empezado |
| Hash del AI.exe del amigo | ⏳ pendiente, lo tiene que pasar el usuario |

## Siguiente paso
1. Conseguir Node 24.22.1 sin tocar el Node del sistema (portable en `~/tools/`, con confirmación del usuario) y recompilar REA: `PATH=<node24>/bin:$PATH npm ci && npm run build:cached`.
2. Registrar el MCP (desde esta carpeta):
   ```
   claude mcp add --scope local rea \
     --env GHIDRA_INSTALL_DIR=/opt/ghidra \
     --env GHIDRA_HEADLESS_MAXMEM=8G \
     --env REA_GHIDRA_STARTUP_TIMEOUT_MS=900000 \
     -- <node> /home/juano/Documents/clones/rea/scripts/rea.mjs mcp
   ```
   El usuario reinicia Claude Code y se verifica con `binary_session {}`.
3. `rea doctor --provider ghidra --json`, abrir `game/AI.exe` y completar el checklist 4.4.
4. Hallazgo 01 → `work/findings/01-inventario.md`.
