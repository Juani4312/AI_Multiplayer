#ifndef AIMP_SYNC_H
#define AIMP_SYNC_H
#include "aimp.h"
#include "net.h"

/* Fase 5 (experimental): "usar" compartido. Ver sync.c.
 *   AIMP_TRACE_USE=1  loguea los usos y sus handlers (solo mira).
 *   AIMP_SYNC_USE=1   además manda los usos del jugador local y aplica los del otro. */
int sync_install(AimpNet *net);
/* Por frame, en el thread principal, después de net_poll. */
void sync_frame(AimpNet *net, uint32_t level);

#endif
