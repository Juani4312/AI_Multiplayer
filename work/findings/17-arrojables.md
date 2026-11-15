# 17 - Objetos arrojados y colocados (y el equipo del jugador)

- **Pregunta:** ¿cómo crea el juego el objeto que tira el jugador (bengala, noisemaker, molotov, pipe bomb, humo, flashbang) o la mina que coloca? ¿Se puede reproducir el mismo lanzamiento en la otra máquina? (pendiente del hallazgo 14)
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Evento de animación `"spawn_projectile"` → `FUN_0064c180` | `0x00666F90` | `0x266F90` |
  | Evento de animación `"place_ied"` → `FUN_0064c510` | `0x00663D40` | `0x263D40` |
  | Tirar el ítem de la mano (inventario, consumo, aviso a la IA) | `0x007D4830` | `0x3D4830` |
  | Arma los 64 bytes del lanzamiento y llama a `FUN_007b5050` | `0x007B9500` | `0x3B9500` |
  | Matriz de la mano + velocidad hacia el punto de impacto | `0x0079C910` | `0x39C910` |
  | **Tirar**: `thiscall(pool, m*, vel*, float 15.0, char *plantilla, float time_left)`, `ret 0x14` | `0x007B5050` | `0x3B5050` |
  | **Colocar**: `thiscall(pool, m*, char *plantilla, float time_left)`, `ret 0xC` | `0x007B5190` | `0x3B5190` |
  | Instancia el composite y le pone los parámetros, `ret 0x18` | `0x007ADE20` | `0x3ADE20` |
  | Pool de objetos arrojados (puntero; lo crea `FUN_007d7650`) | `0x016F0DE4` | `0x12F0DE4` |
  | Fuerza del lanzamiento (int 20, `FUN_00744ae0`) | `0x0163E964` | `0x123E964` |
  | `ProjectileMotion`: inicio del vuelo (posición, velocidad, duración, gravedad) | `0x0060B740` | `0x20B740` |
  | Busca un composite por sufijo del nombre (sin mayúsculas) | `0x00920750` | `0x520750` |

- **Evidencia:** `work/evidence/h17/w1.txt` … `w20.txt`, `zone.txt` (mapa de funciones y strings de `0x790000`–`0x7D8000`) y `z64.txt` (eventos de animación, `0x648000`–`0x668000`). Salidas de `work/scripts/ghidra/Dump.java`; se agregó el comando `fstr:INICIO:FIN`. Prólogos y relocations leídos del archivo con un parser PE propio.
- **Confianza:** alta para la cadena de llamadas, las convenciones (`ret` y lectura de los argumentos en el asm) y el layout de los 64 bytes. Media para el significado de `a3` (15.0): `FUN_007b5050` no lo lee en el camino visto.
- **Cómo verificar en runtime:** `AIMP_TRACE_THROW=1` loguea cada lanzamiento con la plantilla, la posición, la velocidad y el caller. `AIMP_THROW_ECHO=1500` repite cada lanzamiento propio 1,5 s después, 75 cm al costado: si aparece una segunda bengala con la misma curva, la inyección funciona. Con dos máquinas, `AIMP_SYNC_THROW=1`.
- **Verificado en runtime:** no.

## Cadena de un lanzamiento

1. La animación de tirar dispara el evento `"spawn_projectile"`: `FUN_00666f90` → `FUN_0064c180` → `FUN_007d4830(inventario)`.
   - El inventario es `[[Character+0x31C]+0x3C]`. Su `+0x3BC8` es la definición del ítem en la mano.
   - Si `def+0x5C == 3` (`consume_when = OnThrowOrPlace`), lo descuenta (`FUN_0078ebe0`, `FUN_0078eb50`).
2. `FUN_007b9500(def+0x58, def+0x70)` arma 64 bytes en el stack: una identidad 4x4 con `[15] = 9.81`.
3. `FUN_0079c910` los llena:
   - `m[0..11]` = matriz 3x4 de la mano (`FUN_00791ae0`), por filas, con la traslación en `m[3]`, `m[7]` y `m[11]`;
   - `m[12..14]` = velocidad = `normalizar([equipo+0x90] − mano) × 20`, donde `equipo = [Character+0x1100]` y `+0x90` es el punto de impacto del arco de lanzamiento;
   - devuelve 0 si el jugador no tiene equipo en el slot actual.
