# Instrucciones para Claude Code en la laptop Windows (pruebas del mod AI_Multiplayer)

Sos una sesión de Claude Code en la **laptop de pruebas del usuario (Windows 10)**. Otra sesión, en la máquina Linux del usuario (`papito`), desarrolla un mod coop para Alien: Isolation. Tu trabajo es **verificar que el mod funciona en Windows nativo y registrar los resultados** para que la sesión Linux los lea.

**Esta sesión no sigue el flujo general del repo.** `CLAUDE.md` manda a leer `work/PROGRESS.md` y seguir su "Siguiente paso": acá no. Tu única tarea es este documento. Podés leer `mod/README.md` para entender el mod.

## Reglas

- Respondé en español rioplatense.
- **Pedile confirmación al usuario** antes de:
  - instalar software;
  - copiar archivos a la carpeta del juego;
  - ejecutar el juego;
  - cambiar configuración del sistema o del firewall.
- En la carpeta del juego **solo** se agregan `winmm.dll` y `aimp.cfg`. No modifiques, muevas ni borres ningún archivo del juego. Para deshacer: `-Action revert` del script.
- **No subas archivos del juego al repo.** Lo único que se sube es `work/windows/report.md` y `work/windows/logs/*.log`.
- Separá hechos (salidas de comandos, líneas del log) de hipótesis.
- No edites el código del mod (`mod/`). Si algo falla, documentalo con evidencia: lo arregla la sesión Linux.

## Qué es el mod (resumen)

- `winmm.dll` es una DLL proxy: Windows la carga en lugar de la `winmm` del sistema porque está al lado de `AI.exe`. Reenvía todas las llamadas a la real.
- Si la build del juego es la esperada, engancha el loop principal y escribe un log en `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`:
  - estado del juego;
  - posición del jugador;
  - red del engine;
  - etc.
- Opcionalmente (`aimp.cfg`) manda y recibe la posición del jugador por UDP: es la "fase 3".
- Build esperada: `AI.exe` con SHA-256 `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90` (Steam buildid 4940658).
- La DLL ya viene compilada en `mod/dist/winmm.dll`; su SHA-256 está en `mod/dist/SHA256SUMS`. No hace falta compilar nada.

## 0. Preparación

1. Repo: si no está clonado, `git clone https://github.com/Juani4312/AI_Multiplayer.git`. Es privado: el usuario se loguea en GitHub. Si ya está, `git pull`.
   - Sin git: el usuario puede bajar el zip desde GitHub. En ese caso, el reporte se devuelve por Taildrop a `papito` en lugar de por git.
