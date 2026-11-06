# 16 - Árbol de composites de CATHODE: índice pid → entidad (fase 6, paso 4)

- **Pregunta:** ¿Cómo encontrar en el receptor la entidad de un `pid` (hallazgo 15) que nunca disparó un pin? Hace falta para inyectar los disparadores de zona del host, porque los `PlayerTriggerBox` no disparan nada en la carga.
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | `CATHODE::CompositeInstance::vftable` (primaria) | `0x013E1D2C` | `0xFE1D2C` |
  | idem, la de `EntityInterface` (en `+0x1C`) | `0x013E196C` | `0xFE196C` |
  | Constructor de `CompositeInstance` | `0x00965720` | `0x565720` |
  | Buscar hijo por ShortGuid (lineal) | `0x00943BF0` | `0x543BF0` |
  | Comparador: `handle->obj->+0x0C == id` | `0x009616F0` | `0x5616F0` |
  | Uso de la búsqueda (`vt[7]` del composite) | `0x00964D80` | `0x564D80` |
  | Caché de tablas de links (`CATHODE:update_cache`) | `0x008BD720` | `0x4BD720` |
- **Evidencia:** `work/evidence/h16/c1.txt` … `c9.txt` (Ghidra, `Dump.java`) y `a_964d80.md` (asm de REA).
- **Confianza:** alta en el layout (estático); el recorrido completo no está verificado en runtime.
- **Verificado en runtime:** no. Tests con wine (`test_pins`) sobre un árbol falso con el layout de abajo.

## El caché global no sirve

`DAT_0174dfb0` (`{+0 arreglo, +4 capacidad, +8 cantidad}`) no es un registro de entidades. Es un caché de tablas de links:
- `FUN_008bd870` llama a `FUN_008bd720` ("CATHODE:update_cache") cuando el índice de la entidad (`this+8`, 24 bits) no está en la tabla;
- las entradas se vacían en `FUN_008bd3d0` (desalojo) y en los destructores (`FUN_008bd580`, `FUN_008bd620`, `FUN_008bd230`).

Solo entra quien resolvió sus links. Por eso el índice no coincidía entre cargas (corrida 18).

## CompositeInstance

- Constructor `FUN_00965720`: vtable primaria en `+0`, la de `EntityInterface` en `+0x1C` (`param_1[7]`), `+0x2C` = 0, `+0x30` = arg, `+0x38` = 0.
- **`+0x2C` = refptr al arreglo de hijos** `{+8 cantidad, +0xC handles}`:
  - `FUN_009645d0` y `FUN_00964fb0` recorren `i < arr+8` y llaman a un método de `handle->obj` (`+0x2C` y `+0x28`);
  - `FUN_00964d80` busca un hijo con `FUN_00943bf0(ECX = this+0x2C, &id, FUN_009616f0)` (asm `0x964f20`: `PUSH 0x9616f0; LEA ECX,[EDI+0x2c]; PUSH EBX; CALL 0x943bf0`). Es una búsqueda lineal que compara `*(*(handle+0xC)+0xC)` (el ShortGuid del hijo) con el id.
- Cada hijo es un handle (`+4` refcount, `+0xC` objeto completo). El objeto completo tiene en `+0x08` el handle al composite padre (hallazgo 15), así que se puede subir y bajar por el árbol.

## Índice (en `mod/src/pins.c`)

- **Raíces:** se sube por `+0x08` desde las entidades que dispararon algún pin (en la carga hay miles: `on_restored`).
- **Recorrido:** DFS por `+0x2C`. Un hijo es composite si su vtable en `+0` es `0x013E1D2C`.
- **pid:** el mismo que `path_id`, calculado con la pila de ancestros (hash del id del nodo y de cada ancestro hasta la raíz).
- **El this del disparo:** el subobjeto `EntityInterface` es el primer puntero del objeto completo (desde `+4`) a una vtable del módulo con `FUN_008be0d0` en `+0x394`. Se guarda en un caché por vtable.
  - En los disparos vistos, `outer − this` es −0x14 … −0x20, que coincide con esto.
- **Verificación en cada armado:** para cada (entidad, pin) visto, el índice tiene que dar el mismo objeto y el mismo subobjeto. Se loguea `ok / con otro subobjeto / faltan`, y hasta 8 faltantes con la vtable del padre.

## Mensaje

- `FUN_00561c50` (despachador del `PlayerTriggerBox`): mira el instigador del mensaje (personaje con controlador `+0x31C`, flag `+0x48`). Después, según `msg+0x20`:
  - `& 0x1000000` → `on_entered` (`FUN_008c5d50`);
  - `& 0x2000000` → `on_exited` (`FUN_008c5d80`).
  En los dos casos **reenvía el mismo mensaje**.
- Corrida 19, flags de los mensajes:
  - `Box.event` y `TriggerVolumeFilter`: `11000000` (entró) y `12000000` (salió);
  - otros: `08000000` (el más común), `04000000`, `40000000`, `10100003`…

  Los bits `0x0F000000` parecen la clase o subclase del evento (hipótesis).
- No se sabe qué hay en `+0x00…+0x1C` (¿instigador?). Con `AIMP_TRACE_PINS`, el mod ahora vuelca el primer mensaje natural de cada (tipo, pin) para averiguarlo.

## Preguntas abiertas

1. ¿El árbol de composites está completo después de la carga, o hay composites que se instancian después (streaming)? El armado se repite si un pid falta (como mucho cada 2 s).
2. ¿Hay entidades fuera del árbol, con otras raíces que nunca disparan? El log de verificación lo va a mostrar.
3. Layout del mensaje (instigador). ¿Los destinos de `on_event_entered` leen algo además de los flags?
4. Costo del armado bajo Proton: `ReadProcessMemory` sobre el propio proceso pasa por el wineserver. Se loguea en ms.
