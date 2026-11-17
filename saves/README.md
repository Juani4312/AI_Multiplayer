# Partidas guardadas para pruebas

Cada archivo `<nombre>.tar.gz` es una copia completa de los saves del juego en un momento dado. Sirven para probar algo en una zona concreta sin depender de dónde quedó la partida.

| Nombre | Dónde | Para qué | Fecha |
|--------|-------|----------|-------|
| `current` | **La partida real del usuario** (run sin morir, para el logro). La actualiza `mode.sh test` cada vez que se pasa de juego a prueba. | **No es de prueba:** nunca se carga para probar. | Se actualiza sola |
| `safezone` | HAB_ShoppingCentre, sala de espera del transit (jugador en −109,7; 20,5; −9,6). Objetivo: "Return to the Marshal Bureau". | Zona segura, sin enemigos cerca: un humano armado ("Riot Pistol", tipo 4) y tres civiles ("Dave", tipo 6) a unos 60 m; no hay Alien cargado (corrida 76). Es la partida que se usó en las corridas 20–82. | Save del 2026-11-14 04:39 (guardado en el repo el 2026-11-16) |
| `airport` | HAB_Airport, terminal de vuelo (M11, "Save 1 M11 - Spaceflight Terminal"): es `safezone` con su `CHSV00` copiado sobre `CHSV01`. | Otro nivel para probar la copia de saves (corridas 103, 113–115 lo usaron como "save del host"). No hay NPCs (censo, corrida 117). | 2026-11-17 |
| `alien` | SCI_HospitalLower, inicio de la misión 6 "Outbreak" ("Save 2 M6 - San Cristobal Medical Facility"), cargado con Load Game → Load Mission Save → Mission 6 desde `airport`. | **Tiene el Alien:** el censo muestra `NPCs[0] = "BackstageAlien"` (tipo 1) en (−12,79; −7,25; −9,94), con el jugador en (−3,42; −12,25; −3,15) (corrida 117). Para DESIGN_ALIEN. | 2026-11-17 |

## Cambiar entre juego y prueba (Linux, papito)

```
work/scripts/mode.sh status
work/scripts/mode.sh test [save] [--cfg "AIMP_..."]   # guarda la partida del usuario como current, carga el save de prueba e instala el mod
work/scripts/mode.sh play                             # restaura current (descarta la prueba) y desinstala el mod
```

- El save de prueba por defecto es `safezone`. Para probar el Alien o enemigos hace falta otro save; se agrega en esta tabla.
- El estado queda en `saves/.mode` (local). `mode.sh test` solo guarda `current` si el modo era "juego": nunca lo pisa con un save de prueba.

## Uso a mano (Linux, papito)

Con el juego cerrado:

```
work/scripts/saves.sh list
work/scripts/saves.sh load safezone     # backup local del estado actual en saves/backups/ y carga
work/scripts/saves.sh save <nombre>     # guarda los saves actuales con ese nombre
```

Después, en el juego, "Continue Game".

- **Ubicación de los saves:** carpeta de Steam Cloud, `~/.local/share/Steam/userdata/<cuenta>/214490/remote/`:
  - `MSSV01..19.ais`: slots de misión;
  - `CHSV00/01.ais`: checkpoints;
  - `PROGRESSION.ais`;
  - `input.cfg`: controles.
- **`load` reemplaza la partida actual:** el progreso que no esté guardado en `saves/` se pierde, salvo por el backup automático en `saves/backups/` (no se commitea).
- **Steam Cloud:** conviene cargar con Steam cerrado. Si al abrir el juego pregunta qué versión conservar, elegir la local.

## En Windows (laptop)

Los saves están en `C:\Program Files (x86)\Steam\userdata\<cuenta>\214490\remote\`. Para cargar uno:
1. cerrar el juego;
2. copiar la carpeta `remote` aparte, como backup;
3. borrar los `.ais` y el `input.cfg`;
4. descomprimir ahí el `.tar.gz` (por ejemplo, `tar -xzf safezone.tar.gz -C <carpeta remote>`).

Es la misma cuenta de Steam, así que los saves son compatibles.


