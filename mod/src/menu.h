#ifndef AIMP_MENU_H
#define AIMP_MENU_H
#include "aimp.h"
#include "net.h"

/* Opciones del coop y menú "Coop" armado desde el mod (work/DESIGN_CONFIG_MENU.md y DESIGN_DIFFICULTY.md, decisiones
 * del usuario 2026-11-21): selector de dificultad (Normal / Difícil / Pesadilla / Personalizada) y "Opciones
 * avanzadas" (escala del Alien, de humanos y androides, caer y revivir, guardado exigente, inventario y cajas
 * compartidos). Las tres primeras avanzadas las fija la dificultad salvo en Personalizada. Lo decide el host.
 * Se guardan en %LOCALAPPDATA%\AI_Multiplayer\opciones.txt ("clave=valor"). */
enum {
    OPT_DIFF,     /* 0 Normal, 1 Difícil, 2 Pesadilla, 3 Personalizada */
    OPT_ALIEN,    /* escalones sobre la dificultad del juego: 0..3 */
    OPT_HUMANS,   /* ídem para humanos y androides (sin efecto todavía) */
    OPT_DOWNED,   /* caer y revivir: 0 Normal, 1 Difícil */
    OPT_STRICT,   /* guardado exigente: 0/1 */
    OPT_INV,      /* inventario compartido: 0/1 */
    OPT_BOXES,    /* cajas compartidas: 0/1 */
    OPT_N
};
#define OPT_DIFF_CUSTOM 3

#pragma pack(push, 1)
typedef struct {
    uint32_t kind; /* 2 (el 1 es la dificultad de diff.c) */
    uint32_t n;    /* OPT_N del host */
    uint8_t v[16]; /* valores elegidos (no los efectivos) */
} MenuOptsEvt;
#pragma pack(pop)

void menu_install(void);
/* después de instalar todo: guarda la red y aplica las opciones a los módulos (inv.c, pins.c, sync.c, diff.c) */
void menu_start(AimpNet *net);
/* AIMP_EVT_OPTIONS de tipo 2: las opciones del host (el cliente las usa mientras está conectado) */
void menu_remote_event(const AimpEvt *e);
/* por frame, solo en el menú principal (sin jugador) */
void menu_frame(int have_player);
/* valor que se aplica: el que elige este jugador o, en el cliente conectado, el del host */
int menu_opt(int id);
void menu_set(int id, int v);
/* 1 si el menú armado con Scaleform está activo (AIMP_MOD_MENU_ADV) */
int menu_adv_on(void);
/* textos de las claves propias del menú (AI_AIMP_M...); NULL si no es una */
const wchar_t *menu_text(const char *key);

#endif
