# Bitácora

## 2026-11-05 — Identificación de build (sección 3)

### Hechos verificados (`file`, `sha256sum`, `objdump -p/-h`)
- Instalación: `/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation` (Steam appid 214490, buildid 4940658).
- Ejecutable principal: `AI.exe` — `PE32 executable for MS Windows 5.01 (GUI), Intel i386`. **32 bits.**
  - SHA-256: `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`
  - PE timestamp 0x54E3199D (2015-02-17 10:36:13 UTC), ImageBase 0x00400000, SizeOfImage 0x01F81000. *(Corregido el 2026-11-05: antes decía 0x54E3199B, que es el timestamp del export directory.)*
  - DllCharacteristics 0x8140: DYNAMIC_BASE, NX_COMPAT, TS_AWARE. Tiene `.reloc` (0x21A0CA bytes).
  - Secciones: `.text .rdata .data .rsrc .reloc` — no hay sección `.bind` (no se ve wrapper SteamStub). Hipótesis, no verificado a fondo.
  - Imports: steam_api, KERNEL32, USER32, WS2_32, PSAPI, WSOCK32, imagehlp, SETUPAPI, dxgi, d3d11, XINPUT1_3, GDI32, SHELL32, ole32, WINMM, ADVAPI32, WLDAP32.
- No hay build nativa Linux (ELF) instalada.
- Proton: prefijo en `/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490`, versión `9.0-203` (Proton 9.0 Beta).

### Mods/DLLs de terceros ya presentes en la instalación (afectan la carga del mod)
| Archivo | SHA-256 | Qué es |
|---------|---------|--------|
| `d3d11.dll` | 71a8b86d…fac42 | AlienIsolationMovieFix v1.1.0.0 (emoose), proxy |
| `xinput1_3.dll` | 71a8b86d…fac42 (idéntico) | mismo MovieFix |
| `dxgi.dll` | b63df921…0658b | ReShade 6.7.3 (32-bit) |
- `aimoviefix.log` reporta EXE base 0x400000 bajo Wine (la exe se carga en su ImageBase preferida en esa corrida; no asumir que siempre es así → usar RVAs).
- Consecuencia: los slots proxy `dxgi`, `d3d11`, `xinput1_3` están ocupados. `dinput8.dll` **no** está en la tabla de imports de AI.exe (no se cargaría como proxy salvo carga dinámica, sin verificar). Candidatos estáticos libres: `WINMM.dll`, `PSAPI.DLL`, `imagehlp.dll`, `WSOCK32.dll`, `SETUPAPI.dll`, `WLDAP32.dll`.

### Entorno
- Node v26.12.1 (OK, ≥26.12.1). `ps` de procps-ng 4.0.7 (OK).
- Disponibles: git, gh (sin login), cmake, objdump. mingw-w64: pendiente verificar.

### Preguntas abiertas
- ¿Con qué build/SO juega el amigo? ¿Tiene el mismo hash de AI.exe?
- ¿El amigo tiene también MovieFix/ReShade? (no es requisito, pero cambia qué proxy usar en su máquina).
- ¿Hay carga dinámica de dinput8/version.dll? (verificar con REA, strings de LoadLibrary).

## 2026-11-05 — Setup de REA (sección 4)

REA local: `/home/juano/Documents/clones/rea`, versión 6.8.0 (commit 71f680d7c). **Sin build**: no hay `node_modules/` ni `dist/`.
Docs leídas: README, AGENTS, DISCLAIMER, installation (setup, local scope, readiness, Linux ps, Ghidra, heap), CONTRIBUTING (source build).

Requisitos según la doc:
- Ghidra 12.x (>=12.1), verificado por REA con 12.1.4 + JDK 21. Variables: `GHIDRA_INSTALL_DIR`, opcional `JAVA_HOME`, `GHIDRA_HEADLESS_MAXMEM` (default 2G), `REA_GHIDRA_STARTUP_TIMEOUT_MS`.
- Build desde el código fuente: `npm ci && npm run build:cached`. El toolchain de desarrollo fijado es Node 24.22.1 (`.nvmrc`) y npm 11.16.0. Acá hay Node 26.12.1 y npm 12.2.1: el runtime está soportado, pero el build de dev con esa versión no está verificado.
- Registro solo para este proyecto (sin `--scope project`, para no commitear `.mcp.json`):
  `claude mcp add --scope local rea --env ... -- /usr/bin/node /home/juano/Documents/clones/rea/scripts/rea.mjs mcp`

### Checklist 4.4
- [x] Node v26.12.1 (>=26.12.1)
- [x] `ps` procps-ng 4.0.7
- [ ] REA responde (falta el build)
- [ ] Ghidra arranca y abre `game/AI.exe` (Ghidra no está instalado; hay JDK 21 en /usr/bin/java)
- [x] Hash registrado: dde66521…19a90 (copia en `game/AI.exe`, verificada)

Falta también mingw-w64 (i686) para compilar el mod: el paquete `mingw-w64-gcc` 16.2.0 está en extra.

## 2026-11-05 — Decisión de plataforma y mods instalados

**Decisión (usuario):** los dos usan la build de Windows. El usuario la corre con Proton y el amigo en Windows nativo, así que hay un solo set de offsets (PE32 i386).
- La carga del mod sería la misma DLL proxy para los dos. En Windows no hace falta override si la DLL elegida no está en KnownDLLs (hipótesis, verificar para winmm). En Proton hace falta `WINEDLLOVERRIDES="<dll>=n,b" %command%`.
- Compatibilidad con ReShade: deseable, no obligatoria.
- Pendiente: el hash de `AI.exe` del amigo. Si coincide con dde66521…, también es buena evidencia de que el exe es vanilla.

**Mods detectados en la instalación del usuario** (por mtime distinto de la fecha de instalación 2026-06-06, y por `.backup/`). Ninguno toca `AI.exe`:
| Fecha | Archivos | Tipo |
|-------|----------|------|
| 2026-07-14 | `DATA/ALIENCONFIGS/*.BML`, `DATA/BEHAVIOR/*.XML` (~100 árboles de IA), `DATA/CHR_INFO/ATTRIBUTES/*.BML` (incluye ALIEN, THE_PLAYER), `BINARY_BEHAVIOR/_DIRECTORY_CONTENTS.BML` | **Gameplay/IA** (no visual) |
| 2026-07-03 | ~118 copias sueltas de esos mismos XML/BML en la raíz de `DATA/` | Probablemente extraídas por error. No verificado si el juego las lee |
| 2026-01-11 / 2026-10-05 | `BSP_LV426_PT01/WORLD/COMMANDS.PAK`, `TECH_COMMS/WORLD/COMMANDS.PAK` | **Scripts Cathode** (¿el skip de misión?) |
| 2026-10-04/05 | `LEVEL_TEXTURES*.PAK`, headers, `UI/MOVIES/*.USM`, `FONT_CONFIG`, `LENS_FLARE_CONFIG` | Visual |
| varias | `UI.PAK` (con backup), `ENGINE_SETTINGS.XML`, ReShade, MovieFix | Visual/UI |

Implicaciones:
- Análisis estático (hallazgos 01–09): solo importa `AI.exe` → no hace falta copia limpia.
- Runtime y coop: los dos tienen que tener **los mismos** COMMANDS.PAK, BEHAVIOR, ALIENCONFIGS y ATTRIBUTES. Si no, los scripts y la IA divergen. Los mods visuales dan igual.
- Para depurar el hallazgo 08 (IA del Alien) conviene tener la IA vanilla, para no confundir cambios del mod con comportamiento del engine.

