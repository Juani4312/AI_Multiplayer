# Historial del proyecto

Lo que se hizo en cada sesión, del más nuevo al más viejo. Antes estaba al final del README; se movió acá el 2026-11-18 para que el README no crezca sin fin.

- Estado actual y siguiente paso: `work/PROGRESS.md`.
- Evidencia de cada corrida: `work/NOTES.md`.
- Log cronológico corto: `work/LOG.md`.

**2026-11-21 (sesión larga, locales con el peer, corridas 149–169):**
- **Herramientas de formatos:** CathodeLib (OpenCAGE, submódulo) con `cathode-dump`: el árbol de scripts de cada nivel con el pid del mod (83–96 % de los pids del juego coinciden), los personajes, la UI y los BML; JPEXS FFDec para leer la interfaz en Flash. Los ids de 8 bytes de objetos y personajes del juego son el `EntityHandle` de CathodeLib: cada "usar" y cada personaje de un log tiene nombre (`use_names.py`).
- **Avisos sin pausa** probados en el juego (no pausan, se van solos); arreglado el texto repetido y los textos largos.
- **Sincronizado por pines de los scripts, en los dos sentidos** (verificado con el peer en una máquina): el **robot barredor** de la prueba de a dos; **las puertas** (el desbloqueo del mecanismo, cualquiera sea la forma de abrirlas: del otro lado quedan desbloqueadas y se abren al acercarse); **las cajas con botín** (el botín se sortea al abrir, de un pool por máquina: el primero que abre la caja la sortea y para el otro queda cerrada).
- **Avatar mujer:** la preferencia por mujer nunca había funcionado (esqueleto `FEMALENPC`); si no hay ninguna cargada, el mod hace aparecer una obrera con un pin limpio de los scripts (por ahora en HAB_ShoppingCentre).
- **Repo:** fuera del repo y del historial (reescrito) las saves, las instrucciones de Claude, `work_dirs.txt` y los scripts personales; README al día con las herramientas de terceros; capturas en `tmp/screenshots/`.
- **Diseño:** caer y revivir (Normal = desangrado y revivir en persona; Difícil = solo terminal; el Alien mata de una) y el menú Coop con "Opciones avanzadas", que se puede armar desde el mod con Scaleform (`CreateObject` ubicado).
- **Siguiente:** la prueba de a dos (instrucciones 8g: robot, puertas, cajas, avatar mujer, avisos, modo caído) y, cuando el usuario lo pida, el submenú de opciones avanzadas.

**2026-11-20, madrugada (local, corrida 148):**
- **El mod también entra por `xinput1_3.dll`:** en la PC del amigo (Windows 11 25H2) el juego carga la `winmm.dll` del sistema y el mod ni arranca. Ahora el mismo mod sale también como `xinput1_3.dll` (la DLL del mando, que el juego carga desde su carpeta y de la que no depende ninguna DLL del sistema). Si están las dos, arranca una sola vez; si hay otra `xinput1_3.dll` (MovieFix), se puede encadenar.
- Verificado con un test bajo wine y en el juego (Proton): carga, hooks y mando por la variante. Falta probarla en Windows 11.
- Instaladores: `aimp_check.ps1 -Proxy xinput1_3` (Windows; `verify` la sugiere si la winmm no cargó) y `runtime_setup.sh --proxy xinput1_3` (Linux). `INSTALAR_CON_CLAUDE.md` la pone como primer paso cuando el mod no carga.

**2026-11-18, tarde (locales, corridas 134–147, save `alien`):**
- **Pantalla negra al caer por un remate (el problema de la prueba de a dos):** cuando un enemigo remata al jugador (el agarre del Alien), con el modo caído la pantalla quedaba negra para siempre. Ahora se ve el remate y, 4 s después, el caído vuelve a ver y a moverse (sigue caído). Causa: el jugador quedaba en la acción de muerte y con un fundido a negro; el mod lo pasa a la locomoción y borra el fundido.
- **El Alien ignora al caído:** mientras dura el caído, el jugador toma la facción del Alien; el Alien se suelta del remate y se va a los conductos, aunque el caído corra. Al revivir se restaura y lo vuelve a cazar. Si los dos caen, game over normal (verificado con el peer).
- **Avisos dentro del juego:** el mod puede mostrar el diálogo "Warning" del juego con un texto propio. La terminal bloqueada por el guardado exigente lo usa para decir por qué. Pausa el juego hasta el OK. Un aviso que no pause queda pendiente (el cartel de recogidas no se dibujó).
- Herramientas: `AIMP_TRACE_DMG` (golpes y muertes), `memsrv` `LOCO`, `ACT`, `WD`, `MSG`, `CALL`. REA necesita el JDK 27 (Ghidra 12.2).
- `mod/dist/winmm.dll` = `b4de9984…`.

