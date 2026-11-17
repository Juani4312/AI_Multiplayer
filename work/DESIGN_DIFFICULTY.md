# Dificultad del coop (diseño)

_2026-11-17. Planteo del usuario: "el selector de dificultad del juego no alcanza: siendo dos contra un Alien, incluso en Nightmare no es tan difícil (dos personas disparan, se revive en el save). La dificultad podría afectar los ítems que aparecen, la agresividad del Alien, la cantidad de aliens, los respawns de jugadores…"._

## Qué ya hace más fácil el coop (lo que hay que compensar)

- Dos armas y dos pares de ojos: uno distrae y el otro avanza.
- Revivir: el caído vuelve en la terminal de guardado o en un checkpoint, y el game over es solo si caen los dos.
- El Alien hoy persigue solo a un jugador por máquina (DESIGN_ALIEN): el otro está a salvo.
- A favor de la dificultad: con el inventario compartido hay **un solo stock para los dos**, así que los recursos ya están más ajustados que en solitario.

## Lo que trae el juego (estático, 2026-11-17)

`DATA/DIFFICULTYSETTINGS/{NOVICE,EASY,MEDIUM,HARD,IRON}.BML` (XML binario), por plantilla de NPC (`ALIEN`, `ANDROID`, `ANDROID_HEAVY`, `RIOT_GUARD`, `SECURITY_GUARD`, `MELEE_HUMAN`, `FACEHUGGER`…):
- daño: `damage_dealt_scalar`, `damage_received_scalar`, `time_between_shots_scalar`, `attack_pace_modifier(_max, _per_npc)`;
- sentidos: `visual_sense_activation_modifier`, `max_hearing_distance_modifier`, `flash_light_sense_activation_modifier`, `movement_sound_…`, `weapon_sound_…`, `visual_sense_exposure/stance_effect_…`;
- Alien y director:
  - medidor de amenaza: `max_menaces_modifier`, `menace_cool_down_time_modifier`, `menace_gauge_decrease_time_modifier`, `meance_deemed_time_modifier`;
  - cuánto se acerca al acechar: `near_target_exclusion_radius_*_stalk_*`, `near_objective_exclusion_radius_*`;
  - barridos: `backstage_area_sweep_*`, `increase/decrease_sweep_duration_modifier`;
  - conductos: `Vent_Attract_Time_Min/Max`.

`DATA/ALIENCONFIGS/*.BML` (`MILD`, `MODERATE`, `INTENSE`, `BACKSTAGEHOLD`…) tiene los valores base del Alien por situación: `menace_gauge_seconds_to_fill`, `max_menaces`, `ambush_timeout`, `killtrap_time`, `min/max_idle_time`, `role_timeout_*`.

Los nombres de los campos están en `AI.exe`: el cargador tiene una tabla nombre → campo, así que se pueden ubicar las estructuras cargadas y **escalarlas en memoria** sin tocar los archivos del juego.

## Propuesta: un selector "Dificultad del coop" (lo decide el host y se manda al cliente)

| Palanca | Cómo | Dificultad de implementar |
|---------|------|---------------------------|
| **Alien más agresivo y perceptivo** que Nightmare | multiplicar en memoria los parámetros de arriba (sentidos, menace gauge, distancia de acecho, tiempo en conductos) | media: ubicar la estructura por la tabla de nombres; después es escribir floats |
| **El Alien caza a los dos** (y prefiere al que está solo o hace ruido) | DESIGN_ALIEN: objetivo en `cerebro+0x10`; la dificultad regula cuánto cambia de objetivo y a quién prefiere | alta, pero hace falta igual para el coop |
| **Menos recursos** | el host decide al cargar un nivel qué % de pickups existen y los saca en las dos máquinas (`FUN_00795ee0`, ya usado por `AIMP_SYNC_PICKUP`); costo de fabricar; curación de los botiquines | media |
| **Más daño recibido** (androides, humanos) | `damage_received_scalar` del jugador, `damage_dealt_scalar` de los enemigos | baja, una vez ubicada la estructura |
| **Reglas de caída y revivir** (código propio, `death.c`) | ver abajo | baja a media |
| **Más de un Alien** | ver abajo | muy alta, a investigar |

