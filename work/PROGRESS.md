# PROGRESS — estado para retomar

> Punto de entrada para cada sesión nueva de Claude Code. Leelo junto con `prompt.md` (instrucciones), `work/NOTES.md` (bitácora con evidencia) y `work/LOG.md` (log cronológico corto).
> Mantenelo corto: estado actual, decisiones vigentes y siguiente paso. El detalle va en NOTES.md y findings/.

_Última actualización: 2026-11-05, tarde (sesión autónoma: el usuario pidió seguir hasta usar ~70 % de los tokens)_

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
| Mod fase 1+2 (proxy winmm + hook + log de posición) | 🟡 **implementado y probado fuera del juego** (`mod/`, tests con wine OK). Falta la corrida en el juego (requiere al usuario) |
| Hallazgo 04 (cámara y frame) | ✅ `findings/04-camara-y-frame.md` (mapa de tareas, cámara `g_root+0x58`, timer) |
| Hallazgo 10 (NETZ: red del engine) | 🟡 partes 1–7 en `findings/10-netz-red-del-engine.md` (sin transporte ni proveedor; canal por personaje; cola A en loopback; **cola B = salida a la red, descartada en retail**; registro de handlers de la cola A) |
| Hallazgo 05 (creación de personajes) | 🟡 partes 1–2 en `findings/05-creacion-de-personajes.md` (FinishSpawning, controlador de jugador, request y spawner de script) |
| Hallazgo 08 (IA y múltiples jugadores) | 🟡 parte 1 en `findings/08-ia-y-multiples-jugadores.md` (IA mixta) |
| Hallazgo 09 (niveles/checkpoints/guardado) | 🟡 parte 1 en `findings/09-niveles-checkpoints-guardado.md` |
| Hallazgo 06 (scripts/entidades) | 🟡 parte 1 en `findings/06-scripts-y-entidades.md` |
| Hallazgo 07 (items/inventario) | 🟡 parte 1 en `findings/07-items-e-inventario.md` |
| Hallazgo 08 parte 2 (árboles del Alien) | ✅ `TargetIsPlayer` mira el controlador `+0x31C`, no `GetPlayer()` |
| Hash del AI.exe del amigo | ⏳ cuando reinstale (no bloquea hasta la fase 3) |
| KnownDLLs del amigo | ⏳ `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"` |

## Resumen para el usuario (cierre de la sesión de la tarde del 2026-11-05)
- **Mod de la fase 1+2 listo y probado fuera del juego** (`mod/`, ver `mod/README.md`). Es un proxy `winmm.dll` sin CRT con un hook en `ModeManager::Run`. Loguea a `aimp.log`, solo leyendo:
  - la posición y el yaw del jugador, la cámara, el estado y el nivel;
  - NETZ, la sesión y las dos colas de mensajes;
  - el catálogo de endpoints RPC.
  Opcional: `AIMP_TRACE_QUEUES=1` traza tipo, tamaño y call site de cada mensaje. Tests con wine: OK. Analizador del log: `work/scripts/analyze_aimp_log.py`.
- **Hallazgos nuevos** (estáticos):
  - la cola B es la salida a la red de la sesión y en retail se descarta en cada frame (hallazgo 10, parte 6);
  - `FUN_00a2a2b0` registra los handlers de la cola A desde una lista estática, con ids iguales entre máquinas (parte 7);
  - corrección: la lista de niveles tiene doble indirección (hallazgo 09).
- **Bloqueante:** el disco de 1,8 TB no está montado (NTFS "dirty"). Hasta resolverlo no se puede correr el juego.
- Detalle: el primer `git push -q` después de un commit falló dos veces con "failed to push some refs"; el reintento sin `-q` funcionó las dos. No se investigó.

## Pendientes que requieren al usuario (en orden)
1. **Montar el disco de 1,8 TB** (`sda1`): no se montó en el arranque porque el NTFS quedó "dirty" (detalle en `NOTES.md`, sección "Mod fase 1+2"). Opción segura: chkdsk desde Windows; opción rápida: `sudo ntfsfix -d /dev/sda1 && sudo mount /mnt/1,8TB`.
2. **Primera corrida del mod**: copiar `mod/build/winmm.dll` al lado de `AI.exe`, poner `WINEDLLOVERRIDES="winmm=n,b" %command%` en Steam y seguir el checklist de `mod/README.md` ("Primera prueba"). Después revisar `aimp.log` contra `work/RUNTIME_PLAN.md`.
3. Aprobar (o no) que el MCP de REA use el proyecto de Ghidra (ver arriba).
4. KnownDLLs en el Windows del amigo; hash de su `AI.exe` cuando reinstale.
5. Comparar el `COMMANDS.PAK` suelto del disco raíz (`/mnt/1,8TB/COMMANDS.PAK` con el disco desmontado; SHA-256 `dda19f70…8a47f`) con los del backup.

## Siguiente paso
0. Leer `mod/README.md`, `work/RUNTIME_PLAN.md` y `work/DESIGN_OPTIONS.md`.
1. **Con el usuario:** primera corrida del mod (pendiente 2). El log responde los bloques 0–3 del plan de runtime: offsets base, jugador, cámara, NETZ, crecimiento de las colas A y B y catálogo de endpoints RPC. Analizarlo con `python3 -I work/scripts/analyze_aimp_log.py <aimp.log>`. Con eso se cierra la fase 2 y se decide la arquitectura (A/B/C de `DESIGN_OPTIONS.md`).
2. Después de esa corrida:
   - si las colas crecen por frame, segunda corrida con `AIMP_TRACE_QUEUES=1` (experimento 3.6: tipo, tamaño y call site de cada mensaje; el catálogo tipo → handler, 3.5, ya sale en la primera corrida). Con eso, mapear offline cada tipo a su productor y su handler;
   - fase 3: UDP mínimo con la posición.
3. Estático pendiente:
   - 05 parte 2: definición del jugador para el spawn de un segundo personaje;
   - 10: lado de recepción de la sesión (quién despacha lo que entregaría el proveedor) y `FUN_00a2a520`;
   - 06 parte 2, 09 parte 2 (dispatcher de comandos de UI), 07 parte 2 (inventario);
   - 04: matrices de vista/proyección (mejor en runtime).
