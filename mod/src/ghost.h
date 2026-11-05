#ifndef AIMP_GHOST_H
#define AIMP_GHOST_H
#include "aimp.h"
#include "net.h"

/* Fase 4 (experimental): avatar del jugador remoto. Detalle en ghost.c y en
 * work/findings/12-avatar-remoto.md.
 *   AIMP_TRACE_SPAWN=1  loguea cada pedido de spawn (solo mira).
 *   AIMP_GHOST=1        además clona el pedido del jugador y mueve el clon. */
int ghost_install(void);
/* Después de cada Run, en el thread principal. level = índice del nivel actual. */
void ghost_frame(int64_t now, const AimpNet *net, uint32_t level);
/* Personaje que hoy hace de avatar del otro jugador (NPC o clon), o 0. Válido hasta el próximo ghost_frame. */
uint32_t ghost_avatar(void);

#endif
