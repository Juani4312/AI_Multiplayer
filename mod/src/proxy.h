#ifndef AIMP_PROXY_H
#define AIMP_PROXY_H
#include "aimp.h"

/* Carga la winmm real (carpeta de sistema) y resuelve todos los exports.
 * Devuelve 0 si no pudo cargarla: en ese caso cualquier llamada a winmm termina
 * el proceso con un mensaje claro (ver aimp_unresolved en proxy.c). */
int proxy_init(void);
HMODULE proxy_real(void);

#endif