### Reglas de caída y revivir (por nivel de dificultad)

- **Normal:** como hoy: revive en la terminal de guardado o en un checkpoint; si caen los dos, game over.
- **Difícil:** el caído se desangra: tiene X segundos para que el compañero lo reviva **en persona**. El compañero se acerca y usa un botiquín a menos de 2 m, y gasta un botiquín del inventario compartido: se detecta el consumo con `inv.c`. Si no llega a tiempo, game over para los dos.
- **Pesadilla coop:** sin revivir. El que cae queda afuera hasta el próximo checkpoint, o una muerte es game over para los dos.
- Opcional: cantidad máxima de revividas por misión.

### Más de un Alien

- El engine soporta más de uno en un nivel: el nido y el final del juego los tienen. Pero el director y los scripts de cada misión están pensados para uno.
- Crear personajes nuevos desde el mod hizo caer el juego (hallazgo 12). El camino sería usar los spawners del propio nivel o cargar la plantilla del Alien en un nivel que no la trae: es investigación larga y riesgosa.
- Alternativa más barata con un efecto parecido: un solo Alien que aparece más seguido y se queda más (parámetros del menace gauge y del backstage) y que caza a los dos.

### Otras ideas para la tensión del coop

- **Correa:** si los jugadores se separan mucho, el Alien prefiere al que está solo.
- **Ruido compartido:** el ruido de cualquiera de los dos atrae al Alien hacia ese jugador (sale solo cuando el Alien perciba a los dos).
- **Guardado exigente:** la terminal guarda solo si están los dos cerca, o hay un número limitado de guardados por misión.
- Un solo detector de movimiento para los dos (lo tiene uno).
- Combustible del lanzallamas y munición de reserva más escasos (ya son compartidos).

## Decisiones del usuario (2026-11-17, revisión)

- **Ruido compartido: sí, con histéresis.** Si el Alien está buscando o acechando, va hacia el que hace más ruido. Si ya persigue a uno y lo encontró, no cambia de objetivo por el ruido del otro (si no, "es fácil bailarlo"). Propuesta: queda trabado en su objetivo hasta perderlo de vista X segundos o matarlo. Antes de implementarlo, ver si el árbol del Alien ya tiene ese compromiso (en solitario no cambia de objetivo con cualquier ruido). Requiere que el Alien perciba a los dos: en el host, generar estímulos de ruido en el avatar cuando el otro corre o dispara.
- **Guardado exigente: sí.** Solo se guarda si todos los vivos están cerca de la terminal (los caídos o muertos no cuentan). La frecuencia de guardado no importa. **Escrito (2026-11-17, `sync.c`, `AIMP_COOP_SAVE_RULE=1`, test en `test_sync`):** guarda solo el host (el cliente no abre la terminal: su guardado lo pisaría `savesync`) y solo si el compañero vivo está a menos de `AIMP_COOP_SAVE_NEAR_CM` (600). Falta probarlo en el juego (instrucciones 8g) y el aviso en pantalla (el texto del prompt de la terminal no está en `UI.TXT`).
  - **Host caído (pedido del usuario, 2026-11-18):** si el host está caído, el cliente vivo tiene que poder guardar, y al guardar el host revive a su lado. Hoy el cliente queda bloqueado siempre: hay que permitirlo con el host caído (`death_is_downed` del lado del host, que el cliente ve como `death_remote_is_downed`). Como la partida canónica es la del host, ese guardado tiene que quedar también del lado del host:
    - **a) (recomendado)** el host guarda solo, apenas revive: queda junto al cliente y en el mismo mundo. Hace falta disparar un guardado desde el mod (máquina de estados del guardado `FUN_00a637c0`; NOTES, corrida 106);
    - b) la copia de saves al revés: el cliente le manda su save al host, que lo escribe y lo pone en su checkpoint en memoria (`savesync` en el otro sentido).
  - Cómo: el mod ya detecta el uso de la terminal (prefijo `1e38b898`, `sync.c`); si el compañero vivo está lejos, no deja pasar el uso (hook del productor `FUN_00795d00`) y cambia el prompt con `ui.c` ("Esperá a tu compañero").
  - Detalle: la partida canónica es la del host (`savesync`). Si guarda el cliente, el host tiene que guardar en el mismo momento; hace falta disparar un guardado desde el mod (máquina de estados del guardado: `FUN_00a637c0`, `obj+0xC`/`+0x2F4`; NOTES corrida 106).