## 2026-11-05 — Instalación de herramientas y build de REA
- Instalados por el usuario: `ghidra 12.1.2-2` (`/opt/ghidra`; application.java.min=21, decompiler `linux_x86_64` presente) y `mingw-w64-gcc 16.2.0-2`.
- `npm ci` en el repo de REA: OK (739 paquetes). npm 12 bloqueó el postinstall de esbuild (allowScripts). No está claro si afecta.
- `npm run build:cached`: **FALLA** con Node 26.12.1:
  `TypeError [ERR_INVALID_ARG_VALUE]: The property 'options.mode' must be one of: 'strip'. Received 'transform'` en `scripts/lib/verifier-source-loader.mjs:98` (tarea `metadata:check`). Node 26 eliminó el modo `transform` de `stripTypeScriptTypes`. Hace falta Node 24.22.1 (`.nvmrc`).
- npm publicado: `rea-agents@latest` = 6.8.0 (2026-10-25), la misma versión que el repo local (commit 71f680d7c, posiblemente con cambios posteriores al release).
- Solución: Node 24.22.1 portable en `~/tools/node-v24.22.1-linux-x64` (tarball oficial, `sha256sum -c` OK). Con eso `npm ci && npm run build:cached` → 3/3 tareas OK.
- `rea doctor --provider ghidra --json` → healthy (node, ghidra, platform, architecture, installation, version, headless, native_decompiler, java).
- MCP registrado: `claude mcp add --scope local rea --env GHIDRA_INSTALL_DIR=/opt/ghidra --env GHIDRA_HEADLESS_MAXMEM=8G --env REA_GHIDRA_STARTUP_TIMEOUT_MS=900000 -- ~/tools/node-v24.22.1-linux-x64/bin/node /home/juano/Documents/clones/rea/scripts/rea.mjs mcp` → Connected. Backup de `~/.claude.json` hecho antes.

### Checklist 4.4 (actualizado)
- [x] Node (24.22.1 para REA)
- [x] `ps` procps-ng
- [x] REA responde (`--version` 6.8.0, doctor healthy, MCP connected)
- [ ] Ghidra abre `game/AI.exe` (siguiente sesión)
- [x] Hash registrado

## 2026-11-05 — Estado de la instalación después de "reinstalar/verificar"
- El usuario cree haber reinstalado y verificado la integridad. **La evidencia dice que no** (por lo menos no después de instalar los mods): `AI.exe` sigue con hash dde66521…, appmanifest `LastUpdated` 1780718329 (2026-06-06) y buildid 4940658, ningún archivo modificado hoy, y los ~330 archivos de mods (14/07, 05/10, etc.) siguen con sus fechas. Un "verificar integridad" habría restaurado los BEHAVIOR/COMMANDS.PAK modificados.
- El amigo desinstaló el juego. Cuando lo reinstale: hash de AI.exe con `certutil -hashfile AI.exe SHA256`. Esperado: dde66521…19a90 (mismo buildid 4940658).

## 2026-11-05 — CORRECCIÓN: los datos de IA están vanilla
El usuario usó "Insane AI" en algún momento y cree haber verificado la integridad después. Evidencia (`cmp` byte a byte contra `.backup/`, cuyos archivos tienen la fecha de instalación 2026-06-06):
- 3709 de 3710 archivos de `.backup/DATA` son **idénticos** a los actuales. Solo difiere `ENGINE_SETTINGS.XML` (gráficos).
- `ALIENCONFIGS/`, `BEHAVIOR/` y `CHR_INFO/ATTRIBUTES/` son idénticos a la backup. El mtime 2026-07-14 corresponde a la restauración (verificación de Steam o desinstalación del mod), no a un mod activo. **La tabla anterior que los marcaba como "mod de IA" estaba mal.**
- Quedan restos: 24 de los ~115 XML/BML sueltos en la raíz de `DATA/` (fecha 2026-07-03) **difieren** de sus equivalentes vanilla. Probablemente son de Insane AI. Hipótesis: el engine los lee desde `DATA/BEHAVIOR/` etc. y no desde la raíz. No verificado; se puede comprobar en el hallazgo 06/08 viendo qué rutas abre el exe.
- `COMMANDS.PAK` de `BSP_LV426_PT01` (mtime 2026-01-11) y de `TECH_COMMS` (2026-10-05) no tienen backup, así que no se pueden verificar. Son compatibles con el skip de misión.
- Steam "verify integrity" compara cada archivo con los hashes del manifiesto del depot y re-descarga los que no coinciden. **No borra** archivos extra (DLLs de ReShade/MovieFix, sueltos en DATA/, `.backup/`, el `.rar`).

## 2026-11-05 — Limpieza de la carpeta del juego (autorizada por el usuario)
Backup en `/mnt/1,8TB/SteamLibrary/AlienIsolation_backup_AI_Multiplayer/`, fuera de steamapps. Las instrucciones para recuperar están en `LEEME.txt`.
- **Movidos** fuera del juego: los 116 archivos sueltos de la raíz de `DATA/` con fecha 2026-07-03 (restos de Insane AI y el `.rar` de No HUD). Ninguno existe en `.backup/DATA`, así que no son originales. Verificados con sha256 después del `mv`.
- **Copiados** (siguen en su lugar): los dos `COMMANDS.PAK` modificados. Sirven por si se hace "verificar integridad" más adelante.
- Sin tocar: ReShade, MovieFix, texturas, UI.PAK, películas, ENGINE_SETTINGS.XML, `.backup/`, `AI.exe`.
- Estado de gameplay: la IA es vanilla. Lo único de gameplay no verificado son los 2 COMMANDS.PAK (skip de misión). **Pendiente antes de la fase 5:** decidir si se restauran los originales (verificar integridad + reinstalar mods visuales) o si el amigo usa los mismos COMMANDS.PAK.

## 2026-11-05 — Proyecto de Ghidra persistente y hallazgo 01

### Proyecto de Ghidra persistente (checklist 4.4 completo)
- Para no re-analizar en cada sesión (un import fresco de REA tarda ~10 min con este binario), se creó un proyecto persistente, siguiendo `docs/installation.md` → "Reuse an existing Ghidra project":
  `GHIDRA_HEADLESS_MAXMEM=10G /opt/ghidra/support/analyzeHeadless game/ghidra AI_Multiplayer -import game/AI.exe -max-cpu 12 -scriptPath <READir>/bridge/ghidra -postScript ReaGhidraNoReturnFix.java`
  Es el mismo import que hace REA (auto-análisis por defecto + su post-script de no-return). Auto-análisis: **583 s**. NoReturnFix: `cleared=5 sites=106 refit=102`. Proyecto: `game/ghidra/AI_Multiplayer.{gpr,rep}` (437 MB, gitignored). Log en `work/evidence/ghidra-import/`.
- Ghidra crea su config en `~/.config/ghidra` y su caché en `/var/tmp/juano-ghidra/` (normal al usarlo).
- **Uso por CLI** (sin tocar la config del agente), **~10 s por consulta**:
  ```
  export PATH="$HOME/tools/node-v24.22.1-linux-x64/bin:$PATH" GHIDRA_INSTALL_DIR=/opt/ghidra GHIDRA_HEADLESS_MAXMEM=8G \
    REA_GHIDRA_PROJECT_PATH=/home/juano/Documents/Claude/AI_Multiplayer/game/ghidra/AI_Multiplayer.gpr REA_GHIDRA_PROGRAM_NAME=AI.exe
  node /home/juano/Documents/clones/rea/scripts/rea.mjs decompile /home/juano/Documents/Claude/AI_Multiplayer/game/AI.exe 0x... --provider ghidra --format json
  ```
  REA copia el `.gpr`/`.rep` a su runtime y abre la copia en `-readOnly`: el proyecto original no se toca. Modo de solo lectura: no hay renames vía REA.
