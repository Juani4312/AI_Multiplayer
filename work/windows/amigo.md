# Instrucciones para Claude Code en la PC del amigo (Windows 11)

Sos una sesión de Claude Code en la **PC de un amigo del dueño del repo** (Juani). Juani desarrolla un mod coop para Alien: Isolation en su máquina Linux (`papito`), y quiere jugar con tu usuario: `papito` es el **host** y esta PC **se une** por Tailscale. Tu tarea: dejar el mod andando en esta PC y verificar que se conecta.

**No sigas el flujo general del repo** (`work/PROGRESS.md`, `prompt.md`): es el de la sesión de desarrollo. Tampoco `work/windows/instructions.md`: es para otra máquina (la laptop de Juani). Tu tarea es este documento. Para entender el mod podés leer `INSTALACION.md` y `mod/README.md`.

## Reglas

- Respondé en español rioplatense.
- **Pedile confirmación al usuario** antes de: instalar software, copiar o borrar archivos en la carpeta del juego, abrir el juego, y **sobre todo antes de cambiar cualquier configuración de seguridad de Windows** (Smart App Control, Defender, exclusiones). Smart App Control, una vez apagado, no se puede volver a prender sin reinstalar Windows: es decisión del usuario, no tuya.
- En la carpeta del juego solo se agregan `winmm.dll` y `aimp.cfg` (o la DLL que indique este documento). No toques archivos del juego.
- **No hagas commit ni push**: el repo es de Juani. Tu informe va en `work/windows/amigo-report.md` (local); el usuario se lo pasa a Juani (archivo o captura).
- No edites el código del mod (`mod/`). Si algo falla, documentalo con evidencia: lo arregla la sesión de Linux.
- Separá hechos (salidas de comandos) de hipótesis.
- **Los saves:** al unirse, el mod reemplaza los saves de Alien: Isolation de esta PC por los del host (la partida que se juega es la de Juani). Los propios quedan en `%LOCALAPPDATA%\AI_Multiplayer\save_backup\`, pero también se sincronizan con Steam Cloud. Avisale al usuario antes de la primera conexión.

## Estado al 2026-11-18 18:40 (lo que ya se vio)

- Windows 11 Home 25H2, compilación 26200.9974.
- En la carpeta del juego (`D:\SteamLibrary\steamapps\common\Alien Isolation`) ya están `winmm.dll` (SHA-256 `B4DE99842C77E6EAB3EA2FFAD78A562ED2A528A7B0326A7C0B2950FB0979EB78`, la de `mod/dist`) y `aimp.cfg`. `AI.exe` tiene el SHA-256 esperado (`DDE66521…19A90`). La DLL no tiene la marca "bajada de internet".
- `winmm` **no** está en la lista de KnownDLLs del registro.
- **Problema:** con el juego abierto, el proceso `AI.exe` (32 bits) tiene cargada `C:\WINDOWS\SYSTEM32\WINMM.dll` (la del sistema; en 32 bits es `SysWOW64`), no la del mod. Por eso no aparece `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` ni `%TEMP%\aimp.log` y no hay conexión. Ojo: para ver módulos de `AI.exe` hay que usar el PowerShell de 32 bits (`C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe`); el de 64 bits no los muestra.
- En Windows 10 (la laptop de Juani) la misma DLL carga bien.

## 1. Diagnóstico: ¿por qué Windows no usa la `winmm.dll` del juego?

Dos hipótesis. Hay que saber cuál es antes de tocar nada.

**A. La bloquea la Integridad de código / Smart App Control** (la DLL no tiene firma digital).

```powershell
# Estado de Smart App Control: 0 = apagado, 1 = activado, 2 = evaluación
(Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction SilentlyContinue).VerifiedAndReputablePolicyState

# Eventos de bloqueo (PowerShell como administrador; abrir el juego antes, así hay eventos recientes)
Get-WinEvent -LogName 'Microsoft-Windows-CodeIntegrity/Operational' -MaxEvents 100 |
  Where-Object { $_.Message -match 'winmm|Alien Isolation' } |
  Select-Object TimeCreated, Id, Message | Format-List
