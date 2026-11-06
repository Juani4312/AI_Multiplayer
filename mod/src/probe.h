#ifndef AIMP_PROBE_H
#define AIMP_PROBE_H
#include "aimp.h"

/* Sonda de estado (solo lectura): fase 2 del mod + experimentos de solo lectura de
 * work/RUNTIME_PLAN.md (bloques 0 a 3). Corre en el thread principal, después de
 * cada ModeManager::Run. */
void probe_init(void);
void probe_after_run(uintptr_t mm_this, uint32_t ret, int64_t run_ticks, int64_t now_ticks);
/* Pide un volcado completo en la próxima vuelta (además de la tecla y de los
 * volcados automáticos: al aparecer el jugador 0 y 5 s después). */
void probe_request_dump(void);
/* MARCA n en el log con la posición del jugador 0 (why: sufijo opcional). Thread principal. */
void probe_mark(const char *why);
/* El estado de red de la sonda (fase 3), para los módulos que mandan eventos (sync.c). */
#include "net.h"
AimpNet *probe_net(void);

#endif