- **El MCP de REA actual NO usa el proyecto** (arrancó sin `REA_GHIDRA_PROJECT_PATH`). Un `open_binary` por MCP haría otro import fresco de ~10 min. Propuesta para el usuario: ver `PROGRESS.md` (requiere su confirmación porque cambia la config del agente).
- Scripts de Ghidra propios (Java, en `work/scripts/ghidra/`), se corren sobre el proyecto con `-process AI.exe -noanalysis -readOnly`.

### Checklist 4.4
- [x] Ghidra abre `game/AI.exe` (import + análisis OK; REA lo abre en modo proyecto: `rea analyze` → 67.334 procedimientos, 31.817 strings, 7 segmentos, base 0x400000).

### Hallazgo 01 (resumen; detalle en `findings/01-inventario.md`)
- Toolchain: linker 10.0 (VS2010), CRT estático, LARGE_ADDRESS_AWARE, ASLR + DEP + /GS + SafeSEH (2391), sin CFG, sin TLS, sin firma. PDB `E:\Console\Moon-Branches\BUILD\game\pc\AI.pdb` (no distribuido).
- No hay packer, SteamStub, anti-debug ni anti-tamper (`IsDebuggerPresent` solo para asserts/nombres de threads; breakpoints por hardware en código muerto; el único `VirtualProtect` parchea la DLL de la cámara del DK2).
- Exports: 57 de libcurl + 44 de LibOVR. Sirven como huella de build en runtime (`ovr_Initialize` RVA 0xBA83B0).
- Carga dinámica: solo middleware (AMD AGS, NVAPI, LibOVR/HID/OpenGL/D2D/cámara, Havok dbghelp, libcurl ws2_32, CRT). No hay `dinput8`/`version`. `CRASHREPORTER32`/`ATIAGS` no se nombran en el EXE.
- Proxy: **`winmm.dll`** (3 funciones usadas: `timeGetTime`, `timeBeginPeriod`, `timeEndPeriod`). Falta verificar KnownDLLs en el Windows del amigo.
- RTTI: 2077 tipos, 958 en `CATHODE`. 21 rutas `__FILE__` del juego (`menace_manager.cpp`, `character_playable.cpp`, `main.cpp`, …).
- **Restos de multiplayer en el sistema de scripts de Cathode** (`IsMultiplayerMode`, `HostOnlyTrigger`, `SyncOnAllPlayers`, `NetPlayerCounter`, `NetworkedTimer`, `RespawnConfig`, …) y un log que menciona "okay in multiplayer". Hipótesis a investigar en el hallazgo 06.
- Pistas para el hallazgo 02: `SteamAPI_RunCallbacks` se llama desde `FUN_00a58eb0`, que se referencia desde una vtable (`0x13E9B5C`). `timeBeginPeriod(1)` en `FUN_00d1a7d0`, llamado desde `0xD06F0D`.

### Preguntas abiertas nuevas
- KnownDLLs del amigo: `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"` (`winmm` no tiene que estar).
- ¿Se cargan `CRASHREPORTER32.DLL`/`ATIAGS.DLL` en runtime?
- ¿Qué CLSIDs crean los 4 `CoCreateInstance`?

## 2026-11-05 — Hallazgo 02 (game loop)
Detalle en `findings/02-game-loop.md`. Evidencia en `work/evidence/h02/` (salidas de `Dump.java`).
- Cadena: entry `0xA71E61` → `___tmainCRTStartup` → `WinMain` `0xA111A0` → main del engine `0x7DAE80` (`main/main.cpp`).
- Loop principal en `0x7DB220`–`0x7DB25E`: `GameFlow::Tick` (`0x7CAD10`, ECX=`[g_root+0x48]`) + `ModeManager::Run` (`0x7A8460`, ECX=`[g_root+0x3C]`, devuelve "seguir"). `g_root` = `[0x16F0C88]`.
- `ModeManager`: anillo de 16 modos (`+0x00`), actual `+0x40`, índice `+0x44`, estado `+0x4C` (5 = corriendo → `modo->vf[+0x14]`). El modo de nivel tiene la vtable `0x12F43E8` sin RTTI; su update es `FUN_00790000`.
- **Las clases núcleo del engine no tienen RTTI.** Las 958 clases RTTI de `CATHODE` son casi todas tipos de entidad de script.
- ⚠️ Chequeo de licencia de Steam en `FUN_007cad10` (casos 3/7: crash deliberado escribiendo en NULL). No tocar.
- Opciones dev desactivadas en retail (`FUN_00d0fb70` = `return 0`, plegada por ICF). Funcionan: `monitor`, `adapter`, `benchmark`, `engine_settings`.
- Hook recomendado para la fase 1: `ModeManager::Run` (RVA `0x3A8460`, prólogo `51 53 55 56 8B F1 83 7E 4C FF`).

## 2026-11-05 — Hallazgo 03 (jugador)
Detalle en `findings/03-jugador.md`. Evidencia en `work/evidence/h03/`.
- `GetPlayer()` = `FUN_0078baf0`: `[[g_root+0x08]+0x80][0]` si `[[g_root+0x08]+0x84] != 0`. Recibe un índice que no usa. **El engine tiene un array de jugadores con contador.**
- `Character`: vtable `0x12F4FC8` (sin RTTI), constructor `0x7B2390`. Posición `+0x70/+0x74/+0x78` (Y vertical). Yaw `+0x174` en radianes [-π, π] (normalizado en `SetTransform` `0x7C2710` con 1/2π y 2π). `+0x64` = definición (tipo en `+4`).
- Vínculo estático: después de `CALL GetPlayer` se lee `[EAX+0x70..0x78]` y `[EAX+0x174]` (ver `d31.txt`).
- Pendiente (hallazgo 05): quién llena el array de jugadores y asigna `g_root+0x08`.

## 2026-11-05 — Hallazgo 04 (cámara y frame)
Detalle en `findings/04-camara-y-frame.md`. Evidencia en `work/evidence/h04/`; copia de `DATA/TASKS.TXT` en `game/data/`.
- `DATA/TASKS.TXT` define el grafo de tareas por frame (thread principal + 3 workers en PC). `FUN_007d6370` registra cada nombre con su callback (`FUN_00a1a220`).
- Cámara del jugador: `[g_root+0x58]`; cámara activa `[+0x1DC]` con cuaternión en `+0x2C` y posición en `+0x3C` (por uso).
- Timer: dt real `0x16410D0` (double), dt escalado `0x16410C0`, tiempo de juego `0x16410A8`, contador de frames `0x16410DC`.
- Renderer singleton `[0x1766A00]` (array de POVs en `+0x08`). Matrices de vista/proyección: pendiente.
- **NETZ**: `NETZ_DISPATCH` → `[g_root+0x04]->FUN_007dde20()`; `NET_AI_UPDATE` → `FUN_007dfd70()` + `[0x17073FC].FUN_007dd970()`. Capa de red del engine → hallazgo 10.

