# Instalación y configuración

Cómo instalar el mod coop en cada máquina, conectar a los dos jugadores y qué hace cada parámetro de `aimp.cfg`.

> El mod es **experimental**. Probalo con una partida que no te importe perder. Si estás en una run sin morir, no la uses para probar (en Linux, `work/scripts/mode.sh` separa tu partida de las de prueba: ver más abajo).

## Instalación rápida

1. **Las dos máquinas en [Tailscale](https://tailscale.com)**, en la misma red (o el host le comparte su máquina al otro). Anotá la IP `100.x.y.z` del host.
2. **Copiar al lado de `AI.exe`** (Steam → Alien: Isolation → Administrar → Ver archivos locales):
   - `mod/dist/winmm.dll`;
   - `mod/dist/aimp-host.cfg` (el host) o `mod/dist/aimp-join.cfg` (el que se une), **renombrado a `aimp.cfg`**. En el de join, cambiar `100.x.y.z` por la IP del host.
3. **Solo en Linux (Proton):** opción de lanzamiento `WINEDLLOVERRIDES="winmm=n,b" %command%`.
4. Abrir el juego desde Steam. Tiene que aparecer `%LOCALAPPDATA%\AI_Multiplayer\aimp.log`. Si no aparece, el mod no cargó: ver [Problemas comunes](#11-problemas-comunes).
5. El que se une, en **Play Game**, espera "Coop: partida del host copiada. Ya podés entrar" y entra con **Continue Game**. El host también entra con Continue Game.

Con los scripts es un comando, y verifican todo:
- Windows: `work\windows\aimp_check.ps1 -Action install -Preset join -Peer 100.x.y.z:47811` y, con el juego abierto, `-Action verify` ([sección 3](#3-instalar-en-windows)).
- Linux: `work/scripts/runtime_setup.sh install --preset join --peer 100.x.y.z:47811` y `verify` ([sección 4](#4-instalar-en-linux-proton)).

**¿Lo instala Claude Code?** Que siga [`INSTALAR_CON_CLAUDE.md`](INSTALAR_CON_CLAUDE.md): procedimiento con todos los chequeos y el diagnóstico si el mod no carga.

---

Índice:
1. [Antes de empezar](#1-antes-de-empezar)
2. [Conseguir la DLL](#2-conseguir-la-dll)
3. [Instalar en Windows](#3-instalar-en-windows)
4. [Instalar en Linux (Proton)](#4-instalar-en-linux-proton)
5. [El archivo `aimp.cfg`](#5-el-archivo-aimpcfg)
6. [Red: conectar a los dos jugadores](#6-red-conectar-a-los-dos-jugadores)
7. [Configuraciones recomendadas](#7-configuraciones-recomendadas)
8. [Cómo arrancar una partida de a dos](#8-cómo-arrancar-una-partida-de-a-dos)
9. [Referencia de parámetros](#9-referencia-de-parámetros)
10. [Desinstalar](#10-desinstalar)
11. [Problemas comunes](#11-problemas-comunes)

---

## 1. Antes de empezar

**Los dos jugadores necesitan exactamente lo mismo:**

| Qué | Valor | Cómo verificarlo |
|-----|-------|------------------|
| Ejecutable del juego | `AI.exe` de Steam, buildid 4940658 | SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Datos del juego | los de Steam, sin mods que cambien los scripts de los niveles | En Steam: Propiedades → Archivos instalados → Verificar integridad. Un `COMMANDS.PAK` modificado en un solo lado desincroniza los scripts |
| Versión del mod | la misma `winmm.dll` en las dos | `mod/dist/SHA256SUMS` |

- **Sistemas probados:** Linux con Proton 9.0 y Windows 10 nativo. Las dos combinaciones hablan entre sí.
- **Con otra build del juego** el mod no hace nada: queda como proxy de `winmm` y el juego anda normal (el log dice qué no coincidió).
- **Una sola cuenta de Steam en dos PCs:** Steam no deja correr el juego en dos máquinas a la vez con la misma cuenta. Una de las dos tiene que estar en **modo sin conexión**.
- **Otros mods:** ReShade (`dxgi.dll`), `d3d11.dll` y MovieFix (`xinput1_3.dll`) no molestan. El mod usa `winmm.dll`, así que no puede convivir con otro mod que también la use.
- **Red:** [Tailscale](https://tailscale.com) (gratis) en las dos máquinas. Evita abrir puertos en el router.

## 2. Conseguir la DLL

### Opción A: la compilada (recomendada)

Está en el repo: `mod/dist/winmm.dll`, con su hash en `mod/dist/SHA256SUMS`. Verificala antes de copiarla:

```sh
# Linux
cd mod/dist && sha256sum -c SHA256SUMS
```
```powershell
# Windows (PowerShell)
Get-FileHash mod\dist\winmm.dll -Algorithm SHA256   # comparar con mod\dist\SHA256SUMS
```

### Opción B: compilarla

Requisitos: `mingw-w64-gcc` (i686), CMake ≥ 3.20 y make. Para los tests, `wine`.

```sh
cmake -S mod -B mod/build -DCMAKE_TOOLCHAIN_FILE=$PWD/mod/cmake/toolchain-i686-w64-mingw32.cmake
cmake --build mod/build
mod/test/run_tests.sh          # opcional: tests con wine, sin el juego
```

Sale en `mod/build/winmm.dll`. Ojo: cada compilación da un hash distinto (lleva la fecha y el commit). Para jugar de a dos, los dos tienen que usar **el mismo archivo**: lo más simple es que el que compila le pase su DLL al otro, o que los dos usen la de `mod/dist`.

## 3. Instalar en Windows

### Con el script (recomendado)

Desde la raíz del repo, en PowerShell:

```powershell
# 1. Chequeos: hash del juego, KnownDLLs, DLLs que ya hay en la carpeta
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action check

# 2. Instalar con la configuración recomendada (el que se une; el host: -Preset host)
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action install -Preset join -Peer 100.x.y.z:47811

# 3. Abrir el juego desde Steam hasta el menú principal y, con el juego abierto:
powershell -ExecutionPolicy Bypass -File work\windows\aimp_check.ps1 -Action verify
```

`verify` dice si Windows cargó la `winmm.dll` del mod, muestra las líneas importantes del log (build reconocida, red, copia del save) y, si el mod no cargó, el diagnóstico (KnownDLLs, Smart App Control, bloqueos de Integridad de código).

Parámetros de `aimp_check.ps1`:

| Parámetro | Valores | Qué hace |
|-----------|---------|----------|
| `-Action` | `check`, `install`, `verify`, `collect`, `revert` | `check`: chequeos sin tocar nada (con `-Peer`, además hace ping al host). `install`: copia `mod\dist\winmm.dll` (verifica su hash) y escribe `aimp.cfg` en la carpeta del juego. `verify`: con el juego abierto, si cargó y por qué no. `collect`: copia `aimp.log` a `work\windows\logs\`. `revert`: saca `winmm.dll` y `aimp.cfg` |
| `-Preset` | `host`, `join` | la configuración recomendada (`mod\dist\aimp-host.cfg` o `aimp-join.cfg`); `join` necesita `-Peer` |
| `-Peer` | `ip:puerto` | la IP de Tailscale del host y el puerto 47811 (`AIMP_NET_PEER`) |
| `-Extra` | `CLAVE=valor,CLAVE=valor` | claves extra o que reemplazan a las del preset ([referencia](#9-referencia-de-parámetros)) |
| `-Net`, `-Ghost` | | sin preset: `AIMP_NET` y `AIMP_GHOST` sueltos |
| `-Report` | archivo | dónde se escribe el informe (por defecto `instalacion-informe.md` en la raíz del repo) |
| `-Tag` | texto | nombre para los logs de `collect` |

`-ExecutionPolicy Bypass` vale solo para ese proceso: no cambia la configuración del sistema. El script encuentra la carpeta del juego solo (registro de Steam y `libraryfolders.vdf`) y deja todo en un informe (`instalacion-informe.md`).

### A mano

1. Verificar que `winmm` **no** esté en KnownDLLs (si está, Windows ignora la copia del juego y el mod no carga):
   ```
   reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"
   ```
   En Windows 10 no suele estar.
2. Copiar `winmm.dll` al lado de `AI.exe` (por ejemplo, `C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\`).
3. Crear `aimp.cfg` (ver [sección 5](#5-el-archivo-aimpcfg)), en la carpeta del juego o en `%LOCALAPPDATA%\AI_Multiplayer\`.
4. No hace falta ninguna opción de lanzamiento en Steam.

## 4. Instalar en Linux (Proton)

En Proton, Wine usa por defecto su propia `winmm` e ignora la de la carpeta del juego. Hay que decirle que use la "nativa" (la del mod).

### Con el script

```sh
work/scripts/runtime_setup.sh status                                       # qué hay instalado (y dónde está el juego)
work/scripts/runtime_setup.sh install --preset host                        # el host
work/scripts/runtime_setup.sh install --preset join --peer 100.x.y.z:47811  # el que se une
work/scripts/runtime_setup.sh verify                                       # con el juego abierto: ¿cargó? ¿hay red?
work/scripts/runtime_setup.sh revert                                       # lo saca todo
```

- `install` copia la DLL (`mod/build/winmm.dll` si la compilaste; si no, `mod/dist/winmm.dll`), agrega al registro del prefix el override `winmm=native,builtin` solo para `AI.exe` (con backup de `user.reg`), así que **no hace falta opción de lanzamiento**, y escribe `aimp.cfg` en el prefix.
- `--preset` usa `mod/dist/aimp-<preset>.cfg`; `--cfg "A=1;B=2"` agrega claves o reemplaza las del preset (sin `--preset`, es toda la config). Sin ninguna de las dos, borra el `aimp.cfg`.
- Se niega a correr con el juego abierto (Wine reescribe el registro al cerrar). Necesita que el juego se haya abierto una vez con Proton.
- Encuentra el juego y el prefix con las bibliotecas de Steam (`libraryfolders.vdf`). Para forzar rutas: `AIMP_GAME_DIR`, `AIMP_PFX`, `AIMP_DLL`.

**Si estás en una run que no querés arriesgar** (la de esta máquina): `work/scripts/mode.sh test [save] [--cfg "..."]` guarda tu partida en `saves/current.tar.gz`, carga un save de prueba (`saves/`) e instala el mod; `work/scripts/mode.sh play` restaura tu partida y saca el mod.

### A mano

1. Copiar `winmm.dll` al lado de `AI.exe` (`steamapps/common/Alien Isolation/`).
2. En Steam: Alien: Isolation → Propiedades → General → Opciones de lanzamiento:
   ```
   WINEDLLOVERRIDES="winmm=n,b" %command%
   ```
   Ahí también se pueden poner parámetros sueltos, por ejemplo `AIMP_NET=host WINEDLLOVERRIDES="winmm=n,b" %command%` (el entorno tiene prioridad sobre `aimp.cfg`).
3. Crear `aimp.cfg` en `steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer/` (o al lado de `AI.exe`).

## 5. El archivo `aimp.cfg`

Un parámetro por línea, `CLAVE=valor`. Las líneas que empiezan con `#` son comentarios. Solo se leen claves que empiezan con `AIMP_`.

```ini
# ejemplo: el que se une
AIMP_NET=join
AIMP_NET_PEER=100.126.161.126:47811
AIMP_GHOST=3
AIMP_GHOST_WALK=1
```

**Dónde se busca, y quién gana:**
1. las variables de entorno del proceso (opciones de lanzamiento en Proton);
2. `%LOCALAPPDATA%\AI_Multiplayer\aimp.cfg`;
3. `aimp.cfg` al lado de `winmm.dll` (la carpeta del juego).

Si una clave aparece en más de un lugar, vale la del primero de la lista. El log dice al principio qué archivos leyó y qué claves aplicó (`config: … aplicadas: …`).

Ubicaciones de `%LOCALAPPDATA%\AI_Multiplayer\`:
- Windows: `C:\Users\<usuario>\AppData\Local\AI_Multiplayer\`
- Proton: `steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer/`

En esa carpeta también quedan `aimp.log` (y `aimp.log.prev`, el de la vez anterior), `opciones.txt` (lo que elegís en el menú "Coop") y `save_backup/` (tus saves antes de que el mod los reemplace por los del host).

## 6. Red: conectar a los dos jugadores

1. **Tailscale en las dos máquinas**, con la misma cuenta (o compartiendo la máquina del host con el otro). Cada una recibe una IP `100.x.y.z`; se ve con `tailscale ip -4` o en la app.
2. **Elegir quién es el host.** La partida que se juega es la del host: el que se une recibe sus saves. Conviene que el host sea el que tiene la partida más avanzada.
3. **Host:** `AIMP_NET=host`. Escucha en UDP `47811` (`AIMP_NET_PORT`). Por Tailscale no hace falta abrir puertos; si Windows pregunta por el firewall, permitir el acceso.
4. **El que se une:** `AIMP_NET=join` y `AIMP_NET_PEER=<IP de Tailscale del host>:47811`.
5. El orden de arranque no importa: cada uno espera al otro. En el log aparece `NET … recibidos N, perdidos 0` cuando se ven.

En una LAN sin Tailscale también anda (IP local del host), siempre que el puerto UDP esté permitido.

## 7. Configuraciones recomendadas

Lo que hoy está probado o listo para la próxima prueba de a dos. Están en el repo como `mod/dist/aimp-host.cfg` y `mod/dist/aimp-join.cfg` (los presets de los scripts, con las trazas incluidas); copiar el que corresponda como `aimp.cfg`.

**Host:**
```ini
AIMP_NET=host
AIMP_GHOST=3
AIMP_GHOST_WALK=1
AIMP_SYNC_USE=1
AIMP_SYNC_THROW=1
AIMP_SYNC_PICKUP=1
AIMP_SYNC_PINS=1
AIMP_SYNC_INV=1
AIMP_COOP_DOWNED=1
AIMP_COOP_SAVE_RULE=1
```

**El que se une:**
```ini
AIMP_NET=join
AIMP_NET_PEER=<IP de Tailscale del host>:47811
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
```

Qué da esto: el avatar del otro (un NPC que copia su posición y animación), botones y objetos compartidos, inventario y munición comunes, los disparadores de los scripts los decide el host, al caer no hay game over mientras el otro siga en pie, solo guarda el host (con el compañero cerca) y el que se une juega la partida del host.

**Mod instalado pero apagado** (el juego anda normal): `AIMP_GHOST=0` sin `AIMP_NET`, o `AIMP_NOHOOK=1`.

**Para reportar un problema**, sumar trazas (ver [9.9](#99-trazas-para-investigar)), por ejemplo `AIMP_TRACE_USE=1`, `AIMP_TRACE_PINS=1`, `AIMP_TRACE_PINS_MAX=300000`, `AIMP_TRACE_SAVE=1`, `AIMP_TRACE_DMG=1`.

## 8. Cómo arrancar una partida de a dos

1. Los dos abren el juego (el que tenga Steam sin conexión, también).
2. **El que se une, en el menú principal**, espera el cartel en "Play Game": "Coop: copiando la partida del host... no entres todavía" → "partida del host copiada. Ya podés entrar". La primera vez tarda unos segundos (solo copia los saves que cambiaron). "Continue Game" también lo dice.
3. **Dificultad del coop** (opcional): en el menú principal, "Coop" (antes "Extras") → "Dificultad del coop: Normal / Difícil / Pesadilla". Apretar cambia el valor. La que vale es la del host.
4. Los dos entran con **Continue Game**. Unos 8 s después de aparecer, cada uno ve al otro como un civil del nivel (si el nivel no tiene civiles, no hay avatar).
5. **Si uno cae** (`AIMP_COOP_DOWNED=1`): queda con 1 de salud, invencible, y los enemigos lo ignoran. Si lo remató un enemigo, ve la animación y a los 4 s recupera el control. Lo revive el otro guardando en una terminal o pasando un checkpoint. **Si caen los dos**, game over normal: "Load Current Save" en los dos carga el último checkpoint del host.
6. **Guardar:** con `AIMP_COOP_SAVE_RULE=1` guarda el host y solo con el compañero a menos de 6 m (o caído). Si el que se une intenta guardar, el juego le muestra un cartel con el motivo. Con el host caído, el que se une sí puede guardar y su save pasa al host.

## 9. Referencia de parámetros

Valores: `0`/`1` = apagado/prendido. Entre paréntesis, el valor por defecto. Lo marcado **(experimental)** funciona en pruebas de una máquina pero falta probarlo de a dos.

### 9.1 Red

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_NET` | vacío | `host` (escucha) o `join` (se conecta a `AIMP_NET_PEER`). Vacío: sin red |
| `AIMP_NET_PEER` | — | con `join`: `ip:puerto` del host |
| `AIMP_NET_PORT` | 47811 | puerto UDP del host |
| `AIMP_NET_HZ` | 20 | paquetes de estado por segundo |

### 9.2 Avatar del otro jugador

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_GHOST` | 0 | **3** = un NPC civil del nivel hace de avatar del otro jugador. 0 = sin avatar. 1 y 2 son experimentos que hacen caer el juego: no usar |
| `AIMP_GHOST_WALK` | 0 | 1 = el avatar camina y corre con su animación (recomendado) |
| `AIMP_GHOST_PUPPET` | 1 | con `WALK`: el avatar se pone cada frame en la posición exacta del otro ("títere") |
| `AIMP_GHOST_DELAY_MS` | 8000 | cuánto esperar después de aparecer para elegir el NPC |
| `AIMP_GHOST_NPC_TYPE` | 6 | tipo de NPC que se usa (6 = civil) |
| `AIMP_GHOST_PREFER_FEMALE` | 1 | si hay una civil, se la prefiere (Amanda es mujer) |
| `AIMP_GHOST_INVINCIBLE` | 1 | el avatar no recibe daño (matarlo daba game over por civiles) |
| `AIMP_GHOST_CROUCH` | 4 | cómo se muestra el agachado del otro: 4 = pose de agachado del juego; 0 = no se agacha |
| `AIMP_GHOST_DOWNED_HIDE` | 0 | 1 = el avatar del compañero caído desaparece (y vuelve al revivir). 0 = queda acurrucado |
| `AIMP_GHOST_WALK_FAR_CM` | 500 | a partir de qué distancia corre |
| `AIMP_GHOST_WALK_TP_CM` | 1200 | si queda más lejos que esto, se lo reubica de golpe |
| `AIMP_GHOST_WALK_FACE` | 1 | con el otro quieto, el avatar mira hacia donde mira el otro |

Hay más ajustes finos del avatar (`AIMP_GHOST_WALK_*`, `AIMP_GHOST_CROUCH_*`) documentados en `mod/README.md` y en el código (`mod/src/ghost.c`); para jugar no hace falta tocarlos.

### 9.3 Mundo compartido

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_SYNC_USE` | 0 | 1 = lo que uno "usa" (botones, palancas, cajas) pasa también en la otra máquina, hecho por el avatar |
| `AIMP_SYNC_USE_SKIP` | `3e3ec35d,1e38b898,087f97b5` | objetos que no se replican: lockers, terminal de guardado y bocas de conducto (son acciones de cada jugador) |
| `AIMP_SYNC_USE_ACTOR` | 1 | quién hace el "usar" remoto: 1 = el avatar; 0 = tu propio personaje (se teletransporta) |
| `AIMP_SYNC_PENDING_MS` | 60000 | cuánto se guarda un "usar" del otro que todavía no se puede aplicar (otro nivel, sin avatar) |
| `AIMP_SYNC_THROW` | 0 | 1 = bengalas, noisemakers, molotovs y minas aparecen en la otra máquina con la misma trayectoria y terminan donde cayó el original |
| `AIMP_SYNC_PICKUP` | 0 | 1 = un ítem que levanta uno desaparece en la otra máquina **(experimental)** |
| `AIMP_SYNC_INV` | 0 | 1 = inventario compartido: lo que uno agarra lo reciben los dos y lo que uno gasta se descuenta a los dos (munición incluida) |
| `AIMP_SYNC_PINS` | 0 | 1 = los disparadores de zona de los scripts (diálogos, eventos) los decide el host; el que se une aplica los del host y bloquea los suyos **(experimental)** |
| `AIMP_SYNC_PINS_LIST` | zona | qué disparadores se sincronizan (reglas `tipo:pin` en hex). El defecto cubre las zonas de los scripts |
| `AIMP_SYNC_WEAPONS` | 0 | "armas de a una" (cada arma tiene dueño). **Se dejó de usar** (decisión del 2026-11-17): no ponerla |

### 9.4 Partida y guardado

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_SAVE_SYNC` | 0 | en el que se une: en el menú principal copia los saves del host (con backup de los propios en `save_backup/`), así "Continue" carga la partida del host. Mientras juegan, cuando el host guarda, el que se une recibe ese save |
| `AIMP_SAVE_SYNC_MEM` | 1 | con `SAVE_SYNC`: el game over del que se une carga el checkpoint del host |
| `AIMP_SAVE_PUSH` | 1 | con el host caído, si el que se une guarda en una terminal, ese save pasa al host |
| `AIMP_COOP_SAVE_RULE` | 0 | 1 = "guardado exigente": guarda solo el host, y solo con el compañero cerca o caído. El que se une ve un cartel con el motivo |
| `AIMP_COOP_SAVE_NEAR_CM` | 600 | distancia máxima al compañero para poder guardar |

### 9.5 Caer en coop

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_COOP_DOWNED` | 0 | 1 = el jugador no muere: queda "caído" con 1 de salud e invencible, y el otro sigue. Si caen los dos, game over normal. Lo revive el compañero guardando en una terminal o pasando un checkpoint |
| `AIMP_COOP_DEATH_ACT_MS` | 4000 | si a uno lo remata un enemigo (el agarre del Alien), cuánto se deja correr la animación antes de devolverle el control y la imagen. -1 = nunca (la pantalla queda negra) |
| `AIMP_COOP_DOWNED_FACTION` | 3 | facción que toma el caído (3 = la del Alien): así el Alien lo ignora y se va. -1 = no tocarla (el enemigo que remató queda quieto hasta que reviva) |
| `AIMP_COOP_ALIEN_WITHDRAW` | 0 | 1 = el Alien se retira a los conductos cuando se lo suelta **(experimental)** |

### 9.6 Dificultad y menú del mod

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_MOD_MENU` | 1 | "Extras" pasa a ser "Coop" con la dificultad del coop. 0 = devuelve "Extras" y los créditos |
| `AIMP_COOP_DIFF` | 1 | aplica la dificultad del coop elegida en el menú (la del host). 0 = nunca toca la dificultad |
| `AIMP_UI_TEXT` | 1 | textos del mod en la interfaz: carteles del coop en el menú ("esperando al host", "partida copiada"…), etiquetas del menú "Coop" y avisos dentro del juego. 0 = los apaga todos |

### 9.7 Log y seguridad

| Clave | Defecto | Qué hace |
|-------|---------|----------|
| `AIMP_LOG` | `%LOCALAPPDATA%\AI_Multiplayer\aimp.log` | dónde se escribe el log |
| `AIMP_NOHOOK` | 0 | 1 = modo seguro: proxy puro, sin tocar el juego |
| `AIMP_CRASHLOG` | 1 | si el juego se cae, el log registra la excepción, la dirección y la pila |
| `AIMP_STATUS_MS` | 1000 | cada cuánto se escribe el resumen (`ST`) |
| `AIMP_SAMPLE_MS` | 250 | cada cuánto se toma una foto del estado (`EV`) |
| `AIMP_MARK_KEY` | `0x77` (F8) | tecla que deja una `MARCA` en el log, para señalar un momento al reportar |
| `AIMP_DUMP_KEY` | `0x78` (F9) | tecla de volcado completo de estructuras |

### 9.8 Herramientas de prueba (no usar para jugar)

| Clave | Qué hace |
|-------|----------|
| `AIMP_MEMSRV_FILE` | lector de memoria por archivo (`work/scripts/memsrv.py`) |
| `AIMP_MEMSRV_TEST` | habilita comandos que escriben (matar, teletransportar, guardar, llamar funciones…). Solo con saves de prueba |
| `AIMP_AUTOPAD`, `AIMP_AUTOPAD_FILE` | mando virtual con un guion (corridas automáticas) |
| `AIMP_PINS_FILE` | reinyectar disparadores de los scripts a mano |
| `AIMP_COOP_REVIVE_FILE` | revivir al cambiar ese archivo |
| `AIMP_THROW_ECHO` | repetir cada lanzamiento propio (probar los arrojables sin otra máquina) |
| `AIMP_CENSUS` | lista todos los personajes del nivel |

### 9.9 Trazas para investigar

Solo agregan líneas al log; no cambian el juego. Útiles para reportar un problema.

| Clave | Qué registra |
|-------|--------------|
| `AIMP_TRACE_USE` | cada "usar" y su objetivo |
| `AIMP_TRACE_PINS` (+ `AIMP_TRACE_PINS_MAX`, 20000) | los disparadores de los scripts (pines). Para una prueba larga, subir el tope (300000) |
| `AIMP_TRACE_THROW` | lo que se tira o coloca |
| `AIMP_TRACE_PICKUP` | ítems que se levantan |
| `AIMP_TRACE_INV` | cambios de inventario |
| `AIMP_TRACE_SAVE` | cada guardado del juego |
| `AIMP_TRACE_DMG` | golpes al jugador y muertes (quién y desde dónde) |
| `AIMP_TRACE_DIFF` | la carga de las dificultades |
| `AIMP_TRACE_UI`, `AIMP_TRACE_UI_CALLBACKS` | pantallas de la interfaz que se abren y llamadas de la UI |
| `AIMP_TRACE_CTRL`, `AIMP_TRACE_CMD`, `AIMP_TRACE_ANIM`, `AIMP_TRACE_SPAWN`, `AIMP_TRACE_QUEUES` | control del jugador, comandos de NPCs, animaciones, creación de personajes y colas internas (investigación) |

## 10. Desinstalar

- **Windows:** `aimp_check.ps1 -Action revert`, o borrar `winmm.dll` (y `aimp.cfg`) de la carpeta del juego.
- **Linux:** `work/scripts/runtime_setup.sh revert`, o borrar `winmm.dll` y sacar `WINEDLLOVERRIDES` de las opciones de lanzamiento.
- La carpeta `%LOCALAPPDATA%\AI_Multiplayer\` (logs, opciones, backups de saves) se puede borrar a mano. Si el mod reemplazó tus saves por los del host, los tuyos están en `save_backup/`.

## 11. Problemas comunes

| Síntoma | Causa probable | Qué hacer |
|---------|----------------|-----------|
| No aparece `aimp.log` | el mod no cargó | Windows: `winmm.dll` al lado de `AI.exe` (no en una subcarpeta) y que `winmm` no esté en KnownDLLs; `aimp_check.ps1 -Action verify` con el juego abierto dice por qué. Proton: falta `WINEDLLOVERRIDES="winmm=n,b"` (o `runtime_setup.sh install`) |
| Windows 11: archivos bien, pero sin log | el juego usa la `winmm.dll` del sistema: la bloqueó Smart App Control / Integridad de código, o Windows ya la tenía cargada | ver la sección 2 de [`INSTALAR_CON_CLAUDE.md`](INSTALAR_CON_CLAUDE.md). Visto en Windows 11 25H2; en Windows 10 anda |
| El log dice `build NO reconocida` | otra versión del juego | verificar el SHA-256 de `AI.exe` (sección 1); el mod queda inactivo a propósito |
| El juego avisa por Steam Cloud al abrir | los saves cambiaron (los copió el mod) | elegir los archivos **locales** |
| `NET … sin datos del otro jugador` | no se ven por red | revisar IP y puerto en `AIMP_NET_PEER`, que Tailscale esté conectado en los dos, y el firewall de Windows |
| El log dice `SESSION: PARTIDAS DISTINTAS` | cada uno cargó otra partida | el que se une necesita `AIMP_SAVE_SYNC=1` y entrar con "Continue" después del cartel "partida del host copiada" |
| No aparece el avatar | el nivel no tiene civiles, o falta `AIMP_GHOST=3` | es una limitación conocida (`work/CONTINGENCIAS.md`) |
| Una puerta, un robot o una terminal reaccionan solo en una máquina | esas interacciones todavía no se sincronizan | conocido; apretar F8 justo antes y después ayuda a investigarlo |
| Protocolo distinto / no se entienden | DLLs distintas | las dos tienen que tener el mismo hash (`mod/dist/SHA256SUMS`) |

Para reportar algo, mandar el `aimp.log` de las dos máquinas (en Windows, `aimp_check.ps1 -Action collect`) y, si se puede, la hora aproximada o una marca F8 del momento.
