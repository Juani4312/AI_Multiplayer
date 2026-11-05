# 11 - Mensajes RPC observados en runtime

- **Pregunta:** ¿Qué mensajes pasan por las colas A (loopback) y B (sesión/red) durante una partida normal, quién los produce y quién los recibiría en un cliente?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** tabla de abajo (RVAs; VA = 0x400000 + RVA).
- **Evidencia:**
  - corrida 2 del 2026-11-05 con `AIMP_TRACE_QUEUES=1` (Proton 9.0-203, nivel `Production\HAB_ShoppingCentre`): `work/evidence/runtime/run2-juego.log`;
  - catálogo de endpoints (líneas `RPC` del log);
  - Ghidra: `work/evidence/mod/m1.txt`…`m3.txt` (productores y callers).
- **Confianza:** alta para la tabla (observada en runtime y cruzada con el catálogo de endpoints y los call sites). Media para la semántica de cada tipo, salvo 0x29 (spawn) y 0x26 (interacción), que salen del código.
- **Cómo verificar en runtime:** ya está verificado. Una corrida con marcas (F8) y payloads permitiría separar puerta e ítem (ver "Pendiente").
- **Verificado en runtime:** sí.
- **Preguntas abiertas:** al final.

## Checklist de la corrida (usuario, 13:29–13:32)

Línea de tiempo reconstruida del log (t = segundos desde que cargó la DLL; t = 0 ≈ 13:28:39):

| t (s) | Qué se ve en el log | Acción |
|-------|---------------------|--------|
| 72 | modo → nivel 10 `HAB_ShoppingCentre`; carga de 5,9 s dentro de `Run` | cargar la partida |
| 78–79 | 6 NPCs, después aparece el jugador (+1 NPC); ráfaga de RPCs (abajo) | spawn del nivel |
| 79–92 | posición fija | quieto |
| 93–100 | yaw recorre [−π, π] | giro de 360° |
| 101–106 | 2,4 m/s en línea recta, yaw fijo | caminar |
| 110–114 | la cámara baja de 1,57 a 0,85 m sobre los pies | agacharse |
| 130–134 | Y sube de 14,5 a 18,6 | escalera o rampa |
| **140,3** | **A 0x26 → B 0x29** (20 bytes) | interacción 1 (¿puerta?) |
| 153, 160 | volcados F9 | F9 × 2 |
| **≈168–172** | **A 0x26 → B 0x29** (20 bytes) | interacción 2 (molotov, "a las 13:31") |
| 173,6 | A 0x23 → B 0x26 (8 bytes) | ¿fin de la interacción o inventario? |
| 184 | el contador de simulación se congela | menú de pausa |
| 197 | modo → nivel 0 `Frontend` | salir |

## Mensajes observados

Todos en el thread principal. "A" = cola local (`FUN_00a2a290`); "B" = sesión, que sin proveedor se descarta.

| A tipo | Bytes | n | Productor (call site) → caller | Handler A (RVA) | → B tipo (call site en el handler) | Handler B en la sesión (receptor de un cliente) |
|--------|-------|---|--------------------------------|-----------------|-------------------------------------|----------------------------------------------|
| **0x29** | 304 (0x130) | 8 | `FUN_00797490` (`0x39751A`) ← `FUN_007a7a20/a80` | `0x14D10` | **0x2C** (`0x14D79`) | `0x14C90` |
| 0x28 | 8 | 8 | `FUN_00795780` (`0x3957FC`) ← `FUN_007a76e0` | `0x14E30` | 0x2B (`0x14E93`) | `0x14DB0` |
| 0x20 | 8 | 9 | `FUN_007960b0` (`0x396111`) ← `FUN_007a3320` | `0x190C0` | 0x23 (`0x19123`) | `0x19040` |
| 0x22 | 16 | 6 | `FUN_00795f70` (`0x395FDB`) ← `FUN_007a3040` | `0x18E70` | 0x25 (`0x18ED9`) | `0x18DF0` |
| 0x1E | 24 | 2 | `FUN_00797790` (`0x397809`) ← `FUN_007a6570` | `0x389D0` | 0x20 (`0x38A43`) | `0x38940` |
| **0x26** | 20 | 2 | `FUN_00795d00` (`0x395D75`) ← `FUN_007a3110` | `0x18A10` | **0x29** (`0x18A84`) | `0x18980` |
| 0x23 | 8 | 1 | `FUN_00795ee0` (`0x395F41`) ← `FUN_007a2b30` | `0x18D60` | 0x26 (`0x18DC3`) | `0x18CE0` |
| — | 32 | 113 | B directo: `FUN_007e1540` (`0x3E1597`) ← `FUN_007e2710` (zona NETZ) | — | 0x07 | `0x8BE00` |

