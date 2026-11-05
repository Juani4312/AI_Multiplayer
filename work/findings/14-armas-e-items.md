# 14 - Armas, arrojables e ítems: qué RPCs generan (corrida 13)

- **Pregunta:** ¿qué mensajes genera el juego al disparar, tirar algo y levantar un ítem? ¿Se pueden replicar como el "usar"?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Evidencia:** `work/evidence/runtime/run13-armas-items.log` (gitignored; `AIMP_TRACE_QUEUES=1`, `AIMP_TRACE_USE=1`, `AIMP_GHOST=3`, papito sin red, 2026-11-05).
- **Confianza:** alta para qué tipos aparecen y cuándo. Media para lo que significan, que sale de los payloads y de las zonas de código donde se generan.
- **Verificado en runtime:** sí (una corrida). Sin replicar todavía.

## Lo que hizo el usuario y lo que se vio

| Acción (marcas F8) | Mensajes A (call site) | Payload |
|--------------------|------------------------|---------|
| Lanzallamas, ~72–100 s (MARCA 1 77,8) | **A 0x18** (56 bytes, `0x494B9C`, zona de scripts), 14 veces en total → B 0x1A | `id jugador[8] \| id entidad[8] \| id[8] (¿tipo de daño o arma?) \| 06000003 \| 0 \| 1 \| 1 \| pos float[3] \| 1e00ffff`. Ejemplo: entidad `3181058b 3b7d5e1a`, pos (9,51, 14,55, 31,64) |
| (el mismo fuego, sobre el avatar) | **A 0x0C** (16 bytes, `0x6D915B`, zona de IA), 6 veces → B 0x0E | `id jugador[8] \| id del NPC avatar[8]` (`1c3460b3 7e03a5dc` = "CV5"). Parece "agresión del jugador sobre el NPC": por eso el civil quiere huir |
| Cae el Alien y se le dispara (~105–110 s, MARCA 2 113,1) | **A 0x22** (16 bytes, `0x395FDB`, `CharacterManager`): 2 además de los 6 de la carga | Sin muestras nuevas en el log (solo se guardan las 8 primeras por tipo) |
| Bengala | **ningún mensaje** | — |
| Balas del revólver (MARCA 3 147,4) | **A 0x26** = el "usar" (`0x395D75`) → B 0x29 | objetivo `f7fb2113 162bda8d`: el mismo prefijo que el molotov de la corrida 3 (`f7fb2113 d783f856`) |

## Conclusiones

- **Disparar no es un RPC.** Ni el lanzallamas ni la bengala generan un mensaje por la acción. Para que el otro vea el fuego o la bengala habría que hookear el disparo y el lanzamiento, y mandarlos como eventos propios, como el "usar". No investigado.
- **Los efectos del disparo sí se serializan:** daño o impacto sobre entidades (0x18, con posición) y agresión sobre NPCs (0x0C).
  - Replicarlos podría reproducir el daño en la otra máquina, por ejemplo el Alien quemado. Pero la IA de cada máquina es independiente: hay que pensarlo junto con la sincronización de la IA.
- **Levantar ítems es un "usar" (0x26) y ya se replica** con `AIMP_SYNC_USE=1`, porque el prefijo `f7fb2113` no está filtrado. En la otra máquina lo haría el avatar. Falta probarlo con las dos máquinas: ¿desaparece el ítem allá? ¿Quién se lo queda?
- El avatar civil recibe la agresión del lanzallamas porque está a 1,5 m del jugador (modo local). Con red, el avatar está donde está el otro jugador.

## Corrida 14: ítems con dos máquinas

- Balas del revólver (`f7fb2113 162bda8d`, el mismo id que en la corrida 13) y una placa coleccionable (`cd567644 4cafe144`).
  - Se mandan y la otra máquina las aplica con el avatar.
  - **No desaparecen en la otra:** el NPC no puede levantar ítems. Cada uno se queda con su copia: botín por jugador.
- Con el avatar no autoritativo (`AIMP_GHOST_NOAUTH`), su IA no reacciona al lanzallamas. El avatar de la otra máquina, que tenía la IA, sí huyó.

## Pendiente

- ~~Prueba con las dos máquinas de levantar un ítem~~: hecha (botín por jugador). Si se quiere botín compartido, sacar el ítem del mundo en la otra máquina sin dárselo.
- Hookear el disparo (lanzallamas, revólver) y el lanzamiento de arrojables.
- Mirar el payload de 0x22 cuando aparece el Alien: subir `TR_PAYLOAD_SAMPLES` o tomar muestras por período.