## 2026-11-05 — Hallazgo 10 (NETZ, primera parte)
Detalle en `findings/10-netz-red-del-engine.md`. Evidencia en `work/evidence/h10/`.
- `Root::Init` = `FUN_007da540`. Campos de `g_root`: `+0x04` NETZ, `+0x08` gestor de personajes/jugadores (`FUN_007da2d0` → ctor `FUN_007d9260`, 0x9FF0 bytes, recibe NETZ), `+0x3C` ModeManager, `+0x48` GameFlow, `+0x4C` `FUN_007e10d0`, `+0x58` cámara, `+0x90` `script_readable_variables.xml`, entre otros.
- NETZ: ctor `0x7E1CB0`, vtable `0x12F74FC`, 32 canales de 0x30 bytes en `+0x18`, máscara de canales sucios en `+0x10/+0x14`, transporte en `+0x69C` (`vf[1]` = enviar). **Solo el ctor escribe `+0x69C` (en 0)**: no hay transporte en retail.
- Tabla de nombres de mensajes de red en `0x16413B0` (26 tipos; logic/display character, entity manager de cliente, física, disparos, release npc/player).
- "Server created … clients" = Havok Visual Debugger (no es multiplayer).

## 2026-11-05 — Hallazgo 05 (creación de personajes, parte 1)
Detalle en `findings/05-creacion-de-personajes.md`. Evidencia en `work/evidence/h05/`.
- **Corrección al hallazgo 03:** la vtable de un personaje en runtime es `0x12F5D80` (constructor derivado `0x7CEC20`); `0x12F4FC8` es la de la clase base, que se pisa.
- API del `CharacterManager` (nombres de los `strncpy` a `0x16F17F8`): `request_character` `0x7D0470`, `finish_spawning` `0x7B80C0`, `release_player` `0x7999B0`, `release_npc` `0x7997E0`, `release_dead_npc` `0x799B50`, `cleanup_all_deleted_characters` `0x799CE0`, `update1` `0x7CC080`, `update3` `0x7CC2E0`.
- `FinishSpawning`: el flag `def+0x1C` decide jugador (vector `+0x80/+0x84`) o NPC (`+0xD0/+0xD4`). No hay límite de 1 jugador.
- `SpawnCharacter` `0x7D3B50` = slot 1 de la vtable de interfaz `0x12F68E0` (subobjeto `gestor+0x04`). Llama a NETZ (`FUN_007ddc60`) antes de `RequestCharacter`. **Ghidra no desensambló esa zona** (solo se alcanza por vtable): ojo con conclusiones de "sin referencias".

## 2026-11-05 — Hallazgo 08 (IA y múltiples jugadores, parte 1)
Detalle en `findings/08-ia-y-multiples-jugadores.md`. Evidencia en `work/evidence/h08/`.
- IA en `0x6C0000`–`0x740000` (menace `0x6CF080`, alien_attack `0x6E3DB0`, visibility `0x6FCF80`–`0x725330`).
- 24 call sites de `GetPlayer()` en IA (13 funciones); ~70 `GetPlayer()` inlineados en todo el binario.
- Loops sobre todos los jugadores: `FUN_00725330` (visibilidad: jugadores + NPCs), `FUN_00734690`, y varios del `CharacterManager` (`update1` `0x7CC080`, `0x7D3DB0`, `0x7D41B0`…).

## 2026-11-05 — Hallazgo 09 (niveles, checkpoints, guardado; parte 1)
Detalle en `findings/09-niveles-checkpoints-guardado.md`. Evidencia en `work/evidence/h09/`.
- Lista de niveles: `*(0x1BE1C4C)`; nombre `i` en `+i*0x46`; cantidad `+0x2BC0`; índice actual `+0x2BC8`.
- Comandos de la UI de menús (`FUN_0084dbe0`): 75 nombres → IDs (de 3 en 3). Ejemplos: `load_level` 0x7E, `cache_level` 0x81, `load_checkpoint` 0x99, `request_load_checkpoint` 0xD5, `back_to_frontend` 0xC3.
- Guardados en Steam Cloud (`userdata/<id>/214490/remote/`): `CHSV00/01.ais`, `MSSV01-19.ais`, `PROGRESSION.ais`, `input.cfg`; `local/settings.bin`.
- `SHGetFolderPathW(CSIDL_LOCAL_APPDATA)` es de LibOVR, no del guardado.

## 2026-11-05 — Hallazgo 06 (scripts y entidades, parte 1)
Detalle en `findings/06-scripts-y-entidades.md`. Evidencia en `work/evidence/h06/`.
- Entity manager de Cathode: `[0x174EF40]`; `Tick` `0x932040` (estado `+0xA4`, activo `+0xB8 == 1`, lote `+0x80/+0x84`).
- Las vtables de entidades de script con comportamiento (Door, TriggerSimple, HostOnlyTrigger) son idénticas entre sí: el comportamiento está en datos/handlers por tipo.
- Vftables RTTI de tipos clave listadas en el hallazgo (para identificar entidades en runtime).
- `IsMultiplayerMode` = variable booleana (los overrides son get/set genéricos).

## 2026-11-05 — Hallazgo 07 (items, parte 1)
Detalle en `findings/07-items-e-inventario.md`. Evidencia en `work/evidence/h07/`.
- `PICKUP_MANAGER_TICK` = `0x7CA0B0` → `FUN_007c51e0(dt)`: pickups/interacción solo para el jugador 0; `g_root+0xA0` (de `FUN_007b0bb0`) tiene un temporizador en `+0x55B8/+0x55C4/+0x55C8`.
- `pickup_manager.cpp` `0x786300` (casts de física); `thrown_item_manager.cpp` `0x792E00`.
- (Hallazgo 08, parte 2) Árboles de comportamiento Legend en `DATA/BEHAVIOR` (copias en `game/data/BEHAVIOR/`). Fábrica de nodos `FUN_0072b950`. `CONDITION_TARGET_IS_PLAYER` = `0x6EFD10`: `[[ctx+0x34A8]+0x10]` (objetivo) → `+0x31C` (controlador de jugador) → `+0x48` (activo). No usa `GetPlayer()`.

## 2026-11-05 — Hallazgo 10 (parte 2) y seeding de funciones
- `work/scripts/ghidra/SeedVtables.java` creó 22.608 funciones (destinos de vtables no analizados). Backup del proyecto anterior en `game/ghidra_backup/`. El proyecto cambió: REA en modo proyecto va a ver otro fingerprint (es normal).
- `NETZ::AllocateChannel` = `0x7DDC60` (32 canales; id incremental `+0x698`; máscara de replicados `+0x10/+0x14`). Lo llama `SpawnCharacter` por cada personaje y asocia canal ↔ personaje.
- "Sesión de red" = `g_root+0x54` = `*DAT_01be1bd4` (ctor `0xA2A6B0`): `+0x00` proveedor (NULL en retail), `+0x2C`/`+0x2D` flags, `+0x28` int. `IsAuthority()` `0xA2A4F0`/`0xA2A4A0` (1 sin proveedor), `IsClient()` `0xA2A4C0` (0 en retail).

## 2026-11-05 (tarde) — Mod fase 1+2 implementado y probado fuera del juego
Código en `mod/` (README con build, instalación, variables y checklist de la primera prueba). Commit de este bloque.

