# 10 - NETZ: capa de red del engine (dormida en retail)

- **Pregunta:** ¿Tiene Cathode una capa de red propia? ¿Está activa en esta build? ¿Sirve para el coop?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | `Root::Init` (crea todos los subsistemas de `g_root`) | `0x007DA540` | `0x3DA540` |
  | Constructor NETZ (`g_root+0x04`) | `0x007E1CB0` | `0x3E1CB0` |
  | vtable NETZ | `0x012F74FC` | `0xEF74FC` |
  | `NETZ::Dispatch` (tarea `NETZ_DISPATCH`) | `0x007DDE20` | `0x3DDE20` |
  | Lista global de objetos NETZ (cabeza) | `0x016AE244` (dato) | `0x12AE244` |
  | Tabla de nombres de tipos de mensaje de red | `0x016413B0`–`0x01641414` (dato) | `0x12413B0` |
  | Gestor de personajes/jugadores (`g_root+0x08`), factory | `0x007DA2D0` | `0x3DA2D0` |
  | Constructor del gestor (0x9FF0 bytes; recibe NETZ) | `0x007D9260` | `0x3D9260` |
- **Evidencia:** `./work/evidence/h10/n1.txt` … `n8.txt` (salidas de `Dump.java`) y `work/evidence/strings/ascii_n5.txt`.
- **Confianza:**
  - Alta: que la capa existe, su constructor, el dispatch y el campo de transporte.
  - Media-alta: que en retail nunca se le asigna transporte (solo se encontró la escritura a 0 del constructor).
  - Baja: que se pueda reactivar. No se investigó qué código produce los mensajes ni si está condicionado a un modo multijugador.
- **Cómo verificar en runtime:** con el mod, leer `[[g_root+0x04]+0x69C]` (transporte) y la máscara de canales sucios `+0x10/+0x14` durante el juego. Si `+0x69C` es siempre 0 y la máscara cambia, el engine está produciendo mensajes que nadie envía.
- **Verificado en runtime:** **sí** (Corridas del 2026-11-05 con el mod de la fase 1+2 (Proton 9.0-203); logs en `work/evidence/runtime/run1-menu.log` y `run2-juego.log`, resumen en el hallazgo 11). Transporte NETZ `+0x69C` = 0 y proveedor de la sesión = 0 siempre. La máscara de canales pasa a `000000ff:00000001` con personajes en el nivel. Global de la cola A == `g_root+0x50`. Catálogo real: **44 endpoints en la cola A y 47 en la sesión**. Los mensajes se encolan y se vacían dentro del mismo frame (el tamaño leído después de `Run` es siempre 0). Flujo observado: RPC en la cola A → handler local → mensaje equivalente en la cola B (sesión) que se descarta. Detalle en el hallazgo 11.

## Evidencia

1. **Tareas por frame** (`TASKS.TXT` + `FUN_007d6370`, hallazgo 04):
   - `NETZ_DISPATCH` (worker) → `[g_root+0x04]->Dispatch()`.
   - `NET_AI_UPDATE` (worker) → `FUN_007dfd70()`, que también llama al dispatch, + `[0x17073FC].FUN_007dd970()`.
2. **Objeto NETZ** (`FUN_007e1cb0`, llamado desde `Root::Init` y guardado en `g_root+0x04`):

   | Offset | Contenido |
   |--------|-----------|
   | `+0x00` | vtable `0x012F74FC` |
   | `+0x04` | siguiente en la lista global (`DAT_016ae244`) |
   | `+0x08`…`+0x14` | flags y **máscara de canales "sucios"** (2×32 bits, la recorre el dispatch) |
   | `+0x18` | **32 canales de 0x30 bytes** (buffers; constructor/destructor `FUN_00a2ac10`/`FUN_00a2aa90`) |
   | `+0x618` | ids por canal (los usa el dispatch) |
   | `+0x698` | 0 |
   | **`+0x69C`** | **transporte** (interfaz con vtable): el dispatch llama `transporte->vf[1](id_canal, datos, tamaño, flag)` por cada canal sucio. **Si es NULL no se envía nada** |
