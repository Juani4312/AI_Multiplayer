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
