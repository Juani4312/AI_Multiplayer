/* Shim para test_core: globales de dllmain.c y un log_printf a stdout. */
#include <stdio.h>
#include <string.h>
#include "aimp.h"
#include "log.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

void log_printf(const char *fmt, ...)
{
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    aimp_vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("    [log] %s\n", b);
}