4. `FUN_007b5050(pool = [0x16F0DE4], &m, &m[12], 15.0, def+0x58, def+0x70)`, thiscall con `ret 0x14`. Toma la primera entrada libre del pool: 0x80 entradas de 12 bytes, `[0]` en uso y `[4]` handle.
5. `FUN_007ade20(entrada, plantilla, &m, &vel, 10.0, time_left)`:
   - `FUN_00922520` (con `ECX = [0x174EF40]`) busca el composite cuyo nombre **termina** con la plantilla. Compara de atrás para adelante con `toupper` (`FUN_00920750`). `def+0x58` es `"Thrown\%s"` (`FUN_007857a0`, `thrown_object_name`). En los `COMMANDS.PAK` existen `REQUIRED_ASSETS\THROWN\FLARE`, `NOISEMAKER`, `MOLOTOV_COCKTAIL`, `PIPE_BOMB`, `SMOKE_BOMB`, `FLASHBANG`, `EMP_MINE`, `EXPLOSIVE_MINE`, `INCENDIARY_MINE`, `STROBE_GRENADE`, `PLASTIC_CONTAINER_BOMB` y `THROWN_BASE`.
   - Lo instancia (`FUN_00930950`) y le pone `start_position_matrix` (`m`), `start_position` (`m[3]`, `m[7]`, `m[11]`), `velocity`, `angular_velocity` = 0, `motion_duration` (10.0, `[0x12F1C18]`) y `time_left`.
   - Registra un callback de destrucción (`0x7ADC00`, que no es función en Ghidra) y dispara el método `"spawn_explosive"` del composite (`vt+0x10C`).
   - La plantilla (char*) solo se usa para la búsqueda: `FUN_007ade20` no la guarda (solo escribe `[entrada] = 1`).
6. El vuelo lo simula `ProjectileMotion` (`FUN_0060b740`): lee posición, velocidad y duración de sus parámetros (`FUN_008f4df0`, `FUN_008f4e30`, `FUN_008f4e70`) y aplica la gravedad (9.81) con `FUN_00775d80`. **No lee el estado del jugador.**
7. Después, si `[g_root+0x5C] != 0`, `FUN_007d4830` avisa a la IA: `FUN_0070f310(.., def+0x0C, posición)`. Hipótesis: el ruido del lanzamiento. No investigado.

**Colocar** (minas): el evento `"place_ied"` (`FUN_00663d40` → `FUN_0064c510` → `FUN_007d4a00`) llama a `FUN_007b5190(pool, &m, def+0x58, def+0x70)`, thiscall con `ret 0xC`. Es lo mismo con velocidad 0, duración 0 y el método `"place_item"`. La matriz sale de `FUN_0079ccc0`.
- También lo usa `FUN_007c7ef0`, con una matriz identidad en la posición de `FUN_0078e710`: soltar el ítem de la mano (6 callers, entre ellos `IS_EXPIRED`/`ACTIVATE_HELD`, `FUN_007cc960`).

Los dos empiezan con el prólogo SEH de MSVC: `64 A1 00 00 00 00` (`mov eax,fs:[0]`), `6A FF` y `68 <imm32>` (con relocation en `+9`). El hook pisa los 6 bytes del `mov`.

## Definición de ítem (lo que se vio)

`FUN_007857a0` parsea la base de ítems (`FUN_007bcec0`, `"item_database"`):