- **Alien más perceptivo con multiplicadores: sí.**
- **La dificultad del mod es independiente de la del juego:** "Normal" del mod no toca nada (se usan los valores de la dificultad elegida en el juego); las otras multiplican esos valores. El mod guarda los valores base la primera vez que los ve y aplica sobre esa copia, así no se acumulan al recargar. A verificar: que la dificultad base esté en el save (así el cliente, que carga la partida del host, queda con la misma base).
- **Desangrado y revivir en persona: a futuro.** Idea: el caído queda forzado agachado y con el stick escalado (el mod ya intercepta el mando), con el efecto de vida baja del juego; un temporizador; el compañero lo revive gastando un botiquín a menos de 2 m (`inv.c` ve el consumo).
- **Varios aliens: a futuro** (complejo).

## Investigación (2026-11-17, estático y corridas 123–124)

- **Cargador:** `FUN_006f8b00`, método virtual (vtable `0x12F1300`), `ret 0xC`. Parsea un `DifficultySetting` y escribe en la estructura del tercer argumento. Recorrí su código (`push <nombre>` → `atof` → `fstp [reg+off]`) y salió el layout completo:
  - **bloque del Alien** (en la estructura): `+0x00` decrease_sweep_duration, `+0x04` increase_sweep_duration, `+0x08..+0x14` near_target_exclusion_radius (primer acecho mín/máx, siguientes mín/máx), `+0x18..+0x24` near_objective_exclusion_radius (ídem), `+0x28` menace_gauge_decrease_time, `+0x2C` menace_cool_down_time, `+0x30` meance_deemed_time, `+0x34` max_menaces, `+0x38` menace_gauge_seconds_to_fill, `+0x3C/+0x40` sweep_box, `+0x44/+0x48` Vent_Attract_Time min/máx, `+0x4C..+0x64` backstage_area_sweep (role_timeout, ambush_timeout, min/max_distance, min/max_idle_time, killtrap_time);
  - **por plantilla de NPC:** vector en `+0x68..+0x70` de punteros a bloques de 0x40, con el índice por nombre (`FUN_006d8d90` sobre `[g_root+0x98]`): `+0x00` max_hearing_distance, `+0x04/+0x08` weapon_sound (sola/combinada), `+0x0C/+0x10` movement_sound, `+0x14/+0x18` flash_light, `+0x1C/+0x20` visual, `+0x24` damage_dealt_scalar, `+0x28` damage_received_scalar, `+0x2C/+0x30` attack_pace (por NPC/máx), `+0x34` shooting_in_cover, `+0x38` time_between_shots, `+0x3C` suspicious_item_loop. Los valores por defecto son 0 para los modificadores y 1,0 para los factores.
- **Runtime (corrida 123):** el juego carga **las 5 dificultades al arrancar** (0,47 s, antes del menú), cada una en su estructura. El objeto que las maneja (el `this` del cargador) tiene en `+0x08` un arreglo de 5 punteros, en `+0x0C` la cantidad y en `+0x24..+0x2C` el valor 3 (¿índice activo?).
- **Los modificadores del Alien son sumas, no factores** (se suman a los valores base de `ALIENCONFIGS`):