3. **El transporte nunca se asigna.** Búsqueda de `MOV [reg+0x69C], …` en todo `.text`: hay 3 escrituras. Una es el constructor de NETZ, que pone 0. Las otras dos están en `0xF30944`/`0xF35B36`, otra zona (LibOVR, hipótesis) y otra estructura. No hay setter de transporte para esta clase.
4. **Protocolo de mensajes del juego** (tabla de nombres en `.data`, `0x016413B0`):

   | Tema | Mensajes |
   |------|----------|
   | Otros | `UNUSED`, `send_debug_data`, `dunno` |
   | Disparos | `shoot_character`, `shoot`, `shoot_rigid` |
   | Posicionamiento | `discontinuous_positioning` |
   | Personajes | `character_send_message_to_logic`, `character_register_network_character_data`, `character_register_network_action_data`, `character_register_network_secondary_action_data`, `character_get_next_network_action_continuous_buffer`, `get_next_network_secondary_action_continuous_buffer`, `character_register_extrapolated_network_position`, `character_register_accurate_position` |
   | Entidades y física | `client_entity_manager_receive_trigger`, `client_entity_manager_receive_message`, `client_entity_manager_receive_message_card32`, `client_entity_manager_block`, `client_entity_manager_flush_all_messages`, `physics_object_update_networked_object`, `start_ragdoll_death` |
   | **Lógico/display** | `request_logic_character`, `request_display_character` |
   | Liberación | `release_npc`, `release_player` |

   Encaja con un modelo **host-autoritativo**: la lógica de cada personaje corre en un nodo ("logic character") y los demás muestran un "display character" con posiciones extrapoladas. Hay constantes relacionadas, como `CLIENT_PLAYERS_LOGIC_CHARACTER_RUNNING_ON_THE_HOST`, `LOCAL_WITH_NETWORK` y `network_id`.
5. **El gestor de jugadores es "consciente" de la red:** `g_root+0x08` (array de jugadores del hallazgo 03) se construye con `FUN_007d9260(root+0x4C, NETZ, root, …)`.
6. **Nodos de script de multiplayer** (hallazgo 01): `IsMultiplayerMode`, `HostOnlyTrigger`, `SyncOnAllPlayers`, `NetPlayerCounter`, `NetworkedTimer`, `RespawnConfig`, etc.

**Descartado como multiplayer:** `"Server created and will poll for new client(s) on port"`, `"A new network client has been received"`, `"Client deleted."` (`FUN_00c1d310`/`c1df10`/`c1da60`). Son del **Havok Visual Debugger** (`hkVisualDebugger.cpp`, strings vecinas), una herramienta de debug.

## Implicaciones para el diseño

- La red del mod (ENet sobre UDP, prompt §6) sigue siendo necesaria: NETZ no trae transporte.
- **Oportunidad (hipótesis, no verificada):** si el código que produce y consume mensajes NETZ sigue compilado y se ejecuta cuando el juego "cree" que está en modo red, el mod podría **implementar la interfaz de transporte** (`+0x69C`, método 1 = enviar) con ENet y reutilizar la replicación del engine (personajes lógicos/display, triggers de entidades, física, disparos). Para la fase 3 (coop real) eso valdría mucho más que replicar todo a mano.
- **Riesgos:**
  - el código de producción/consumo puede estar desactivado (stubs `return 0`, como las opciones dev del hallazgo 02);
  - puede estar condicionado a un flag de "modo multijugador" que retail nunca activa;
  - el protocolo puede estar a medio terminar.
- Mientras tanto, el plan por fases del prompt (ghost → mundo compartido → coop) sigue igual. NETZ se evalúa antes de la fase 5.

## Preguntas abiertas (siguiente investigación de NETZ)

