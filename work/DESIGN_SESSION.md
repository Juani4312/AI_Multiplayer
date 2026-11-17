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

**Decisión del usuario (2026-11-17): se comparte todo.** "Cuando uno agarra balas, un arma, key items, lo que sea, lo recibimos los dos, y cuando algo se consume se le consume a los dos." Las armas (lanzallamas incluido) las tienen los dos. Los casos del punto 4 quedan resueltos así:
- armas, llaves e ítems de misión: los tienen los dos;
- munición, botiquines, componentes y arrojables: un solo contador para los dos;
- lo único a definir es la munición cargada en el arma de cada uno: se propone compartir la reserva y que cada uno tenga su cargador.

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

### Armas de a uno (idea del usuario, 2026-11-17)

"Que cada arma la pueda tener uno solo a la vez: un solo lanzallamas, accesible por los dos pero no al mismo tiempo."

**Opciones:**
- **a) La tiene el que la saca (recomendada):** las armas están en el inventario de los dos. Cuando uno la equipa, en la máquina del otro se le saca del inventario (o se la desequipa si la tenía en la mano) y no la puede elegir en el menú radial. Cuando el primero cambia de arma, vuelve a estar disponible para los dos.
  - Hace falta saber qué arma tiene equipada cada uno (el equipo del personaje está en `Character+0x1100`, hallazgo 19) y mandarlo por red.
  - Si los dos la sacan en el mismo instante, decide el host.
- **b) Pasársela:** el arma está en un solo inventario y el dueño se la da al otro con una acción explícita, cerca, o con un "usar" sobre el avatar. Es más realista, pero lleva más trabajo de interfaz.

**En los dos casos:**
- La munición de reserva sigue siendo compartida. El cargador viaja con el arma.
- Lo que no es arma (botiquines, bengalas, componentes, llaves) sigue siendo un solo contador para los dos.

**Decisión del usuario (2026-11-17): un híbrido de a) y b), con distancia.**

- Cada arma tiene un **dueño**: el último que la equipó. Al levantarla del mundo, el dueño es el que la agarró.
- **Cerca** (menos de D metros, mismo nivel): funciona como a). Cualquiera de los dos puede equiparla si el otro no la tiene en la mano, y al equiparla pasa a ser el dueño.
- **Lejos** (más de D): solo el dueño puede equiparla. Para el otro aparece no disponible en el menú de armas hasta que estén cerca y el dueño no la esté usando.
- **Confirmado por el usuario (2026-11-17):** D = 4 m, con histéresis (se vuelve "cerca" a menos de 4 m y "lejos" a más de 5 m, para que no parpadee en el límite).
- Casos borde (confirmados):
  - si el dueño está caído (DESIGN_DEATH) o se desconecta, el arma queda libre para el otro sin importar la distancia;
  - si los dos la equipan en el mismo instante, decide el host.
- Implementación: estado `{arma: dueño, en_mano}` en el host. Por red, `EQUIP`/`UNEQUIP` y la distancia; la del avatar ya se conoce en cada frame. Bloquear la selección en el menú radial o desequipar enseguida si el juego no deja bloquear.

**Implementación (2026-11-17, `mod/src/weapons.c`, `AIMP_SYNC_WEAPONS=1`, evento 8):**
- **Arma en mano:** `equipo = [Character+0x1100]`. Si `equipo+0x1510 != 0`, la instancia es `equipo[equipo+0x150C]`, la misma que el `slot+4` del inventario: de ahí sale el ShortGuid del arma.
- **Sacar:**
  - si el arma no está disponible (el dueño es el otro y está lejos o la tiene en la mano, sin estar caído ni desconectado), el mod la guarda con la función del juego `FUN_007c3980`;
  - si está disponible, el que la saca pasa a ser el dueño y avisa con `EQUIP`. Al guardarla avisa con `UNEQUIP`.
- **Al mismo tiempo:** gana el host.
- **Asentamiento:** 3 s al cargar (corrida 96: el juego tiene la escopeta en la mano ~0,1 s al cargar).
- **Verificado en una máquina (corridas 96–97):**
  - la lectura del arma en mano (la escopeta transitoria al cargar se mapeó bien);
  - los eventos del otro (sacó y guardó la pistola, con el dueño registrado);
  - que no salen eventos al cargar.
- **Sin verificar:** el bloqueo, porque con el mando virtual no se puede sacar un arma en `safezone`. Hace falta probarlo con alguien al mando.
