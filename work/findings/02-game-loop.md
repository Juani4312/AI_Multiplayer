# 02 - Game loop / tick principal

- **Pregunta:** ¿Dónde está el loop principal y qué punto estable sirve para ejecutar código del mod una vez por frame, en el thread del juego?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | `WinMain` | `0x00A111A0` | `0x6111A0` |
  | Main del engine (`main/main.cpp`): init + loop + shutdown | `0x007DAE80` | `0x3DAE80` |
  | Cuerpo del loop principal | `0x007DB220`–`0x007DB25E` | `0x3DB220` |
  | Puntero al objeto raíz (`g_root`) | `0x016F0C88` (dato) | `0x12F0C88` |
  | `ModeManager::Run` (máquina de estados, thiscall, ECX = `[g_root+0x3C]`) | `0x007A8460` | `0x3A8460` |
  | `GameFlow::Tick` (thiscall, ECX = `[g_root+0x48]`) | `0x007CAD10` | `0x3CAD10` |
  | Message pump (`PeekMessageA`/`DispatchMessageA`) | `0x00A12FB0` | `0x612FB0` |
  | vtable de la clase "modo de nivel" | `0x012F43E8` | `0xEF43E8` |
  | Update por frame del nivel (vtable `+0x14`) | `0x00790000` | `0x390000` |
  | Constructor del modo de nivel (vtable base `0x012F3114`) | `0x0078FEE0` | `0x38FEE0` |
- **Evidencia:** `./work/evidence/h02/dump1.txt` … `dump14.txt`. Salidas de `work/scripts/ghidra/Dump.java` (decompilación, asm, xrefs, vtables y argumentos de llamadas), con sus `.log`.
- **Confianza:** alta para la estructura del loop, los offsets de `g_root`, la máquina de estados y los prólogos (salen de asm y decompilación). Media para los nombres que les puse a las clases (`ModeManager`, `GameFlow`, "modo de nivel"); son interpretación.
- **Cómo verificar en runtime:** fase 1 del mod (ver abajo): hookear `0x3A8460` y loguear por frame el estado (`+0x4C`) y el modo actual (`+0x40`).
- **Verificado en runtime:** no.
- **Preguntas abiertas:** al final.

## Cadena de arranque

`entry` (`0xA71E61`) → `___tmainCRTStartup` (`0xA71CF4`) → `WinMain` = `FUN_00a111a0(0x400000, 0, cmdline, nShowCmd)`.

`WinMain`:
1. Parsea la línea de comandos (`FUN_00a0f2d0`) en una tabla de pares nombre/valor (`DAT_01bda090`/`DAT_01bda490`, cantidad en `DAT_01bd7f9c`).
2. Lee `"monitor"` y `"adapter"` con `FUN_00a0f4a0`.
3. Llama a `FUN_007dae80`, el main del engine.

`FUN_007dae80` hace el init por etapas. Cada etapa se valida con `FUN_00a147b0(línea, "…\\main/main.cpp")`, probablemente "¿hubo error fatal?". Si falla la carga de datos muestra el MessageBox de "verify the integrity of the game cache". La última etapa es `FUN_007d7650`, que carga `data\tasks.txt`, `animation.pak`, etc. Después viene el loop:

```asm
007db214  MOV byte ptr [0x017073ee],BL      ; flag "corriendo" = 1
007db220  CALL 0x00a69c90                    ; nullsub (ret de 1 byte, marcador vacío)
007db225  CALL 0x00689310
007db22a  MOV EDX,[0x016f0c88]               ; g_root
007db230  MOV ECX,[EDX+0x48]
007db233  CALL 0x007cad10                    ; GameFlow::Tick
007db238  MOV EAX,[0x016f0c88]
007db23d  MOV ECX,[EAX+0x3c]
007db240  CALL 0x007a8460                    ; ModeManager::Run -> AL = seguir
007db245  MOV BL,AL
007db247  CALL 0x00a69c90                    ; nullsub
007db24c  MOVZX ECX,byte ptr [0x016f0dd8]
007db254  CALL 0x00a69c90                    ; nullsub(arg)
007db25c  TEST BL,BL
007db25e  JNZ 0x007db220
007db260  CALL 0x007ccde0                    ; shutdown
```

