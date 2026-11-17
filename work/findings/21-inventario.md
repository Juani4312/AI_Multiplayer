# 21 - Inventario del jugador y base de ítems

- **Pregunta:** ¿dónde está el inventario y cómo se suman y restan ítems con las funciones del juego? Hace falta para el inventario compartido (decisión del usuario: "compartimos todo"; `work/DESIGN_SESSION.md`).
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`. `g_root` = `[0x016F0C88]`.

  | Qué | Dónde |
  |-----|-------|
  | Inventario del jugador 0 | `[g_root+0x44]`: 34 slots (`0x22`) de `0x14` bytes desde `+0x68`: `+0` definición, `+4` instancia (armas), `+8` cantidad |
  | Bloqueo del inventario (si no es 0, no se agrega) | `inventario+0x3BBC` |
  | Agregar: `thiscall(inv, def, cantidad, flag)`, `ret 0xC`; respeta el máximo (`def+0x1C`) | `0x0079BB20` (envoltorio `0x007AA4E0`, el que usa `AddToInventory`) |
  | Slot de una definición (−1 si no está) | `0x0077F330` |
  | Vaciar un slot: `thiscall(inv, slot)`, `ret 4` (refresca la UI de armas) | `0x0078DDF0` |
  | Entidades `AddToInventory` / `RemoveFromInventory` | `0x00574EF0` / `0x00575210` |
  | Base de ítems (la carga `FUN_007bcec0` desde `DATA\gbl_item.bml`) | `[g_root+0x40]`: `+0x04` cabeza de una lista de todas las definiciones (nodos `{def, anterior, siguiente}`, `FUN_00795280`), `+0x48` cantidad (45) |
  | Definición de un ítem | `+0x08` nombre (`char*`), `+0x10` id secuencial, **`+0x14` ShortGuid del nombre**, `+0x18` cantidad por defecto, `+0x1C` máximo, `+0x30` categoría (`char*`) |

- **Evidencia:** `work/evidence/h20/i1.txt` … `i11.txt` (Ghidra; local). Runtime con el lector de memoria (`work/scripts/memsrv.py`): slots, definiciones y la lista completa de 45 ítems. `def+0x14` coincide con `shortguid(nombre)` (`work/scripts/shortguid_check.py`) en scrap, flamethrower, pistol y flare. Corridas 93–94: `work/evidence/runtime/run93…` (local).
- **Confianza:** alta (estático, más runtime: sumar, restar y vaciar verificados).
- **Verificado en runtime:** sí (corridas 93–94, una máquina, con eventos del peer).

## Los 45 ítems (id, nombre, máximo, categoría)

0 INVALID_ITEM; 1 keycard; 2 motiontracker; 3 radioscanner; 4 memento_dogtag; 5 detonator; 6 torch_battery (6); 7 gasoline (3); 8 high_voltage_battery (3); 9 explosive (3); 10 pipe (3); 11 sharp_blade (6); 12 gel (6); 13 adhesive (6); 14 scrap (100); 15 mission_oscillator; 16 flare_nophysics. Todos estos son Component.

Tool, de a 1: 17 emergency_axe, 18 pistol, 19 flamethrower, 20 shotgun, 21 boltgun, 22 cattleprod.

Ammo: 23 9mm_pistol_ammo (60), 24 shotgun_shells (60), 25 shotgun_incendiary_shells (40), 26 dum_dum_pistol_mag (40), 27 shotgun_solid_slug_shells (40), 28 flamer_fuel (500), 29 tazer_pistol_ammo (40), 30 bolt (40), 31 charge (10).

Health: 32 small_medikit (6).

Holdable: 33 pipe_bomb (3), 34 molotov_cocktail (3), 35 emp_mine (3), 36 smoke_bomb (3), 37 flashbang (3), 38 noisemaker (3), 39 explosive_mine (10), 40 incendiary_mine (10), 41 flare (6), 42 flare_box (10), 43 wide_area_chem_light (10), 44 chem_light (10).

## Inventario compartido (`mod/src/inv.c`)

- **Detectar:** cada 100 ms se leen los 34 slots, se suman por ShortGuid y se comparan con la lectura anterior. Los cambios salen en `AIMP_EVT_INV`: hasta 13 pares `(ShortGuid, delta)` por evento.
- **Aplicar:**
  - la definición se busca por ShortGuid en la lista de la base;
  - delta positivo: `FUN_0079bb20`;
  - delta negativo: se baja la cantidad del slot, y si llega a 0, se vacía con `FUN_0078ddf0`;
  - el resultado se suma a la lectura anterior, así no vuelve como eco.
- **Asentamiento:** al aparecer un jugador nuevo (carga), durante 3 s solo se actualiza la base (`AIMP_INV_SETTLE_MS`). Corrida 93: las armas entran ~0,3 s después de la aparición y se mandaban como "+1".
- **Resultados:**
  - corrida 93: bengalas 2 → 4 → 3, botiquín 1 → 2 → 0 (slot vaciado), combustible 200 → 190 y noisemaker 0 → 2 → 0 (ítem nuevo), sin eco;
  - corrida 94: sin eventos espurios al cargar, escopeta recortada al máximo (1 → 1), munición 9 mm 5 → 10 y molotov nuevo 0 → 1.

## Preguntas abiertas

1. La munición cargada en el arma (instancia en el slot `+4`) no está en el slot. La reserva es la que se comparte; el cargador viaja con el arma (DESIGN_SESSION).
2. ¿El HUD o la rueda de ítems refrescan una cantidad escrita a mano en el slot? Agregar y vaciar usan las funciones del juego, pero restar sin vaciar escribe `+8` directo. No se pudo ver en la rueda: no encontré el botón con el mando virtual.
3. "Armas de a una" (DESIGN_SESSION): falta ubicar el arma equipada (`Character+0x1100`, hallazgo 19) y cómo bloquearla en la rueda.