1. ¿Qué código escribe en los canales NETZ (marca la máscara `+0x10/+0x14` y llena los buffers)? ¿Corre en retail?
2. ¿Dónde se consumen los mensajes entrantes (switch por tipo)? ¿Qué método de la vtable `0x012F74FC` recibe?
3. ¿Qué flag representa "modo multijugador" (lo que lee el nodo `IsMultiplayerMode`)? ¿Quién lo escribe?
4. ¿Qué es `[0x17073FC]` (segunda mitad de `NET_AI_UPDATE`) y `g_root+0x4C` (`FUN_007e10d0`)?
5. ¿Cómo se representa un segundo jugador en el gestor `g_root+0x08` (`+0x80`/`+0x84`)? (hallazgo 05)

---

# Parte 2 - Canales por personaje y "autoridad" de red

- **Evidencia:** `./work/evidence/h10/n9.txt` … `n15.txt`; `./work/evidence/h05/e13.txt` (`SpawnCharacter` decompilado). Antes hubo que crear funciones en destinos de vtables con `work/scripts/ghidra/SeedVtables.java` (ver abajo).
- **Confianza:** alta para el flujo (decompilación directa). Media para los nombres.

## Cada personaje reserva un canal NETZ

`SpawnCharacter` (`0x7D3B50`, hallazgo 05):
1. Calcula `replicado`: si `request+0x4C`, lo saca de una tabla de `[root+0xAC0]` por id (`request+0x48`); si no, de `IsAuthority()`.
2. `canal = NETZ->AllocateChannel(replicado)` (`FUN_007ddc60`):
   - toma un slot libre de un bitmap (`+0x08`, `+0x0C…`; 32 canales);
   - le asigna un id incremental (`+0x618[slot] = +0x698++`);
   - si `replicado`, lo marca en la máscara que recorre el dispatch (`+0x10/+0x14`);
   - inicializa el buffer (`FUN_00a2abd0`, `FUN_00a2a900`) y devuelve `NETZ + 0x18 + slot·0x30`.
3. `pj = RequestCharacter(bloque, request)`.
4. `bloque+0x164 = [this+0x24]->vf[+0x10](canal, pj)`: **asocia el canal con el personaje**.
5. Si el personaje **no** es autoritativo (`bloque+0x4F == 0`), lo registra además en otra lista (`FUN_00789e70`).

## La "sesión de red" (`g_root+0x54`, global `DAT_01be1bd4`)

- Constructor `FUN_00a2a6b0`: pone **`+0x00` (proveedor de red) en NULL**, reserva un buffer de 0x4000 bytes y registra `DAT_01be1bd4 = this`. Es el único writer del global.
- `IsAuthority()` (`FUN_00a2a4f0`, también `FUN_00a2a4a0(this)`): si el proveedor es NULL devuelve **1**; si no, `proveedor->vf[1]()`.
- `IsClient()` (`FUN_00a2a4c0`): devuelve 1 solo si hay proveedor, flag `+0x2C` y `!proveedor->vf[1]()`. **En retail es siempre 0.**
- Otros accesores: `+0x28` (`FUN_00a2a490`), byte `+0x2D` (`FUN_00a2a480`). Lista global de objetos de red: `DAT_01be1bd0`, recorrida por `FUN_00a2a620`.

**Conclusión:** el engine distingue **autoridad local vs. cliente** y reserva canales de replicación por personaje. Sin proveedor (retail) todo es autoridad local y nada se transmite: NETZ no tiene transporte (parte 1).

## Palanca potencial (hipótesis de alto riesgo, no probar sin plan)

Un mod podría instalar un **proveedor** propio en `[DAT_01be1bd4]+0x00` y el flag `+0x2C`, y un **transporte** en `[g_root+0x04]+0x69C`. Así el engine trabajaría en su modo cliente/host original, con canales por personaje y personajes no autoritativos. No se sabe:
- qué interfaz completa esperan el proveedor y el transporte;
- si el código de serialización y recepción está completo en retail;
- qué otras partes del juego se rompen en modo cliente.

Requiere investigar la vtable esperada del proveedor (`vf[1]` = ¿es autoridad?) y experimentos en runtime controlados.

## Nota de herramientas: funciones que Ghidra no había creado

