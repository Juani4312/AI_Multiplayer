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
| Node 24.22.1 portable | ✅ `~/tools/node-v24.22.1-linux-x64` (SHA-256 verificado contra SHASUMS256.txt). El Node del sistema (26) queda intacto |
| REA build (`npm ci && npm run build:cached` con Node 24) | ✅ REA 6.8.0 compilado |
| `rea doctor --provider ghidra` | ✅ healthy (todos los checks) |
| REA registrado como MCP (scope local) | ✅ `claude mcp get rea` → Connected. Backup previo en `~/.claude.json.bak-before-rea-*` |
| Skill de REA | ⚠️ no instalada (el scope local no la instala). Es opcional |
| Checklist 4.4 | falta: abrir `game/AI.exe` en Ghidra vía REA |
| Hallazgo 01 (inventario del binario) | ⏳ no empezado |
| Hash del AI.exe del amigo | ⏳ pendiente, lo tiene que pasar el usuario |

## Siguiente paso
1. (Sesión nueva, después de reiniciar Claude Code) Verificar que están las tools MCP de REA: llamar `binary_session` con `{}` y leer `result.tool_availability`.
2. Leer las docs de REA que faltan para el análisis: `docs/cli.md`, `docs/native-investigation.md`, `docs/binary-diagnostics.md`, `docs/pe-imports.md`, `docs/mcp-contracts.md` (en `/home/juano/Documents/clones/rea`).
3. Abrir `/home/juano/Documents/Claude/AI_Multiplayer/game/AI.exe` con el provider `ghidra` (el binario es grande; el timeout de arranque es de 15 min). Marcar el checklist 4.4 en NOTES.md.
4. Hallazgo 01 → `work/findings/01-inventario.md`: imports (incluida la carga dinámica de DLLs con LoadLibrary, para confirmar la elección del proxy), strings relevantes, símbolos/RTTI, protecciones.

### Cómo correr REA por CLI (sin MCP)
```
export PATH="$HOME/tools/node-v24.22.1-linux-x64/bin:$PATH" GHIDRA_INSTALL_DIR=/opt/ghidra
node /home/juano/Documents/clones/rea/scripts/rea.mjs <comando> --json
```
