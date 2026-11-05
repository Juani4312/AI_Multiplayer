#ifndef AIMP_TRACE_H
#define AIMP_TRACE_H
#include "aimp.h"

/* Experimento 3.6 de work/RUNTIME_PLAN.md: qué tipos de mensaje encolan los
 * productores (cola A = loopback, cola B = sesión/red), cuántos, de qué tamaño, desde
 * qué thread y desde qué call site. Solo cuenta: no cambia argumentos ni resultados.
 * Se activa con AIMP_TRACE_QUEUES=1. */
int trace_install(void);
int trace_enabled(void);
/* Llamar desde el thread principal: informe del período (cada AIMP_TRACE_MS). */
void trace_report_period(void);
/* Totales de toda la sesión, con call sites. */
void trace_report_totals(const char *why);

/* Para tests: registrar un mensaje como si lo hubiera visto el hook. */
void trace_note(int queue, uint32_t type, uint32_t size, uintptr_t ret_addr);
/* Para tests: instalar los hooks sobre funciones arbitrarias con el mismo prólogo. */
int trace_install_at(void *wrap_a, void *wrap_b);

#endif
