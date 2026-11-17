# Sesión de coop: partida común e inventario compartido (diseño)

_2026-11-17. Preguntas del usuario: "¿qué pasa si cada uno tiene un save distinto?" y "¿se puede compartir el inventario?"._

## 1. Cómo funciona hoy el host y el join

- Cada máquina corre **su propio juego completo** y carga **su propio save**. El mod no transmite el mundo: manda por UDP (Tailscale) la posición, el yaw, la cámara y el nivel del jugador (20 por segundo) y eventos sueltos ("usar", arrojables, pines de los scripts, ítems).
- "Host" solo decide quién manda en los disparadores de los scripts (fase 6): el cliente bloquea los suyos y aplica los del host.
- La única verificación es el **número de nivel**:
  - el avatar, el "usar" y los pines se aplican solo si los dos están en el mismo nivel;
  - si no, se guardan para cuando estén.
- **No se compara la misión ni el progreso.** Con saves distintos en el mismo nivel, cada juego tiene otros NPCs, otras puertas abiertas y otro estado de los scripts. Los pines del host disparan cosas que en el cliente no corresponden. Con niveles distintos, el avatar no aparece y los eventos se encolan.

### Qué se rompe con saves distintos

- NPCs, Alien, puertas y objetivos distintos en cada máquina; la sincronización de pines empeora las cosas.
- Revivir al lado del compañero (DESIGN_DEATH) solo funciona en el mismo nivel y con el mismo estado; si no, el caído aparece en un mundo que no corresponde.

### Propuesta: la partida la define el host

1. **Al conectar, comparar:** el cliente manda un resumen de su save (nivel, misión, hash del archivo de save actual y de `PROGRESSION.ais`). Si no coincide con el del host, se avisa y no se sincroniza nada.
2. **Copiar el save del host:**
   - el host manda su save actual por la red (los slots son de 1 a 2 MB; hace falta un canal confiable con fragmentación y confirmación, que el UDP de hoy no tiene);
   - el cliente lo guarda como un slot propio (con backup, como `saves.sh`) y lo carga;
   - plan B: hacerlo a mano con `saves.sh` o una carpeta compartida.
3. **Guardar juntos:** cuando el host guarda en una terminal, el cliente guarda también (o recibe el save del host). Así un game over de los dos recarga el mismo punto en las dos máquinas.
4. **Ojo con Steam Cloud:** con una sola cuenta, la laptop offline no sincroniza, y por eso arrancó en otra misión. Con el amigo (otra cuenta) los saves son de cada uno: la copia del punto 2 es necesaria igual.

## 2. Inventario compartido

**Lo que hay:**
- Los ítems del mundo se procesan solo para el jugador 0 (hallazgo 07).
- `AIMP_SYNC_PICKUP` ya saca del mundo del otro el ítem que levantó uno (`FUN_00795ee0`, sin probar con dos máquinas).
- El inventario en sí (dónde vive y cómo se representa) **no está ubicado**. Hay entidades de script `AddToInventory`, `RemoveFromInventory`, `PlayerHasItem*` y `InventoryItem`.

**Propuesta: el inventario lo tiene el host y los cambios viajan como deltas:**
1. Ubicar el inventario del jugador y las funciones del juego para sumar y restar un ítem (las que usan `AddToInventory`/`RemoveFromInventory`).
2. Hookear los cambios locales: levantar, usar un botiquín, gastar munición, fabricar (consume componentes), tirar una bengala. Cada cambio se manda como un evento `item, +n/−n`.
3. El otro aplica el delta con las mismas funciones (sin volver a mandarlo). El host corrige si hay diferencias: cada tanto manda el inventario completo.
4. Casos a decidir:
   - las armas (¿los dos tienen el lanzallamas si uno lo encontró?);
   - las llaves y los ítems de misión (deberían ser compartidos);
   - la munición cargada en el arma (¿se comparte el cargador o solo la reserva?);
   - dos gastos al mismo tiempo del último ítem: gana el host y el otro recibe la corrección.

**Dificultad:** media.
- Lo más largo es ubicar el inventario y sus funciones: estático más el lector de memoria (`memsrv`), comparando la memoria antes y después de levantar algo.
- La red ya existe; el canal de eventos se repite 3 veces y descarta duplicados, y alcanza para deltas chicos.
- Para el estado completo conviene el canal confiable del punto 1.2, que sirve para las dos cosas.
