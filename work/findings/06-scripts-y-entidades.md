# 06 - Sistema de scripts y entidades de Cathode (parte 1)

- **Pregunta:** ¿Cómo está organizado el sistema de entidades/scripts (puertas, terminales, triggers) y cómo se identifican las entidades en runtime? Lo necesitamos para sincronizar eventos (fase 5).
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Entity manager de Cathode (puntero global) | `0x0174EF40` (dato) | `0x134EF40` |
  | `EntityManager::Tick` (tarea `ENTITY_MANAGER_TICK`) | `0x00932040` | `0x532040` |
  | Callback de la tarea `ENTITY_MANAGER` (llama a `FUN_0092fd30` dentro de la sección crítica `0x174A7EC`) | `0x00783500` | `0x383500` |
  | Sección crítica del entity manager | `0x0174EF48` (dato) | |
  | Tabla de tipos de entidad de script (nombres + pines) | `.rdata`, offset de archivo `0xFD9510`–`0xFD9B40` | |
- **Evidencia:** `./work/evidence/h06/s1.txt` … `s5.txt`; `./work/evidence/strings/cathode_classes.txt` (958 clases RTTI `CATHODE::`); hallazgo 01 §7.
- **Confianza:** alta para el entity manager global y las vtables RTTI. Media para la interpretación de los estados del tick.
- **Cómo verificar en runtime:** recorrer las entidades del manager y contar vtables conocidas. En un nivel con puertas tienen que aparecer `CATHODE::Door`.
- **Verificado en runtime:** no.

## Entity manager

- `[0x0174EF40]` es el objeto central del mundo de Cathode. Lo usan la tarea `ENTITY_MANAGER_TICK`, `ZONE_MANAGER`, `SpawnCharacter` (hallazgo 05) y el callback de `HAVOK_POST_CHARACTER_UPDATE` (lee `+0xB8`).
- `EntityManager::Tick` (`0x932040`) trabaja dentro de una sección crítica:

  | Campo | Uso |
  |-------|-----|
  | `+0xA4` | estado (1–4; ¿carga/descarga de composites?) |
  | `+0xB8` | `== 1` cuando está activo |
  | `+0x80`/`+0x84` | lote pendiente; se procesa con `FUN_00930a50` y se vacía |
  | `+0xC0` | callback opcional que puede frenar el procesamiento |
  | `+0x88` | flags |

## Tipos de entidad: identificación por vtable

- Las 958 clases RTTI `CATHODE::*` son los tipos de entidad de script (más algunos sistemas). Cada clase tiene **una** vftable con RTTI, referenciada solo por su constructor.
- **El comportamiento no está en la vtable.** `Door`, `TriggerSimple` y `HostOnlyTrigger` difieren de una entidad-variable trivial exactamente en los mismos 4 slots (`+0x1C`, `+0x20`, `+0x38`, `+0x5C`). La lógica propia de cada tipo está dirigida por datos (tablas de pines y métodos por tipo, `COMMANDS.PAK`). Se investiga en la parte 2.
- **Sirve para identificar en runtime:** `*(void**)entidad == base + RVA_vftable` dice de qué tipo es. Vftables útiles (VA):

  | Tema | Tipo → vftable |
  |------|----------------|
  | Puertas | `Door` `0x013727DC`, `DoorStatus` `0x013B4944` |
  | Terminales | `AccessTerminal` `0x013912E4`, `TerminalContent` `0x01390B9C`, `TerminalFolder` `0x01390F3C`, `UI_Keypad` `0x01390094` |
  | Triggers | `TriggerSimple` `0x013466FC`, `TriggerTouch` `0x0136DE9C` |
  | Checkpoints y niveles | `Checkpoint` `0x01360F9C`, `CheckpointRestoredNotify` `0x013621BC`, `LevelLoaded` `0x013604A4`, `SwitchLevel` `0x01382D8C` |
  | Items | `PickupSpawner` `0x0138A57C`, `MultiplePickupSpawner` `0x0138A91C`, `AddToInventory` `0x0138813C`, `RemoveFromInventory` `0x013884DC`, `InventoryItem` `0x01389E3C` |
  | Multiplayer | `IsMultiplayerMode` `0x0136BDD4`, `HostOnlyTrigger` `0x013695F4`, `NetPlayerCounter` `0x01368EAC`, `NumConnectedPlayers` `0x0136A474`, `SyncOnAllPlayers` `0x013683CC`, `SyncOnFirstPlayer` `0x0136876C`, `AllPlayersReady` `0x0136802C`, `NetworkedTimer` `0x0136AF54` |
  | Jugador y cámara | `VariableThePlayer` `0x01340164`, `GetCurrentCameraPos` `0x013193DC` |

- `IsMultiplayerMode` sobrescribe además 4 métodos (`+0x08`, `+0x0C`, `+0x48`, `+0x4C`), pero son conversiones genéricas de **variable booleana** (get/set como bool o float). **El flag de "modo multijugador" no está en esa clase**: el valor de la variable le llega desde el engine o los datos. Sigue abierta la pregunta 3 del hallazgo 10.

## Registro de comandos de UI

Ver hallazgo 09 (`FUN_0084dbe0`): las entidades de UI usan nombres internados (`FUN_0046a550` copia el nombre a un string con refcount, no es un hash) asociados a IDs.

## Preguntas abiertas (parte 2)

1. ¿Dónde se resuelve el comportamiento por tipo (tablas de métodos/pines), y cómo se dispara un método de entidad, por ejemplo abrir una puerta?
2. ¿Cómo se enumeran las entidades vivas del manager (estructura de `[0x174EF40]`)?
3. ¿Qué escribe el valor de `IsMultiplayerMode`, `NumConnectedPlayers`, etc.?
4. Relación con los mensajes NETZ `client_entity_manager_receive_trigger`/`_message` (hallazgo 10): ¿el entity manager tiene una ruta de "trigger remoto"?
