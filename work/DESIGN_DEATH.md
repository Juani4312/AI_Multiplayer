# Muerte en coop: diseño y estado

_2026-11-17. Pedido del usuario: "si uno muere, que el otro siga jugando, y que el muerto respawnee al completar una misión o en un save"._

## Cómo muere el jugador en el juego original (corridas 90–91)

- `FUN_007d0c10` thiscall(personaje, atacante), `ret 4`, mata: aplica un daño igual a la salud actual (`Character+0x33C`) con la función central de daño `FUN_007caac0`. La llama, entre otras, la acción `ACT_PLAYER_DEATH` (`FUN_00668120`), que es el remate del Alien.
- Después de la muerte:
  - la cámara cae al piso durante ~2 a 3 s;
  - a los ~4 s el contador del juego se congela (pausa) y aparece el menú `game_over_menu` ("Load Current Save / Load Previous Save / Load Mission Save", con un tip);
  - el modo sigue siendo el del nivel; no hay cambio de modo.
- Los scripts reaccionan: `Player_Sensor.Death` ×4, `CHR_DeathMonitor`. Pero el game over no sale de un pin visible: lo decide el engine.
  - `FUN_00853830` (`vt+0x38` de una pantalla de UI, vtable `0x013001f0`) abre `game_over_menu` con `FUN_0081f8f0`.
  - Con un hook en `FUN_0081f8f0` (`AIMP_TRACE_UI=1`) el game over **no** pasó por ahí (corrida 91): el disparador real sigue sin ubicar.
- El engine trae restos de un sistema de respawn de multijugador: entidades `RespawnConfig` (`respawn_mode`, `respawn_wait_time`), `RespawnExcluder`, `TeamRespawnBonus` y `NoLocalRespawnBonus`, y el pin `finished_respawning`. No se investigaron.

## Lo que ya anda: "caído" en vez de muerto (corrida 92)

- **Dentro de `FUN_007caac0`:** si `Character+0x4253 != 0` y el golpe es mayor o igual a la salud, el juego lo recorta a `salud − 1`. Es un flag del propio juego de "no puede morir".
- **`AIMP_COOP_DOWNED=1`** (`mod/src/death.c`, experimental, apagado por defecto):
  - el mod pone `+0x4253 = 1` en el jugador local (el juego a veces lo vuelve a 0, así que se revisa cada 100 ms);
  - cuando la salud llega a 1, el jugador queda **caído**: invencible con los mismos flags que `CHR_SetInvincibility` (`+0x2E8`/`+0x2EA |= 8`).
- **Verificado con `KILL`** (la misma función que usa el juego): salud 1 de 1000, sin game over, y el jugador sigue moviéndose.
- **Revivir:** con `AIMP_COOP_REVIVE_FILE` (por ahora solo para pruebas), salud al máximo y sin invencibilidad. Verificado.

## Por red (corrida 95, una máquina con el peer)

`AIMP_EVT_COOP` (7, `CoopEvt` de `death.h`):
- **Caído y revivido:** al quedar caído, el mod avisa; el otro pone al avatar en la pose de caído (`AIMP_GHOST_DOWNED_ANIM`, `cower_low_idle`). Al revivir, avisa y el avatar vuelve a la normalidad.
- **Revivir:** si el vivo usa una terminal de guardado (id que empieza con `1e38b898`, ya detectado en `sync.c`), manda "revivir" con su posición. El caído revive con salud máxima y aparece a 0,8 m del que guardó.
- **Los dos caídos:** el último en caer saca su flag "no puede morir" y se mata como lo hace el juego (`FUN_007d0c10`), así que hay game over normal. Con el game over de las dos máquinas, los dos recargan su save (DESIGN_SESSION: guardar juntos).

Corrida 95 (`safezone`): caído y revivido del compañero (avatar en pose), `KILL` local → caído y aviso, "revivir" → teletransporte a (−104,20; 20,55; −9,60) con salud 1000, compañero caído más `KILL` local → "los dos caídos" y menú de game over (captura).

## Lo que falta (propuesta)

1. **Qué hace el caído.** Hoy se mueve libre, invisible para el daño. Opciones:
   - a) "fantasma": se mueve libre, el Alien y los NPCs lo ignoran (falta: que la IA no lo vea), y en la otra máquina el avatar desaparece;
   - b) espectador: la cámara sigue al avatar del compañero y el personaje queda quieto y escondido. Es más prolijo, pero hay que controlar la cámara.
2. ~~**Red:**~~ hecho (`AIMP_EVT_COOP`, corrida 95). Falta decidir si el avatar del caído se oculta en vez de acurrucarse.
   - **Para ocultarlo:** la entidad `NPC_SetInvisible` usa `FUN_0078b100` thiscall(personaje, bool), `ret 4`. Pone `Character+0x2D9` y oculta el modelo (componente `+0x3F0`, `FUN_00691b00`) y el equipo (`FUN_00782870`). Es invisibilidad visual, no para los sentidos de la IA.
   - **Escrito (2026-11-17 tarde, sin probar en el juego):** `AIMP_GHOST_DOWNED_HIDE=1` (apagado por defecto) oculta el avatar cuando el compañero cae y lo vuelve a mostrar al revivir (`ghost.c`).
   - Para que el Alien ignore al caído hace falta otra cosa: la percepción, `DESIGN_ALIEN.md`. `IGNORE_PLAYER` es el valor 0 del enum `ALERTNESS_STATE` (`IGNORE_PLAYER`, `ALERT`, `AGGRESSIVE`; ShortGuids en `0x0174C460..68`, nombres en `FUN_008b6610`): el estado de alerta de los NPCs humanos. Puede servir para que los humanos ignoren al caído; el Alien va por la percepción (`FUN_00730c30`).
