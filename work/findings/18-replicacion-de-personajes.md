# 18 - Replicación de personajes del engine (NETZ): codifica, pero el transporte de canales es un stub

- **Pregunta:** ¿el engine trae la replicación de personajes del modo red original (posición, acciones, animación, armas, salud)? ¿Sirve para animar al avatar (fase 4b) y para un Alien "títere" en el cliente (etapa 3)?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA |
  |-----|----|
  | Gestor de replicadores = `[g_root+0x4C]` (`FUN_007e10d0`, vtable `0x012F72CC`; pool de objetos de 0x440 bytes) | `0x007E10D0` |
  | `vf[0x10]` = crear replicador `(canal, personaje)` (lo llama `SpawnCharacter`, `bloque+0x164`) | `0x007E1040` |
  | Constructor del replicador (vtable `0x012F7270`) | `0x007DE3A0` |
  | Lista estática de los 11 tipos de mensaje (cabeza `0x016F2DAC`; entradas `{next, nombre, fn, u16 tipo}` desde `0x01641E1C`, stride 0x10) | `.data` |
  | Registro de los 11 tipos en el canal de cada personaje (ids 0..10) | `0x00789E20` |
  | Dispatch de NETZ (tarea `NETZ_DISPATCH`) | `0x007DDE20` |
  | **Stub**: datos pendientes de un canal (siempre devuelve vacío) | `0x00A2AB80` |
  | **Stub**: "empezar mensaje" en el canal (devuelve `canal+0x2A/+0x2B`; nadie usa el resultado) | `0x00A2ABB0` |
  | Escritor de `update_action_continuous_block` (ejemplo) | `0x007E09A0` |

- **Evidencia:** `work/evidence/h18/n1.txt` … `n25.txt` (Ghidra, `Dump.java`). Tablas de `.data` leídas del archivo con un parser PE propio. Los inicializadores estáticos (`0x1148E70…`) no estaban definidos como funciones en Ghidra: por eso los nombres no tenían xrefs.
- **Confianza:** alta para la estructura y los stubs (asm directo). Media para el significado de cada tipo de mensaje (sale del nombre).
- **Cómo verificar en runtime:** no hace falta para la conclusión principal: con `FUN_00a2ab80` vacío, el dispatch nunca llama al transporte aunque se instale uno.
- **Verificado en runtime:** no.

## Lo que hay

1. **Un replicador por personaje.** `SpawnCharacter` (`0x7D3B50`) reserva un canal NETZ, registra en él los 11 tipos (`FUN_00789e20`) y pide al gestor `[g_root+0x4C]` un replicador: `vf[0x10](canal, personaje)` = `FUN_007e1040`. El constructor (`FUN_007de3a0`) lo **apila como listener del personaje** (`Character+0x1160/+0x1164`, `FUN_0077a520`). Es la misma pila que recibe los eventos del arma (`0x100029` empezar a disparar, `0x10002A` parar…, hallazgo 17).
2. **Los 11 tipos de mensaje** (lista en `.data`; la función de cada uno es el handler de recepción):

   | id | Nombre | Handler (deserializador) |
   |----|--------|--------------------------|
   | 0 | `update_action_continuous_block` | `0x7E1980` (`FUN_00783b10`) |
   | 1 | `update_action_pos` | `0x7E19C0` (`FUN_00783b70`) |
   | 2 | `character_change_action` | `0x7E1A00` (`FUN_00783bb0`) |
   | 3 | `secondary_action_continuous_block` | `0x7E1A40` |
   | 4 | `character_change_secondary_action` | `0x7E1A80` |
   | 5 | `infrequent_character_data` | `0x7E1AC0` |
   | 6 | `character_discontinuous_positioning` | `0x7E1B00` (lee 32 bytes crudos) |
   | 7 | `animation_context_change` | `0x7E1B40` |
   | 8 | `CharacterWeaponStatusUpdate` | `0x7E1B80` (`FUN_00783d70`, 532 bytes) |
   | 9 | `HealthStatusUpdate` | `0x7E1BC0` |
   | 10 | `PVPCommunicationMsg` | `0x7E1C00` |

   El orden de la lista es el inverso al de los inicializadores. Los ids que se asignan en runtime pueden no coincidir con esta tabla: se leen del `u16` de cada entrada (`+0x0C`).
   Cada handler deserializa en un struct del stack y llama a `receptor->vf[1](struct, stream)`. El receptor (el "display character" de un cliente) no se encontró.
3. **El replicador codifica.** Ejemplo: `FUN_007e09a0` (llamado desde `FUN_007e2870` ← `vt[4]` = `FUN_007e3a30`):
   - compara el bloque de acción continua con el último mandado (`+0x50`);
   - respeta intervalos de reenvío (`+0xD0`, tiempos del componente `Character+0x36C`);
   - arma el mensaje en buffers locales.
   Los escritores de cada tipo leen el id de la lista (`MOVZX [0x01641E28]`, etc.): `0x7E0B86`/`0x7E0DFB`, `0x7E0C76`, `0x7E0F56`, `0x7E0986`, `0x7E0764`, `0x7E058B`, `0x7DF195`, `0x7DF215`, `0x7E034E`, `0x7E03C6`, `0x7DF295`/`0x7E74CB`.
4. **Pero el canal es un stub:**
   - "empezar mensaje" (`FUN_00a2abb0(tipo, confiable)`) solo devuelve `canal+0x2A` o `+0x2B`, y el escritor no usa el resultado. La escritura de los bytes en el stream se compiló vacía;
   - el dispatch (`FUN_007dde20`) pide los datos de cada canal con `FUN_00a2ab80`, que **siempre devuelve un buffer vacío**. Por eso nunca llega a `transporte->vf[1](id, datos, tamaño, confiable)`, aunque `NETZ+0x69C` tuviera un transporte.

## Consecuencias

- **La opción C (revivir la red original) no sale gratis:** habría que reimplementar la capa de canales (escribir los bytes, leerlos y despacharlos), además del transporte y del lado receptor (display characters), que no se ubicó. Queda descartada como camino corto.
- **Animación del avatar (fase 4b) y Alien títere:** hay que replicarlas a mano, pero el replicador marca qué datos usaba el engine: el bloque de acción continua (`update_action_continuous_block`), la posición de la acción (`update_action_pos`), los cambios de acción (`character_change_action`) y el contexto de animación (`animation_context_change`).
- Para encontrar cómo aplicarlos a otro personaje, conviene mirar qué lee el replicador del personaje (`Character+0x36C`, el componente de animación) y buscar el setter equivalente: lo que el `display character` hubiera llamado al recibirlos.
- Se podría capturar el mensaje ya codificado en el host con un hook en el escritor (por ejemplo `FUN_007e09a0`, que recibe datos y tamaño), pero sin el receptor no hay dónde aplicarlo.

## Preguntas abiertas

1. ¿Existe en el binario la clase receptora (display character) con `vf[1]` para estos mensajes? Buscar vtables cuyo `vf[1]` acepte los structs de `FUN_00783b70`/`FUN_00783bb0`.
2. ¿Qué es `Character+0x36C` (vtable con `+0x14`, `+0x18`, `+0x1C` = tiempos de reenvío)? Es el candidato a "componente de animación" de la fase 4b.
3. ¿Qué datos lleva el bloque de acción continua (`param_3` de `FUN_007e09a0`, tamaño `param_2`)? Un hook de traza en runtime lo mostraría: es lo mínimo para reproducir la animación del otro.
