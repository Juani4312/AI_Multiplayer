# Menú de configuración del mod dentro del juego (diseño)

_2026-11-17. Pedido del usuario: "un menú de configuración del mod (dificultad, si los ítems se comparten, cosas así)". Propuesta del usuario: "usar el menú de config de video o alguno de esos que ya vienen, con los mismos sliders y selectores; no algo muy complejo"._

## Qué se quiere configurar (ideas del usuario)

- dificultad del coop;
- qué se comparte: inventario, munición, ítems del mundo;
- otras opciones simples del mod: sí/no o una lista corta de valores.

Hoy todo eso se configura con `aimp.cfg` (`AIMP_SYNC_INV`, `AIMP_COOP_DOWNED`, etc.), fuera del juego.

## Lo que se sabe de los menús (2026-11-17, estático)

- La interfaz es Scaleform (Flash). Las pantallas, incluidas las de opciones, están definidas en los SWF de `DATA/UI.PAK`: las etiquetas de las opciones (`AI_UI_MENU_SUBTITLES`, `AI_UI_MENU_SPEAKER_MODE`, `AI_UI_MENU_SFX_VOLUME`…) **no están en el exe**.
- La UI le pide cosas al juego con llamadas registradas por nombre en la función de `0x8410b1` (cada nombre con su manejador, p. ej. `TranslateString` → `FUN_00810270`). Lista completa en esa zona (`.rdata` `0x12FF3C0..0x12FF800`):
  - `TranslateString` (textos; ya enganchado en `ui.c` vía `FUN_00990b60`);
  - **`GenericCallback`, `GenericCallbackInt`, `GenericCallbackFloat`**: lo más probable es que por acá la UI avise los cambios de opciones (un id más un valor entero o real). **Sin verificar**;
  - `get_localised_button_name`, `get_button_from_action_mapping`, `RegisterInputListener…`, `ActivateLayer`/`DeactivateLayer`, `continue_requested`, `setLanguage`, `UseContainer`, etc.
- Textos de las opciones: `DATA/TEXT/<idioma>/UI.TXT` (UTF-16, `[CLAVE]` y `{texto}`). Desde `ui.c` ya se puede cambiar cualquier texto que la UI pida.

## Camino propuesto (simple, como pidió el usuario)

1. **Ver el protocolo de las opciones (runtime):** enganchar los manejadores de `GenericCallback*` y cambiar opciones en el menú (subtítulos, rango dinámico, brillo) para ver qué id y qué valor manda cada una. También ver cómo la UI pregunta el valor actual (otra llamada, o el juego se lo empuja al abrir la pantalla).
2. **Elegir dónde poner las opciones del mod**, de menos a más trabajo:
   - a) **Reusar entradas que en PC no hacen nada:** el SWF trae opciones de consola, como "Second Screen Settings", "Twitter Link" y "Facebook Link". Si alguna es un selector, se le cambia el texto con `ui.c` ("Coop: compartir ítems") y el mod intercepta su callback para guardar su propio valor. No toca archivos del juego.
   - b) **Reusar una opción real con un "modo mod":** por ejemplo, un selector que solo tiene sentido en el menú principal. Riesgo: tapa una opción del juego.
   - c) **Agregar entradas nuevas al SWF:** habría que modificar `UI.PAK` (archivo del juego) y distribuirlo. Más flexible, pero toca la instalación y es lo más trabajoso.
3. **Guardar:** el mod escribe los valores en su `aimp.cfg` (`%LOCALAPPDATA%\AI_Multiplayer\`). Las opciones que tienen que coincidir en las dos máquinas (qué se comparte) las decide el host y se mandan al conectar, como el inventario completo.
4. **Dificultad:** el juego ya tiene un selector de dificultad (`AI_UI_MENU_DIFFICULTY`, `AI_UI_MENU_DIFFICULTY_CHANGE_CONFIRMATION`) y archivos en `DATA/DIFFICULTYSETTINGS`. Una "dificultad del coop" podría ser:
   - que el host fuerce la suya en el cliente;
   - o ajustes propios del mod (daño, munición), elegidos con el mismo tipo de selector.

## Decisión (2026-11-18): reusar botones de Extras

El usuario propuso reemplazar un botón que ya existe ("tipo el de créditos del menú principal"). El submenú **Extras** tiene varios que en coop no se usan: `AI_UI_FRONTEND_CREDITS`, `_GALLERY`, `_DEV_DIARIES`, `_MAKING_OF`, `_DELETED_SCENES`, `_TRAILER`, `_ACHIEVEMENTS`.

**Primera versión, sin pantallas nuevas:**
- cada botón es una opción del mod que **cambia de valor al apretarlo** ("Coop: dificultad — Normal" → "Difícil" → …);
- el nombre y la descripción se cambian con `ui.c` (las etiquetas se vuelven a pedir al entrar al submenú; la descripción, al resaltar);
- **hay que interceptar la acción del botón** para que no abra los créditos: ver cómo avisa la UI (candidatas: `GenericCallback*`, `ActivateLayer`) y no dejarla pasar;
- el valor se guarda en `aimp.cfg`; lo que tiene que coincidir lo manda el host.

Más adelante, si hace falta: abrir la pantalla de opciones con sliders.

## Siguiente paso

Corrida corta: `AIMP_TRACE_UI_CALLBACKS=1` (por hacer), que loguee cada `GenericCallback*` con sus argumentos, y alguien que cambie opciones en el menú (alcanza con el mando virtual).
