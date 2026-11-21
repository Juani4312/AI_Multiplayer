# work/tools — herramientas de terceros que usamos

Herramientas de la comunidad de modding de Alien: Isolation, usadas **solo para leer** formatos del juego
(scripts de niveles, UI, configuraciones). Sirven para orientarse y para ponerle nombre a lo que ve el mod;
**nunca son fuente de offsets**: todo offset sigue saliendo de Ghidra/REA o del runtime (`prompt.md`).
Nada de esto escribe sobre la instalación del juego.

| Herramienta | Qué es | Dónde | Licencia | Para qué la usamos |
|---|---|---|---|---|
| [CathodeLib](https://github.com/OpenCAGE/CathodeLib) (OpenCAGE, Matt Filer) | Librería C# que lee y escribe los formatos de CATHODE | submódulo `third_party/CathodeLib` (commit `69ef3a8`, 2026-10-24) | MIT | Base de `cathode-dump` |
| `cathode-dump/` (nuestra) | Programa de consola sobre CathodeLib | este repo | — | Volcar scripts de niveles, UI y BML |
| [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler) 26.5.0 | Descompilador de Flash/Scaleform (`.GFX`) | `~/tools/ffdec-26.5.0` (no se commitea; `get-ffdec.sh` lo baja y verifica el SHA-256) | GPL-3 | Leer el ActionScript de la UI (popups, HUD, menús) |

Requisitos: `dotnet-sdk` (10) y Java 21 (FFDec). Clonar con submódulos: `git submodule update --init --recursive`.

## cathode-dump

```
work/tools/cathode-dump.sh [NIVEL ...]     # todo (o solo esos niveles) -> work/evidence/cathode/
cathode-dump commands <COMMANDS.PAK> [salida.tsv]
cathode-dump bml <archivo.BML> [salida.xml]
cathode-dump pak2 <UI.PAK> <carpeta>
```
(`bin/out/cathode-dump` después del primer `cathode-dump.sh`.)

### `commands`: árbol de scripts con el pid del mod

Una fila por entidad instanciada (recorre los composites desde las 3 raíces: nivel, Global, PauseMenu), con:
`pid` (el mismo de `mod/src/pins.c`), `depth`, `id` (ShortGuid de la entidad), `type` (función o composite),
`type_name`, `name`, `path` (ruta de nombres), `links_out` (pin → entidad.pin), la cadena de ancestros e `inst` (id de instancia
del composite que la contiene, el `EntityHandle` de CathodeLib).

**Objetivos de los "usar":** el id de 8 bytes del RPC 0x26 (`objetivo XXXXXXXXYYYYYYYY` en el log) es `id` + `inst` con los bytes
en orden de memoria (6 de 6 de la corrida 119). `work/scripts/use_names.py <log> <volcado>` les pone nombre.

**Verificado contra las trazas de pines del juego** (`work/evidence/runtime/*.log`): coinciden id, tipo y
profundidad en **11 726 de 14 055** pids de HAB_ShoppingCentre (83 %) y **10 178 de 10 582** de SCI_HospitalLower (96 %).
Regla del pid (`work/NOTES.md`, "cathode-dump"): `hash(entidad, composites intermedios…, terminal)`, donde el
terminal es `ShortGuid.InstanceGuid` (`0x4AF05BFE`) para la raíz del nivel y el guid del composite para Global y PauseMenu.
Lo que no está en el volcado son entidades que el juego crea al vuelo (arrojables, armas, NPCs que aparecen) y
composites compartidos que el juego instancia más arriba que en el archivo.

Uso típico: buscar un pid de un log (`zgrep -P '^5d44a924\t' work/evidence/cathode/commands/*.tsv.gz`) o
buscar entidades por tipo (`zgrep -P '\tDoor\t'`).

Los modelos/colisiones de cada entidad no se resuelven (CathodeLib los pide para cargar; se le pasan vacíos).

### `pak2` y FFDec: la UI

`UI.PAK` → `ui/DATA/UI/*.GFX`. Si el mod NoHUD está activo, `cathode-dump.sh` usa la copia original de
`mods/NoHUD-crosshair/bak/orig/`. Para el ActionScript:
```
~/tools/ffdec-26.5.0/ffdec.sh -export script work/evidence/cathode/gfx/POPUPOVERLAY work/evidence/cathode/ui/DATA/UI/POPUPOVERLAY.GFX
```

### `bml`

Las configuraciones `.BML` a XML: dificultades (`DIFFICULTYSETTINGS/*.BML`), `GLOBALCONSTANTS`, `GBL_ITEM`.
Los árboles de comportamiento ya vienen en XML (`DATA/BEHAVIOR/`), originales desde que se borró SmartAI (2026-11-21;
copia también en `game/data/BEHAVIOR`). **Ojo:** el mod MildlyInsaneAI reemplaza `DATA/CHR_INFO/ATTRIBUTES/ALIEN.BML` mientras está activo
(el original queda en `mods/MildlyInsaneAI/bak/orig/`).

## Lo que no usamos (y por qué)

- Las herramientas de 2014 de [ModDB "Alien Isolation Extractors"](https://www.moddb.com/mods/alien-isolation-extractors)
  (texturas, audio, modelos, UI Mod Tool, BML): las reemplaza CathodeLib/OpenCAGE, del mismo autor.
- [OpenCAGE](https://github.com/MattFiler/OpenCAGE)/AssetEditor (interfaz gráfica, Windows): opcional para mirar
  a mano; usa la misma CathodeLib.
- [OpenIsolation](https://github.com/OpenIsolation/OpenIsolation): SDK en etapa muy temprana, sin offsets ni build declarada.