**2026-11-18 (locales, corridas 126–133):**
- **Menú "Coop":** en el menú principal, "Extras" pasa a llamarse "Coop". Su botón es **"Dificultad del coop"** (Normal, Difícil o Pesadilla) y cambia al apretarlo. Normal deja la dificultad del juego tal cual; cada escalón suma otro salto como el de Hard a Nightmare: el Alien acecha más cerca, vuelve antes, hay una amenaza más y sale antes de los conductos. La elige el host y se la manda al cliente (`ui.c`, `diff.c`; verificado en memoria).
- **Host caído: guarda el cliente.** Con el host caído, la terminal de guardado del cliente se abre. El host revive a su lado y, unos segundos después, recibe ese save: lo escribe con backup de los suyos y lo pone en su checkpoint en memoria (`savesync` en sentido inverso). Verificado de los dos lados con el peer: el game over del host cargó el save del "cliente". Falta probarlo con una terminal real de a dos (`work/windows/instructions.md` 8g).
- **Guardar desde el mod** (`gamesave.c`): el guardado del juego (`FUN_007a90b0`) escribe el checkpoint que ya está en memoria, no el estado del momento.
- `mod/dist/winmm.dll` era `3ea5509b…`; la actual está más arriba.

**2026-11-17, noche (prueba de a dos, corrida 119, y locales 120–122):**
- **Prueba con dos máquinas:** la copia del save y la partida común andan, y el inventario y la munición se comparten. Arreglado después: el avatar que quedaba agachado, funciones que el mod no reconocía en Windows (ASLR), avisos de guardado vacíos y la escotilla que quedaba ocupada por el avatar. "Armas de a una" se deja de usar: los dos tienen todas las armas y la reserva de munición es común.
- **Cartel en el menú:** en Play Game y en Continue Game el juego muestra el estado del coop ("esperando al host… no entres todavía", "partida del host copiada. Ya podés entrar").
- **Pendiente:** las interacciones del mundo (un robot que abre una compuerta se movió solo en una máquina) y una pantalla negra al quedar caído por un enemigo. Próxima prueba: `work/windows/instructions.md` 8g.
- `mod/dist/winmm.dll` era `fb157b7a…`; la actual está más arriba.

**2026-11-17, tarde (corridas 102–118, locales con el peer):**
- **Copia del save del host** (`AIMP_SAVE_SYNC=1` en el que se une; `mod/src/savesync.c`): en la pantalla de título o el menú, el cliente trae del host los saves que tenga distintos, y "Continue" carga la partida del host (verificado: cargó otro nivel). Usa la misma API de Steam Cloud que el juego y comprime con un LZ propio (24 MB de saves → 3,5 MB; los 22 saves llegaron idénticos en 5,9 s). Guarda un backup de los saves propios antes de pisarlos.
- **Guardar juntos:** cuando el host guarda, el cliente recibe ese save aunque esté jugando. Si su propio juego lo pisa (lo hace al morir), el mod lo repone.
- **Game over en coop:** el game over no recarga el archivo, sino un buffer del checkpoint en memoria. El cliente mantiene ahí el checkpoint del host, así que con los dos caídos, "Load Current Save" recarga el mismo punto en las dos máquinas (verificado, corridas 113–115). Corregido además: después de ese game over el modo caído quedaba apagado.
- **Avatar del caído oculto** (`AIMP_GHOST_DOWNED_HIDE=1`, opcional): el avatar del compañero caído desaparece y vuelve al revivir (verificado).
- **Al revivir o pararse estando quieto, el avatar se para** (`npc_neutral_idle_base`). Antes se quedaba acurrucado.
- **Save con el Alien** (`saves/alien.tar.gz`, misión 6): el Alien aparece en el censo como "BackstageAlien" y, con el jugador quieto, termina estacionado justo encima de él (por los conductos). Base para el Alien compartido.
- `mod/dist/winmm.dll` era `f6b9beca…` (8f); la actual está más arriba.

