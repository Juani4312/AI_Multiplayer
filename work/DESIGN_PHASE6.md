# Fase 6: scripts y misión compartida (diseño, 2026-11-05)

## El problema

Cada máquina corre sus propios scripts de nivel (CATHODE). Un disparador se ejecuta cuando **su** jugador entra en una zona.

Corrida 14: unos NPCs que conversan y se van aparecieron dos veces. Primero en la PC, cuando entró ese jugador, y después en la laptop, cuando entró el otro. Si cada uno avanza la misión por su lado, el mundo diverge: puertas, objetivos, cinemáticas y spawns del Alien.

## Lo que hay en el engine

| Pieza | Estado | Fuente |
|-------|--------|--------|
| Plomería RPC A → B → sesión, ejecutada en local en retail | **probada**: spawn, usar | hallazgos 10, 12, 13 |
| Inyectar un A en la otra máquina hace que el engine ejecute la acción por su camino normal | **probada** con el "usar" | hallazgo 13 |
| ~26 handlers A de la zona de scripts (`0x48A250`–`0x48BF90`): copian un payload fijo (0x20, 0x60…) y lo pasan a B | estático (`work/evidence/h12/f19.txt`) | este documento |
| Nombres de mensajes de red de entidades: `client_entity_manager_receive_trigger`, `_receive_message`, `_block`, `_flush_all_messages` | estático | hallazgo 10 |
| Productores de scripts en A: `FUN_00491C00` (0x18 bytes; lo llaman callbacks de NPCs humanos, por ejemplo `FUN_0067c8e0`), `FUN_00494B30` (el daño 0x18 de la corrida 13), `FUN_00494BD0`, `FUN_00491CB0` | estático. En las corridas solo se vio el de daño | `f20.txt`, `f21.txt` |
| Nodos de script de multijugador (`HostOnlyTrigger`, `SyncOnAllPlayers`, `IsMultiplayerMode`, …) | **no los usa ningún nivel** | hallazgo 06 |

**Conclusión:** el engine tiene con qué transportar mensajes de entidades, pero los scripts distribuidos no generan mensajes para los disparadores. En un solo jugador, un disparador de zona se ejecuta directo, sin pasar por las colas. Hace falta un mecanismo propio.

## Opciones

1. **Replicar disparadores a nivel CATHODE (la más completa).**
   - Hookear la función que dispara un pin de una entidad.
   - En el host, capturar la referencia a la entidad (camino de ShortGuids, estable entre máquinas, hallazgo 06) y el ShortGuid del pin.
   - En el cliente, inyectar ese disparo y bloquear el disparo local de las mismas entidades.
   - Requiere ubicar la función de despacho de pines, armar un filtro (qué entidades decide el host) y resolver la entidad desde su referencia en el cliente.
2. **Sincronizar por checkpoints (la más simple).** Cuando el host llega a un checkpoint o guarda, el cliente carga el mismo (hallazgo 09). Corrige la divergencia cada tanto, pero entre checkpoints cada uno ve su versión.
3. **Híbrida.** Empezar por los disparadores de zona que arrancan diálogos, cinemáticas y objetivos (lo que más se nota), con la opción 1, y usar la opción 2 como red de seguridad.

Recomendación: la opción 3. Primero hay que medir qué despacha CATHODE cuando el jugador entra a la zona del diálogo.

## Próximos pasos de RE

1. Ubicar el despacho de pines de CATHODE:
   - los slots `+0x1C`, `+0x20`, `+0x38` y `+0x5C` de las entidades con comportamiento (`Door`, `TriggerSimple`; hallazgo 06);
   - los usos del ShortGuid de `trigger` / `on_enter` / `on_finished`.
2. Una corrida con un hook de traza en el candidato: loguear (entidad, pin) cuando el jugador entra a la zona del diálogo en Tech_Hub. Con F8 antes y después.
3. Probar la inyección en el cliente: disparar el mismo pin sobre la misma entidad y ver si arranca el diálogo.
4. Diseñar el bloqueo en el cliente (que no dispare solo lo que decide el host) y el filtro.

## Mientras tanto

- Ya replicado: botones, cajas, ítems (botín por jugador) y lockers o terminales (no se replican, a propósito).
- Avatar: NPC sin IA.
- Lo que no se sincroniza: la IA y el Alien, el disparo de armas y los scripts.