## `ModeManager::Run` (`0x7A8460`): una vuelta = un frame

Campos de `this` (`[g_root+0x3C]`):

| Offset | Uso |
|--------|-----|
| `+0x00`…`+0x3C` | anillo de 16 punteros a objetos "modo" |
| `+0x40` | modo actual |
| `+0x44` | índice en el anillo (`& 0xF`) |
| `+0x4C` | estado; `-1` = salir |
| `+0x2DC` | byte que se resetea al cambiar de modo |

Cada iteración hace `SleepEx(0,1)`, pump de mensajes y `switch(estado)`:

| Estado | Acción |
|--------|--------|
| 0 | toma el siguiente modo del anillo; si es NULL devuelve 0 (salir) → 1 |
| 1 | `FUN_008091e0(1)` → 2 |
| 2 | estado = `modo->vf[+0x0C]()` |
| 3 | carga `DATA\global\Animation.pak`, …, `modo->vf[+0x04]()` (setup/carga del nivel) → 5 o 7 |
| 4 | si `!modo->vf[+0x08]()` → 5 |
| **5** | **`r = modo->vf[+0x14]()` (update por frame); si `r != 5`, cambia de estado** |
| 6 | `modo->vf[+0x1C]()` |
| 7 | `modo->vf[+0x18]()` (salida), → 0 |

El `do/while` interno se repite mientras el estado cambie. Cuando el handler devuelve el mismo estado (5 mientras se juega), sale y devuelve 1. Por eso **cada vuelta del loop de `main` ejecuta exactamente un update de nivel**.

## Modo de nivel

- Vtable `0x012F43E8`, sin RTTI:

  | Slot | Función |
  |------|---------|
  | `+0x00` | `FUN_007db420` (destructor) |
  | `+0x04` | `FUN_007db4b0` (setup/carga, 7 KB, bombea mensajes durante la carga) |
  | `+0x08` | `FUN_006508c0` |
  | `+0x0C` | `FUN_00c622f0` |
  | `+0x10` | nullsub |
  | **`+0x14`** | **`FUN_00790000`** (update por frame) |
  | `+0x18` | `FUN_007d9810` (salida) |

- Objetos de 0x110 bytes. Los crean `FUN_0079a6b0` y `FUN_0079a740`, que guardan el índice en `modo+4` y comparan nombres de nivel contra `"FE_LEVEL"`. Hipótesis: hay un objeto modo por nivel (front-end y niveles de juego).
- `FUN_00790000` hace, en orden: flags de input (`DAT_016f194c`), varias actualizaciones de subsistemas (`FUN_009ac550`, `FUN_0097cb10`, `FUN_009f19c0`, `FUN_00a048d0`, …) y `"OpenScene"`. Quedan 69 líneas sin revisar (`dump11.txt`). Los subsistemas concretos (personajes, IA, física, scripts, render) se identifican en los hallazgos 03–08.

## Threads

- **El update de juego corre en el thread principal**: el de `WinMain` → loop → `ModeManager::Run` → `FUN_00790000`.
- Threads secundarios:
  - `_beginthreadex` desde un sistema de jobs/threads con nombre: `FUN_0085e4a0`/`FUN_00863af0`, que nombran el thread con `FUN_0085e3f0` (SetThreadName).
  - También desde `FUN_00ce6cc0`, `FUN_00f6a640`, `FUN_00fb14f0` (LibOVR, hipótesis) y libcurl (`FUN_010f8ad0`).
  - `CreateThread` + `SetThreadAffinityMask` en `FUN_00c3a350` (hipótesis: Wwise).