## Lectura

1. **Las colas no persisten entre frames.** Los mensajes se encolan y se vacían en el mismo frame: el tamaño leído después de `Run` fue 0 en 16.218 frames. Para capturarlos hay que hookear el encolado, como hace el trazado, o el flush.
2. **Patrón RPC confirmado.** El stub productor hace `if (!IsClient())` y encola en A. El handler de A ejecuta la acción localmente y emite el mensaje de red equivalente en B. B tiene su propio handler de recepción en la sesión, que en un cliente ejecutaría la misma acción. Es el diseño cliente/servidor original, completo en retail para estos tipos.
3. **Volumen bajo.** Fuera de la ráfaga del spawn del nivel (8 spawns + ~30 mensajes de setup + 113 B `0x07`), en 2 minutos de juego hubo **3 mensajes**. La IA, las puertas automáticas y el movimiento **no** generan RPCs en estas colas. El movimiento y la IA tendrían que ir por los canales NETZ (máscara `000000ff:00000001` en el juego), que no tienen transporte.
4. **Tipo 0x26 = interacción con un objeto usable.**
   - `FUN_007a3110` mira el `"pickupOverlay"` (el cartel de "usar"), estados del personaje (`+0x293`, `+0x297`, `+0x307`), que tenga controlador de jugador (`+0x31C`) y que haya un objetivo.
   - Si el objetivo tiene el bit `+0x96 & 0x10` reproduce `"Play_ACCESS_Denied"`.
   - Llama a `FUN_00772840(objetivo+0x58, personaje+0x54, flag)`.
   - Encola 20 bytes: id de 8 bytes del objetivo (`+0x58/+0x5C`), id de 8 bytes del personaje (`Character+0x54/+0x58`, p. ej. `b222f72f 72519ab8`) y un flag.
5. **Tipo 0x29 = spawn** (request de 0x130 bytes; hallazgos 05 y 10).

## Consecuencias para el coop (`DESIGN_OPTIONS.md`)

- **Eventos discretos (opción B):** los RPCs de personajes e interacciones ya están serializados con ids de entidad de 8 bytes, iguales entre máquinas si el nivel y los datos son iguales. Capturar B en el host y entregarlo al handler de recepción de la sesión del cliente es viable para estos tipos.
- **Interacciones del jugador remoto:** inyectar un 0x26 con el id del personaje remoto podría hacer que el juego ejecute la interacción para ese personaje. Hipótesis, no probado.
- **Movimiento y animación:** no pasan por estas colas. Opción A (transform manual) o los canales NETZ (investigar qué escribe en ellos).

## Corrida 3: acciones separadas con marcas F8 (13:39–13:46, `work/evidence/runtime/run3-interacciones.log`)

El usuario apretó F8 antes y después de cada acción. Las marcas quedan en el log con frame y posición.

| Acción | Marcas (t) | Mensajes entre las marcas |
|--------|------------|---------------------------|
| Botón para llamar al transit | 1–2 (189,5–192,4) | A 0x26: objetivo `044106b1 6f529959` |
| Puerta automática | 3–4 (216,8–222,7) | **ninguno** |
| Caja 1 | 5–6 (238,7–242,1) | A 0x26: objetivo `eed179bb 3c7301a5` |
| Caja 2 | 7–8 (248,2–250,2) | A 0x26: objetivo `eed179bb 6abd978c` |
| Molotov del piso | 9–10 (256,7–258,2) | A 0x26: objetivo `f7fb2113 d783f856`; 1,5 s después, **A 0x23** `5d9b3722 00000000` |
| Viaje en transit (sin marcas) | — | 8× **A 0x24** (88 bytes: matrices y posiciones float) → B 0x22 (96 bytes, desde `0x396197`); después, cambio de nivel 10 → 16 `Production\Tech_Hub` (carga de 6,2 s) |
| Entrar al locker | 11–12 (355,7–360,4) | A 0x26: objetivo `3e3ec35d b0c13784` |
| Salir del locker | 13–14 (361,3–364,8) | **ninguno** |
| Terminal (abrir, 2–3 opciones, cerrar con Q) | 15–16 (372,9–387,3) | A 0x26 al abrir: objetivo `1e38b898 68328994`; las opciones y la salida **no** generan mensajes |