**2026-11-17, 03:00–04:15 (trabajo autónomo, corridas 93–101, locales con el peer simulado):**
- **Partida del usuario protegida:** es una run sin morir. `work/scripts/mode.sh test` guarda su partida en `saves/current.tar.gz` y carga un save de prueba (`safezone`); `mode.sh play` la restaura y desinstala el mod.
- **Inventario compartido** (`AIMP_SYNC_INV=1`, hallazgo 21): lo que uno levanta, gasta o fabrica le pasa también al otro. Al conectarse y después de cada carga, el host manda su inventario completo y el cliente se iguala.
- **Muerte en coop por red** (`AIMP_COOP_DOWNED=1`, `work/DESIGN_DEATH.md`):
  - el que cae queda "caído" y el avatar del otro lado se acurruca;
  - usar una terminal de guardado revive al compañero a tu lado;
  - si caen los dos, game over normal.
- **Armas de a una** (`AIMP_SYNC_WEAPONS=1`): cada arma tiene un dueño, con las reglas de distancia acordadas. La lectura del arma en mano y los eventos están verificados; el bloqueo falta probarlo con alguien al mando.
- **¿Misma partida?** Al conectar, el log dice `SESSION: misma partida` o `SESSION: PARTIDAS DISTINTAS`.
- Para la próxima prueba con dos máquinas: `work/windows/instructions.md`, sección **8e**. DLL en `mod/dist`.

**2026-11-17, madrugada (corridas 83–92, locales):**
- **El avatar se ve agachado:** cuando el otro se agacha, el civil recibe una pose agachada del juego (`idle_crouch`, con una mano en el piso) mediante el comando `PLAY_ANIMATION`, y se desliza en esa pose mientras el otro avanza. Los humanos no tienen ciclo de caminar agachado. También andan `cower_low_idle` (acurrucado) y `jb_kn_idlebase01` (arrodillado) (`AIMP_GHOST_CROUCH_ANIM`). Hallazgo 20.
- **Avatar mujer:** si en la zona hay una civil (esqueleto `FEMALE`), el mod la elige como avatar. Falta verlo en una zona con mujeres: en la sala del transit hay solo hombres.
- **Muerte en coop (experimental, `AIMP_COOP_DOWNED=1`):** en vez de morir, el jugador queda "caído" (salud 1, invencible), sin game over, usando un flag del propio juego (`Character+0x4253`). Diseño y pendientes en `work/DESIGN_DEATH.md`.
- **Herramientas:** lector de memoria en vivo (`AIMP_MEMSRV_FILE` + `work/scripts/memsrv.py`) y trazas de animaciones y de UI.
- `mod/dist/winmm.dll` = `5473f92e…`. **El mod quedó desinstalado** para que el usuario juegue.

**Siguiente:** prueba con dos máquinas (instructions.md 8d, con el agachado nuevo); para la muerte en coop, el evento de red y revivir en la terminal de guardado; una partida con el Alien.

**2026-11-16, madrugada (trabajo autónomo, corridas 72–79):**
- **Fase 6 por red en una máquina:** el cliente aplica los disparos de zona del host (9 de 9) y bloquea los suyos.
- **Ítems compartidos (`AIMP_SYNC_PICKUP`):** al levantar un ítem, en la otra máquina se saca con la función del juego; se busca por pid. Escrito, falta probarlo de a dos (instructions.md 8d).
- **Censo de personajes (`AIMP_CENSUS`):** en la partida actual no hay Alien. Hay un humano armado, que sirve como avatar (`AIMP_GHOST_NPC_TYPE=4`).
- **Agachado:** descartado por ahora: en el armado rompe la pose. Queda apagado por defecto.
- **Alien compartido:** plan en `work/DESIGN_ALIEN.md`; hace falta una partida con el Alien activo.
- **Arrojables:** se encontró el cuerpo físico (Havok) de cada bengala enganchando su constructor; su posición sigue a la bengala hasta que queda quieta (corridas 80–81). El que tira la usa para avisar dónde quedó. Falta probarlo con dos máquinas.
- Limitaciones: `work/CONTINGENCIAS.md`. DLL para la laptop: `072c93b7…`.