### Verificaciones estáticas antes de escribir el hook (evidencia en `work/evidence/mod/hook1.txt`…`hook3.txt`)
- `ModeManager::Run` (`0x7A8460`, 564 bytes): **un solo caller** (`0x7DB240`, seguido de `MOV BL,AL`). Epílogos `MOV AL,1`/`XOR AL,AL` + `RET` simple (`0x7A868B`, `0x7A8693`): **sin argumentos de stack**. El `RET 0x4` de `0x7A86FF` es de la función siguiente.
- Nadie salta a `0x7A8461`…`0x7A8466` (xrefs vacías); el loop interno vuelve a `0x7A8471`. Los 6 bytes robados (`51 53 55 56 8B F1`) son 5 instrucciones completas sin direccionamiento relativo.
- En el archivo (`game/AI.exe`, parser propio `pecheck.py` en el scratchpad): bytes en RVA `0x3A8460` = `51 53 55 56 8b f1 83 7e 4c ff`; **ninguna relocation** toca el prólogo; TimeDateStamp `0x54E3199D`; exports `ovr_Initialize` RVA `0xBA83B0`, `curl_easy_init` RVA `0xCEAB20`; `AI.exe` importa de WINMM **solo por nombre** (`timeGetTime`, `timeEndPeriod`, `timeBeginPeriod`).
- **Corrección (hallazgo 09 y RUNTIME_PLAN):** la lista de niveles es `L = **(0x1BE1C4C)`. `FUN_00a2e800`/`FUN_00a2e810` = `MOV EAX,[0x1BE1C4C]; MOV EAX,[EAX]; RET`; `GetCurrentLevelName` (`0x78BBE0`) = `FUN_00a2e810() + modo->m_4·0x46`.
- El flush de la cola A (`FUN_007ddae0` vía el thunk `0x7DDC00`) lo llama `0x774FA0`, un callback de tarea: corre **dentro** de `ModeManager::Run`. Entonces el tamaño de la cola leído después de `Run` es lo producido en ese frame después del flush.

### Diseño del mod
- Proxy `winmm.dll` **sin CRT** (`-nostdlib`; único import `KERNEL32.dll`, 32 funciones). 205 exports: 202 por nombre (unión de la winmm de Proton 9 y de `libwinmm.a` de mingw-w64, que sale de la de Windows; 16 son solo de Windows) + ordinales 2–4. Cada export es `jmp [puntero]`; el puntero arranca en un thunk que, si se llama a algo que la real no tiene, loguea `FATAL` con el nombre, muestra un MessageBox y termina el proceso.
- La real se carga desde `GetSystemDirectoryW()\winmm.dll` en `DllMain` (como hacen los proxies habituales).
- Guardas: TimeDateStamp, SizeOfImage, RVAs de `ovr_Initialize`/`curl_easy_init` y los 10 bytes del prólogo. Si algo no coincide: proxy puro, sin hooks.
- Hook: JMP rel32 + NOP sobre los 6 bytes del prólogo; trampolín propio (sin MinHook). El detour (`fastcall`, `force_align_arg_pointer`) llama al original (`thiscall`) y después a la sonda, y devuelve el EAX completo del original.
- Sonda (`probe.c`, solo lectura con `ReadProcessMemory` sobre el propio proceso; los globales de `.data` se leen directo):
  - por frame: estado/modo del `ModeManager`, tamaño de la cola A, tecla F9;
  - cada 250 ms: foto completa y eventos `EV`;
  - cada 1 s: resumen `ST`;
  - volcados en crudo automáticos al aparecer el jugador y 5 s después, y con F9.
- Log: `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` (o `AIMP_LOG`). No escribe nada en la carpeta del juego.

### Tests (con `wine` 11.20 del sistema, prefix descartable en el scratchpad; `mod/test/run_tests.sh`)
- `test_core`: hook sobre una función con el mismo prólogo que `ModeManager::Run` → instala, rechaza bytes distintos sin tocar nada, preserva EBX/ESI/EDI/EBP y ESP llamando con el stack desalineado, rama AL=0, remove. Formateador contra el CRT: OK. Nota: el `ucrtbase` de Wine imprime `%hhu` de 300 como "300"; C99 (y el mod) dan "44".
- `test_probe`: imagen de `AI.exe` simulada (32 MB, objetos en los offsets de `game.h`) → 36 verificaciones del log OK (posición/yaw, cámara−jugador = (0, 1,6, 0), lista y cambio de nivel, transiciones de estado por frame, volcados, cola A, transporte "INESPERADO").
- `test_host`: un .exe que importa `winmm.dll` por nombre carga el proxy y la real (`C:\windows\system32\winmm.dll`); `timeGetTime`, `timeBeginPeriod`/`timeEndPeriod` y `waveOutGetNumDevs` se reenvían; 189 exports resueltos, ausentes solo los 16 de Windows. La guarda rechaza el .exe de prueba ("build NO reconocida").
- Bugs encontrados por los tests y corregidos antes del commit: el resumen usaba una foto de hasta 15 frames de antigüedad (ahora fuerza una foto nueva) y los estados transitorios entre fotos se perdían (ahora el estado se lee por frame).

### Pendiente para la primera corrida (requiere al usuario)
- **El disco de 1,8 TB no está montado**: en el arranque de las 12:08, `ntfs3` rechazó `sda1` con "volume is dirty and force flag is not set" (lo mismo pasó con `nvme0n1p3`, la partición de Windows). Opciones del usuario (root):
  - (a) arrancar Windows y correr `chkdsk /f`, y desactivar el inicio rápido;
  - (b) `sudo ntfsfix -d /dev/sda1` y después `sudo mount /mnt/1,8TB`.
  Sin el disco no hay juego ni Steam library.
- Debajo del punto de montaje (en el disco raíz) hay un `COMMANDS.PAK` suelto: 3.572.520 bytes, mtime 2026-01-11 17:48, SHA-256 `dda19f70ab8abe5194fa7a372a680ed8c5aae6a30842101547da8f724f48a47f`. La fecha coincide con el `COMMANDS.PAK` modificado de `BSP_LV426_PT01`. Hay que compararlo con las copias del backup cuando el disco esté montado.
- Colocar `winmm.dll` en la carpeta del juego y la opción de lanzamiento (`WINEDLLOVERRIDES="winmm=n,b" %command%`): confirmación del usuario.

### Trazado de las colas (experimento 3.6) y hallazgo 10 parte 6
- Los dos wrappers de encolado (`FUN_00a2a290` → cola A `[0x1BE1BC8]`; `FUN_00a2a810` → sesión `[0x1BE1BD4]`) tienen el prólogo `55 8B EC 8B 45 10 8B 4D 0C 8B 55 08 50 51`, sin relocations (verificado en el archivo). El mod los hookea solo con `AIMP_TRACE_QUEUES=1`.
- `FUN_00a2a5c0` (update de la sesión): sin proveedor, el buffer B se vacía sin despacharse. Detalle en `findings/10-netz-red-del-engine.md`, parte 6.
- Tests: `test_probe` hookea dos wrappers falsos con el mismo prólogo; el original se ejecuta 462/462 veces a través del trampolín; el tipo se enmascara a u16; los informes por período y los totales salen bien.

### Analizador del log (`work/scripts/analyze_aimp_log.py`)
- Cruza `aimp.log` con cada chequeo de `RUNTIME_PLAN.md` (bloques 0–3) y marca OK / FALLA / sin datos / info, con la evidencia del log.
- Probado con el log sintético de `test_probe` (detecta el transporte "inesperado" simulado y la cámara desplazada) y con el log real del proxy en `test_host` (guardas rechazadas, como corresponde a un .exe que no es `AI.exe`).
- Ruta de log por defecto verificada bajo Wine: `C:\users\<usuario>\AppData\Local\AI_Multiplayer\aimp.log` (en Proton el usuario es `steamuser`).

## 2026-11-05 (tarde) — Primera corrida en el juego: cambios hechos en la instalación (todo reversible)
Autorizado por el usuario ("corré y modificá lo que necesites, siempre que lo anotes y lo puedas revertir"). Revertir todo: `work/scripts/runtime_setup.sh revert`.
- Estado previo: sin `winmm.dll` en la carpeta del juego, sin LaunchOptions para 214490 en `localconfig.vdf`, sin overrides propios en el prefix (ReShade/MovieFix cargan por los overrides `d3d11`/`dxgi` = nativa que pone Proton para DXVK). Steam cerrado.
- 13:18 `runtime_setup.sh install`:
  - backup `/mnt/1,8TB/SteamLibrary/AlienIsolation_backup_AI_Multiplayer/runtime/user.reg.20261105-131820`;
  - copia `winmm.dll` (sha256 `35ce4f18…`, commit 926f034) a la carpeta del juego;
  - agrega `[Software\\Wine\\AppDefaults\\AI.exe\\DllOverrides] "winmm"="native,builtin"` a `pfx/user.reg`.
