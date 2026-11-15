#ifndef AIMP_CTRL_H
#define AIMP_CTRL_H
#include "aimp.h"

/* Trazas (solo miran) para la animación del avatar y un segundo jugador (NOTES, 2026-11-15).
 *   AIMP_TRACE_CTRL=1  controlador de jugador del jugador 0 (Character+0x31C, 0x4C bytes) y su acción
 *                      principal (Character+0x36C) cada vez que cambian, a AIMP_TRACE_CTRL_MS (100) como mucho.
 *   AIMP_TRACE_CMD=1   hook de SwitchCommand (FUN_00678e20): controlador de comandos, tipo y dueño. */
int ctrl_install(void);
/* Por frame, en el thread principal. */
void ctrl_frame(int64_t now, int64_t freq);

#endif