**2026-11-16 (corridas 65–68, locales):**
- **Paso del avatar:** el "se desliza" probablemente venía de que el civil animaba `WALK` (~1,2 m/s) mientras el otro caminaba a 2,46 m/s. Ahora hay tres pasos: `WALK`, `FAST_WALK` (de 1,8 a 3 m/s) y `RUN`.
- **Agacharse:** se encontró la función del juego que agacha a un NPC (`FUN_007cae20`) y la tabla de posturas del personaje. Las banderas quedan bien, pero el civil sigue parado: lo más probable es que no tenga animaciones de agachado.
- **Arrojables:** el composite se encuentra por un camino fijo de punteros, pero no siempre es el vivo; la tasa sigue en ~50–75 %. Falta leer el cuerpo físico.
- **Fase 6 por red (corridas 72–73, una máquina con el peer):** el cliente aplicó los 9 disparos de zona que mandó el host, con los mismos identificadores, y bloqueó los suyos al caminar. Falta la prueba con dos máquinas.
- Limitaciones conocidas en `work/CONTINGENCIAS.md`.
- **Prueba conjunta (corrida 69):** el deslizamiento quedó aceptado (es responsivo), el agachado se aplica pero el civil no tiene la animación, y los arrojables reales no se ubicaron (0 de 5). Corridas 70–71: la bengala no vuela con `ProjectileMotion` sino con la física; lo siguiente sería leer su cuerpo de Havok.

**2026-11-15, noche (corridas 52–64):**
- **Títere con dos máquinas (corrida 52):** "funciona perfecto… responsivo y exacto el movimiento". 0,00 m en las dos máquinas, 20 paquetes/s, 0 excepciones.
- **Agacharse (corrida 53):** el comando `IDLE` con las posturas 0 a 5 no agacha al civil, ni siquiera sin reubicarlo cada frame. Ese camino queda descartado. Pistas: `crouched_or_not`, `ConditionIsCrouching`.
- **Arrojables que caen igual (corridas 54–64):** la parábola ya salía igual (`ProjectileMotion` calcula la posición en función del tiempo); la diferencia aparece en el choque y los rebotes. Ahora el que tira avisa dónde se asentó su objeto (`AIMP_EVT_THROW_REST`). El otro libera su réplica con la función del juego (`FUN_007a10f0`, la de `kill_explosive`) y crea una quieta en ese punto. Se verificó en una máquina con el peer (4 de 4 réplicas reubicadas, 144 fps).
- `mod/dist/winmm.dll` = `907e36b5…` (commit `8624e99`). En papito el mod quedó en pasivo.

**Siguiente:**
1. **Prueba con dos máquinas de los arrojables:** tirar bengalas desde cada una, contra el piso y contra paredes. Ver que la réplica salte al punto del original, y en el log `THROW fijo` de los dos lados (`work/windows/instructions.md`, sección 8b).
2. Animación del títere: el usuario lo vio "deslizarse" en la laptop. Revisar el `MOVE_TOWARDS` que anima las piernas.
3. Agacharse: buscar dónde decide la locomoción del NPC si va agachado (`crouched_or_not`, `ConditionIsCrouching`).
4. Arrojables: un camino determinista hasta el composite, o leer el parámetro de posición por ShortGuid. Hoy se encuentra por búsqueda en ~3 de 4.

**2026-11-15 (sesión larga, corridas 20 a 34):**
- **Arrojables** (hallazgo 17, corrida 20): la bengala, el noisemaker, etc. se reinyectan con la misma trayectoria (`AIMP_SYNC_THROW=1`, protocolo v2). Verificado en una máquina.
- **Avatar que camina y corre** (hallazgo 19, corridas 21 a 34):
  - Cada personaje tiene **dos controladores de comandos**: el de tareas (`Character+0x3820`, donde manda su IA) y el de scripts (`Character+0x3570`). El árbol de comportamiento del civil solo ejecuta los comandos de script en la rama cinemática.
  - El mod le da un `MOVE_TOWARDS` en el controlador de scripts, fuerza la motivación cinemática del avatar (hook de `FUN_006d3630`, `ret 8`) y le actualiza el destino cada 200 ms.
  - **Corre** si queda a más de 5 m. El enum `MOVE` (`SLOW_WALK`, `WALK`, `FAST_WALK`, `RUN`…) salió de una tabla de nombres del propio `AI.exe`.
  - Antes de ubicarlo espera a que termine el `PLAY_ANIMATION` de su IA (si no, se caía del piso). Sin red sigue al jugador a 2 m; por red va a la posición del otro y, quieto, mira hacia donde mira el otro.
  - Lecciones: nunca `SwitchCommand(ctrl, NULL)` (se cae) y verificar siempre el `ret N` en el asm antes de hookear (un `ret 8` declarado como `ret 4` hizo caer el juego).