2. Todo se corre desde la raíz del repo, con PowerShell:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action <acción> [...]
   ```
   `Bypass` vale solo para ese proceso: no cambia la política del sistema.
3. Cada acción agrega su resultado al final de `work/windows/report.md`.
4. **Si Steam instaló el juego copiándolo desde `papito`** (transferencia por red local): `papito` tiene mods. Son ReShade, MovieFix, una versión vieja del `winmm.dll` del mod y dos `COMMANDS.PAK` modificados. Antes del paso 1, preguntale al usuario si ya hizo "Verificar integridad de los archivos del juego" en la laptop con Steam cerrado en `papito`. Si no, que lo haga y avise.

## 1. Chequeo del entorno (no modifica nada)

```
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action check
```

Registra Windows, la carpeta del juego, el SHA-256 de `AI.exe` y el buildid, las DLLs que hay en la carpeta del juego (ReShade, etc.), **KnownDLLs**, Tailscale (IP propia y la de `papito`) y los perfiles del firewall.

Revisá el resultado con el usuario:

| Qué | Esperado | Si no |
|-----|----------|-------|
| SHA-256 de `AI.exe` | `DDE66521…19A90` | Parar: otra build. Que el usuario verifique la integridad en Steam. Anotarlo en el reporte. |
| winmm en KnownDLLs | **no** está | Parar: el proxy no puede cargar en esta máquina. Anotarlo (dato clave para la sesión Linux). |
| Ya hay un `winmm.dll` ajeno en el juego | no hay | No pisarlo. Anotarlo y preguntar. |
| Tailscale | instalado y logueado; `papito` con IP 100.x.y.z | Solo hace falta para el paso 3. |
| `dxgi.dll`, `d3d11.dll`, `xinput1_3.dll` en el juego | no hay | Vinieron de `papito` (ReShade y MovieFix). No bloquean. Avisale al usuario y anotalo. |

Anotá también en el reporte el SHA-256 de los dos `COMMANDS.PAK` que en `papito` están modificados. No bloquea: es un dato para la sesión Linux. Corré esto desde la carpeta del juego que dio el chequeo:
```
Get-FileHash -LiteralPath 'DATA\ENV\PRODUCTION\BSP_LV426_PT01\WORLD\COMMANDS.PAK','DATA\ENV\PRODUCTION\TECH_COMMS\WORLD\COMMANDS.PAK' | Format-List
```

| Archivo | SHA-256 en `papito` |
|---------|---------------------|
| `BSP_LV426_PT01\WORLD\COMMANDS.PAK` | `DDA19F70AB8ABE5194FA7A372A680ED8C5AAE6A30842101547DA8F724F48A47F` |
| `TECH_COMMS\WORLD\COMMANDS.PAK` | `42FD34D7FAF7EA9A22B961D7C6A0C12700FA90327CA1F796F445CC958ECBDD3A` |

Con la integridad ya verificada:
- si el hash de la laptop **difiere**, el de la laptop es el vanilla y el de `papito` es el del mod;
- si es **igual**, el de `papito` ya era vanilla.

## 2. Primera corrida: ¿carga el mod en Windows?

1. Con el OK del usuario, instalar **sin red**:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net none
   ```
   Si da "acceso denegado" (carpeta del juego en `Program Files`), pedir al usuario que abra PowerShell como administrador y repetir.
2. El usuario arranca el juego desde Steam, llega al **menú principal**, espera unos 20 s y lo cierra (Salir).
   - Si el juego **no arranca** y aparece un error del estilo "no se encontró el punto de entrada **X** en la biblioteca winmm.dll": copiar el texto exacto. Es un export que le falta al proxy.
   - Si aparece un cartel "AI_Multiplayer (proxy winmm)": copiar el texto (dice qué función faltó).
3. Recolectar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag primera
   ```
4. Qué tiene que aparecer en el log (lo muestra `collect`):

| Línea | Significado |
|-------|-------------|
| `plataforma: Windows 10.0.…` (sin "Wine") | corre en Windows nativo |
| `proxy: winmm real = C:\WINDOWS\SYSTEM32\winmm.dll` o `SysWOW64` | cargó la winmm real |
| `proxy: N exports resueltos, M ausentes …` | anotar N, M y la lista de ausentes |
| `build: … OK` (3 líneas) y `build reconocida` | la build coincide |
| `hook ModeManager::Run: instalado` | el hook está puesto |
| `FASE 1: primer frame dentro de ModeManager::Run` | el mod corre por frame en Windows |
| `ST frames …` y `modo: estado 5 (corriendo) … "Production\Frontend"` | lecturas del estado OK |

5. Opcional pero útil: el usuario carga una partida (Continuar), camina unos 10 s, aprieta **F8** (marca) y sale. Después, `collect -Tag partida`. Tiene que haber líneas `J0 pos=(…)` que cambian y `MARCA 1`.

## 3. Prueba de red con la máquina Linux (fase 3)

Requisitos:
- Tailscale en las dos máquinas, con la misma cuenta;
- el chequeo muestra la IP de `papito`;
- **la sesión Linux tiene el juego abierto en modo host** (UDP 47811). Pedile al usuario que lo coordine con esa sesión y que confirme cuando el juego Linux esté en el menú o en una partida.

1. Instalar en modo join, con la IP de `papito` que dio el chequeo:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer <IP_de_papito>:47811
   ```
