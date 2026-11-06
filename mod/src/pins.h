#ifndef AIMP_PINS_H
#define AIMP_PINS_H
#include "aimp.h"

/* Fase 6, paso 1 (hallazgo 15): traza de los pines de salida de CATHODE.
 * Hook en FUN_008be0d0 (EntityInterface::vt[+0x394], "disparar pin"): por cada (entidad, pin)
 * loguea las primeras apariciones con el ShortGuid del pin, el índice de la entidad, la
 * vtable concreta, la cantidad de links y quién lo disparó. Solo mira.
 * Se activa con AIMP_TRACE_PINS=1. AIMP_TRACE_PINS_N = apariciones detalladas por
 * (entidad, pin) (2 por defecto); AIMP_TRACE_PINS_MAX = tope de líneas detalladas (20000). */
int pins_install(void);
int pins_enabled(void);
/* Desde el thread principal, cada AIMP_TRACE_MS: los (entidad, pin) más frecuentes del período. */
void pins_report_period(void);
void pins_report_totals(const char *why);

/* Para tests: instalar el hook sobre una función con el mismo prólogo. */
int pins_install_at(void *target);

#endif