- **Herramientas:** corridas automáticas con el mando virtual y capturas de la ventana del juego; `aimp_peer.py` en v2 con `--replay` y `--offset`.
- `mod/dist/winmm.dll` está actualizada (commit `85c21ff`). En papito el mod quedó instalado en pasivo (`AIMP_GHOST=0`).

**Siguiente (mañana):**
1. **Prueba con dos máquinas** (papito host + laptop join). La laptop necesita la DLL nueva de `mod/dist/`, porque el protocolo cambió a v2. Config de las dos: `AIMP_GHOST=3`, `AIMP_GHOST_WALK=1`, `AIMP_SYNC_USE=1`, `AIMP_SYNC_THROW=1`, más `AIMP_NET=host` en papito y `AIMP_NET=join` con `AIMP_NET_PEER=<IP de Tailscale de papito>:47811` en la laptop. Ver que cada avatar camine y corra hacia el otro jugador, que gire cuando el otro está quieto, y que las bengalas y los noisemakers aparezcan del otro lado.
2. Si anda, dejar `AIMP_GHOST_WALK=1` por defecto con `AIMP_GHOST=3`.
3. Después, en estático: el disparo de las armas de fuego (hallazgo 17, "El equipo del jugador"), y más adelante el Alien.
4. Pendientes menores: `AIMP_SYNC_PINS` con dos máquinas; una corrida de control para el arranque corto.

**2026-11-15, noche (corridas 46–51), el títere:** el avatar ya no decide nada por su cuenta. Cada frame se pone en la posición exacta del otro jugador (0,00 m de diferencia en la mayoría de las muestras), con la animación de caminar o correr según la velocidad del otro. Que se agache cuando el otro se agacha todavía no funciona. Falta probarlo con dos máquinas: la DLL nueva está en `mod/dist/`.

**2026-11-15, noche (corridas 39–45), "dos Amandas":** se puede crear una segunda Amanda (`THE_PLAYER`) desde el mod, clonando el pedido de spawn de autoridad del jugador, sin que se caiga el juego y sin que se lleve la cámara. Pero **es invisible**: el juego solo trae modelos de Amanda en primera persona (las cinemáticas son videos). Por eso el avatar sigue siendo un NPC, ahora más responsivo:
- calcula la velocidad del otro y apunta adonde va a estar;
- corre cuando el otro corre;
- detecta cuándo el otro se agacha. Falta que el NPC se agache: se haría con el comando `IDLE` y `desired_stance`.

**2026-11-15, tarde (corridas 36–38):** primera prueba de la caminata con dos máquinas. El avatar camina y sigue natural en las dos. Hay que tener el mismo guardado: el mod no sincroniza el nivel, así que la laptop tiene que bajar la nube de Steam antes de pasar a sin conexión. Arreglos después de la prueba:
- el avatar ya no gira raro ni camina cuando el otro solo mueve la cámara: queda "plantado" hasta que el otro se mueve;
- el avatar es **invencible**, con el mismo flag que `CHR_SetInvincibility`, así matarlo no da game over por civiles;
- los arrojables caen distinto porque el engine integra el vuelo con el tiempo de cada frame. Ya se sabe dónde está su posición (sonda); falta corregir la réplica.

**2026-11-15, tarde (corrida 35):** el avatar ya vuelve a arrancar con movimientos cortos del otro (1–1,6 m, ≤1 s de retardo). Salió del layout de los parámetros de `MOVE_TOWARDS` (hallazgo 19): el flag `start_instantly`, más una orden nueva si queda quieto a más de 1 m. Los dos están activados por defecto y `mod/dist/winmm.dll` está actualizada (`a88bc1a`).

El detalle completo está en `work/PROGRESS.md` (estado), `work/NOTES.md` y `work/LOG.md` (bitácora) y `work/findings/19-control-de-personajes.md`.