2. El usuario arranca el juego en la laptop. Si Windows Firewall pregunta por `AI.exe`, **permitir en redes privadas** (con el OK del usuario).
3. En las dos máquinas, cargar una partida y moverse unos 30 s. En la laptop, F8 al empezar a moverse y F8 al terminar. Salir del juego en la laptop.
4. Recolectar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag red
   ```
5. Qué tiene que aparecer:
   - `NET: join hacia <IP>:47811, 20 paquetes/s`;
   - `NET: primer estado del otro jugador: nivel …`;
   - una vez por segundo, `NET join <IP>:47811: enviados ~20, recibidos ~20, perdidos 0 … remoto: nivel N con jugador pos=(…)`, con una posición que cambia si el jugador Linux se mueve;
   - si `recibidos` queda en 0 y `enviados` en ~20, los paquetes de vuelta no llegan: firewall, IP equivocada, o el juego Linux no está en host.

## 4. Volver al estado original

Si el usuario quiere jugar sin el mod:
```
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action revert
```

## 5. Reporte y entrega

1. Al final de `work/windows/report.md`, agregá un resumen corto:
   - qué funcionó y qué no;
   - los números clave (exports ausentes, fps, paquetes enviados, recibidos y perdidos);
   - cualquier error, con su texto exacto.
2. Commit y push solo de `work/windows/report.md` y `work/windows/logs/`:
   ```
   git add work/windows/report.md work/windows/logs
   git commit -m "Pruebas en Windows: <resumen corto>"
   git push
   ```
   Si el push falla por la primera vez (pasó en Linux), reintentar una vez.
3. Avisale al usuario que ya está subido, para que la sesión Linux haga `git pull`.

## 6. Avatar del otro jugador por red (fase 4, experimental)

En cada máquina, un civil del nivel (un NPC "INNOCENT") pasa a seguir la posición del jugador de la otra. Probado en Linux sin red el 2026-11-05: el civil sigue al jugador local sin errores.

Requisitos:
- los del paso 3;
- `git pull`, porque hay una DLL nueva en `mod/dist`;
- que la sesión Linux tenga su juego en host con `AIMP_GHOST=3`. Pedile al usuario que lo coordine.

1. Instalar en modo join con el avatar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer <IP_de_papito>:47811 -Ghost 3
   ```
2. Steam en modo sin conexión en la laptop (ver "Problemas conocidos"). Arrancar el juego y cargar la partida. El juego Linux también tiene que estar en una partida, en el mismo nivel.
3. Unos 8 s después de entrar, un civil tendría que ponerse donde está el jugador de la otra máquina y seguirlo. El usuario aprieta F8 cuando lo ve, F8 otra vez después de mover al jugador de la otra máquina, y sale.
4. Recolectar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag avatar
   ```
5. Qué tiene que aparecer:
   - `GHOST: NPC elegido para mover: NPCs[…]`;
   - `GHOST: primer teletransporte … [red]`;
   - líneas `GHOST: personaje en (…), objetivo (…) [red]` en las que el objetivo cambia;
   - ninguna `EXCEPCIÓN`.
   Si aparece una `EXCEPCIÓN`, copiarla con las 3 líneas siguientes (registros y stack): es lo que necesita la sesión Linux.

## 7. "Usar" compartido (fase 5, experimental)

Cuando el jugador de una máquina usa algo (botón, caja, locker, terminal, ítem), el mod se lo manda a la otra. La otra lo aplica como si lo hubiera usado su propio jugador. Sin probar en el juego: puede no tener efecto o hacer caer el juego. El registro de excepciones deja la causa en el log.

Requisitos: los del paso 6, con `git pull` (hay DLL nueva), y la sesión Linux en host con `AIMP_GHOST=3` y `AIMP_SYNC_USE=1`.

1. Instalar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer <IP_de_papito>:47811 -Ghost 3 -Extra AIMP_SYNC_USE=1
   ```