```

Si hay eventos que nombran `winmm.dll` de la carpeta del juego (IDs 3033, 3034, 3077, 3089…), es esta. También puede haber un aviso de Windows tipo "Se bloqueó parte de esta aplicación". Mirá además Seguridad de Windows → Historial de protección (Defender).

**B. Windows ya tenía cargada la `winmm` del sistema** cuando el juego la pidió (alguna DLL del sistema que se carga antes depende de ella, y las dependencias de las DLL "conocidas" siempre salen de System32). En ese caso el cargador ni siquiera mira la carpeta del juego.

La prueba definitiva es **Process Monitor** (Sysinternals, de Microsoft; pedir permiso para bajarlo: https://learn.microsoft.com/sysinternals/downloads/procmon):
- filtros: `Process Name is AI.exe` y `Path contains winmm`;
- abrir el juego hasta el menú y detener la captura;
- si **no** aparece ningún acceso a `D:\...\Alien Isolation\winmm.dll`, es la B (nunca la buscó ahí);
- si aparece un `CreateFile`/`Load Image` de esa ruta y después la de `SysWOW64`, la buscó y la descartó (apunta a la A).

Guardá en el informe las salidas de los comandos y lo que mostró ProcMon (captura o exportación CSV filtrada).

## 2. Qué hacer según el resultado

- **A (bloqueo por firma):** no cambies nada de seguridad por tu cuenta. Explicale al usuario las opciones y que Juani decida con él:
  - apagar Smart App Control (no se puede volver a prender sin reinstalar Windows);
  - si es Defender y no Smart App Control, una exclusión para la carpeta del juego.
- **B (winmm precargada):** no hay arreglo del lado de esta PC. La sesión de Linux va a preparar una variante del mod que entra por otra DLL que el juego carga desde su carpeta (candidata: `xinput1_3.dll`, la del mando). Cuando esté, va a figurar en este documento con sus pasos: el usuario hace `git pull` y seguís desde acá.

## 3. Cuando el mod cargue

Señal: aparece `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`, y empieza con `AI_Multiplayer 0.2.0 …`, `DLL: D:\…\Alien Isolation\…` y `config: … aplicadas: AIMP_NET=join …`.

1. Tailscale: el usuario ya aceptó la invitación de Juani. `ping 100.126.161.126` (es `papito`) tiene que responder.
2. Con Juani hosteando (su juego abierto), en el log tienen que aparecer líneas `NET` con `recibidos` subiendo y `perdidos 0`.
3. En el menú principal → Play Game, el texto de abajo pasa a "Coop: copiando la partida del host... no entres todavía" y después "Coop: partida del host copiada. Ya podés entrar (Continue Game)". Recién ahí, Continue Game.
4. Si Steam pregunta por Steam Cloud, archivos **locales**.

El `aimp.cfg` de esta PC (ya copiado) es:

```ini
AIMP_NET=join
AIMP_NET_PEER=100.126.161.126:47811
AIMP_GHOST=3
AIMP_GHOST_WALK=1
AIMP_SYNC_USE=1
AIMP_SYNC_THROW=1
AIMP_SYNC_PICKUP=1
AIMP_SYNC_PINS=1
AIMP_SYNC_INV=1
AIMP_COOP_DOWNED=1
AIMP_COOP_SAVE_RULE=1
AIMP_SAVE_SYNC=1
AIMP_TRACE_USE=1
AIMP_TRACE_PINS=1
AIMP_TRACE_PINS_MAX=300000
AIMP_TRACE_SAVE=1
AIMP_TRACE_DMG=1
```

## 4. Informe

En `work/windows/amigo-report.md`: versión de Windows, resultado del diagnóstico (A o B, con la evidencia), qué se cambió y qué no, y, si el mod cargó, las primeras 40 líneas del log y las líneas `NET`. El usuario se lo pasa a Juani junto con `aimp.log`.