3. **Revivir de verdad:** hecho con la terminal de guardado (corrida 95, con el peer). **Checkpoint (escrito, sin probar):** cuando la entidad `Checkpoint` de los scripts dispara `finished_saving` (vtable rva `0xE51880`/`0xE514E8`, ShortGuid `5bacef5d`; lo ve el hook de pines, así que hace falta `AIMP_SYNC_PINS` o `AIMP_TRACE_PINS`), el vivo pide revivir al caído. Falta al completar un objetivo, y probarlo con dos máquinas:
   - cuando el vivo usa una terminal de guardado (el "usar" ya se detecta y la terminal ya se identifica en `AIMP_SYNC_USE_SKIP`);
   - o cuando pasa un checkpoint o completa un objetivo;
   - el caído aparece al lado del compañero (teletransporte, como el avatar).
4. ~~**Si caen los dos:**~~ hecho (corrida 95). game over normal. Se saca `+0x4253` y se llama a `FUN_007d0c10` sobre el jugador, y los dos recargan su save. Con `AIMP_SAVE_SYNC=1` el cliente recarga el checkpoint del host (corrida 115). Después de la recarga el estado de caídos se reinicia y el modo caído vuelve (corrida 116; antes quedaba apagado).
5. ~~**Remate del Alien:**~~ resuelto (corridas 134–137, save `alien`). Con la salud recortada, el remate dejaba la cámara congelada y la **pantalla negra para siempre** (lo mismo que vio el usuario en la corrida 119 con un `RIOT_GUARD`):
   - el remate construye en el slot de acción del jugador (`Character+0x36C`) la acción de muerte (vtable rva `0xEAE690`; arranque `FUN_00675f10`: saca la invencibilidad, llama a `FUN_007d0c10` y prende `+0x298/299/29C/2A9/2BC`);
   - además un fundido a negro en el singleton `0x019D0970` (`+0x1B48` opacidad 1,0);
   - **arreglo (`death.c`):** si matan al jugador local estando en una acción que no es la locomoción, a los 4 s (`AIMP_COOP_DEATH_ACT_MS`) se llama a `FUN_00658080` (vuelve a la locomoción del jugador: destruye la acción de muerte, cuyo destructor `FUN_0065b630` apaga los flags) y se pone la opacidad en 0. Se ve el remate entero, después vuelve la imagen y el caído camina (capturas `run136-*`, `run137-*`);
   - **corrección (corridas 140–142):** el Alien "no lo volvía a rematar" porque quedaba trabado en su mitad del remate (acción rva `0xEAE5D0`), esperando que el jugador termine de morir. Si se lo suelta (`FUN_0065bdd0`, acción por defecto), vuelve a rematar al caído cada ~4,5 s (corrida 141), y la retirada (`NPC_WithdrawAlien`, ver abajo) no lo frena;
   - **regla actual (corridas 146–147):** al caer, la **facción** del jugador (`Character+0x2E4`, 1) pasa a la del Alien (3) y se restaura al revivir (y antes del game over). El Alien deja de tomarlo como enemigo: el atacante se suelta enseguida y **se va a los conductos**, y no vuelve aunque el caído corra haciendo ruido (33 s). Al revivir, el Alien vuelve a cazarlo (en la 147 bajó a los 13 s). Esto cumple el punto 1a ("fantasma": la IA lo ignora). `AIMP_COOP_DOWNED_FACTION` (3; -1 = no tocarla).
   - **regla anterior (respaldo con `AIMP_COOP_DOWNED_FACTION=-1`):** el atacante queda **retenido** mientras el jugador está caído y se lo suelta al revivir. Como cada máquina tiene su propio Alien, que solo persigue a su jugador, retener el del caído no afecta al compañero. Corrida 142: el caído se alejó 4,5 m con el Alien quieto; al revivir, el Alien se soltó y lo volvió a cazar (lo normal);
   - retirada del Alien (`death_alien_withdraw`, como la entidad `NPC_WithdrawAlien`, `FUN_00496da0`): `w = [Alien+0x34F0]`, `c = [w+0x18]`; `FUN_007349d0(c)`, `FUN_006c5ef0(c, 0)`, `FUN_0072fc20(c, 1)` (modo 1, director en `+0x34D0`), `[w+0xC] = 0`. Sin efecto visible con el objetivo caído al lado; queda como opción (`AIMP_COOP_ALIEN_WITHDRAW=1`) para probar al revivir.
   - Para el Alien compartido (DESIGN_ALIEN) sirve la facción: la IA ignora al caído sin retener al Alien.
   - Falta: probar remates de humanos y androides (otras acciones; la regla es general: todos llaman a `FUN_007d0c10`).
6. **Host:** el host no necesita estar vivo. Su proceso sigue simulando el mundo aunque esté caído. Solo se corta si cierra el juego.

## Herramientas nuevas para esto

- `AIMP_MEMSRV_FILE` + `work/scripts/memsrv.py`: leer memoria del juego en vivo (`chars`, `read`, `find` de texto alcanzable por punteros).
- `AIMP_MEMSRV_TEST=1`: habilita `KILL <chr>`, solo sobre personajes de la partida actual.
- `AIMP_TRACE_UI=1`: pantallas de UI que se abren, con los retornos de la pila.
