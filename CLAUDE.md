# AI_Multiplayer — coop mod para Alien: Isolation

> **¿Te pidieron instalar el mod para jugar (no desarrollarlo)?** No sigas este archivo: tu tarea está en `INSTALAR_CON_CLAUDE.md`.
> **¿Estás en `papulaptop`** (la laptop Windows de pruebas del usuario; `hostname`)? Tu tarea está en `work/windows/instructions.md`.

Antes de hacer nada en una sesión nueva:
0. **Corré `work/scripts/check_install.sh`** (solo lee, unos segundos) y tené en cuenta lo que muestra antes de probar o instalar algo:
   - si el juego está abierto (el usuario puede estar jugando: no lo cierres sin preguntar);
   - el modo (juego o prueba) y si el mod coop está instalado;
   - los mods de terceros de `<juego>/mods/` (los que interfieren se apagan con `mode.sh test`);
   - qué partida está cargada: nunca probar sobre la del usuario ni sobre la run sin morir;
   - las DLL sueltas y si a algún nivel le faltan archivos.
   Las texturas de `DATA/ENV` las maneja el usuario: no tocarlas.

Después leé en este orden:
1. `work/PROGRESS.md`: estado actual y siguiente paso.
2. `prompt.md`: instrucciones y reglas completas del proyecto (rutas, qué pedir antes de hacer, formato de hallazgos).
3. `work/NOTES.md`: bitácora con la evidencia. Leé las secciones relevantes según el paso.

Reglas clave (el detalle está en prompt.md):
- Respondé en español rioplatense.
- La instalación de Steam (`"/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation"`) es **solo lectura**. Trabajá sobre `game/`.
- Ningún offset sale de memoria: todo con evidencia de REA/Ghidra o de runtime.
- La partida del usuario es una run sin morir: probá siempre con `work/scripts/mode.sh test` (save de prueba) y al terminar `mode.sh play` (restaura su partida y saca el mod). Nunca pruebes sobre `saves/current`.
- Pedí confirmación antes de instalar, descargar, ejecutar el juego o cambiar configuración.
- Al cerrar cada bloque de trabajo, actualizá `work/PROGRESS.md` y `work/NOTES.md` y hacé commit y push.
- Los commits van con la cuenta personal del usuario (la de los commits existentes), nunca con la de la facultad: el repo tiene `user.name`/`user.email` en su config local; verificalo con `git config user.email` antes de commitear.
