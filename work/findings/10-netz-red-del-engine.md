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
