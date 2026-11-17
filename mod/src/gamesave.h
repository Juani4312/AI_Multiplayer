#ifndef AIMP_GAMESAVE_H
#define AIMP_GAMESAVE_H
#include "aimp.h"

/* Guardar la partida desde el mod (2026-11-18, DESIGN_DIFFICULTY: con el host caído, el cliente guarda y el host guarda al
 * revivir; también "guardar juntos").
 * FUN_007a90b0: thiscall([g_root+0x78], a1, a2, a3, a4), ret 0x10. Es el guardado: lock (0x16F1910), chequea que guardar
 * esté habilitado (+0x824), arma el nombre del slot (FUN_00a51880) y llama al sistema de guardado (0xA5xxxx). La llaman:
 *   el Checkpoint de los scripts (0x5A270D) con (checkpoint_type, 1, 0, 0), después de serializar con FUN_009337c0;
 *   0x7A9B17 con (0, 0, 0, 1); 0x7B2C2B con (0, x, 0, 1); 0x7A9B50 y 0x7B29F8.
 * AIMP_TRACE_SAVE=1: loguea cada llamada (argumentos y retorno). */
void gamesave_install(void);
/* Llama al guardado del juego con esos argumentos (desde el thread principal). 1 si se llamó. */
int gamesave_now(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

#endif
