# 01 - Inventario del binario

- **Pregunta:** ¿Qué es exactamente `AI.exe`? Formato, toolchain, imports y exports, carga dinámica de DLLs, símbolos/RTTI, strings útiles y protecciones. En particular: ¿qué DLL conviene usar como proxy para cargar el mod?
- **Build y SHA-256 del binario:** Windows `AI.exe`, PE32 i386, Steam buildid 4940658, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`. COFF TimeDateStamp `0x54E3199D` (2015-02-17 10:36:13 UTC).
- **Dirección(es) / RVA / offset:** ver las tablas. Todas las VAs suponen ImageBase `0x400000`; RVA = VA − `0x400000`. El binario tiene ASLR y relocations, así que el mod tiene que trabajar con RVAs.
- **Evidencia:** en `./work/evidence/` (no se commitea):
  - REA portable (sin Ghidra): `rea/inspect-pe-imports.json` (ev_0feda758…), `rea/inspect-native-exports.json` (ev_1d6d7ae2…), `rea/inspect-pe-load-config.json` (ev_a29a19b7…), `rea/inspect-pe-certificates.json` (ev_ae6097aa…), `rea/inspect-pe-resources.json` (ev_49b1361b…), `rea/inspect-byte-entropy-64k.json` (ev_a556dc42…).
  - REA + Ghidra (proyecto persistente): `rea/analyze-overview.json`, `rea/decompile-0x*.json`, `rea/xrefs-0x*.json`.
  - Ghidra headless: `ghidra-import/analyzeHeadless.log` (import y auto-análisis), `ghidra/inventario01.txt` (uso de cada import y argumentos de las llamadas, salida de `work/scripts/ghidra/Inventario01.java`).
  - Script propio: `inventario01_extra.txt` (salida de `work/scripts/inventario01.py`: debug directory, manifest y escaneo de nombres de DLL).
  - `strings/`: `ascii_n5.txt`, `utf16le_n5.txt` (GNU strings, offsets de archivo), `rtti_names.txt`, `cathode_classes.txt`, `source_files.txt`.
- **Confianza:** alta para todo lo que sale de cabeceras y tablas del PE, y para los argumentos de las llamadas que recuperó el decompilador. Media para la atribución de funciones a cada librería (por rango de direcciones y por strings). Esas atribuciones están marcadas como hipótesis.
- **Cómo verificar en runtime:** ver la sección "Verificación en runtime".
- **Verificado en runtime:** no. El único dato de runtime es `aimoviefix.log`, que reporta que bajo Wine el EXE cargó en `0x400000`.
- **Preguntas abiertas:** al final.

## 1. Identidad y toolchain

| Dato | Valor | Fuente |
|------|-------|--------|
| Formato | PE32 (magic 0x10B), i386, subsistema GUI, OS mínimo 5.01 | objdump, REA |
| Linker | 10.0, consistente con Visual Studio 2010 | objdump `MajorLinkerVersion 10` |
| CRT | estático: no importa `MSVCR*`/`MSVCP*`. Las funciones del CRT están identificadas (`__mtinit`, `__initptd`, `___crtCorExitProcess`, …) | REA imports; Ghidra Function ID |
| COFF TimeDateStamp | `0x54E3199D` = 2015-02-17 10:36:13 UTC. El export directory dice `0x54E3199B`, 2 s antes | `inventario01.py`, REA exports |
| CheckSum | `0x014D4B2F` | objdump |
| ImageBase / SizeOfImage | `0x00400000` / `0x01F81000` | objdump |
| Entry point | RVA `0x671E61` (VA `0xA71E61`), dentro de `.text` | `inventario01.py` |
| Characteristics | `0x122`: EXECUTABLE_IMAGE, **LARGE_ADDRESS_AWARE**, 32BIT_MACHINE | objdump |
| DllCharacteristics | `0x8140`: **DYNAMIC_BASE** (ASLR), **NX_COMPAT** (DEP), TERMINAL_SERVER_AWARE | objdump |
| PDB | CodeView RSDS, GUID `F076EA77-77E3-4825-90F4-B02E4F197749`, age 1, `E:\Console\Moon-Branches\BUILD\game\pc\AI.pdb`. **No se distribuye**: Ghidra no lo encontró y REA reporta "load state unknown" | `inventario01.py`, log de Ghidra, `analyze-overview.json` |
| Ghidra | lenguaje `x86:LE:32:default`, compilador `windows`. **67.007 funciones** (193 thunks) + 327 externas; 31.826 strings definidas (REA cuenta 31.817 en su inventario) | `inventario01.txt`, `analyze-overview.json` |

"Moon" parece ser el nombre interno del proyecto (`Moon-Branches`). Hipótesis.

## 2. Secciones

| Sección | RVA | VirtualSize | Raw offset | RawSize | Notas |
|---------|-----|-------------|------------|---------|-------|
| `.text` | `0x00001000` | `0x00D9D13B` | `0x00000400` | `0x00D9D200` | ~13,6 MiB de código |
| `.rdata` | `0x00D9F000` | `0x004965CC` | `0x00D9D600` | `0x00496600` | IAT en `0xD9F000` (tamaño `0x56C`), import dir en `0x1232DB4`, export dir en `0x1234A40` |
| `.data` | `0x01236000` | `0x00B1DD28` | `0x01233C00` | `0x00070600` | **~11 MiB virtuales, 450 KiB en disco**: la mayoría son globales inicializados en cero (BSS). Ahí deberían vivir los singletons del engine |
| `.rsrc` | `0x01D54000` | `0x0001171C` | `0x012A4200` | `0x00011800` | |
| `.reloc` | `0x01D66000` | `0x0021A0CA` | `0x012B5A00` | `0x0021A200` | base relocations `0x1B0DEC` bytes; el resto es padding |

Ghidra agrega `Headers` (`0x400000`–`0x400400`) y `tdb` (`0xFFDFF000`, el bloque que crea el analizador de TEB).

## 3. Protecciones y mitigaciones

| Aspecto | Resultado | Evidencia |
|---------|-----------|-----------|
| Packer / cifrado | **No hay indicios.** Secciones estándar de MSVC, entry point en `.text`, entropía del archivo 6,63 bits/byte y ningún bloque de 64 KiB ≥ 7,2. La única racha "low" es la cola de `.reloc` (offset `0x1470000`, 392.192 bytes) | `inspect-byte-entropy-64k.json`, `inventario01.py` |
| DRM wrapper (SteamStub) | **No hay sección `.bind`** y el entry point no salta a un stub. El juego usa Steamworks normal: `SteamAPI_RestartAppIfNecessary` y `SteamAPI_Init` en `FUN_00a540d0`. **No se toca** | REA imports; `inventario01.txt` |
| Firma Authenticode | No (no hay security directory) | `inspect-pe-certificates.json` |
| TLS callbacks | No hay TLS directory: no corre código antes del entry point | `inspect-pe-load-config.json` |
| /GS | Sí: security cookie en VA `0x165F890` (`.data`) | load config |
| SafeSEH | Sí: tabla en VA `0x160B050` con **2391** handlers | load config |
| CFG | No (GuardFlags ausente). Es lo esperable con VS2010 | load config |
| ASLR | **Sí**, con relocations. Bajo Wine cargó en `0x400000` (`aimoviefix.log`). En Windows nativo probablemente se reubique: **siempre RVAs** | DllCharacteristics; `.reloc` |
| Anti-debug | **No se encontró.** `IsDebuggerPresent` se usa en 5 lugares: `FUN_0085d000` (wrapper `return IsDebuggerPresent()!=0;`, 511 referencias, probablemente asserts), `FUN_0085e3f0` (nombra threads con `RaiseException(0x406D1388)` solo si hay debugger), `FUN_0085d140` (printf de debug a `OutputDebugStringW`), y el CRT (`__call_reportfault`, `___report_gsfailure`). No hay `CheckRemoteDebuggerPresent`, `NtQueryInformationProcess` ni strings de protectores conocidos | `inventario01.txt`, `decompile-0x0085d000.json`, `decompile-0x0085e3f0.json`, `decompile-0x0085d140.json`, `xrefs-0x0085d000.json` |
| Debug registers | `FUN_0085d490` pone un breakpoint por hardware (DR0–DR3/DR7) con `Get/SetThreadContext`; `FUN_0085d610` es la pareja. Es tooling de desarrollo y tiene **0 referencias** (código muerto, salvo uso vía punteros que Ghidra no ve) | `decompile-0x0085d490.json`, `xrefs-0x0085d490.json`, `xrefs-0x0085d610.json` |
| Auto-modificación | El único `VirtualProtect` está en `FUN_00fe9be0`: abre 5 bytes RW en `DAT_01fbfbb8+0xC9`, escribe `B8 00 00 00 00` (`mov eax,0`) y restaura. `DAT_01fbfbb8`/`DAT_01fbfbbc` son funciones de la DLL de la cámara del DK2 (`OVBaseIFX64.dll`, ver §5): parchea código **de esa DLL**, no del juego. Hipótesis: es parte de LibOVR. No se encontró verificación de integridad de `.text` | `decompile-0x00fe9be0.json`, `inventario01.txt` |

Conclusión: nada del binario impide analizarlo, depurarlo o inyectarle código. Solo hay que respetar ASLR (RVAs) y no tocar Steamworks.

## 4. Imports estáticos

17 DLLs, 330 funciones, sin delay-load ni bound imports. imphash (pefile) `b1d7e3f99c9ee5d17e877e21f37e18be`.

| DLL | Funciones | Uso (sitios de código según Ghidra) | ¿Proxy? |
|-----|-----------|--------------------------------------|---------|
| `steam_api.dll` | 14 | Steamworks: Init/RestartApp/Shutdown, interfaces, callbacks. `SteamAPI_RunCallbacks` en `FUN_00a58eb0`, que **se llama desde una vtable** (`0x13E9B5C`) | No (es Steam; no se toca) |
| `KERNEL32.dll` | 167 | heap, threads, archivos, sync, `LoadLibraryA/W`, `GetProcAddress`, … | No (sistema) |
| `USER32.dll` | 63 | ventana, mensajes (`PeekMessageA/W`, `DispatchMessageA/W`), raw input, cursor | No (sistema) |
| `WS2_32.dll` | 32 (29 por ordinal) | sockets: libcurl (`FUN_01103190`) y otros | No (KnownDLL, hipótesis) |
| `PSAPI.DLL` | 1 | `GetProcessMemoryInfo` (1 sitio) | Posible; ver §8 |
| `WSOCK32.dll` | 2 (ord. 17, 20) | 3 y 11 sitios | Posible; ver §8 |
| `imagehlp.dll` | 8 | `SymInitialize`, `StackWalk64`, … (callstacks) | Posible; ver §8 |
| `SETUPAPI.dll` | 6 | `SetupDi*` (enumeración de dispositivos; HID de LibOVR, hipótesis) | Posible; ver §8 |
| `dxgi.dll` | 2 | `CreateDXGIFactory`, `CreateDXGIFactory1` | **Ocupada** (ReShade) |
| `d3d11.dll` | 1 | `D3D11CreateDeviceAndSwapChain` | **Ocupada** (MovieFix) |
| `XINPUT1_3.dll` | 3 (ord. 2, 3, 4) | 1 sitio cada uno | **Ocupada** (MovieFix) |
| `GDI32.dll` | 2 | `GetStockObject`, `SwapBuffers` | No (sistema) |
| `SHELL32.dll` | 1 | `SHGetFolderPathW` | No (sistema) |
| `ole32.dll` | 3 | `CoInitializeEx`, `CoCreateInstance` (4 sitios), `CoUninitialize` | No (sistema) |
| `WINMM.dll` | 3 | `timeGetTime` (3 sitios), `timeBeginPeriod(1)` / `timeEndPeriod(1)` (2+2). Una pareja está en un módulo de timer del engine (`FUN_00d1a7d0`/`FUN_00d1a7f0`, llamado desde `0xD06F0D`) y la otra en `FUN_00fac480`/`FUN_00fac4d0` (zona de LibOVR, hipótesis) | **Candidata principal** |
| `ADVAPI32.dll` | 6 | `Crypt*` (hash; 1 función) | No (sistema) |
| `WLDAP32.dll` | 16 (por ordinal) | LDAP de libcurl (1 función por import) | Posible; ver §8 |

Slots de IAT útiles (RVA): `LoadLibraryA` `0xD9F1F4`, `LoadLibraryW` `0xD9F138`, `GetProcAddress` `0xD9F1F0`, `GetModuleHandleA` `0xD9F1FC`, `GetModuleHandleW` `0xD9F214`, `timeGetTime` `0xD9F3F4`, `timeEndPeriod` `0xD9F3F8`, `timeBeginPeriod` `0xD9F3FC`, `PeekMessageW` `0xD9F2FC`, `PeekMessageA` `0xD9F388`, `D3D11CreateDeviceAndSwapChain` `0xD9F4E8`, `CreateDXGIFactory1` `0xD9F4F4`, `SteamAPI_RunCallbacks` `0xD9F55C`, `QueryPerformanceCounter` `0xD9F040`.

## 5. Carga dinámica de DLLs

Todas las llamadas a `LoadLibraryA/W`, `GetModuleHandleA/W` y `GetProcAddress`, con el argumento recuperado por el decompilador de Ghidra (`inventario01.txt`). La columna "Subsistema" es una atribución por strings y rango de direcciones (hipótesis).

| Función (VA) | Carga | Símbolos que resuelve con `GetProcAddress` | Subsistema |
|--------------|-------|---------------------------------------------|------------|
| `FUN_00a0f770`, `FUN_00a12310` | `amd_ags.dll`, `amd_ags64.dll` (`LoadLibraryA`) | `agsInit`, `agsDeInit`, `agsCrossfireGetGPUCount`, `agsDriverGetVersionInfo`, `agsEyefinityGetConfigInfo` | AMD GPU Services (el juego trae `AMD_AGS.DLL`) |
| `FUN_00ff4ff0`, `FUN_00ff4ec0` | `nvapi.dll` (antes prueba `GetModuleHandleA`) | `nvapi_QueryInterface`, `nvapi_pepQueryInterface` | NVAPI |
| `FUN_00ff51d0` | `nvpowerapi.dll` (antes prueba `GetModuleHandleA`) | — | NVIDIA |
| `FUN_00fc6390` | `hid.dll` | `HidD_*` (11 funciones), `HidP_GetCaps` | HID (LibOVR, junto con `SetupDi*`) |
| `FUN_00feaaf0` | `OVBaseIFX64.dll` (`LoadLibraryW`) | `OVCam_ConnectionCheck`, `InitOVDriver`, `WriteRegister`, `ReadRegister`, `ChangeSensorMode`, `SendRegFile`, … | Cámara del Oculus DK2 |
| `FUN_00fe5de0` | `Opengl32.dll` (`LoadLibraryW`) | `gl*` (17), `wglGetProcAddress` | OpenGL (LibOVR, hipótesis) |
| `FUN_00fd2610` | `d2d1.dll`, `Dwrite.dll` (`LoadLibraryW`) | `D2D1CreateFactory`, `DWriteCreateFactory` | sin identificar (zona de LibOVR) |
| `FUN_00aa4450` | `dbghelp.dll`, `kernel32.dll` | `Sym*`, `StackWalk64`, `RtlCaptureContext`, `RtlCaptureStackBackTrace` | Havok StackTracer |
| `FUN_01103190` | `WS2_32.DLL` | `WSACreateEvent`, `WSACloseEvent`, `WSAEventSelect`, `WSAEnumNetworkEvents` | libcurl |
| `FUN_00a97910`, `FUN_00d0c1f0`, `FUN_00fb7280`, `FUN_00fb72e0` | `kernel32.dll` | `GetLogicalProcessorInformation`, `InitializeCriticalSectionAndSpinCount` | compatibilidad de SO |
| `___crtMessageBoxW`, `__mtinit`, `___crtCorExitProcess` | `USER32.DLL`, `KERNEL32.DLL`, `mscoree.dll` | `MessageBoxW`…, `Fls*`, `CorExitProcess` | CRT de MSVC |
| `FUN_00a12f00`, `FUN_00a130c0` | `GetModuleHandleA(NULL)` (módulo propio) | — | sin identificar |

Lo que **no** aparece (escaneo byte a byte, ASCII y UTF-16LE, sin distinguir mayúsculas; `inventario01_extra.txt`):
- `version.dll`, `dinput8`, `dinput.dll`, `xinput1_4`, `xinput9_1_0`, `d3dcompiler`, `dsound`, `x3daudio`: **0 apariciones.** No hay carga dinámica de los proxies "clásicos".
- `winmm`, `psapi`, `wsock32`, `imagehlp`, `setupapi`, `wldap32`: **una sola aparición cada uno**, y es el nombre del descriptor de import (offset = name RVA − `0x1A00`). Nadie los carga además por nombre.
- `CRASHREPORTER32.DLL` y `ATIAGS.DLL` están en la carpeta del juego pero `AI.exe` **no los nombra** (0 apariciones de "crashreporter"/"atiags"). Probablemente sean restos del depot. `CrashReporter32` exporta `CrashReporter::Initialise`, `SetMongoDB`, etc., depende de MSVCR100/MSVCP100 y su PDB es `C:\Console\CODER\Utils\CrashReporter\lib\vs2010-x32\crashreporter32.pdb`. Hipótesis: no se cargan.
- Audio: Wwise usa XAudio2 (RTTI `CAkSinkXAudio2`). XAudio2 2.x se crea por COM (`CoCreateInstance`), no por `LoadLibrary`. Hipótesis: no se verificó qué CLSID usan los 4 sitios de `CoCreateInstance`.

## 6. Exports (raro en un EXE)

`AI.exe` exporta **101 funciones**: 57 de **libcurl** (`curl_easy_*`, `curl_multi_*`, …) y 44 de **LibOVR** (`ovr_Initialize`, `ovrHmd_*`, …). Son linkeos estáticos re-exportados. Sirven como **huella de build en runtime**: por ejemplo, `ovr_Initialize` está en RVA `0xBA83B0` y `curl_easy_init` en RVA `0xCEAB20` (`inspect-native-exports.json`). El mod puede comparar `GetProcAddress(GetModuleHandle(NULL), "ovr_Initialize") - base` contra `0xBA83B0` antes de aplicar cualquier offset. Si no coincide, es otra build y no debe hookear nada.

## 7. Símbolos, RTTI y strings relevantes

- **Sin símbolos de debug** (no hay PDB). Ghidra identificó funciones del CRT por Function ID.
- **RTTI MSVC completo:** 2077 type descriptors y 2117 vftables (Ghidra). Por namespace: `CATHODE` 958, global 789, `OVR` 217, `AK` (Wwise) 36, `std` 9, Havok (`hk*`) unas decenas. Las 958 clases `CATHODE` incluyen los tipos de entidad de script y los sistemas del engine (`cathode_classes.txt`). Esto da nombres de clases, vftables y, a partir de ellas, métodos, aunque no haya símbolos. Es la base para los hallazgos 03–08.
- Ejemplos por tema (nombres RTTI, `CATHODE::…`):
  - Jugador/cámara: `PlayerCamera`, `PlayerCameraMonitor`, `CameraFinder`, `GetCurrentCameraPos`, `GetCurrentCameraTarget`, `VariableThePlayer`, `PlayerTorch`, `Chr_PlayerCrouch`.
  - Entidades y scripts: `Entity`, `EntityInterface`, `EntityState`, `CompositeInterface`, `ScriptInterface`, `ScriptVariable`, `ZoneableEntity`, `Zone`.
  - Mundo: `Door`, `DoorStatus`, `AccessTerminal`, `UI_Keypad`, `PickupSpawner`, `AddToInventory`, `InventoryItem`, `Checkpoint`, `SaveGlobalProgression`, `LevelLoaded`, `SwitchLevel`.
  - IA: `NPC_AlienConfig`, `NPC_SetupMenaceManager`, `NPC_SetAlienDevelopmentStage`, `NPC_WithdrawAlien`, `PathfindingAlienBackstageNode`.
- **Restos de multiplayer en Cathode** (strings y RTTI). La tabla de tipos de entidad de script, en offsets de archivo `0xFD9510`–`0xFD9B40` (`.rdata`), define cada tipo seguido de sus pines:
  - `AllPlayersReady` (`on_all_players_ready`, `activation_delay`)
  - `SyncOnAllPlayers` (`on_synchronized`, `on_synchronized_host`)
  - `SyncOnFirstPlayer` (`on_synchronized_local`)
  - `NetPlayerCounter` (`on_full`, `on_empty`, `is_full`, `contains_local_player`)
  - `BroadcastTrigger`, `HostOnlyTrigger`, `SpawnGroup`
  - `RespawnConfig` (`respawn_mode`, `respawn_wait_time`)
  - `NumConnectedPlayers`, `NumPlayersOnStart`, `NumDeadPlayers`
  - `NetworkedTimer` (`time_elapsed`, `time_left_sec`)
  - `IsMultiplayerMode`, `ParticipatingPlayersList`

  También están las constantes `HOST_ONLY_TRIGGER`, `NUM_CONNECTED_PLAYERS`, `IS_MULTIPLAYER_MODE`, `MULTIPLAYER_MOTIVATION` y `Multiplayer_Communications`. Y un log: `"WARNING: %i character %i is not found! Most likely it has already been deleted, this is okay in multiplayer, but suspicious if it triggers constantly"` (offset `0xEF4540`). **Hipótesis:** el sistema de scripts conserva nodos de un modo multijugador (descartado o de otro título de Cathode). No se sabe si hay código de red funcional detrás. Se investiga en el hallazgo 06. Si existiera, podría ahorrar mucho trabajo, pero no hay que contar con eso.
- **Rutas de código fuente** (`__FILE__`) del juego: 21, en `source_files.txt`. Son anclas para buscar funciones por xref:
  - `behavior_manager/menace_manager.cpp`, `alien_attack_manager.cpp`, `visibility_manager.cpp`
  - `main/main.cpp`, `character_playable.cpp`, `character_stealth_state.cpp`, `pickup_manager.cpp`, `thrown_item_manager.cpp`, `look_at_manager.cpp`, `player_projectile.cpp`
  - `CATHODE/{activity,display,filter,nav,renderable,weapon}_entities.cpp`
  - `Physics/Havok_{character,dynamics}.cpp`, `havok_ragdoll.cpp`
  - `display/pov_zone_matching.cpp`, `Pathfinding/Query/NearestReachablePosition.cpp`
- **Middleware identificado** por strings, RTTI y exports:
  - libcurl **7.29.0** (`"CLIENT libcurl 7.29.0"`)
  - Havok **2012.2.0** (`hk_2012.2.0-r1`, rutas `K:\Other\DevRel\Clients\CreativeAssemblyUK\2012.2.0\…`): Physics, Animation y Behavior
  - Scaleform GFx con AS3 (las strings `DRM*`/`drmAuthenticate` son clases de Flash, **no** DRM del juego)
  - Wwise (`AK::`, `CAk*`)
  - zlib 1.2.3
  - LibOVR (Oculus DK1/DK2)
  - NVAPI y AMD AGS

## 8. Conclusión sobre el proxy DLL

1. Los proxies "clásicos" no sirven: `dinput8`/`version` no se cargan nunca, y `dxgi`/`d3d11`/`xinput1_3` están ocupadas.
2. La carga dinámica no ofrece nada mejor. Todo lo que se carga dinámicamente es middleware que depende del hardware (AGS solo en AMD, NVAPI solo en NVIDIA, LibOVR/HID/cámara solo con Rift) o se carga tarde (dbghelp, curl).
3. Entre los imports estáticos libres, **`winmm.dll` sigue siendo la mejor candidata**:
   - se carga con el EXE, antes del entry point;
   - `AI.exe` usa solo 3 de sus funciones;
   - no la carga nadie más por nombre;
   - es independiente de ReShade y MovieFix.

   Requisitos para el proxy:
   - **reexportar todas** las exports de la `winmm` del sistema, no solo las 3 que usa `AI.exe`, porque otros módulos del proceso pueden importarla;
   - no hacer trabajo pesado en `DllMain` (loader lock): arrancar el mod en un thread o en el primer `timeGetTime`;
   - en Proton, `WINEDLLOVERRIDES="winmm=n,b" %command%`.
4. **Pendiente de verificar en el Windows del amigo:** que `winmm` **no** esté en KnownDLLs (si estuviera, Windows ignora la copia de la carpeta del juego). Es una hipótesis, no está verificado. Por la misma razón, `psapi`, `imagehlp`, `setupapi`, `wldap32` y `ws2_32` son malas candidatas si están en KnownDLLs (también sin verificar).

## Verificación en runtime

1. **Lista de módulos:** en la fase 1 del mod, loguear los módulos cargados (`EnumProcessModules` o ToolHelp) en el arranque y después de cargar un nivel. Alternativa en Proton: `WINEDEBUG=+loaddll`. Confirma qué DLLs se cargan dinámicamente y si `CRASHREPORTER32`/`ATIAGS` se cargan o no.
2. **KnownDLLs (Windows del amigo):** `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"`. `winmm` no tiene que aparecer.
3. **Base y build:** loguear `GetModuleHandle(NULL)` y la RVA de `ovr_Initialize` (esperada `0xBA83B0`) y de `curl_easy_init` (esperada `0xCEAB20`), más el TimeDateStamp del header (`0x54E3199D`).

## Preguntas abiertas

- ¿Se cargan realmente `CRASHREPORTER32.DLL` y `ATIAGS.DLL`? (runtime)
- ¿Qué CLSIDs crean los 4 `CoCreateInstance`? (`FUN_0099fe30`, `FUN_00c31ee0`, `FUN_00c702b0`, `FUN_00c70550`; se espera XAudio2 entre ellos)
- ¿Qué subsistema carga `d2d1`/`Dwrite` (`FUN_00fd2610`)?
- ¿Los nodos de script de multiplayer tienen código de red funcional detrás? (hallazgo 06)
- KnownDLLs en el Windows del amigo (ver arriba).
