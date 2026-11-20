# Smart AI — instalación pendiente (sin README)

El comprimido (`Smart AI-9-1-0-1561684922.rar`) no trae instrucciones. Contenido: `Alien_Isolation_Insane_Mod_FIX/`
con `ALIENCONFIGS/` (19), `BEHAVIOR/` (85), `BINARY_BEHAVIOR/_DIRECTORY_CONTENTS.BML` y `CHR_INFO/ATTRIBUTES/` (16):
121 archivos de IA (árboles de comportamiento, configuraciones del Alien, atributos de personajes).

**Instalación deducida (sin confirmar):** cada carpeta va a la de igual nombre dentro de `DATA/` del juego. 115 de los
121 archivos pisan uno que ya existe ahí; los 6 que no existen son variantes que dejó el autor
(`ALIENCONFIGS/CANTEEN_old.BML` y 5 `*_noconflict.XML`), que probablemente no se copian.

**Interfiere con el desarrollo del coop: sí, mucho.** Cambia la IA del Alien, los androides y los humanos, y las
configuraciones del Alien (`ALIENCONFIGS`), que son la base que escala la dificultad del coop (`diff.c`). Todo lo
verificado (remates, caído con la facción del Alien, retirada) se hizo con la IA original. Además, en una partida de a
dos los dos tienen que tener la misma IA o las simulaciones divergen.

Para habilitarlo: confirmar la instalación (página del mod en Nexus) y crear `toggle.sh` como el de `../NoHUD-crosshair/`
con `MOD_INTERFIERE=1` y un par `origen|destino` por archivo (por ejemplo
`"Alien_Isolation_Insane_Mod_FIX/BEHAVIOR/ALIEN_BEHAVE.XML|DATA/BEHAVIOR/ALIEN_BEHAVE.XML"`).
