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
- **Verificado en runtime:** no.

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
