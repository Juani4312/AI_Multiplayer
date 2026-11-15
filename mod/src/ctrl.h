#ifndef AIMP_CTRL_H
#define AIMP_CTRL_H
#include "aimp.h"

/* Trazas (solo miran) para la animación del avatar y un segundo jugador (NOTES, 2026-11-15).
 *   AIMP_TRACE_CTRL=1  controlador de jugador del jugador 0 (Character+0x31C, 0x4C bytes) y su acción
 *                      principal (Character+0x36C) cada vez que cambian, a AIMP_TRACE_CTRL_MS (100) como mucho.
 *   AIMP_TRACE_CMD=1   hook de SwitchCommand (FUN_00678e20): controlador de comandos, tipo y dueño. */
int ctrl_install(void);
/* Filtro de SwitchCommand (antes de ctrl_install): si devuelve 0, el cambio de comando no se hace y el
 * comando se cancela con su vt+0x24 (libera su slot). Lo usa AIMP_GHOST_WALK para que la IA no le
 * cambie el comando al avatar. Corre en el thread que llama a SwitchCommand. */
typedef int (*CtrlSwitchFilter)(uint32_t ctrl, uint32_t cmd);
void ctrl_set_switch_filter(CtrlSwitchFilter fn);
/* Por frame, en el thread principal. */
void ctrl_frame(int64_t now, int64_t freq);

#endif