- Mucho código que solo se alcanza por vtables no estaba definido como función: `SpawnCharacter`, `NETZ::AllocateChannel` y la evaluación de `TargetIsPlayer`.
- `work/scripts/ghidra/SeedVtables.java` recorre `.rdata` buscando corridas de ≥3 punteros a `.text` y crea las funciones que faltan:
  - 3.985 corridas, 5.971 zonas desensambladas, **22.608 funciones nuevas**;
  - corre en 35 s y modifica el proyecto de análisis;
  - backup previo en `game/ghidra_backup/`.
- Los conteos de "funciones" de los hallazgos 01–09 son de antes de este paso (67.007).

## Parte 2 (cont.) - El spawn se replica como mensaje

- El spawner de script (`FUN_00490e30`, hallazgo 05) termina en `FUN_007a7a20`/`FUN_007a7a80`:
  ```c
  FUN_00787450(req, …);                  // inicializa el request de spawn (0x130 bytes)
  if (IsAuthority()) FUN_00797490(req);  // solo la autoridad spawnea
  ```
- `FUN_00797490` (thiscall sobre la interfaz de spawn del `CharacterManager`):
  ```c
  n = sesion->m_28;                       // FUN_00a2a490 (≥1 para que haya spawns; ¿cantidad de pares/jugadores?)
  if (n >= 1 && (!IsClient() || n < 1)) {
      FUN_00789f30(req);
      this->vf[1](req);                   // SpawnCharacter(req)  (0x7D3B50)
      FUN_00789f60(req);
      memcpy(copia, req, 0x130);
      if (!sesion->flag_2D)               // FUN_00a2a480
          FUN_00a2a290(DAT_016361e4, copia, 0x130);   // ← serializa el spawn hacia los pares
      else if (DAT_016361f4 && PTR_FUN_016361f0)
          PTR_FUN_016361f0(DAT_016361f4, copia);      // callback local alternativo
  }
  ```
- **Interpretación (hipótesis):** el modelo original es host-autoritativo. El host spawnea y difunde el request de 0x130 bytes; un cliente (`IsClient()`) no spawnea por su cuenta. En retail no hay proveedor, así que todo es host local, y queda por ver si `FUN_00a2a290` escribe igual en un buffer (el de 0x4000 bytes de la sesión) que nadie envía.
- Para el coop: si el request de 0x130 bytes es autosuficiente, **el host del mod podría capturarlo** (hook en `FUN_00a2a290` o en `FUN_00797490`) y reenviarlo al cliente para reproducir los spawns de NPCs/Alien. Así ambos mundos tendrían los mismos personajes.

---

# Parte 3 - Colas de mensajes: el engine serializa eventos replicables (sin red)

- **Evidencia:** `./work/evidence/h10/n16.txt` … `n18.txt`.
- **Confianza:**
  - alta: la forma de las colas y el patrón de los productores;
  - media: que los productores corran en retail; sale de las condiciones que se leyeron, falta verificarlo en runtime;
  - baja: el consumo/flush, que no se encontró todavía.

## Dos colas de salida

| Cola | API (thiscall sobre la sesión/cola) | Escritor real | Productores |
|------|--------------------------------------|---------------|-------------|
| A | `FUN_00a2a290(u16 tipo, void* datos, size_t n)` | `FUN_00a2a160` | **31** |
| B | `FUN_00a2a810(…)` | `FUN_00a2a760` | **más de 40** (Ghidra corta la lista en 40) |

`FUN_00a2a160` toma una sección crítica (`this+0x14`), agranda un buffer (`this+0x30` datos, `this+0x34` tamaño) y **agrega `[u16 tipo][payload]`. No mira si hay proveedor de red.**

Productores de la cola A por zona:

| Zona | Funciones |
|------|-----------|
| scripts | `0x491C00`, `0x491CB0`, `0x494B30`, `0x494BD0` |
| entidades/física | `0x654050`–`0x654430` (7) |
| IA | `0x6D9060`, `0x6D90F0` |
| `CharacterManager` | `0x795780`–`0x7960B0` (10), `0x797490` (spawn, request de 0x130 bytes), `0x797790` |
| NETZ | `0x7E1210`–`0x7E18E0` (7) |