### Payloads (bytes en orden de memoria)

- **0x26** (20 bytes) = `objetivo[8] | personaje[8] | flag[4]`.
  - Personaje en HAB_ShoppingCentre: `2ff722b2 b89a5172`, que como dwords da `0xB222F72F 0x72519AB8` = `Character+0x54/+0x58`.
  - Personaje en Tech_Hub: `2ff722b2 099d5b9b`; cambia la segunda mitad.
  - El flag es siempre `01587c00`.
  - Las dos cajas comparten la primera mitad (`eed179bb`).
  - Hipótesis: id = par (grupo/composite, instancia) de ShortGuids.
- **0x29** (spawn, 304 bytes; se ven los primeros 64): `índice | 3 handles (familia 0x..9b3722) | 1 | id[8] (segunda mitad 82fc64f8 común a todos) | 0 | id[8] | id del personaje[8] | tipo de definición | …`. El #2 es el jugador: id `2ff722b2 b89a5172`, tipo 0. Los NPCs tienen tipos 3 y 6 y ids que empiezan con `1c3460b3`.
- **0x28** (8 bytes) = `índice del spawn | flags` (`01a41301`, `013ef77b`); uno por personaje del spawn.
- **0x23** (8 bytes) = un handle de la familia `0x..9b3722` (`5d9b3722`). Aparece después de agarrar un ítem: hipótesis, fin del pickup o alta en el inventario.
- **0x24** (88 bytes) = transforms (matrices con `0x3f800000` = 1,0 y posiciones); 8 antes de la carga del transit.
- **B 0x07** (32 bytes, ~113 al cargar el nivel) = `16 bytes en 0 | id[8] | contador/flag`. Hipótesis: registro de entidades.

### Lectura

1. **0x26 es "usar" genérico**: botones, contenedores, ítems, lockers y terminales pasan todos por el mismo RPC, con la referencia de 8 bytes de la entidad objetivo. Para la etapa 2 del proyecto (mundo compartido) es el punto de sincronización de las interacciones.
2. **Lo que pasa después de "usar" es local**: salir del locker, las opciones de la terminal y las puertas automáticas no generan mensajes.
   - Las puertas automáticas dependen de triggers de proximidad.
   - Con dos jugadores en el mismo mundo, cada juego abre las suyas: hay que ver si los triggers miran a todos los personajes (hallazgo 06).
3. Las referencias de entidad parecen estables entre corridas para los objetos del nivel. Los ids de los personajes cambian por nivel en la segunda mitad.

## Pendiente

- ~~Separar puerta e ítem~~: hecho en la corrida 3.
- Confirmar que la referencia de 8 bytes es estable entre corridas y máquinas: cargar el mismo checkpoint dos veces y comparar los objetivos de 0x26.
- Probar la inyección: encolar en A un 0x26 con un objetivo conocido (p. ej. una caja) y ver si se abre.
- Semántica de 0x1E, 0x20, 0x22, 0x23, 0x28 y del B 0x07 (113 por nivel, zona NETZ `0x7E1540`).
- Qué escribe en los canales NETZ durante el juego (máscara `ff:01`).

## Preguntas abiertas

- ¿Las puertas automáticas (por proximidad) generan algún mensaje? En esta corrida no se vio ninguno fuera de los dos 0x26.
- ¿El id de 8 bytes es el par de ShortGuids (entidad, instancia) de Cathode? Se podría confirmar buscando esos valores en `COMMANDS.PAK` (hallazgo 06).
