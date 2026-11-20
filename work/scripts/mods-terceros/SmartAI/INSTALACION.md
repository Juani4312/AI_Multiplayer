# Smart AI — instalación (fuente: página del mod en Nexus, captura del usuario 2026-11-20)

> This is ONLY the AI from "Insane Mod" by Nightek1. […] increases view, hearing, AI and damage caused by the enemies of
> the game. The alien […] will be searching in the ventilation ducts also used by you, will hear walking in an adjoining
> room […]. Installation: Extract files inside DATA directory of Alien Isolation folder

El comprimido trae todo dentro de `Alien_Isolation_Insane_Mod_FIX/`. Lo que va a `DATA/` es su **contenido**
(`ALIENCONFIGS`, `BEHAVIOR`, `BINARY_BEHAVIOR`, `CHR_INFO`): extraer también la carpeta envoltorio dejaría
`DATA/Alien_Isolation_Insane_Mod_FIX/…`, que el juego no lee. `toggle.sh` copia los 121 archivos: 115 pisan originales
(guardados en `bak/orig`) y 6 son nuevos (`ALIENCONFIGS/CANTEEN_old.BML` y 5 `*_noconflict.XML`; se borran al desactivar).

**Interfiere con el coop: sí, mucho** (IA del Alien, androides y humanos; base de la dificultad del coop; los dos
jugadores necesitan la misma IA). `mods-desarrollo.sh desactivar` (y `mode.sh test`) lo apagan.
