#ifndef AIMP_PROXY_H
#define AIMP_PROXY_H
#include "aimp.h"

/* Carga la DLL real (winmm o xinput1_3, según la variante; carpeta de sistema o
 * AIMP_PROXY_REAL) y resuelve todos los exports.
 * Devuelve 0 si no pudo cargarla: en ese caso cualquier llamada a la DLL termina
 * el proceso con un mensaje claro (ver aimp_unresolved en proxy.c). */
int proxy_init(void);
HMODULE proxy_real(void);
const char *proxy_name(void); /* "winmm" o "xinput1_3" */

#endif