| campo | arreglo[0] | [1] | [2] | [3] | [4] |
|-------|-----|-----|-----|-----|-----|
| radio de acecho mín/máx (m) | +10/+10 | +5/+5 | 0/0 | **−5/0** | +12/+15 |
| menace decrease / cool down (s) | +6 | +3 | 0 | **−3** | +9 |
| menace deemed time (s) | −10 | −5 | 0 | 0 | −15 |
| max_menaces | −2 | −1 | 0 | **+1** | −3 |
| seconds_to_fill | −1,5 | −0,75 | 0 | 0 | −1,5 |
| Vent_Attract min/máx (s) | +20/+60 | +10/+20 | 0/0 | **−10/−20** | +30/+80 |
| backstage role_timeout (s) | +40 | +20 | 0 | 0 | +60 |
| backstage min/max distance | 0/0 | 0/0 | 0/0 | **−5/−10** | 0/+10 |

  Si el índice es Novice, Easy, Medium, Hard, Nightmare: Medium es la base, Hard acerca al Alien y lo hace volver más seguido, y **Nightmare tiene el bloque del Alien más suave que Novice**. Puede ser que en Nightmare la dificultad venga de otro lado: los sentidos, el `Template_Name HARD` que se ve en `IRON.BML`, o detectores y HUD. Hay que confirmar el orden cambiando la dificultad en el menú y viendo el índice.
- **Cómo implementar la dificultad del mod sobre la del juego:**
  - en el hook del cargador, guardar los 5 punteros (cada carga parte de los archivos: no hay acumulación);
  - según la dificultad del mod (que manda el host), **sumar deltas propios a las 5 estructuras**. Así la dificultad elegida en el juego sigue mandando como base, y "Normal" del mod suma 0;
  - deltas del lado difícil: radio de acecho −5/−10 m más, menace −3/−6 s más, max_menaces +1/+2, Vent_Attract −10/−20 s más, backstage distance −5/−10, y los sentidos del ALIEN (`+0x00..+0x20` de su plantilla) en positivo;
  - se pueden cambiar en caliente (al conectar el cliente), porque el juego lee las estructuras mientras corre. A confirmar con un efecto visible: con el radio de acecho muy negativo, el Alien del save `alien` se acerca antes.

### Corrida 125 (save `alien`, misión 6)

- **Índice activo:** el objeto de la dificultad tiene en `+0x24..+0x2C` el índice activo. Vale **2** en el menú y **3** después de cargar la partida: **la dificultad viene del save**. Como el cliente carga la partida del host, los dos quedan con la misma base.
- **Orden del arreglo** (deducido de los valores, que forman una escalera): `[0]` Easy (+10), `[1]` Medium (+5), `[2]` **Hard = base (0)**, `[3]` Nightmare (−5, +1 amenaza), `[4]` Novice (+12/+15). Novice al final cuadra con que se agregó en un parche. La partida vieja del usuario (`safezone`/`alien`) está en **Nightmare** (índice 3; **confirmado por el usuario**, así que el orden queda confirmado). Sin confirmar en el menú: el menú de opciones de juego dentro de la partida no tiene selector de dificultad (tiene esquema de control, sensibilidad, invertir ejes, asistencia de puntería y vibración, con sliders y selectores: sirve de modelo para DESIGN_CONFIG_MENU).
- **Prueba de efecto** (`memsrv W`, nuevo):
  - con la estructura activa endurecida (radios de acecho −30 m, menace −10 s, max_menaces +3, conductos −120/−240 s, role_timeout −200, distancias de backstage −20/−30) y recargando el checkpoint, el Alien quedó estacionado justo encima del jugador **a los ~42 s** y no se movió más;
  - con los valores originales, en la misma sesión, recorrió los conductos 154 s **sin estacionarse**; en la corrida 118 se estacionó a los ~74 s;
  - es evidencia de que el juego usa esos valores, pero no es concluyente: al principio de la misión 6 el script retiene al Alien en los conductos. Para ver el efecto en la caza hace falta avanzar hasta que baje (alguien jugando).
- `memsrv`: `W <addr> <hex>` (solo con `AIMP_MEMSRV_TEST=1`).

## Orden sugerido

1. Ubicar las estructuras de dificultad cargadas (tabla nombre → campo) y probar un multiplicador sobre un parámetro del Alien en el save `alien`. Es el primer resultado visible.
2. Reglas de caída y revivir por nivel (`death.c`): son propias, no dependen del engine.
3. Pickups por porcentaje (host).
4. El Alien que caza a los dos (DESIGN_ALIEN). Es la pieza grande.
5. El selector en el menú de opciones (DESIGN_CONFIG_MENU).
