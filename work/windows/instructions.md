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

## Problemas conocidos

- **No aparece `aimp.log`:** el proxy no cargó. Revisar KnownDLLs, que `winmm.dll` esté al lado de `AI.exe` (no en una subcarpeta) y que el juego haya llegado a arrancar.
- **`build NO reconocida`:** otra versión de `AI.exe`. El mod queda como proxy puro: el juego anda normal pero no hay datos.
- **El juego crashea al arrancar con el mod:** probar con `AIMP_NOHOOK=1` en `aimp.cfg` (solo proxy, sin hooks). Si con eso anda, el problema es el hook; si no, es el proxy. Anotar cuál.
- **ReShade u otros mods en la carpeta:** no molestan. Igual anotarlos (los lista el chequeo).
- **Steam no deja abrir el juego en las dos máquinas con la misma cuenta.** Poné Steam en modo sin conexión en la laptop y dejá `papito` conectado. Así se hizo la prueba de red del 2026-11-05.
