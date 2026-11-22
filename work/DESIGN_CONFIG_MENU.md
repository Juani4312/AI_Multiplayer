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

## Hecho (2026-11-18, corridas 127–128): "Extras" → "Coop"

- El usuario prefirió reemplazar **Extras** directamente ("más visible y capaz más fácil").
- Hechos: entrar a Extras no llama al juego (el submenú vive en el SWF); en PC Extras tiene **un solo botón, Credits**. Ese botón no pasa por `GenericCallback*` ni `ActivateLayer` (traza `AIMP_TRACE_UI_CALLBACKS=1`, corrida 127): llama a la acción `show_credits_screen`, registrada en `0x852608` junto a otras del frontend (`show_attract_screen` → `FUN_0082aa50`); su manejador es **`FUN_0082aa60`** (cdecl(params*): toma el objeto del frontend y salta a `FUN_00823310`, que abre `credits_screen`).
- **`ui.c`, `AIMP_MOD_MENU=1` (por defecto):** "Extras" se llama "Coop" (`AI_UI_FRONTEND_EXTRAS`, desc "Opciones del mod coop"); el botón es "Dificultad del coop: Normal/Difícil/Pesadilla" (`AI_UI_FRONTEND_CREDITS`), que cambia al apretarlo (hook de `FUN_0082aa60`, sin abrir los créditos). Se guarda en `%LOCALAPPDATA%\AI_Multiplayer\opciones.txt` (`dificultad_coop=N`); `ui_coop_difficulty()` lo expone. **Corrida 128:** la etiqueta se actualiza al instante al apretar (capturas `shots/run128-menu-coop*.png`).
- **Para más opciones:** las demás entradas de Extras de otras plataformas (Image Gallery, Dev Diaries, Trailer, Making Of, Deleted Scenes, Achievements) podrían estar ocultas según la plataforma (`Get_Current_Platform`, `FUN_008103a0`) o el contenido; habilitarlas daría más botones. Alternativa: que un mismo botón recorra varias opciones.
- **Efecto (corrida 129):** el valor ya cambia la dificultad (`diff.c`, ver DESIGN_DIFFICULTY) y lo decide el host (`AIMP_EVT_OPTIONS`); en el cliente conectado el botón dice "(la eligió el host)".

## Siguiente paso

- Más opciones en el menú "Coop" (compartir ítems, reglas de guardado): o habilitar más botones de Extras (ver arriba), o que el mismo botón recorra "opción: valor" con dos acciones (apretar cambia el valor; otro botón cambia de opción).
- `AIMP_TRACE_UI_CALLBACKS=1` ya existe: loguea `GenericCallback`, `GenericCallbackInt` y `ActivateLayer`. Sirve para ver el protocolo de la pantalla de opciones (sliders) si algún día se usa.

## Submenú "Opciones avanzadas": cómo hacerlo (investigación 2026-11-21, FFDec + REA)
- **Por qué Extras tiene un solo botón:** el menú lo arma el ActionScript de `NEWFRONTENDMENU.GFX` (`frontend/NewFrontendMenu.as`, sobre `GameMenu.as`): `_extrasItem.page.addItem(this._creditsItem)` y nada más en PC. `_devDiariesItem` (making of, escenas borradas, tráiler) existe pero no se agrega.
- **Las piezas existen:** `MenuItemExpandable(clave, handler, ?)` (abre una subpágina: `.page`), `MenuItemOptionOptionStepper(clave, valores[], claves_de_valores[], handler)` (como la dificultad de Opciones > Juego), `MenuItemOptionCheckBox(clave, valor, ?, handler)`, `MenuItemOptionSlider`, y `MenuPage.addItem(item)`. Las etiquetas son claves de texto: el hook de textos del mod (`det_lookup`, `ui.c`) ya traduce claves propias. Los handlers del juego avisan al motor con `ExternalInterface.call(prefijo + "request_set_difficulty", valor)`.
- **Camino elegido: armar el submenú desde el mod con la API de Scaleform 4 (sin tocar `UI.PAK`):**
  - `FUN_0082b450` (llamar una función de ActionScript desde el motor, la usa el cartel de consejos): toma el lock `[0x174A78C]+4`, llama `Movie vt+0x6c(GetCurrentThreadId())` y `vt+0xC0`, y después `FUN_0081c1a0(pantalla+0x68, nombre, args…)`. `pantalla+8` = `GFx::Movie`; **`pantalla+0x68` = el `GFx::Value` del objeto ActionScript de la pantalla** (el `GameMenu`).
  - `GFx::Value` = 0x18 bytes (`ObjectInterface*`, tipo: 6 = string, 0x40 = gestionado; dato). `ObjectInterface` vt: `+0x08` Release, `+0x10` GetMember, `+0x14` SetMember, `+0x18` **Invoke**; las de arreglo vienen después (a verificar).
  - **`AS3::MovieRoot::CreateObject` = `FUN_00ee1e60`** thiscall(movieRoot, `Value* resultado`, `const char* clase`, `const Value* args`, `unsigned n`): con clase null usa `"Object"`; error "Exception in CreateObject(\"%s\")" (`0x1487988`). Está en la entrada 53 de la vtable de MovieRoot (`0x14872cc`, sin RTTI); al lado: `0xee1d00`/`0xee1db0` (CreateString/W), `0xee21b0` (CreateArray), `0xee2260` (CreateFunction). El puntero al MovieRoot hay que buscarlo en el `Movie` (campo que apunte a un objeto con esa vtable) en runtime.
  - Navegación pública: `GameMenu.menu` (public) → `rootPage` → `items` → el ítem de Extras → `page.addItem(nuevo)`; o agregar antes de entrar al menú.
  - Callbacks: en vez de `CreateFunction`, el mod puede leer cada tanto el valor de cada ítem (`selectedIndex`, `selected`) con `GetMember`.
- **Siguiente:** en runtime, ubicar la pantalla del frontend (nombre para `FUN_00810420`), el MovieRoot y probar `CreateObject("frontend.menu.MenuItemOptionCheckBox", …)` + `addItem` en la página de Extras.