- El COMMANDS.PAK modificado (BSP_LV426_PT01) es, según el usuario, un mod que muestra un cartel para saltear la misión 9; no cambia otra cosa. Para el coop, los dos jugadores tienen que tener el mismo.

### Resultados de la primera corrida, en el menú (13:22, `aimp.log` del prefix; evidencia del timer en `work/evidence/mod/t1.txt`…`t3.txt`)
- **Fase 1 verificada en runtime** (Proton 9.0-203, Wine 9.0; Windows 10.0.19043 reportado):
  - proxy cargado, 189 exports resueltos;
  - las 5 guardas de la build OK; hook instalado en `0x7A8460` (base 0x400000 también bajo Wine);
  - primer frame del hook a los 39,7 s (el primer `Run` tardó 35,7 s, la carga inicial); el menú corre a ~355–366 fps.
- **Verificado en runtime:**
  - `g_root` y `ModeManager` (estado 5, modo con vtable `0x12F43E8` = modo de nivel);
  - la lista de niveles con **doble indirección** (35 niveles; actual 0 = `Production\Frontend`);
  - el `dt` real (`0x16410D0`);
  - NETZ transporte = 0 y sesión proveedor = 0 (como predijo el estático);
  - el global de la cola A `[0x1BE1BC8]` == `g_root+0x50`;
  - colas A y B vacías en el menú.
- **Catálogo RPC (experimento 3.5):** la cola A tiene **44 endpoints** y la sesión **47**. El estático había visto 8 en la sesión: Ghidra no desensambla todos los inicializadores. El endpoint del spawn (`0x16361D8`) es el **tipo 0x29** de la cola A (handler RVA `0x14D10`). Lista completa en el log (líneas `RPC`).
- **Contador de frames `0x16410DC` = 0 en el menú.** No es un offset mal: `FUN_007ddae0` usa `FUN_00773050` (sube el contador, acumula el tiempo de juego `0x16410A8`, `dt` escalado) solo si `([DAT_016f194c+4] & DAT_012f3014) == 0`. Si no, usa `FUN_00773280`, que solo actualiza el `dt` real y los acumuladores de tiempo real (`0x16410B0`/`B8`). Entonces `0x16410DC` cuenta **frames de simulación**, no frames dibujados. Además `FUN_007d9810` (salida del nivel) pone en 0 el tiempo de juego.

## 2026-11-05 (tarde) — Fase 3: UDP mínimo (código, tests y prueba local en el juego)
- Código: `mod/src/net.c`/`net.h`.
  - Protocolo propio v1: paquete `AimpPkt` de 76 bytes con magia `AIMP`, versión, tipo (STATE/BYE), flags (bit 0 = hay jugador), seq, hora del emisor, nivel, contador de simulación, posición, yaw, cuaternión y posición de la cámara, e id de 8 bytes del personaje.
  - Host/join por `aimp.cfg`, 20 paquetes/s, sockets no bloqueantes en el hook del frame, Winsock resuelto en tiempo de ejecución.
  - El host aprende la dirección del cliente del primer paquete; al salir se manda BYE.
- Tests:
  - `test_net` (host + join por 127.0.0.1 en el mismo proceso): 60/60 paquetes en cada sentido, sin pérdidas, BYE procesado. El primer intento falló por un bug del test, no de la red: la macro evaluaba `net_init` dos veces y el segundo `bind` daba `WSAEADDRINUSE`.
  - Peer de prueba `work/scripts/aimp_peer.py` (Python, hace de otro jugador con una posición que da vueltas).
- **Prueba en el juego (Proton), juego = host, peer = cliente en la misma máquina (13:52–13:56):**
  - en el menú, el peer recibió 462 paquetes del juego ("nivel 0, sin jugador"), ~19/s, sin pérdidas; el juego recibió el estado del peer a 20/s, con 27–40 ms de antigüedad;
  - después el usuario cargó la partida (`Tech_Hub`), caminó (marcas F8 en t = 158,8 y 191,6) y guardó en una terminal de guardado (marca 3, t = 203,3); esa corrida fue sin trazado, así que el mensaje de la interacción no quedó registrado;
  - totales del juego: 2571 paquetes enviados, 2979 recibidos, **0 perdidos**.
  - del lado del peer (`work/evidence/runtime/run4-peer.log`): 2108 paquetes del juego, 0 perdidos. Con la partida cargada llega **el estado real del jugador**:
    - nivel 16, "con jugador";
    - la posición sigue la caminata y coincide con las marcas F8 (p. ej. (4,49, 13,28, 33,09) = marca 3);
    - el yaw cambia al girar y el contador de simulación avanza;
    - id `2ff722b2099d5b9b`, el mismo de los RPC 0x26 en Tech_Hub.
  - **Fase 3 verificada en local, en los dos sentidos.**
- Aprendido: un proceso lanzado en segundo plano desde un shell no interactivo ignora SIGINT, y Python con la salida redirigida guarda todo en buffer. `aimp_peer.py` ahora imprime con flush y maneja SIGTERM.
- **Pendiente para cerrar la fase 3:** prueba por Tailscale con el amigo. Tailscale no está instalado en la máquina del usuario: instalarlo requiere su confirmación.

## 2026-11-05 — Laptop de pruebas: el juego se copió desde papito por red local
- El usuario instaló el juego en la laptop (Windows 10) y Steam lo copió desde `papito` (transferencia por red local). Duda: ¿vinieron los mods?
- La FAQ oficial de Valve (help.steampowered.com, 46BD-6BA8-B012-CE43) vino sin cuerpo al leerla; según el resumen del buscador, se transfiere solo el contenido del juego (no saves, configuración, Workshop, Cloud ni shaders). Los archivos extra (DLLs de ReShade/MovieFix, `winmm.dll`, presets) no son parte del juego. **No confirmado** si la transferencia valida los archivos oficiales modificados.
- Plan: "Verificar integridad" en la laptop con Steam cerrado en `papito` (así lo que difiera baja de los servidores de Steam). No hace falta reinstalar. Verificar no borra extras: revisar la raíz del juego a mano.
- SHA-256 en `papito` de los dos `COMMANDS.PAK` modificados (para comparar con la laptop ya verificada):
  - `DATA/ENV/PRODUCTION/BSP_LV426_PT01/WORLD/COMMANDS.PAK`: `DDA19F70AB8ABE5194FA7A372A680ED8C5AAE6A30842101547DA8F724F48A47F`
  - `DATA/ENV/PRODUCTION/TECH_COMMS/WORLD/COMMANDS.PAK`: `42FD34D7FAF7EA9A22B961D7C6A0C12700FA90327CA1F796F445CC958ECBDD3A`
- Si en la laptop verificada difieren, los de la laptop son los vanilla y se confirma que los de `papito` son del mod. Si son iguales, los de `papito` ya eran vanilla. La comparación vuelve en `work/windows/report.md` (agregado al paso 1 de `work/windows/instructions.md`).