2. Steam sin conexión, arrancar el juego y cargar la partida en las dos máquinas, en el mismo nivel.
3. El usuario aprieta F8 en la máquina donde va a actuar y usa algo cerca: un locker, la terminal de guardado o una caja. Después mira la otra pantalla: ¿pasó lo mismo ahí? Después hace lo mismo al revés.
4. Recolectar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag usar
   ```
5. Qué buscar en el log:
   - del lado que usó: `SYNC uso local … (jugador local)` y `SYNC   mandado al otro jugador`;
   - del lado que recibe: `SYNC: aplicando uso remoto (evento N): objetivo …`, y después `SYNC handler A del uso` y `SYNC handler de sesión del uso` con el mismo objetivo;
   - cualquier `EXCEPCIÓN`, copiada con sus 3 líneas;
   - lo que vio el usuario en cada pantalla. Es lo más importante: sin eso no se sabe si tuvo efecto.

## 8. Avatar que camina y arrojables por red (fases 4b y 5b, experimental; 2026-11-15)

El civil que hace de avatar ya no se teletransporta: **camina y corre** hasta donde está el jugador de la otra máquina y, cuando el otro está quieto, gira hacia donde mira. Además, las bengalas, noisemakers, molotovs y minas que tira un jugador aparecen en la otra máquina con la misma trayectoria. Las dos cosas se probaron solo en una máquina.

Requisitos: los del paso 6, con `git pull` (hay DLL nueva en `mod/dist`, protocolo v2: con la DLL vieja no se entienden) y la sesión Linux en host. En `papito` el jugador lo mueve la sesión Linux con un mando virtual; el usuario juega en la laptop. **Las dos partidas tienen que estar en el mismo nivel** (`papito` está en `HAB_ShoppingCentre`): si la laptop carga otro nivel, anotarlo y avisar.

1. Instalar:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer <IP_de_papito>:47811 -Ghost 3 -Extra AIMP_GHOST_WALK=1,AIMP_SYNC_USE=1,AIMP_SYNC_THROW=1
   ```
   Verificar antes que el SHA-256 de `mod\dist\winmm.dll` coincida con `mod\dist\SHA256SUMS`.
2. Steam sin conexión, arrancar el juego y cargar la partida. Avisarle al usuario que le diga a la sesión Linux "la laptop está en la partida".
3. Qué mirar en la pantalla de la laptop (el usuario aprieta **F8** en cada momento interesante):
   - un civil aparece cerca y **camina** (no salta) hacia donde está el jugador de `papito`, que se mueve solo;
   - si el de `papito` se aleja más de 5 m, el civil **corre**;
   - cuando el de `papito` se queda quieto, el civil se para y gira hacia donde mira el otro;
   - el usuario **tira una bengala** (y si tiene, un noisemaker) en la laptop: la sesión Linux mira si aparece en `papito`.
4. Recolectar y subir:
   ```
   powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action collect -Tag caminata
   ```
5. Qué buscar en el log:
   - `GHOST: caminata: MOVE_TOWARDS ...` y líneas `GHOST: caminata (caminando): ... [red] a N m`, con N chico la mayor parte del tiempo;
   - `move_type 3` cuando corre;
   - `gira de ... hacia donde mira el otro`;
   - `THROW` del lado que tiró (mandado) y, si `papito` tiró algo, la inyección recibida;
   - cualquier `EXCEPCIÓN`, con sus 3 líneas.
   - Y lo que vio el usuario: ¿camina con animación? ¿se traba, atraviesa paredes o cae del piso?

### 8b. Dónde cae el arrojable (2026-11-15, DLL `907e36b5…`)

Misma instalación que el paso 8 (con `git pull`). Novedad: cuando la bengala del que tira se asienta, el mod le avisa al otro dónde quedó; si la réplica cayó a más de 15 cm, se borra y aparece quieta en ese punto. Se ve el vuelo igual que antes y, al final, un "salto" corto de la réplica.

Qué mirar: tirar bengalas desde cada máquina y comparar dónde quedan en las dos pantallas. En el log buscar `THROW fijo`:
- del lado que tiró: `el propio se asentó en (...) ... mandado` (si sale `no se pudo leer la posición`, no se manda nada y la réplica queda donde cayó);
- del lado que recibe: `la réplica cayó a N m ... se borra y se pone en (...)` o `se deja`.

### 8c. Prueba conjunta: pasos, agachado y arrojables (2026-11-16, DLL `3d086a38…`)

Misma instalación que el paso 8, con `git pull` (DLL nueva; el protocolo sigue en v2). Novedades:
- **Paso del avatar:** entre caminar y correr ahora hay "caminar rápido" (`FAST_WALK`). El jugador camina a 2,46 m/s y antes el civil animaba un `WALK` de ~1,2 m/s ("se desliza"); ahora usa `WALK` hasta 1,8 m/s, `FAST_WALK` hasta 3 m/s y `RUN` por encima.
- **Agachado:** cuando el otro se agacha, el mod llama a la función del juego que agacha a un NPC. En las pruebas locales el civil **siguió parado** (seguramente no tiene animación de agachado); queda para confirmar a ojo.
- **Arrojables:** más diagnóstico en el log para ubicar la posición del objeto.

