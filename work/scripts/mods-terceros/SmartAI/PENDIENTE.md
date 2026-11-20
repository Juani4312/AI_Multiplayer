# Smart AI — instalación deducida (sin README)

**2026-11-20:** a pedido del usuario ("poneme los mods que descargue") ya tiene `toggle.sh` con la instalación deducida de
abajo (115 archivos, sin las 6 variantes). Probado: activar → los 115 son los del mod; desactivar → los 115 originales
vuelven idénticos (hash). Si el mod no se comporta como se espera, confirmar la instalación en su página de Nexus.

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

`toggle.sh` tiene `MOD_INTERFIERE=1`: `mods-desarrollo.sh desactivar` (y `mode.sh test`) lo apagan.
