# Bitácora

## 2026-11-05 — Identificación de build (sección 3)

### Hechos verificados (`file`, `sha256sum`, `objdump -p/-h`)
- Instalación: `/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation` (Steam appid 214490, buildid 4940658).
- Ejecutable principal: `AI.exe` — `PE32 executable for MS Windows 5.01 (GUI), Intel i386`. **32 bits.**
  - SHA-256: `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`
  - PE timestamp 0x54E3199B (Tue Feb 17 2015), ImageBase 0x00400000, SizeOfImage 0x01F81000.
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