Qué mirar (F8 en cada momento interesante):
1. El otro camina normal: ¿las piernas del civil van al ritmo o se desliza? Lo mismo corriendo.
2. El otro se agacha, quieto y caminando: ¿el civil se agacha? (esperable: no).
3. Bengalas desde cada máquina, contra el piso y contra paredes: ¿la réplica termina donde la original?

En el log: `títere: camina rápido`, `velocidad: avatar … el otro …`, `FUN_007cae20 (agacharse)`, `THROW fijo` (incluidas las líneas `diag`).

### 8d. Ítems compartidos y disparadores del host (2026-11-16, DLL `072c93b7…`)

Misma instalación que el paso 8, con `git pull`, sumando a `-Extra`: `AIMP_SYNC_PICKUP=1,AIMP_TRACE_PICKUP=1,AIMP_SYNC_PINS=1,AIMP_TRACE_PINS=1` (lo mismo en `papito`). Novedades:
- **Ítems:** cuando un jugador levanta un ítem, en la otra máquina tendría que desaparecer sin que nadie lo reciba. Hasta ahora cada uno tenía su copia.
- **Zonas de los scripts (fase 6):** el host decide las zonas (entrar o salir de áreas que arrancan diálogos o eventos); el cliente aplica las del host y no dispara las suyas. Ya funcionó por red en una sola máquina.

- **Arrojables:** el que tira ahora lee la posición real de su bengala del cuerpo físico (Havok) y le avisa al otro dónde quedó; la réplica salta a ese punto. Con `AIMP_SYNC_THROW=1` (como en 8b).

Qué mirar (F8 en cada momento):
0. Bengalas desde cada máquina, contra paredes y el piso: ¿la réplica termina donde la original? En el log del que tira: `hkpRigidBody … camino constructor` y `el propio se asentó en … mandado`.
1. Uno levanta un ítem (balas, piezas, una bengala del piso): ¿desaparece en la pantalla del otro? ¿El otro lo puede levantar igual?
2. El host entra en una zona con diálogo o música: ¿arranca también en el cliente? Y si el cliente entra solo, ¿no arranca?

En el log: `PICKUP recogido` (con `pid` e `ids`) del lado que levantó, y `PICKUP remoto: … acá … (la entidad local con el mismo pid | los ids del otro)` del otro lado. `PINSYNC al cliente`, `PINSYNC aplicado`, `PINSYNC bloqueado`.

### 8e. Agachado, inventario compartido, muerte en coop y armas de a una (2026-11-17, DLL `3f54f468…`)

Misma instalación que el paso 8, con `git pull` (DLL nueva de `mod/dist`), sumando a `-Extra` (y lo mismo en `papito`): `AIMP_SYNC_INV=1,AIMP_COOP_DOWNED=1,AIMP_SYNC_WEAPONS=1,AIMP_SYNC_USE=1`. El agachado nuevo ya viene activado por defecto.

**Importante: las dos máquinas tienen que cargar la MISMA partida.** El mod ahora lo verifica: en el log tiene que aparecer `SESSION: misma partida`; si dice `SESSION: PARTIDAS DISTINTAS`, cargar el mismo save. Copiar a la laptop un save de prueba de `saves/` (no la partida real del usuario), como dice `saves/README.md`, "En Windows".

Novedades:
- **Agachado:** cuando el otro se agacha, su avatar adopta una pose agachada (con una mano en el piso) y se desliza en ella mientras avanza.
- **Inventario compartido:** todo lo que uno levanta, gasta o fabrica le pasa también al otro (munición, botiquines, bengalas, componentes, armas).
- **Muerte en coop:** si uno recibe un golpe mortal, queda "caído" (salud 1, invencible, sin game over). Su avatar en la otra máquina queda acurrucado. Si el otro usa una terminal de guardado, el caído revive a su lado. Si caen los dos, game over normal.
- **Armas de a una:** cada arma tiene dueño (el último que la sacó). Cerca (menos de 4 m) cualquiera puede sacarla si el otro no la tiene en la mano; lejos (más de 5 m), solo el dueño. Si no está disponible, el juego la guarda enseguida.