Los de la cola B están sobre todo en `0x48A250`–`0x48BF90` (~30, zona de scripts/entidades), `0x414AF0`…, `0x418A10`… y `0x7E14B0`/`0x7E1540`.

## Patrón de cada productor (RPC "ejecutar local + difundir")

```c
// FUN_00491c00 (script), igual que FUN_00797490 (spawn)
if (!IsClient()) {                       // en retail siempre: no hay proveedor
    pre(datos);
    this->vf[1](datos);                  // ejecuta la acción localmente
    post(datos);
    copia = datos (n bytes);
    if (!sesion->flag_2D)                // FUN_00a2a480
        FUN_00a2a290(TIPO_GLOBAL, &copia, n);   // DAT_01637d24 (0x18 bytes), DAT_016361e4 (0x130 bytes), …
    else if (callback_global) callback_global(ctx, &copia);
}
```

Los tipos de mensaje son globales (`DAT_01637d24`, `DAT_016361e4`, …), probablemente registrados al inicio a partir de los nombres del enum de la parte 1.

## Por qué importa

- Si en retail los productores corren y encolan (todo indica que sí cuando `IsClient()` es 0 y `flag_2D` es 0), **el engine ya está generando, frame a frame, un flujo de eventos replicables**: spawns, acciones de scripts/entidades, física, IA, personajes. Un hook en `FUN_00a2a160`/`FUN_00a2a760` (o en el flush) le daría al host del mod ese flujo sin tener que identificar cada sistema a mano.
- Del lado del cliente, re-inyectar esos mensajes requiere el **consumidor** (la ruta de recepción, `client_entity_manager_receive_*`, etc.) y un modo "cliente" (`IsClient()`), es decir, el proveedor de la parte 2. Es más arriesgado y queda para después.

## Experimento de runtime prioritario (cuando exista el mod de la fase 1)

1. Contar por frame las llamadas a `FUN_00a2a160` y `FUN_00a2a760`, con su `tipo` y `n`.
2. Ver el tamaño de las colas (`+0x34`) y si alguien las vacía (y cuándo).
3. Correlacionar los tipos con acciones del juego: abrir una puerta, agarrar un item, aparición de NPCs/Alien.

## Preguntas abiertas

- ¿Quién vacía las colas (flush/descartar)? Candidatos: `FUN_00a2a620` (recorre `DAT_01be1bd0`) y el dispatch de NETZ.
- Mapa tipo → nombre: los globales `DAT_016*`/`DAT_0163*` contra el enum de la parte 1.
- ¿Qué es `flag_2D` y cuándo se usa el callback global (`PTR_FUN_016361f0`/`PTR_FUN_01637d30`) en lugar de la cola?

---

# Parte 4 - Flush por frame y despacho en *loopback*

- **Evidencia:** `./work/evidence/h10/n19.txt` … `n21.txt`.
- **Confianza:** alta para la mecánica (decompilación directa). La semántica de cada handler no se investigó.

## Objetos

- La **cola A** es `g_root+0x50` (objeto de 0x40 bytes; constructor `FUN_00a2a340`, el único que escribe `+0x34`):

  | Campo | Contenido |
  |-------|-----------|
  | `+0x00` | **sumidero** (interfaz; NULL en retail) |
  | `+0x08` | **tabla de handlers por tipo** (`{fn, ctx}`, indexada por el `u16` del mensaje) |
  | `+0x14` | sección crítica |
  | `+0x2C`… | buffer (`+0x30` datos, `+0x34` tamaño) |
  | `+0x3C` | byte "despachando" |

- La sesión de red de la parte 2 es `g_root+0x54` (otro objeto).

## Flush (`FUN_00a2a3f0`, lo llama `FUN_007ddae0` una vez por frame)

