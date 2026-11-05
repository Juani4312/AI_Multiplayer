/* Registro de excepciones (VEH). Si el juego se cae, el log dice dónde: código, dirección
 * (módulo + offset; en AI.exe, la RVA), tipo de acceso, registros y las direcciones de
 * retorno a .text de AI.exe que haya en el stack. No maneja nada: siempre devuelve
 * EXCEPTION_CONTINUE_SEARCH, así el juego sigue su camino normal. Se registran como mucho
 * 6 excepciones por sesión (solo códigos de crash; las de C++ y similares se ignoran).
 * AIMP_CRASHLOG=0 lo apaga. */
#include "crash.h"
#include "game.h"
#include "log.h"

static volatile LONG g_n;

static int is_crash_code(DWORD c)
{
    switch (c) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case 0xC0000409: /* STATUS_STACK_BUFFER_OVERRUN (/GS, __fastfail) */
    case 0xC0000374: /* STATUS_HEAP_CORRUPTION */
        return 1;
    }
    return 0;
}

static int in_text(uintptr_t a)
{
    return a >= g_aimp_exe + AIMP_TEXT_RVA && a < g_aimp_exe + AIMP_TEXT_RVA + AIMP_TEXT_SIZE;
}

static void where(char *out, size_t cap, uintptr_t a)
{
    if (g_aimp_exe && a >= g_aimp_exe && a < g_aimp_exe + AIMP_EXPECT_SIZEOFIMAGE) {
        aimp_snprintf(out, cap, "AI.exe+0x%x", (unsigned)(a - g_aimp_exe));
        return;
    }
    HMODULE h = NULL;
    wchar_t w[MAX_PATH];
    char u[MAX_PATH * 3];
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)a, &h) &&
        GetModuleFileNameW(h, w, MAX_PATH)) {
        const wchar_t *b = w;
        for (const wchar_t *p = w; *p; p++)
            if (*p == L'\\' || *p == L'/')
                b = p + 1;
        aimp_snprintf(out, cap, "%s+0x%x", aimp_utf8(u, sizeof u, b), (unsigned)(a - (uintptr_t)h));
        return;
    }
    aimp_snprintf(out, cap, "0x%08x (fuera de módulos)", (unsigned)a);
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *c = ep->ContextRecord;
    if (!is_crash_code(er->ExceptionCode) || InterlockedIncrement(&g_n) > 6)
        return EXCEPTION_CONTINUE_SEARCH;
    char w[200];
    where(w, sizeof w, (uintptr_t)er->ExceptionAddress);
    DWORD tid = GetCurrentThreadId();
    log_printf("EXCEPCIÓN 0x%08lx en %s, thread %lu%s (si nadie la maneja, el juego se cae)", er->ExceptionCode, w, tid,
               tid == g_aimp_main_tid ? " (principal)" : "");
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        log_printf("   %s en 0x%08x", er->ExceptionInformation[0] == 0 ? "lectura" : er->ExceptionInformation[0] == 1 ? "escritura" : "ejecución",
                   (unsigned)er->ExceptionInformation[1]);
    log_printf("   eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx", c->Eax, c->Ebx, c->Ecx,
               c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    uint32_t st[96];
    if (mem_read(c->Esp, st, sizeof st)) {
        char line[600];
        int n = aimp_snprintf(line, sizeof line, "   stack, retornos posibles a AI.exe (offset:rva):");
        int k = 0;
        for (int i = 0; i < 96 && k < 20 && n > 0 && (size_t)n < sizeof line - 24; i++)
            if (in_text(st[i])) {
                n += aimp_snprintf(line + n, sizeof line - (size_t)n, " +%x:%x", (unsigned)(i * 4), (unsigned)(st[i] - g_aimp_exe));
                k++;
            }
        log_printf("%s", line);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void crash_install(void)
{
    if (!aimp_env_int(L"AIMP_CRASHLOG", 1))
        return;
    if (AddVectoredExceptionHandler(1, veh))
        log_printf("registro de excepciones (VEH) instalado: si el juego se cae, la causa queda en este log");
}
