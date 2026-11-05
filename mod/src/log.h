#ifndef AIMP_LOG_H
#define AIMP_LOG_H
#include "aimp.h"

/* Abre el log. Ruta: %AIMP_LOG% si está definida; si no,
 * %LOCALAPPDATA%\AI_Multiplayer\aimp.log; si no, %TEMP%\aimp.log.
 * El log anterior se renombra a .prev. */
void log_open(void);
void log_close(void);
/* Una línea con prefijo de tiempo (segundos desde log_open). */
void log_printf(const char *fmt, ...) __attribute__((format(gnu_printf, 1, 2)));
/* Volcado en filas de 16 bytes: hex + ASCII. */
void log_hexdump(const char *title, uintptr_t addr, size_t len);
/* Volcado en filas de 4 dwords: hex + interpretación como float. */
void log_dump32(const char *title, uintptr_t addr, size_t len);
double log_now(void);
const char *log_path_utf8(void);

#endif