```c
if (cola->tamaño != 0) {
    if (cola->sumidero == NULL)  Dispatch(&buffer);            // retail
    else if (cola->sumidero->vf[1](&buffer)) Dispatch(&buffer); // con red: primero lo entrega el sumidero
    cola->tamaño = 0;
}
```

`FUN_007ddae0` es parte del arranque del frame: llama al timer (`FUN_00773050`, `FUN_00773280`) y a `QueryPerformanceCounter` (hallazgo 04).

## Despacho (`FUN_00a2a220`)

```c
cola->despachando = 1;
for (off = 0; off < n; ) {
    tipo = *(u16*)(buf + off); off += 2;
    h = cola->handlers[tipo];                 // {fn, ctx}
    off += h.fn(h.ctx, buf + off);            // el handler devuelve cuántos bytes consumió
}
cola->despachando = 0;
```

## Lectura del diseño

1. Un sistema produce una acción y la encola: `[u16 tipo][payload]` (parte 3).
2. Al inicio del frame siguiente, el flush entrega el buffer al **sumidero** (en el diseño original, la red) y lo **despacha a los handlers registrados**.
3. En retail no hay sumidero, así que el despacho es local (*loopback*). **El camino de "recepción" se ejecuta en cada partida.**

## Plan de coop que esto habilita (hipótesis a verificar)

- **Host:** hook en el flush (`FUN_00a2a3f0`) o un sumidero propio en `g_root+0x50 → +0x00`, que es la interfaz prevista por el engine (`vf[1](&buffer) → bool`). Se copia el buffer de cada frame y se manda al cliente.
- **Cliente:** inyectar los buffers recibidos en su cola (con `FUN_00a2a160` o escribiendo el buffer antes del flush) para que sus handlers apliquen los eventos del host.
- **Problemas a resolver:**
  - **duplicados**: el cliente también produce sus propios eventos locales; habría que filtrarlos por tipo o, en el cliente, entrar en modo `IsClient()`;
  - **identidad de entidades**: los payloads seguramente referencian entidades por id o handle, que tienen que coincidir entre máquinas (mismo nivel, mismo orden de spawn, mismos datos);
  - **determinismo y orden**.
- Primer experimento (sin red): loguear por frame los tipos y tamaños que pasan por `FUN_00a2a220`, y qué handler (`fn`) atiende cada tipo.

---

# Parte 5 - Registro de endpoints RPC de la **sesión** (corregido)

> **Corrección (misma sesión, verificado en asm):** `RegisterAllRPCs` llena la tabla de handlers de la **sesión** (`g_root+0x54`, `+0x08`/`+0x0C`), no la de la cola A. La cola A (`g_root+0x50`) tiene **su propia** tabla en `cola+0x08`: en `FUN_00a2a3f0` el despachador `FUN_00a2a220` se llama con `ECX = cola` (`0xA2A448: mov ecx,edi`). `FUN_007ddae0` hace el flush de `[g_root+0x50]` (`0x7DDAE8`) y después actualiza la sesión `[g_root+0x54]` con `FUN_00a2a5c0` (`0x7DDAF8`). No se sabe todavía quién llena la tabla de la cola A, que es donde viven los tipos de los 70+ productores, como el del spawn `DAT_016361e4`.

- **Evidencia:** `./work/evidence/h10/n22.txt` … `n26.txt`.
- **Confianza:** alta para `RegisterAllRPCs`. Media para el layout del endpoint (se infiere de esa función y de los globales del spawn).

## `RegisterAllRPCs()` = `FUN_00a2a620`

Lo llaman `FUN_007ddc30` y `FUN_007e68c0`, en la inicialización de la zona NETZ.

```c
n = largo(lista DAT_01be1bd0);            // endpoints registrados estáticamente
redimensionar(sesion->handlers(+0x08), n); // sesion = *DAT_01be1bd4 (g_root+0x54)
id = 0;
for (ep = DAT_01be1bd0; ep; ep = ep->next) {
    ep->tipo(+0x0C, u16) = id;
    sesion->handlers[id] = &ep->fn(+0x04);   // par {fn, ctx}
    id++;
}
```

