# 15 - Pines de CATHODE: despacho, identidad de entidades y reinyección (fase 6)

- **Pregunta:** ¿Por dónde pasa el disparo de un pin de script (diálogos, puertas, objetivos) y cómo se identifica la misma entidad en dos máquinas, para replicar los disparadores?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Disparar pin de salida (`EntityInterface::vt[+0x394]`) | `0x008BE0D0` | `0x4BE0D0` |
  | Wrapper genérico `this->vt[0x394](ctx, &pin, msg, 0.0, 0)` | `0x008BD920` | `0x4BD920` |
  | Búsqueda del pin en la tabla de links (`this+4`) | `0x008BD270` | `0x4BD270` |
  | Entrega demorada al entity manager | `0x009271C0` | `0x5271C0` |
  | ShortGuid de un nombre (hallazgo 06) | `0x008BF7F0` | `0x4BF7F0` |
  | Caché global de entidades (`CATHODE:update_cache`) | `0x0174DFB0` (dato) | `0x134DFB0` |
- **Evidencia:** `work/evidence/h15/f1.txt` … `f24.txt` (Ghidra), `slot_scan.txt`, `shortguids.tsv`, `vtables.tsv`; corridas 15 a 18 en `work/evidence/runtime/run15-*.log` … `run17-*.log` (gitignored).
- **Confianza:** alta (estático + runtime en 4 corridas).
- **Verificado en runtime:** sí (corridas 15 a 18, Proton, HAB_ShoppingCentre y Tech_Hub).

## Despacho

- Las vtables RTTI de `TriggerSimple`, `Door`, etc. **no** tienen el comportamiento: sus slots `+0x1C`/`+0x20`/`+0x38` son getters triviales de valores por defecto (`FUN_004a25f0` devuelve un vector global), y las dos vtables son idénticas en los 26 primeros slots.
- Las vtables de entidad miden `0x398` bytes (230 slots). En `+0x394` está `FUN_008be0d0` en **777 de las 918 clases CATHODE con RTTI** (las demás no son entidades). Las clases concretas sin RTTI también lo heredan. Por ejemplo, `TriggerSimple` concreta tiene un objeto con herencia múltiple: vtable `0x01262250` en `+0` y vtable de `EntityInterface` `0x01261EB8` en `+0x14`, con `FUN_008be0d0` en `+0x394`.
- Hay unos 40 wrappers por pin (uno por nombre: `on_triggered`, `on_finished`, `on_entered`…), del estilo `FUN_008cd7c0` → `FUN_008bd920(&DAT_017523c8 = ShortGuid("on_triggered"))`. Los ShortGuids de los nombres se calculan en inicializadores estáticos (`PUSH "nombre"; MOV ECX, global; CALL 0x8BF7F0`; 7742 sitios y 5851 nombres distintos, en `shortguids.tsv`).
- **Firma** (asm de `FUN_008bd920` y `FUN_008be0d0`, `RET 0x18`):
  `thiscall(EntityInterface *this, Handle **ctx, uint32 *pin, Msg **msg, double demora, uint32 flag)`.
- `FUN_008be0d0`:
  1. busca el pin en la tabla de `this+4` (objeto con `+8` cantidad y `+0xC` entradas de 12 bytes: `[0]` pin, `[8]` primer link; búsqueda binaria `FUN_008bd010`);
  2. recorre los links (`link+8` destino, `link+0xC` siguiente);
  3. para cada destino, o lo dispara directo (`vt+0x394`) o lo encola en el entity manager con la demora (`FUN_009271c0`). Se entrega en el frame siguiente.
- Prólogo para el hook: `55 8B EC 6A FF 68 <imm32 relocado>`; se roban 5 bytes.

## Objetos

| Objeto | Layout |
|--------|--------|
| `EntityInterface` (`this`) | `+0` vtable, `+4` tabla de links (refptr), `+8` índice en el caché (24 bits) \| flags. **El índice no es estable**: coincide solo en 2201/9901 entre dos cargas |
| Handle (`*ctx`) | `+4` refcount atómico, `+0xC` objeto completo de la entidad. En todos los disparos vistos, `ctx` es la propia entidad, a 0x14–0x20 bytes antes de `this` |
| Objeto completo | `+0x08` handle al `CompositeInstance` padre; `+0x0C` **ShortGuid de la entidad** (está en el `COMMANDS.PAK`: 78/78); `+0x10` **ShortGuid del tipo** (`TriggerSimple` = `dd5a65d6`, `Door` = `af6549bb`, `LogicGate` = `4c72c570`…) |
| Mensaje (`*msg`) | Objeto de un pool, de 0x28 bytes: `+0x20` flags, `+0x24` refcount (`FUN_0091e1e0`). Al llegar a 0 vuelve al pool `DAT_0174ef3c` (`FUN_009271c0`) |