Qué mirar (F8 en cada momento):
1. Agacharse quieto y agachado caminando: ¿el avatar se ve agachado del otro lado?
2. Levantar munición o un botiquín, usar un botiquín, fabricar algo: ¿cambia igual en el inventario de los dos? En el log: `INV: local: … ` de un lado e `INV: remoto: … x -> y` del otro.
3. Sacar la pistola en una máquina; en la otra, intentar sacarla: cerca, ¿la toma (y en la primera la sigue teniendo en la mano?); lejos, ¿se guarda sola? En el log: `WEAPONS: saca …`, `WEAPONS: … no está disponible`, `WEAPONS: el otro sacó …`.
4. Muerte (solo con un save de prueba, nunca con la partida real): dejarse matar por un enemigo en una máquina. ¿Queda caído en vez de game over? ¿El avatar se acurruca en la otra? Usar una terminal de guardado con el vivo, o pasar un checkpoint (con `AIMP_SYNC_PINS=1`): ¿el caído revive a su lado? En el log: `DEATH: el jugador quedó caído`, `DEATH: revivido por el compañero`.

### 8f. Copia del save del host (2026-11-17, DLL `f6b9beca…`)

La DLL nueva sirve también para la prueba 8e (es compatible). Con `git pull`, instalar `mod/dist/winmm.dll` como siempre.

