# 09 - Niveles, checkpoints y guardado (parte 1)

- **Pregunta:** ¿Cómo sabe el juego en qué nivel está, cómo carga niveles y checkpoints, y dónde guarda? Necesitamos mantener a los dos jugadores en la misma etapa.
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Puntero a la lista de niveles (`LevelList*`) | `0x01BE1C4C` (dato) | `0x17E1C4C` |
  | Getter `return *DAT_01be1c4c` (41 usos) | `0x00A2E810` | `0x62E810` |
  | `ModeManager::GetCurrentLevelName()` (`lista + modo->m_4 · 0x46`) | `0x0078BBE0` | `0x38BBE0` |
  | `ModeManager::GetCurrentLevelPath()` (`"data\env\%s\"`) | `0x0078BDB0` | `0x38BDB0` |
  | Chequeos de DLC (`PRODUCTION\DLC`, `ChallengeMap`, `SalvageMode`, `BSPNostromo`) | `0x0078BC60`–`0x0078BD60` | |
  | Registro de comandos de la UI de menús (nombre → ID) | `0x0084DBE0` | `0x44DBE0` |
  | Enumeración de `*.ais` (capa de plataforma/Steam) | `0x00A599F0`, `0x00A623A0`, `0x00A62630` | |
- **Evidencia:** `./work/evidence/h09/s1.txt` … `s6.txt`; `./work/evidence/h02/dump14.txt` (máquina de estados). El listado de la carpeta de Steam Cloud se hizo solo leyendo.
- **Confianza:** alta para la lista de niveles y el registro de comandos. Media para el uso de los comandos (no se vio el dispatcher).
- **Cómo verificar en runtime:** leer `[[0x1BE1C4C]+0x2BC8]` (índice) y el nombre en `[0x1BE1C4C] + índice·0x46` al cargar distintos niveles.
- **Verificado en runtime:** no.

## Lista de niveles

```
LevelList* L = *(LevelList**)0x01BE1C4C;
  L + i*0x46      : nombre del nivel i (char[0x46]; p. ej. rutas tipo "Production\DLC\…")
  L + 0x2BC0      : cantidad de niveles
  L + 0x2BC8      : índice del nivel actual (lo escribe ModeManager::Run en el estado 0: modo->m_4)
  L + 0x2BCC      : flag (lo consulta la factory de modos)
```

Cada nivel tiene su objeto "modo" (hallazgo 02). El `ModeManager` los recorre con su máquina de estados: el estado 3 carga/arma el nivel (`FUN_007db4b0`) y el 5 lo corre. La factory (`FUN_0079a740`) crea un modo por entrada y compara contra `"FE_LEVEL"`, el nivel del front-end.

## Comandos de la UI (Scaleform → juego), `FUN_0084dbe0`

El registro asocia 75 nombres con IDs (de 3 en 3). Los relevantes:

| ID | Comando | ID | Comando |
|----|---------|----|---------|
| `0x7E` | `load_level` | `0x81` | `cache_level` |
| `0x84` | `set_difficulty` | `0x87` | `update_current_checkpoint` |
| `0x8A` | `update_previous_checkpoint` | `0x8D` | `update_mission_checkpoints` |
| `0x90` | `update_debug_checkpoints` | `0x93` | `load_current_checkpoint` |
| `0x96` | `load_previous_checkpoint` | `0x99` | `load_checkpoint` |
| `0x9C` | `cache_checkpoint` | `0x9F` | `jump_to_debug_checkpoint` |
| `0xBA` | `save_game_settings` | `0xC3` | `back_to_frontend` |
| `0xC9` | `request_quit_game` | `0xCF`/`0xD2`/`0xD5` | `request_load_current/previous_checkpoint`, `request_load_checkpoint` |

El resto son opciones (`set_field_of_view` 0x0, gráficos, audio, controles, `on_page_changed`, `on_item_selected`, …).

**Para el mod:** mandar a los dos jugadores al mismo nivel/checkpoint podría hacerse invocando estos comandos, igual que lo hace el menú, en lugar de manipular estructuras internas. Falta encontrar el dispatcher (el `switch` por ID) y cómo se le pasan los parámetros (nombre del nivel/checkpoint).

## Guardado

- Va por **Steam Remote Storage (Steam Cloud)**. En la máquina del usuario: `~/.steam/steam/userdata/<id>/214490/`.

  | Archivo | Tamaño | Contenido (hipótesis por nombre) |
  |---------|--------|----------------------------------|
  | `remote/CHSV00.ais`, `CHSV01.ais` | 2 MB c/u | checkpoints |
  | `remote/MSSV01`–`MSSV19.ais` | 1 MB c/u | slots de misión o guardado manual |
  | `remote/PROGRESSION.ais` | 10 KB | progreso global |
  | `remote/input.cfg` | | mapeo de controles |
  | `local/settings.bin` | | opciones |

- El código que enumera `*.ais` está en la capa de plataforma (`0xA5xxxx`–`0xA6xxxx`), junto a `SteamAPI_Init` (`FUN_00a540d0`). Hay 21 sitios que usan `SteamRemoteStorage()` (hallazgo 01).
- Hay un **sistema de replay** (`"Replay system: starting to save replay, checkpoint %d"`, `"… loaded replay from '%s' - checkpoint %d"`). No se sabe si está activo en retail; podría servir para entender el determinismo.

## Implicaciones para el coop

- **Mismo nivel:** el host puede mandar su índice de nivel (`L+0x2BC8`) o el nombre, y su checkpoint; el cliente lo compara antes de conectar.
- **Mismos datos:** los dos necesitan los mismos `COMMANDS.PAK` (ver la nota de PROGRESS sobre los `COMMANDS.PAK` modificados del usuario) y la misma build.
- No hace falta copiar los guardados entre máquinas si el mod puede ordenar "cargar el checkpoint X" vía los comandos de la UI. Requiere el dispatcher.

## Preguntas abiertas (parte 2)

1. ¿Dónde está el dispatcher de los comandos de la UI y qué parámetros recibe `load_level`/`load_checkpoint`?
2. ¿Cómo se nombran los checkpoints (`"checkpoint_name"` aparece en un JSON) y qué hay en `CHSV*.ais` vs. `MSSV*.ais`?
3. ¿El sistema de replay está activo?
4. Script entities `Checkpoint`, `SaveGlobalProgression`, `SwitchLevel`, `LevelLoaded`: ¿qué disparan?
