# PROGRESS — estado para retomar

> Punto de entrada para cada sesión nueva de Claude Code. Leelo junto con `prompt.md` (instrucciones), `work/NOTES.md` (bitácora con evidencia) y `work/LOG.md` (log cronológico corto).
> Mantenelo corto: estado actual, decisiones vigentes y siguiente paso. El detalle va en NOTES.md y findings/.

_Última actualización: 2026-11-05, tarde (pruebas en la laptop Windows: fases 1–3 verificadas en Windows nativo y por Tailscale)_

## Decisiones vigentes
- Build objetivo: **Windows `AI.exe` PE32 i386**, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`. El usuario juega con Proton 9.0-203 y el amigo en Windows nativo.
- Mod: DLL proxy cross-compilada con `i686-w64-mingw32` (CMake). **Proxy elegido (provisorio): `winmm.dll`** (hallazgo 01). Tiene que reexportar todas las exports de la winmm del sistema. Falta verificar KnownDLLs en el Windows del amigo.
- `AI.exe` tiene ASLR: el mod trabaja siempre con **RVAs**. Para validar la build en runtime: RVA de `ovr_Initialize` = `0xBA83B0`.
- Compatibilidad con ReShade: deseable, no obligatoria.
- La IA del usuario es vanilla (verificado). **Solo `BSP_LV426_PT01/WORLD/COMMANDS.PAK` está modificado** (comparado con la laptop, que tiene el de Steam; `TECH_COMMS` es el original). Antes de la fase 5 hay que restaurarlo o que el amigo use el mismo (NOTES, "COMMANDS.PAK: solo uno").

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
| Mod fase 1+2 (proxy winmm + hook + log de posición) | ✅ **verificado en runtime** (2026-11-05, Proton): fase 1 (código por frame) y fase 2 (posición, yaw, cámara). ✅ **También en Windows 10 nativo** (laptop de pruebas: 195 exports resueltos y 10 ausentes que el juego no usa, ASLR reubica `AI.exe` y anda igual; `work/windows/report.md`) |
| Mod fase 3 (UDP mínimo con la posición) | ✅ **verificada por Tailscale** (2026-11-05): laptop Windows 10 nativo (join) ↔ papito Proton (host). Los dos sentidos, 0 perdidos, y las posiciones recibidas coinciden con las marcas F8 del otro lado. Mejoras menores pendientes: ritmo real ~16/s en vez de 20 y backlog del socket antes del primer frame |
| Hallazgo 12 (avatar remoto: pedido de spawn y teleport) | 🟡 `findings/12-avatar-remoto.md`: flujo del spawn de autoridad (`0x797490`), layout del pedido de 0x130 bytes y teleport (`0x7C6330`). Falta runtime |
| Mod fase 4 (avatar del otro jugador) | ✅ **v1 por red OK** (corrida 9, Windows nativo ↔ Proton): en cada máquina un NPC humano del nivel sigue al jugador de la otra (`AIMP_GHOST=3`; `mod/src/ghost.c`). Clonar personajes hace caer el juego (hallazgo 12, parte 2) |
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
- El mod quedó instalado en el juego con el trazado activo (`aimp.cfg`). `work/scripts/runtime_setup.sh revert` lo saca todo.

## Pendientes que requieren al usuario
0. ~~Tailscale para cerrar la fase 3~~: hecho con la laptop. papito es 100.126.161.126 y papulaptop, 100.126.159.45. Para el amigo:
   - instala Tailscale y se une a la red del usuario;
   - el host pone `AIMP_NET=host` y el que se une, `AIMP_NET=join` y `AIMP_NET_PEER=<IP de Tailscale del host>:47811` (en `aimp.cfg`);
   - con dos máquinas y **una sola cuenta de Steam**, una de las dos tiene que estar en modo sin conexión: Steam no deja correr el juego en dos PCs a la vez.
1. **Corrida del experimento de la fase 4** (ver "Siguiente paso" 2): unos 2 minutos en la máquina Linux. Puede cerrar el juego si el clon rompe algo.
   - (Opcional) corrida corta con marcas F8 para separar puerta e ítem (instrucciones en el hallazgo 11, "Pendiente").
2. Aprobar (o no) que el MCP de REA use el proyecto de Ghidra.
3. KnownDLLs en el Windows del amigo; hash de su `AI.exe` cuando reinstale; que use el mismo COMMANDS.PAK (mod de saltear la misión 9) o los dos el vanilla.

## Siguiente paso
1. **Fase 3:** ✅ cerrada por Tailscale (Windows nativo ↔ Proton). Mejoras chicas en `mod/src/net.c`:
   - mantener 20 paquetes/s con `last_send += send_ticks` (hoy salen ~16/s a 60 fps);
   - en el primer `net_poll`, vaciar el backlog sin contar huecos (hoy da 361 "perdidos" falsos y un estado viejo de 35 s).

   ENet queda para la fase 5, si hacen falta eventos confiables; descargarlo requiere confirmación.
2. **Fase 4b, que el avatar se vea bien:**
   - probar `8bc1f7e`, que vuelve a poner al NPC si su IA lo corre;
   - apagar la IA del NPC elegido (que no camine, no huya ni ataque) y pasarle la animación de caminar cuando el remoto se mueve;
   - elegir el NPC lejos del jugador local y lejos de los scripts.
   Después: crear un personaje propio (reutilizando la clave de uno liberado) y pasar a la fase 5 (mundo compartido: puertas e ítems con el RPC 0x26 del hallazgo 11).
3. Investigar qué escribe en los canales NETZ durante el juego: posible camino para el movimiento de personajes.
4. Estático pendiente: semántica de los RPCs 0x1E/0x20/0x22/0x23/0x28 y del B 0x07; dispatcher de los comandos de UI (hallazgo 09); inventario (07).