**Qué hace:** con `AIMP_SAVE_SYNC=1`, la laptop (la que se une) trae los saves del host (`papito`) cuando el juego está en la pantalla de título o en el menú principal. Después, "Continue Game" carga la partida del host: ya no hace falta copiar saves a mano. Antes de escribir, el mod guarda una copia de los saves propios de la laptop en `%LOCALAPPDATA%\AI_Multiplayer\save_backup\<fecha>\`. Además, cuando el host guarda (terminal o checkpoint), la laptop recibe ese save; y si su propio juego lo pisa (al morir), el mod lo repone.

**Configuración:** en la laptop, sumar `AIMP_SAVE_SYNC=1` a `-Extra`. En `papito`, nada nuevo (el host siempre sirve sus saves, solo los lee). `papito` tiene que tener cargado un save de prueba (`work/scripts/mode.sh test`), nunca la partida real.

Pasos:
1. Abrir el juego en `papito` y llegar al menú. Después, abrir el juego en la laptop **sin tocar nada en la pantalla de título**.
2. En el log de la laptop, esperar:
   - `SAVESYNC: pidiendo el save del host`;
   - `SAVESYNC: manifiesto del host: N saves; distintos: …`;
   - `SAVESYNC: backup de … saves propios en …`;
   - `SAVESYNC: save del host copiado`.
   Por Tailscale, la copia completa (unos 3,5 MB) debería tardar pocos segundos.
3. En las dos: Play Game → Alien: Isolation → Continue Game. En el log: `SESSION: misma partida`.
4. Opcional, guardar juntos: el host usa una terminal de guardado. En la laptop: `SAVESYNC: el host guardó (aviso …)` y `save del host copiado`.
5. Opcional, game over (solo con un save de prueba): con `AIMP_COOP_DOWNED=1` en las dos, que caigan los dos. En las dos: "Load Current Save". Las dos tienen que aparecer en el último checkpoint del host. En el log de la laptop: `SAVESYNC: checkpoint en memoria (…) reemplazado por el del host`. Después de la recarga, el log tiene que decir `DEATH: carga nueva: se reinicia el estado de caídos`.

**Volver a los saves propios de la laptop:** con el juego cerrado, copiar los archivos de la carpeta de `save_backup` a `C:\Program Files (x86)\Steam\userdata\<cuenta>\214490\remote\`.

Reportar: los tiempos de la copia (líneas `SAVESYNC` con su hora) y si "Continue" cargó el mismo lugar que el host.

### 8g. Segunda prueba de a dos: load y game over (2026-11-17, DLL `361846c0…`)

Arreglos después de la primera prueba conjunta (8e/8f):
- **Agachado:** al pararse caminando, el avatar quedaba agachado. Ahora siempre recibe la animación de parado.
- **Windows:** el mod no reconocía funciones cuyo código tiene direcciones absolutas, porque Windows carga el juego en otra dirección (ASLR). Así fallaba "guardar el arma" (`FUN_007c3980 … no coincide`). Ahora el mod lo tiene en cuenta: esa línea no tendría que aparecer más.
- **Saves:** el host avisa "guardé" solo si cambió el contenido. Antes avisaba unas 17 veces en 23 minutos sin cambios.
- **Menú del mod:** en el menú principal, "Extras" ahora se llama **"Coop"**; adentro, "Dificultad del coop" cambia entre Normal, Difícil y Pesadilla al apretarlo (todavía no cambia nada en el juego; se guarda en `%LOCALAPPDATA%\AI_Multiplayer\opciones.txt`). Los créditos quedan inaccesibles con el mod (`AIMP_MOD_MENU=0` los devuelve).
- **Mensajes en el menú:** en Play Game, la línea de abajo muestra el estado del coop ("esperando al host… no entres todavía", "partida del host copiada. Ya podés entrar"). "Continue Game" dice "esperá al host", "esperá, copiando" o "partida del host". No hace falta mirar el log para saber cuándo entrar.
- **Conductos:** entrar a un conducto ya no se replica. Antes el avatar del otro "entraba" y la escotilla quedaba ocupada: en la laptop no aparecía la "E".
- **Armas de a una: se deja de usar.** Los dos tienen todas las armas y la munición de reserva es compartida (inventario compartido). Instalar **sin** `AIMP_SYNC_WEAPONS`.

Configuración en la laptop (`git pull` y reinstalar):
```
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Net join -Peer 100.126.161.126:47811 -Ghost 3 -Extra AIMP_GHOST_WALK=1,AIMP_SYNC_USE=1,AIMP_SYNC_THROW=1,AIMP_SYNC_PICKUP=1,AIMP_SYNC_PINS=1,AIMP_TRACE_PINS=1,AIMP_TRACE_PINS_MAX=300000,AIMP_TRACE_USE=1,AIMP_SYNC_INV=1,AIMP_COOP_DOWNED=1,AIMP_COOP_SAVE_RULE=1,AIMP_SAVE_SYNC=1
```
**Guardado exigente** (`AIMP_COOP_SAVE_RULE=1`, nuevo): en coop guarda solo el host, y solo si el otro está a menos de 6 m o caído. En la laptop, la terminal de guardado no se abre (en el log: `SYNC   terminal de guardado bloqueada: en coop guarda el host`). Probar: el host intenta guardar lejos de la laptop (no se abre) y cerca (se abre).

Para investigar el robot: **apretar F8 justo antes de activarlo** (deja una `MARCA` en el log) y otra vez cuando termine de moverse. Las dos máquinas tienen la traza de pines con tope alto.
Qué probar: el paso 5 de la 8f (caen los dos, game over, "Load Current Save" en las dos) y volver a mirar el agachado. En el log de la laptop, buscar `SAVESYNC: checkpoint en memoria`, `DEATH: los dos jugadores están caídos` y `DEATH: carga nueva`.

## Problemas conocidos

- **No aparece `aimp.log`:** el proxy no cargó. Revisar KnownDLLs, que `winmm.dll` esté al lado de `AI.exe` (no en una subcarpeta) y que el juego haya llegado a arrancar.
- **`build NO reconocida`:** otra versión de `AI.exe`. El mod queda como proxy puro: el juego anda normal pero no hay datos.
- **El juego crashea al arrancar con el mod:** probar con `AIMP_NOHOOK=1` en `aimp.cfg` (solo proxy, sin hooks). Si con eso anda, el problema es el hook; si no, es el proxy. Anotar cuál.
- **ReShade u otros mods en la carpeta:** no molestan. Igual anotarlos (los lista el chequeo).
- **Steam no deja abrir el juego en las dos máquinas con la misma cuenta.** Poné Steam en modo sin conexión en la laptop y dejá `papito` conectado. Así se hizo la prueba de red del 2026-11-05.