- **Layout de un endpoint RPC:** `+0x00` next, `+0x04` fn (handler de recepción), `+0x08` ctx, `+0x0C` tipo (u16).
  - Ejemplo (spawn): endpoint estático en `0x016361D8`. `ctx` = `0x016361E0`, que lo escribe el constructor del `CharacterManager`. `tipo` = `0x016361E4`, que es lo que `FUN_00797490` pasa a la cola. `0x016361F0`/`0x016361F4` son el callback/ctx alternativo.
  - Los endpoints son objetos estáticos con vtable propia (`FUN_007795c0`/`FUN_007879d0` son constructor/destructor de uno). El constructor del `CharacterManager` inicializa varios (vtables `0x012F3410`, `0x012F3424`, `0x012F3438`, `0x012F344C`, `0x012F3460`, …).
- Los ids de los endpoints de la **sesión** salen del orden de la lista estática, así que son iguales en dos máquinas con el mismo `AI.exe`. Para los tipos de la **cola A** hay que verificar en runtime que también coincidan, porque dependen de un registro todavía no ubicado.

## Catálogo pendiente

Para mapear id → tipo → handler en runtime: recorrer `DAT_01be1bd0` y loguear `(id, fn, ctx)`. Después correlacionar con el enum de nombres de la parte 1 y con las acciones del juego.

### Endpoints registrados estáticamente (parte 5, cont.)

- Escrituras a la cabeza de la lista `DAT_01be1bd0`: **8 inicializadores estáticos** (`FUN_01140c60`, `…ca0`, `…ce0`, `…d20`, `…d60`, `…da0`, `…de0`, `…e00`). Cada uno hace `ep->next = cabeza; cabeza = &ep`.
- Endpoints: `0x01636534`, `0x01636558`, `0x0163657C`, `0x016365A0`, `0x016365C4`, `0x016365E8`, `0x0163660C`, `0x0163661C` (casi todos separados 0x24).
- El tipo que usa el spawn (`DAT_016361e4`) no está entre estos 8. Es coherente con la corrección: pertenece a la tabla de la cola A, no a la de la sesión.
- Los valores iniciales de esos globales en `.data` son el stub `0x8D21E0`; se inicializan en runtime y su valor final no se puede leer estáticamente.

---

# Parte 6 - La cola B es el buffer de la sesión y sin proveedor se descarta

- **Evidencia:** `./work/evidence/mod/q1.txt` (encolado A/B y wrappers) y `q2.txt` (update de la sesión, escritores de los globales, ctor de la cola A). Bytes y relocations verificados en el archivo con un parser PE propio.
- **Confianza:** alta (decompilación y asm directos). No verificado en runtime.

## Los dos caminos de encolado

| | Wrapper (cdecl `tipo, datos, tamaño`) | Callers | Encolar (thiscall, `ret 0xC`) | Objeto | Buffer |
|---|---|---|---|---|---|
| A | `FUN_00a2a290` | 31 | `FUN_00a2a160` | `[0x1BE1BC8]` = cola A (lo escribe solo su ctor `FUN_00a2a340`, en `0xA2A3D7`) | `+0x30` datos, `+0x34` tamaño, capacidad `+0x38` = 0x4000; sección crítica `+0x14` (spin 0xFA0) |
| B | `FUN_00a2a810` | 41 | `FUN_00a2a760` | `[0x1BE1BD4]` = **la sesión** (`g_root+0x54`) | `+0x14` objeto buffer, `+0x18` datos, `+0x1C` tamaño; **sin lock** |

- Cada wrapper es el único caller de su función de encolado (refs = 1).
- Prólogo de los dos wrappers: `55 8B EC 8B 45 10 8B 4D 0C 8B 55 08 50 51`, sin relocations; después viene `MOV ECX,[global]`, relocado. `FUN_00a2a160` tiene una relocation en `+6` (el `PUSH imm32` del SEH): no se hookea ahí.

## Update de la sesión (`FUN_00a2a5c0`, fastcall `ECX = sesión`, llamado después del flush de la cola A)

