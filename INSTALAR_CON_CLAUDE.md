# Instalar el mod con Claude Code (procedimiento y chequeos)

Para una sesión de Claude Code a la que le piden **instalar el mod para jugar** en una máquina (Windows o Linux), no para desarrollarlo. La guía para personas es [`INSTALACION.md`](INSTALACION.md); acá está el procedimiento paso a paso con los chequeos y qué hacer si algo falla.

**No sigas el flujo de desarrollo del repo** (`work/PROGRESS.md`, `prompt.md`, `work/NOTES.md`): es para la sesión que programa el mod. Tu tarea es este documento.

## Reglas

- Hablá en el idioma del usuario (el repo está en español rioplatense).
- **Pedí confirmación antes de:** instalar software, copiar o borrar archivos en la carpeta del juego, abrir el juego, y **cualquier cambio de seguridad de Windows** (Smart App Control, Defender, exclusiones, firewall). Smart App Control, una vez apagado, no se puede volver a prender sin reinstalar Windows: explicáselo y que decida el usuario.
- En la carpeta del juego solo se agregan `winmm.dll` y `aimp.cfg`. No modifiques, muevas ni borres archivos del juego.
- No edites el código del mod (`mod/`). Si algo no anda, juntá evidencia (salidas de comandos, log) y dejala en el informe.
- **No hagas commit ni push** salvo que el usuario sea el dueño del repo y lo pida. El informe queda local.
- Separá hechos (salidas de comandos, líneas del log) de hipótesis.
- **Antes de la primera conexión como "join", avisale al usuario:** el mod reemplaza sus saves de Alien: Isolation por los del host (la partida que se juega es la del host). Los suyos quedan en `%LOCALAPPDATA%\AI_Multiplayer\save_backup\`, pero el cambio también llega a su Steam Cloud. Si le importa su partida, que copie antes la carpeta de saves.

## 0. Datos que necesitás del usuario

- ¿Hostea o se une? El host es el dueño de la partida que se juega.
- Si se une: la **IP de Tailscale del host** (`100.x.y.z`) y que ya esté en su red (invitación aceptada o máquina compartida). El puerto es 47811.
- Si va a hostear: su IP de Tailscale, para pasársela al otro (`tailscale ip -4`).

## 1. Windows

Todo desde la raíz del repo, en PowerShell. El script deja todo en `instalacion-informe.md` (raíz del repo, ignorado por git).

```powershell
# 1. Chequeos (no toca nada): Windows, carpeta del juego, hash de AI.exe, KnownDLLs, Tailscale, ping al host
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action check -Peer 100.x.y.z:47811
```

| Qué | Esperado | Si no |
|-----|----------|-------|
| Juego encontrado | carpeta de Steam con `AI.exe` | ¿Está instalado por Steam? Versiones de otras tiendas no están soportadas |
| SHA-256 de `AI.exe` | `DDE66521…19A90` | Parar. Steam → Propiedades → Archivos instalados → Verificar integridad. Si sigue distinto, no es la build soportada |
| `winmm` en KnownDLLs | no está | Parar: en esa máquina el mod no puede cargar. Anotarlo |
| `winmm.dll` ajeno en la carpeta | no hay | No pisarlo: preguntar qué es |
| Ping al host (si se une) | responde | Revisar Tailscale y la IP |

```powershell
# 2. Instalar (con confirmación del usuario): copia mod\dist\winmm.dll (verifica su hash) y escribe aimp.cfg
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Preset join -Peer 100.x.y.z:47811
#   o para el host:  -Action install -Preset host
#   claves extra o distintas:  -Extra AIMP_X=1,AIMP_Y=0
```

```powershell
# 3. El usuario abre el juego desde Steam (sin opciones de lanzamiento) hasta el menú principal. Con el juego abierto:
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action verify
```

`verify` mira qué `winmm.dll` cargó el juego (lo hace desde un PowerShell de 32 bits, porque `AI.exe` es de 32 bits y el de 64 no ve sus módulos) y el log de esta corrida:

- **"El mod cargó"** → seguí con la sección 3 (conexión).
- **"El mod NO cargó"** → seguí con la sección 2 (diagnóstico). El script ya muestra KnownDLLs, el estado de Smart App Control y los bloqueos de Integridad de código (estos solo como administrador).

Para deshacer: `-Action revert`.

## 2. Windows: el mod no carga (el juego usa la `winmm.dll` del sistema)

Visto en Windows 11 25H2 (compilación 26200): con los archivos bien copiados y los hashes correctos, `AI.exe` carga `C:\WINDOWS\SYSTEM32\WINMM.dll` y no la del juego. No aparece `aimp.log` (ni en `%LOCALAPPDATA%\AI_Multiplayer` ni en `%TEMP%`). En Windows 10 la misma DLL carga bien. Hay dos causas posibles; averiguá cuál antes de tocar nada.

**A. Smart App Control / Integridad de código bloquea la DLL** (no tiene firma digital).
- `verify` como administrador muestra los eventos de `Microsoft-Windows-CodeIntegrity/Operational` que nombran `winmm.dll` (IDs 3033, 3034, 3077, 3089…) y el estado de Smart App Control (`HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy`, `VerifiedAndReputablePolicyState`: 0 apagado, 1 activado, 2 evaluación).
- También: Seguridad de Windows → Historial de protección, y avisos del tipo "Se bloqueó parte de esta aplicación".
- **Qué hacer:** explicale las opciones al usuario y que decida: apagar Smart App Control (sin vuelta atrás) o, si es Defender, una exclusión para la carpeta del juego. No lo hagas por tu cuenta.

**B. La `winmm` del sistema ya estaba cargada** cuando el juego la pidió: alguna DLL del sistema que se carga antes depende de ella (las dependencias de las DLL "conocidas" se cargan siempre desde System32), y el cargador ni mira la carpeta del juego.
- Prueba definitiva: **Process Monitor** de Sysinternals (pedir permiso para bajarlo: https://learn.microsoft.com/sysinternals/downloads/procmon). Filtros `Process Name is AI.exe` y `Path contains winmm`; abrir el juego hasta el menú; detener la captura.
  - Si **nunca** aparece un acceso a `…\Alien Isolation\winmm.dll`: es B.
  - Si aparece esa ruta y después la de `SysWOW64`: la buscó y la descartó (apunta a A).
- **Qué hacer:** del lado de esa máquina no hay arreglo. Hace falta una variante del mod que entre por otra DLL que el juego carga desde su carpeta (candidata: `xinput1_3.dll`). Dejá la evidencia en el informe para quien desarrolla el mod.

## 3. Linux (Proton)

```sh
work/scripts/runtime_setup.sh status                               # detecta el juego y el prefix; hash de AI.exe
work/scripts/runtime_setup.sh install --preset join --peer 100.x.y.z:47811   # o --preset host
# el usuario abre el juego desde Steam (no hace falta opción de lanzamiento: el override va en el registro del prefix)
work/scripts/runtime_setup.sh verify                               # ¿cargó? ¿hay red?
work/scripts/runtime_setup.sh revert                               # deshacer
```

- El script encuentra la biblioteca de Steam solo (`libraryfolders.vdf`); si no, `AIMP_GAME_DIR`, `AIMP_PFX`.
- Se niega a correr con el juego abierto. Necesita que el juego se haya abierto al menos una vez con Proton (para que exista el prefix).
- Si el usuario tiene opciones de lanzamiento propias con `WINEDLLOVERRIDES`, que incluyan `winmm=n,b`.

## 4. Conexión (las dos plataformas)

Con el mod cargado:
1. Tailscale: las dos máquinas conectadas; el que se une hace ping a la IP del host.
2. Con los dos juegos abiertos, el log (`%LOCALAPPDATA%\AI_Multiplayer\aimp.log`) muestra líneas `NET … recibidos N, perdidos 0` con `recibidos` subiendo. `verify` lo resume ("hay datos del otro jugador").
3. El que se une, en el menú principal → **Play Game**, espera el texto de abajo: "Coop: copiando la partida del host... no entres todavía" → "Coop: partida del host copiada. Ya podés entrar (Continue Game)". Recién ahí, **Continue Game**. El host también entra con Continue Game.
4. Si Steam pregunta por Steam Cloud: archivos **locales**.
5. En el log tiene que aparecer `SESSION: misma partida`. Si dice `PARTIDAS DISTINTAS`, el que se une entró antes de que terminara la copia: salir al menú y volver a entrar con Continue.

Si no hay datos del otro: IP y puerto en `aimp.cfg`, Tailscale conectado en los dos, firewall de Windows (permitir a `AI.exe` en redes privadas si lo pregunta), y que los dos tengan **la misma** `winmm.dll` (`mod/dist/SHA256SUMS`).

## 5. Informe

Al terminar, el informe (`instalacion-informe.md` en Windows; en Linux, la salida de `status` y `verify`) más el `aimp.log` son lo que hay que pasarle a quien desarrolla el mod si algo no anduvo. Resumí: sistema y versión, qué chequeos pasaron, qué se instaló, resultado de `verify`, y para un fallo, la causa con su evidencia (sección 2).
