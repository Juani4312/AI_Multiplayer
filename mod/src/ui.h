#ifndef AIMP_UI_H
#define AIMP_UI_H
#include "aimp.h"

/* Mensajes del mod en el menú del juego (2026-11-17, pedido del usuario: "un cartel que indique que se está copiando
 * y no entrar, y luego que se puede jugar").
 *
 * La interfaz (Scaleform) pide los textos con la llamada "TranslateString" (registrada en 0x8410b1, manejador
 * FUN_00810270), que usa FUN_00990b60 thiscall(base de textos = VA 0x1766A48, const char *clave) -> const wchar_t *
 * (o NULL: la UI muestra la clave). Claves de DATA/TEXT/<idioma>/UI.TXT. Se engancha FUN_00990b60 y, con la red
 * activa, para algunas claves del frontend se devuelve el texto original con el estado del coop:
 *   AI_UI_FRONTEND_CAMPAIGN_DESC        descripción de abajo en Play Game: el estado del coop
 *   AI_UI_FRONTEND_CONTINUE_GAME        "Continue Game" (también es su descripción): "esperá" o "partida del host"
 * (el título y la descripción del menú principal se piden una sola vez al cargar, antes de la red: no sirven).
 * AIMP_UI_TEXT=0 lo apaga. */
void ui_install(void);
/* Menú del mod (AIMP_MOD_MENU=1, por defecto): "Extras" se llama "Coop" y su botón cambia la dificultad del coop.
 * 0 Normal (la del juego), 1 Difícil, 2 Pesadilla. Se guarda en %LOCALAPPDATA%\AI_Multiplayer\opciones.txt. */
int ui_coop_difficulty(void);
/* Aviso dentro del juego con el diálogo "Warning" del juego (FUN_0077c9e0, botón OK). Se muestra en el próximo frame
 * con jugador (ui_frame, thread principal). El texto es UTF-8. Pausa el juego local hasta que se acepta. */
void ui_message(const char *utf8);
/* Aviso que NO pausa: el cartel de consejos del HUD (overlay de tutorial), ms milisegundos (<= 0: 6000, lo del juego).
 * Se muestra en el próximo frame con jugador. Uno a la vez: uno nuevo reemplaza al anterior. */
void ui_toast(const char *utf8, int ms);
void ui_frame(int have_player);

/* Idioma de los textos del mod: 1 español, 0 inglés (opción "Idioma del mod" o el idioma del juego). */
int ui_lang_es(void);
#define TR(es, en) (ui_lang_es() ? (es) : (en))

#endif
