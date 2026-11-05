/* AI_Multiplayer (AIMP): definiciones comunes del mod.
 *
 * El mod se compila SIN CRT (-nostdlib): solo importa kernel32. Así el proxy no
 * agrega dependencias al orden de carga del juego. Por eso hay un runtime mínimo
 * propio (rt.c) y un formateador propio (fmt.c).
 */
#ifndef AIMP_H
#define AIMP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define AIMP_VERSION "0.2.0 (fases 1-3: proxy, hook por frame, sonda de estado, UDP del jugador)"
#ifndef AIMP_GIT_HASH
#define AIMP_GIT_HASH "desconocido"
#endif

/* rt.c: reemplazos de las funciones de string.h (el compilador también puede
 * generar llamadas a memcpy/memset). */
void *aimp_memcpy(void *dst, const void *src, size_t n);
void *aimp_memset(void *dst, int c, size_t n);
int aimp_memcmp(const void *a, const void *b, size_t n);
size_t aimp_strlen(const char *s);
size_t aimp_wcslen(const wchar_t *s);
/* Copia con truncado; siempre termina en 0 si cap > 0. Devuelve el largo copiado. */
size_t aimp_wcscpy(wchar_t *dst, size_t cap, const wchar_t *src);
size_t aimp_wcscat(wchar_t *dst, size_t cap, const wchar_t *src);

/* fmt.c: printf mínimo. Soporta %d %i %u %x %X %p %s %c %f %e %% con flags
 * "-0+ ", ancho, precisión y largos hh h l ll z. Devuelve el largo que tendría
 * la salida completa (como C99); la salida se trunca a cap-1 y termina en 0. */
int aimp_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int aimp_snprintf(char *buf, size_t cap, const char *fmt, ...)
    __attribute__((format(gnu_printf, 3, 4)));

/* UTF-16 -> UTF-8 (trunca). Devuelve buf. */
char *aimp_utf8(char *buf, size_t cap, const wchar_t *ws);

/* Variables de entorno de configuración. */
int aimp_env_int(const wchar_t *name, int def);
int aimp_env_str(const wchar_t *name, wchar_t *out, size_t cap);

/* mem.c: lecturas que no pueden crashear (ReadProcessMemory sobre el propio
 * proceso). Para punteros del heap del juego. Los globales de .data del EXE se
 * pueden leer directo: la sección siempre está mapeada. */
int mem_read(uintptr_t addr, void *out, size_t n);
int mem_u32(uintptr_t addr, uint32_t *out);
int mem_u8(uintptr_t addr, uint8_t *out);
int mem_f32(uintptr_t addr, float *out);
/* Lee un string terminado en 0 (como mucho cap-1 bytes); 0 si no se pudo leer. */
int mem_cstr(uintptr_t addr, char *out, size_t cap);
/* Escritura que no crashea (WriteProcessMemory). Solo la usa la fase 4 (ghost.c),
 * sobre campos que ya se validaron con una lectura. */
int mem_write(uintptr_t addr, const void *src, size_t n);

/* Estado global. */
extern HMODULE g_aimp_self;      /* nuestra DLL */
extern uintptr_t g_aimp_exe;     /* base del EXE en runtime */
extern DWORD g_aimp_main_tid;    /* thread que cargó la DLL (el principal) */

#endif