| Offset | Campo |
|--------|-------|
| `+0x0C` | id que se pasa al aviso de la IA |
| `+0x50` | `"Equipment\%s"` (`held_object_name`) |
| `+0x54` | tiene objeto arrojable |
| `+0x55` | `droppable_when_held` |
| `+0x56` | se puede colocar o soltar (lo pone en 0 el `place_ied`) |
| `+0x58` | `"Thrown\%s"` (`thrown_object_name`): la plantilla |
| `+0x5C` | `consume_when`: `Never`, `OnEquip`, `OnActivate`, `OnThrowOrPlace` (= 3, por el uso en `FUN_007d4830`) |
| `+0x60` | `drop_when_consume` |
| `+0x64` | `activated_by`: `HoldToActivate`, `PressToActivate`, `CannotBeActivated` |
| `+0x68` | `cancellable_duration_in_seconds` |
| `+0x70` | float que llega como `time_left` |

La tabla de definiciones por tipo de arma o ítem es `[[0x16F182C]+0x20C][índice]`. Campos vistos: `+0x20` perdigones (máx. 16), `+0x2C/+0x30/+0x34` dispersión, `+0x1CC/+0x27C` radio de efecto y `+0x278` flags.

## El equipo del jugador (`[Character+0x1100]`, para las armas de fuego)

- `+0x00..+0x28`: 11 slots (objetos con vtable; `vt+0x14` devuelve la instancia del arma, con `+0xD0` = índice en la tabla de definiciones y `+0xE0` = cantidad).
- `+0x150C`: slot actual (9 = ítem en la mano, 10 = ninguno); `+0x1514`, el anterior; `+0x151C`, el siguiente.
- `+0x1528`: Character; `+0x152C`: estado de la cámara o animación (`+4` = id); `+0x34`: estado de apuntado (0 nada, 2 apuntando).
- `+0x90`: punto de impacto del arco de lanzamiento; `+0xA8`: flag que limpia el evento `"trigger_pulled"` (`FUN_006647d0`).
- Update por frame: `FUN_007ccfb0` → … → `FUN_007c8510` (`player_projectile.cpp`): raycasts por perdigón (`FUN_007f9ad0`); resultados en `+0x1300`/`+0x1400`, cantidad en `+0x1530`.
- **Las armas son composites de CATHODE.** `FUN_0079c150` instancia por slot `Weapons\Shotgun_Standard` (0), `Pistol_Standard` (1), `MeleeWeapon` (2), `Cattleprod` (3), `Flamethrower_Standard` (4), `Boltgun_Standard` (5) y `Equipment\Motion_Tracker` (6), con los parámetros `starting_ammo` y `give_holstered`. El disparo se resuelve en las entidades del composite (`AIMED_WEAPON`, `BulletChamber`, `WEAPON_Effect`…). No se siguió.

## Para el mod: `AIMP_SYNC_THROW` (`mod/src/throw.c`)

- Hooks en `FUN_007b5050` y `FUN_007b5190`: cada llamada del jugador local sale como `AIMP_EVT_THROW` (112 bytes: operación, matriz, velocidad, `a3`, `time_left` y plantilla). El protocolo pasa a la **v2** (eventos de 112 bytes de datos): hay que actualizar las dos máquinas.
- El otro llama a la misma función por el trampolín, así no vuelve a mandarse. Antes verifica que esté en el mismo nivel, con pool y jugador. La matriz y la velocidad van en buffers alineados a 16, y la plantilla en una tabla estática propia.
- Los detours solo encolan (con sección crítica): el evento de animación podría correr fuera del thread principal. El log, la red y los ecos se procesan en `throw_frame`, en el thread principal, después de `ModeManager::Run`.
- Test: `mod/test/test_throw.c`, con stubs que tienen el prólogo SEH y los `ret` reales.

## Preguntas abiertas

1. ¿Crear el composite desde el thread principal, después de `ModeManager::Run`, es seguro? El juego lo hace dentro del update de animación del personaje. Se ve en la corrida con `AIMP_THROW_ECHO`.
2. ¿El composite usa al jugador local para algo, como dueño del daño o luz en la mano? Si es así, la copia remota quedaría atribuida al jugador de esa máquina.
3. `FUN_0070f310` (aviso a la IA del lanzamiento): ¿hace falta repetirlo en la otra máquina para que el Alien reaccione igual?
4. ¿Qué es el `a3` = 15.0 de `FUN_007b5050`?
5. Disparo de las armas de fuego: falta ubicar el momento del disparo dentro del composite del arma.