```c
if (sesion->proveedor && !sesion->proveedor->vf[1]())   // ¿conectado?
    { FUN_00a2a520(); return; }
if (sesion->tamaño_B != 0) {                              // +0x1C
    if (sesion->proveedor) {
        sesion->proveedor->vf[2](&sesion->buffer_B);      // +0x14: enviar
        sesion->proveedor->vf[4](sesion->m_24);
    }
    sesion->tamaño_B = 0;                                 // se vacía SIEMPRE
}
```

## Consecuencias

1. **Cola A**: se despacha localmente (*loopback*, parte 4) y además va al sumidero si lo hay.
2. **Cola B**: **solo se envía por la red**. En retail, sin proveedor, se descarta en cada frame sin despacharse. Es el stream de salida que el engine habría mandado a los otros jugadores, y los 41 productores lo siguen generando.
3. Para el coop (opciones B/C de `DESIGN_OPTIONS.md`): capturar la cola B del host da los mensajes que el diseño original pensaba como replicación. Del lado del cliente entrarían por el camino de recepción de la sesión (tabla de handlers `+0x08/+0x0C`, `RegisterAllRPCs`), que no se ubicó todavía.
4. Experimento (mod, `AIMP_TRACE_QUEUES=1`): hookear los dos wrappers y registrar tipo, tamaño, thread y call site de cada mensaje. La sonda por defecto ya loguea el tamaño de las dos colas después de cada frame.

## Preguntas abiertas

- ¿Qué hace `FUN_00a2a520` (cuando el proveedor dice "no conectado")?
- ¿Quién consume los mensajes recibidos de la sesión (el lado de recepción)?
- ¿Las dos colas tienen tipos en el mismo espacio de ids? La sesión usa los ids de `RegisterAllRPCs` (8 endpoints estáticos); la cola A, su propia tabla.

---

# Parte 7 - Quién llena la tabla de handlers de la cola A

- **Evidencia:** `./work/evidence/mod/q3.txt` (decompilación de `FUN_00a2a2b0` y `FUN_00a2a220`, xrefs de `DAT_01be1bcc`) y `q4.txt` (asm de los inicializadores).
- **Confianza:** alta para el mecanismo. La cantidad de endpoints no se puede contar estáticamente (ver abajo).

- **`FUN_00a2a2b0` es el `RegisterAll` de la cola A.** Cuenta la lista estática `DAT_01be1bcc`, redimensiona la tabla `[colaA+0x08]` (cantidad en `+0x0C`) y, en el orden de la lista, asigna `tipo = 0, 1, 2…` (u16 en `endpoint+0x0C`) y guarda `&endpoint->fn` en la tabla. Es el mismo layout de endpoint que el de la sesión (parte 5): `{next, fn, ctx, u16 tipo}`.
- **La tabla de la cola A es de punteros** a pares `{fn, ctx}`. El despachador (`FUN_00a2a220`) hace `h = tabla[tipo]; consumidos = h->fn(h->ctx, payload)`; si el mensaje no tiene payload, pasa `NULL`.
- Las listas las arman **inicializadores estáticos** (zona `0x1140840`–`0x1140F00`). Cada uno ejecuta `endpoint.next = lista; lista = &endpoint`. Ejemplos: cola A ← `0x1636190`, `0x1636520`, `0x16366F4`; sesión ← `0x1636534`. Los endpoints son globales de `.data`.
- Ghidra no ve todos los inicializadores: el del spawn (endpoint `0x16361D8`, tipo en `0x16361E4`) no tiene xrefs. Solo los referencia la tabla de inicialización del CRT, igual que pasó con `SpawnCharacter` (hallazgo 05). Por eso **el catálogo se lee en runtime**: el mod recorre las dos listas cuando la tabla de la cola A ya tiene handlers (líneas `RPC` del log). Eso cubre el experimento 3.5.
- Los ids dependen del orden de inicialización del CRT, que es fijo para un mismo binario: **coinciden entre máquinas con el mismo `AI.exe`** (lo mismo que la parte 5 para la sesión).
