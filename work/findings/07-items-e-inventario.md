# 07 - Items, pickups e inventario (parte 1)

- **Pregunta:** ¿Cómo maneja el juego los pickups y el inventario? Lo necesitamos para sincronizar items entre jugadores (fase 5).
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Callback de la tarea `PICKUP_MANAGER_TICK` | `0x007CA0B0` | `0x3CA0B0` |
  | Update de pickups/interacción del jugador 0 (`dt` escalado) | `0x007C51E0` | `0x3C51E0` |
  | Gestor ligado a pickups: `g_root+0xA0` (lo crea `FUN_007b0bb0` en `Root::Init`) | — | — |
  | Casts de física de `pickup_manager.cpp` (línea 0x6F9) | `0x00786300` | `0x386300` |
  | `thrown_item_manager.cpp` (objetos arrojados) | `0x00792E00` | `0x392E00` |
- **Evidencia:** `./work/evidence/h07/i1.txt`, `i2.txt`; las vftables de entidades de inventario están en el hallazgo 06.
- **Confianza:** media (estructura general). El inventario en sí no se ubicó todavía.
- **Cómo verificar en runtime:** loguear `[g_root+0xA0]+0x55B8` y los flags `Character+0x293/0x298/0x29C` al acercarse a un item y agarrarlo.
- **Verificado en runtime:** no.

## Lo encontrado

- `PICKUP_MANAGER_TICK` (thread principal, dentro de la sección crítica del entity manager `0x174A7EC`) solo corre si el juego no está en pausa:
  1. `FUN_007c51e0(dt_escalado)`: el update de pickups/interacción. **Trabaja únicamente con el jugador 0**: `GetPlayer()` inline, `[[g_root+8]+0x80][0]`. Usa flags del personaje: `+0x29C` (se resetea si vale 1), `+0x298`, `+0x293`.
  2. Un temporizador en `[g_root+0xA0]`: `+0x55C4` acumula dt; al superar `+0x55C8` decrementa el contador `+0x55B8`.
- `pickup_manager.cpp` (`FUN_00786300`) arma tres casts de física con `FUN_007f9ad0` (tipos `DAT_01641374`, `0x15`, `0x1D`), probablemente para detectar items alcanzables.
- Las entidades de script de inventario son tipos de Cathode: `PickupSpawner`, `MultiplePickupSpawner`, `AddToInventory`, `RemoveFromInventory`, `InventoryItem`, `PlayerHasItem*` y `GetInventoryItemName`. La lógica (quién tiene qué) queda del lado de los datos de script.

## Implicaciones para el coop

- Como la interacción y los pickups se procesan **solo para el jugador 0**, el jugador remoto no puede "agarrar" items con la lógica del engine en su propia instancia de mundo compartido sin intervención. Hace falta un esquema host-autoritativo:
  1. El cliente detecta su pickup local (en su propia instancia, en la fase 5).
  2. Se lo informa al host.
  3. El host lo valida y difunde "item X tomado".
  4. Los dos lo quitan del mundo.
- Con NETZ (hallazgo 10) quizás habría mensajes para esto (`client_entity_manager_receive_trigger`), pero no está confirmado.

## Preguntas abiertas (parte 2)

1. ¿Dónde vive el inventario del jugador (¿en `Character`, en `g_root+0xA0` o en otro gestor?) y cómo se representa un item (id de tipo + cantidad)?
2. ¿Qué evento dispara "item tomado" (método de la entidad `PickupSpawner`)?
3. ¿Qué es exactamente `g_root+0xA0` (`FUN_007b0bb0`)?
