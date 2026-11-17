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
- **Guardado exigente: sí.** Solo se guarda si todos los vivos están cerca de la terminal (los caídos o muertos no cuentan). La frecuencia de guardado no importa.
  - Cómo: el mod ya detecta el uso de la terminal (prefijo `1e38b898`, `sync.c`); si el compañero vivo está lejos, no deja pasar el uso (hook del productor `FUN_00795d00`) y cambia el prompt con `ui.c` ("Esperá a tu compañero").
  - Detalle: la partida canónica es la del host (`savesync`). Si guarda el cliente, el host tiene que guardar en el mismo momento; hace falta disparar un guardado desde el mod (máquina de estados del guardado: `FUN_00a637c0`, `obj+0xC`/`+0x2F4`; NOTES corrida 106).
- **Alien más perceptivo con multiplicadores: sí.**
- **La dificultad del mod es independiente de la del juego:** "Normal" del mod no toca nada (se usan los valores de la dificultad elegida en el juego); las otras multiplican esos valores. El mod guarda los valores base la primera vez que los ve y aplica sobre esa copia, así no se acumulan al recargar. A verificar: que la dificultad base esté en el save (así el cliente, que carga la partida del host, queda con la misma base).
- **Desangrado y revivir en persona: a futuro.** Idea: el caído queda forzado agachado y con el stick escalado (el mod ya intercepta el mando), con el efecto de vida baja del juego; un temporizador; el compañero lo revive gastando un botiquín a menos de 2 m (`inv.c` ve el consumo).
- **Varios aliens: a futuro** (complejo).

## Orden sugerido

1. Ubicar las estructuras de dificultad cargadas (tabla nombre → campo) y probar un multiplicador sobre un parámetro del Alien en el save `alien`. Es el primer resultado visible.
2. Reglas de caída y revivir por nivel (`death.c`): son propias, no dependen del engine.
3. Pickups por porcentaje (host).
4. El Alien que caza a los dos (DESIGN_ALIEN). Es la pieza grande.
5. El selector en el menú de opciones (DESIGN_CONFIG_MENU).