- El render usa **deferred contexts** de D3D11. Evidencia de runtime: `ReShade.log` registra 4 `ID3D11Device::CreateDeferredContext`. Por eso el device es multithread, y leer estado del juego fuera del thread principal puede dar carreras.

## ⚠️ No tocar: chequeo de licencia en `GameFlow::Tick`

`FUN_007cad10`, casos 3 y 7: llama a `SteamApps()->vf[0]()` y, si devuelve false, escribe en la dirección 0 (`*(int*)0 = 0xFFFFFFFF` / `0x742345FF`). Es un **crash deliberado**: chequeo de licencia de Steam. El mod **no debe hookear, parchear ni saltear `FUN_007cad10`** (regla del proyecto: no tocar DRM/licencias). Hookear `ModeManager::Run` (`0x3A8460`), que corre después, no lo afecta.

## Línea de comandos

- **Funcionan** (`FUN_00a0f4a0` + thunk `0x00a0f4e0`): `monitor`, `adapter`, `benchmark`, `engine_settings`. Sintaxis del parser: nombre precedido por `-`, `/` o `:`, separado del valor por `=`, `:` o espacio (`-adapter=0`). Hipótesis en el detalle; no se probó.
- **No funcionan en retail:** `skip_frontend`, `level`, `paused`, `fixed_time_step`, `realtime`, `noperf`, `stats`, `no_ui`, `debug_text_enabled`, `vsync_at_30`, `makemovie`, `no_data_sync`, `checked_memory`, `extended_memory`, `persistence`, `texture_streaming_budget` y otras. Se consultan con `FUN_00d0fb70`, que es `return 0;` (3 bytes, plegado por el linker). Para cargar un nivel directo habría que hacerlo desde el mod (hallazgo 09).
- `benchmark` + `BENCHMARK_TEMPLATE.TXT` (en la carpeta del juego) podrían servir para pruebas automáticas reproducibles. Hipótesis.

## Punto de enganche recomendado para la fase 1 del mod

1. **`ModeManager::Run`, RVA `0x3A8460`** (thiscall, devuelve `bool` en AL). Se llama una vez por frame desde el loop principal, en el thread principal, después de `GameFlow::Tick`. Prólogo verificado: `51 53 55 56 8B F1 83 7E 4C FF`.
   - `push ecx; push ebx; push ebp; push esi; mov esi,ecx` = 6 bytes sin direccionamiento relativo: se puede reubicar en un trampolín propio, sin MinHook.
   - El detour llama al original y después corre el código del mod.
   - Para saber si se está jugando: `[[g_root+0x3C]+0x4C] == 5`; el modo actual está en `[[g_root+0x3C]+0x40]`.
2. Alternativa que también corre **durante las cargas** (útil para mantener viva la red): el message pump, RVA `0x612FB0`. Prólogo `8B 0D <abs32> 83 EC 1C`: el `mov ecx,[abs32]` tiene una dirección reubicada por ASLR, pero se copia ya reubicada en runtime.
3. Antes de parchear, el mod tiene que verificar los bytes exactos del prólogo y la RVA de `ovr_Initialize` (`0xBA83B0`, hallazgo 01). Si no coinciden, no hookea nada.
4. No usar `IDXGISwapChain::Present`: lo hookean ReShade y MovieFix, y no se verificó en qué thread corre.

## Preguntas abiertas

- ¿Qué hay en `+0x40`/`+0x44` cuando el juego está en el menú vs. en un nivel? (runtime)
- Contenido completo de `FUN_00790000` y su orden respecto del render; ¿dónde se calcula el delta time?
- ¿Qué threads crea el sistema de jobs y con qué nombres? (los nombres llegan por parámetro a `FUN_0085e4a0`)
- ¿Qué es `FUN_00689310` (condicionado a `DAT_016b1095`)?