## 2026-11-05 — Pruebas en Windows 10 nativo (laptop), pasos 1 y 2
Evidencia: `work/windows/report.md` y `work/windows/logs/aimp-partida-20261105-110343.log` (commit c0d31b1, hecho por la sesión de Claude Code en la laptop, con el kit del commit 31fe898).
- Windows 10 Home 10.0.19045, 64-bit. `AI.exe` SHA-256 dde66521…19a90, buildid 4940658: **misma build**. En la carpeta del juego solo estaban las DLLs oficiales (AMD_AGS, ATIAGS, CRASHREPORTER32, STEAM_API): la copia por red local desde `papito` **no trajo** ReShade ni MovieFix.
- **winmm no está en KnownDLLs** (lista completa en el reporte). Instalar en `Program Files (x86)\Steam\…` no pidió administrador.
- `proxy: winmm real = C:\Windows\system32\winmm.dll`, **195 exports resueltos, 10 ausentes**: DrvClose DrvDefDriverProc DrvOpen DrvOpenA DrvSendMessage GetDriverFlags OpenDriverA mmioInstallIOProc16 winmmDbgOut winmmSetDebugLevel. El juego no llamó a ninguno: no hubo cartel del stub.
- `build reconocida`. Hook en `0x00d08460` → **base de `AI.exe` 0x00960000: ASLR la reubicó** (en Proton carga en 0x400000). El mod trabaja con RVAs y anduvo igual: confirma la decisión del hallazgo 01.
- `FASE 1` a los 35,1 s (Frontend). Carga de Tech_Hub (nivel 16): J0 `Character(ok)`, 41 resúmenes con movimiento, `MARCA 1` con la posición. fps promedio 58,1. Salida limpia (`ModeManager::Run devolvió 0`, `DLL_PROCESS_DETACH`), sin errores en el Visor de eventos.
- **Fases 1 y 2 verificadas también en Windows nativo.**
- Detalle del kit: PowerShell 5.1 lee el log UTF-8 como ANSI (`mÃ³dulos`) si no se le pasa `-Encoding UTF8`. Las líneas que copia `collect` al reporte salen bien.

## 2026-11-05 — Fase 3 por Tailscale: laptop (Windows 10, join) ↔ papito (Proton, host)
Evidencia del host: `work/evidence/runtime/run5-tailscale-host.log` (gitignored). La de la laptop vuelve en `work/windows/`.
- Tailscale: `papito` 100.126.161.126 (Linux), `papulaptop` 100.126.159.45 (Windows). Sin firewall en papito.
- Misma cuenta de Steam en las dos: el juego no puede correr en las dos a la vez con Steam online. Solución: **Steam en modo offline en la laptop**, papito online. (Cerré el juego de papito desde el menú, la laptop arrancó offline y después relancé papito.)
- Mientras papito estaba cerrado, un listener pasivo en UDP 47811 (Python, solo recibe) vio el primer paquete AIMP de `100.126.159.45:62046` (STATE, menú, sin jugador): la ida laptop → papito por Tailscale anda, y el firewall de Windows deja salir.
- Con papito en host: `NET: cliente conectado desde 100.126.159.45:62046` en el primer frame (35,3 s). Conexión ~290 s, hasta el BYE de la laptop (`el otro extremo se despidió`).
- Totales del host: enviados 5127, recibidos 4732, inválidos 0, **perdidos 361, todos en el primer segundo**. Es el backlog: el socket del host se abre al cargar la DLL (0,006 s), pero se lee desde el primer frame (35 s). En ese tiempo el buffer de recepción (rmem_default 212992) se llenó con ~238 paquetes viejos y el kernel descartó el resto. **Después: 0 perdidos.**
- Huecos sin pérdida durante la carga de nivel de la laptop (6,5 s y 3 s): mientras carga no corre el hook por frame, así que la laptop no manda nada y la secuencia sigue sin saltos.
- Estado remoto recibido: nivel 0 sin jugador (menú) → nivel 16 sin jugador → **nivel 16 con jugador** en (4,55, 13,62, 33,59), el mismo punto de guardado (la partida viene por Steam Cloud). La posición y el yaw cambian con la caminata de la laptop: x de 4,5 a 11,1, z de 31,7 a 34,6, yaw de −3,14 a −2,19.
- Edad del último paquete al momento del resumen: mediana 28 ms, p95 83 ms, máx. 278 ms. Incluye el intervalo entre paquetes (~60 ms), así que no es la latencia.
- **Ritmo real 15–19 paquetes/s en vez de 20:** `net_due` exige ≥ 50 ms desde el último envío y se envía una vez por frame, así que a 58–60 fps muchas veces se esperan 4 frames (66,7 ms). Arreglo simple: `last_send += send_ticks` (con tope si se atrasa mucho).
- Mejora pendiente: en el primer `net_poll`, vaciar el backlog sin contar huecos, o abrir el socket en el primer frame. Así no se toma un estado viejo de 35 s ni se cuentan pérdidas falsas.
- **Lado laptop** (`work/windows/logs/aimp-red-20261105-114132.log`, reporte en `work/windows/report.md`, paso 3):
  - Tailscale 1.104.5 (winget). `tailscale ping papito`: directo por LAN (192.168.1.6:41641). La interfaz de Tailscale queda como red Private y el Wi-Fi como Public. **Windows no preguntó por `AI.exe`**: en modo join el socket solo manda primero, y las respuestas entran por el estado de la conexión. Si el amigo hace de host en Windows, ahí sí puede aparecer el cartel del firewall.
  - `NET: cerrado (enviados 5503, recibidos 5141, perdidos 0 en total)`. 60 fps en partida. Salida limpia.
- **Los dos sentidos verificados con las marcas F8.** Cada máquina recibió exactamente la posición que la otra registró en sus marcas:
  - papito MARCA 1 (2,43, 13,28, 33,20) yaw 1,607 → la laptop recibió (2,431, 13,279, 33,195) yaw 1,607;
  - papito MARCA 2 (−0,15, 13,28, 34,04) → la laptop recibió (−0,146, 13,279, 34,038);
  - laptop MARCA 1 (7,59, 13,28, 33,34) yaw −1,728 → papito recibió (7,592, 13,278, 33,339) yaw −1,728;
  - laptop MARCA 2 (6,62, 13,28, 34,61) yaw −2,048 → papito recibió (6,617, 13,277, 34,610) yaw −2,048.
- **Fase 3 cerrada:** Windows 10 nativo ↔ Linux/Proton por Tailscale, en los dos sentidos.

## 2026-11-05 — COMMANDS.PAK: solo uno de papito es distinto del original
Dato de la laptop (`work/windows/report.md`, "COMMANDS.PAK en la laptop"):
| Archivo | laptop | papito |
|---------|--------|--------|
| `BSP_LV426_PT01/WORLD/COMMANDS.PAK` | `EB9FDBF197308941F6E43CDE7BD1712024011A16967A4819C71F2FB49076ED77` | `DDA19F70…A47F` (**distinto**) |
| `TECH_COMMS/WORLD/COMMANDS.PAK` | `42FD34D7…DD3A` | igual |
- **Confirmado por la laptop** (commit 5264a56): el usuario borró la copia por red, reinstaló desde Steam y verificó la integridad. Salió todo correcto y no se reemplazó nada; los hashes no cambiaron. **El original de `BSP_LV426_PT01` es `EB9FDBF1…ED77`.** La pregunta de si la copia por red trae archivos modificados quedó sin respuesta, porque esa copia se borró.
- Conclusión: **en papito solo `BSP_LV426_PT01/WORLD/COMMANDS.PAK` está modificado.** `TECH_COMMS` es el original (su mtime 2026-10-05 no indica un mod). El pendiente de PROGRESS pasa de "2 COMMANDS.PAK" a uno.