## Identidad estable: `pid`

- El ShortGuid de la entidad **no es único** en el nivel: es su id dentro del composite (prefab), que se instancia muchas veces. Corrida 17: 763 de 2023 ids aparecen en más de una entidad (una puerta, en unas 30).
- `externo+0x08` es un handle al `CompositeInstance` padre: en 60 de 124 volcados apunta al handle de un `CompositeInstance` conocido. Los demás apuntan a composites que no dispararon pines.
- **`pid` = hash de los ShortGuids de la entidad y de cada padre hasta la raíz** (`path_id` en `mod/src/pins.c`). Equivale a la ruta de instancia de CATHODE.
  - Único: corrida 17, 9938 instancias, **0 colisiones**. Profundidad de 1 a 10.
  - Determinista entre procesos: entre las corridas 17 y 18 (dos arranques del juego, misma partida), **9901/9901** instancias comunes, con el mismo tipo, id y profundidad (`work/scripts/compare_pids.py`).
  - Depende solo de los datos del nivel, así que tendría que ser igual en la otra máquina con el mismo `COMMANDS.PAK`. Falta verificarlo con la laptop.

## Lo que se ve en runtime

- Unos 1000 disparos por segundo en juego, casi todos en el thread principal (184 de 43 000 en otro thread), con anidamiento de hasta 4.
- **Al cargar una partida, el estado se restaura disparando pines:** en Tech_Hub, unos 6700 `on_restored` más `on_restored_true/false` de `Variable*` y `LogicSwitch`. Es lo que haría la opción "sincronizar por checkpoints".
- Los pines más frecuentes en juego son de pensamiento y animación (`on_think`, `8fecea29`, …). Los que tienen links son los que mueven el grafo (unos 500 de 12 000 en la carga).

## Reinyección (experimento del paso 3)

- `FIRE <pid> <pin>` (archivo `AIMP_PINS_FILE`) vuelve a llamar a `FUN_008be0d0` sobre la entidad. Usa copias propias del handle y del mensaje del primer disparo visto, con refcount `0x10000000`, para que el motor nunca las libere ni las devuelva al pool.
- **Corrida 17:** 4 inyecciones de `LogicSwitch f14051ea.on_true` (pid `3a7cb07d`). En las 4, el destino (`TriggerSequence 4c32145d`, pid `3cc91288`) disparó sus 3 pines en el frame siguiente (unos 7 ms), con el mismo contador que el pin inyectado (n19→n19, n53→n53, n59→n59, n66→n66). Es el mismo patrón que el ciclo natural (n54→n54). Sin excepciones.
- **Conclusión:** un pin de salida reinyectado se propaga por el grafo de CATHODE igual que uno natural.

## Para el mod

- `mod/src/pins.c` (`AIMP_TRACE_PINS=1`): traza por (entidad, pin) con id, tipo, pid, links y quién lo disparó. La tabla se renueva en cada nivel y conserva las claves de los últimos 3 s, porque el nivel nuevo empieza a disparar ~1 s antes de que la sonda vea el cambio.
- `work/scripts/analyze_pins.py` traduce los registros (con `shortguids.tsv` y `vtables.tsv`). `vtables.tsv` sale de `work/scripts/ghidra/MapVtables.java` y mapea 4133 vtables concretas a su clase.
- `mod/src/autopad.c` (`AIMP_AUTOPAD` / `AIMP_AUTOPAD_FILE`) es un mando virtual por la IAT de `XINPUT1_3` (ordinales 2 y 4). Sirve para probar sin nadie al teclado. **Cuidado:** un guion a ciegas en el menú llegó a "Start new game" (corrida 17). En el menú hay que verificar cada paso con una captura.

## Preguntas abiertas

1. ✅ Raíces de zona (corrida 19): `PlayerTriggerBox` (`2f3efad3`) `on_entered`, y `Box.event` → `TriggerVolumeFilter` (`f86133d4`) `on_event_entered`. Los `PlayerTriggerBox` no disparan nada en la carga, así que para inyectar en el receptor hace falta un índice pid → entidad armado en la carga.
2. Bloqueo en el cliente: no llamar al original para los (pid, pin) que decide el host. Hay que ver si los pines de salida alcanzan o si hace falta bloquear también los de entrada.
3. Los mensajes reinyectados reusan el `msg` del primer disparo visto. Habría que ver si algún destino lee datos del mensaje (instigador, valor) que deberían venir del host.
4. Verificar que el `pid` sea igual entre papito y la laptop.