## 2026-11-05 — Fase 4: preparación estática y experimento del avatar (hallazgo 12)
- Spawn de autoridad `FUN_00797490` = `thiscall(this, pedido*, flag)`, `ret 8`: observadores antes → `this->vf[1](pedido)` → observadores después → copia de 0x130 bytes a la cola A (RPC 0x29). Prólogo `55 8B EC 53 56 57 8B F9`: hookeable con 6 bytes.
- Pedido de 0x130 bytes: `FUN_00787450` copia 0xE4 bytes de los parámetros del spawner (`FUN_00777010`: encabezado de 0x64 bytes, nombre en `+0x64` y composite en `+0xA4`, de 0x40 cada uno) y completa la cola (`+0xF0…+0x12F`: ids/refs con valores por defecto `[0x174E254]` y −1).
- Comparando los 8 payloads de 0x29 que ya había (run3): `+0x00` = clave (1…8), `+0x28…+0x2F` = id de 8 bytes (el del jugador `2ff722b2 b89a5172`), `+0x30` = tipo (0 jugador, 3/6 NPCs), `+0x1C` = 0 en la copia. No hay floats de posición en los primeros 64 bytes.
- Teleport: `FUN_007c6330(pj, pos*, yaw, p3)`, `ret 0xC` → `SetTransform(pj, pos, yaw, p3, 0)`.
- Código: `mod/src/ghost.c` (`AIMP_TRACE_SPAWN`, `AIMP_GHOST`, `AIMP_GHOST_DELAY_MS`, `AIMP_GHOST_FROM`), `mem_write` en `mem.c`. Test nuevo `test_ghost`: stubs de código máquina en las RVAs reales, con los prólogos y el `ret N` originales; verifica el balance del stack, la captura, la máquina de estados, el controlador, el teleport y el reinicio por nivel. **Todos los tests OK.**
- Falta: la corrida en el juego (requiere al usuario).

## 2026-11-05 — Corrida 6 (fase 4, `AIMP_GHOST=1`): el clon por el spawn de autoridad hizo caer el juego
Evidencia: `work/evidence/runtime/run6-ghost.log` (gitignored).
- Hook en `FUN_00797490` instalado en el juego real. Al cargar Tech_Hub, 6 pedidos con nombre:
  - 1 = `THE_PLAYER` (tipo 0, id `2ff722b2099d5b9b`);
  - 2–5 = `INNOCENT` (tipo 6);
  - 6 = `ALIEN` (tipo 1).

  **`+0xA4` es el nombre de la definición** (`CHR_INFO/ATTRIBUTES`), no el composite. `+0x64` (nombre) viene vacío.
- Pedidos completos: **posición en `+0x50…+0x5B`** y **yaw en `+0x60`** (el del jugador: (4,55, 13,62, 33,59) y −π, el punto de guardado).
  - `+0xF0` = la clave otra vez: 1, 2, 6.
  - El del jugador además trae tres refs en `+0x04…+0x0C` y `+0x4C` = 1.
  - Los INNOCENT traen bloques en `+0x100` (6 ShortGuids) y `+0x118` (0…5), con `+0xF4` = 1.
- `this` del spawn de autoridad = `0x07809f10`, vtable `0x12F6900`, igual al `ctx` del endpoint A 0x29. Su **`vf[1]` es `ret 4` (vacío)** y su `vf[2]` es `mov al,1; ret 4`. En retail ni el spawn de autoridad ni el handler del 0x29 (`FUN_00414d10`) crean el personaje: solo avisan a listas de observadores y reenvían a la cola B.
  - Las cuatro listas (`0x16F322C`/`0x16F3230`, `0x16A6728`/`0x16A672C`) tienen **una sola referencia cada una en todo el binario**, la lectura de su dispatcher. No hay escritores, ni con base cercana: casi seguro están vacías.
- `GHOST: clonando…` con clave 64 → el spawn volvió (1 jugador, 5 NPCs, 0 pendientes). **El juego se cerró en el frame siguiente**, sin `DLL_PROCESS_DETACH`. El log no tiene buffer (`WriteFile` por línea) y el mod solo hace lecturas seguras en ese estado, así que fue dentro del `Run` del juego. Sin evidencia de dónde: no había registro de excepciones.
- Sospecha concreta: en el clon quedó `+0xF0` = 1 (la clave original) con `+0x00` = 64.
- Cambios para la corrida 7:
  - `crash.c`: VEH que loguea excepciones de crash (código, módulo+RVA, acceso, registros, retornos a `.text`), probado en `test_ghost`;
  - hook también en `SpawnCharacter` (`0x7D3B50`, prólogo `51 53 55 56 57 8B 7C 24 18`, `ret 4`);
  - log de las cuatro listas de observadores;
  - clon por `SpawnCharacter` (no por la autoridad) con la clave en `+0x00` y en `+0xF0`;
  - modo `AIMP_GHOST=2`: primero mueve un INNOCENT existente 20 s (prueba del teleport) y después intenta el clon.

## 2026-11-05 — Corrida 7 (`AIMP_GHOST=2`): camino real del spawn y crash del teleport (alineación)
Evidencia: `work/evidence/runtime/run7-ghost-npc.log` (gitignored).
- Las cuatro listas de observadores, leídas en runtime: **vacías** (confirmado).
- **Camino real de creación:** `SpawnCharacter` (`0x7D3B50`) lo llama el **handler de sesión tipo 0x2E** (`FUN_00414a70`, retorno en `0x414AC0`).
  - `ctx` = `CM+0x04`; su vtable `0xEF68E0` tiene `vf[1]` = `SpawnCharacter`.
  - El handler copia un mensaje de **0xF0 bytes** a un buffer alineado a 16 y llama a `ctx->vf[1](copia)`.
  - Orden al cargar Tech_Hub: ALIEN, THE_PLAYER y los 4 INNOCENT, con las mismas claves, ids y posiciones que los pedidos de autoridad. El primer bloque de 0xE4 bytes difiere, así que no es una copia literal.
  - `this` del spawn de autoridad = `CM+0x10`. Los endpoints A 0x2B / 0x2A / 0x29 tienen `ctx` `CM+0x00` / `+0x08` / `+0x10`.
- La corrida 6 queda explicada a medias: el clon mandado por la autoridad (A 0x29 → B 0x2C → … → sesión 0x2E) llegó a `SpawnCharacter` en el frame siguiente y algo ahí se cayó. El encadenamiento B → sesión no está mapeado.
- **Modo NPC:** eligió `NPCs[1]` "CV5" (INNOCENT, tipo 6, sin controlador). **El primer teleport hizo caer el juego**: `EXCEPCIÓN 0xc0000005 en AI.exe+0x1414 … lectura en 0xffffffff`. Stack: `SetTransform` (`0x3C2796`) ← `FUN_007c6330` (`0x3C63A0`).
  - `FUN_00401410` copia la posición con **`MOVAPS`**: el vector tiene que ser de 16 bytes y estar alineado a 16. El acceso en `0xffffffff` es como Windows reporta un `MOVAPS` desalineado (#GP).
  - El mod pasaba un `float[3]` del stack. **Error del mod, no del juego.**
- Arreglo: vector estático `float[4]` alineado a 16, que conserva el cuarto componente del personaje (`+0x7C`). El buffer del clon también va alineado a 16. El clon ahora sale de lo que recibió `SpawnCharacter` (0xF0 bytes).
  - `test_ghost` verifica la alineación de cada posición y de cada pedido, y que se conserve `w`. El stub copia 16 bytes, como `MOVAPS`.
- Lección: el registro de excepciones dio la causa en una corrida. Cualquier puntero a vector que se pase al engine tiene que estar alineado a 16.
