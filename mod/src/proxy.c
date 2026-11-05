/* Proxy de winmm.dll.
 *
 * Cada export del proxy es un stub "jmp [puntero]". El puntero arranca apuntando a
 * un thunk de "no resuelto" y proxy_init() lo reemplaza por la función de la winmm
 * real. La lista de exports sale de winmm_exports.inc (generada por
 * tools/gen_winmm_exports.py); el .def con los mismos nombres la publica.
 */
#include "proxy.h"
#include "log.h"

/* 1) Punteros a las funciones reales. */
#define X(n, o) extern void aimp_f_##n(void); void *aimp_p_##n = (void *)aimp_f_##n;
#define W(n, o) X(n, o)
#define O(o) extern void aimp_f_ord##o(void); void *aimp_p_ord##o = (void *)aimp_f_ord##o;
#include "winmm_exports.inc"
#undef X
#undef W
#undef O

/* 2) Stub exportado (jmp indirecto) + thunk de "no resuelto" que pasa el ordinal. */
#define AIMP_STUB(sym, ordstr)                                                     \
    __asm__(".section .text\n\t.p2align 4\n"                                       \
            "\t.globl _aimp_s_" sym "\n_aimp_s_" sym ":\n\tjmp *_aimp_p_" sym "\n" \
            "\t.globl _aimp_f_" sym "\n_aimp_f_" sym ":\n\tpushl $" ordstr "\n"    \
            "\tcall _aimp_unresolved\n");
#define X(n, o) AIMP_STUB(#n, #o)
#define W(n, o) AIMP_STUB(#n, #o)
#define O(o) AIMP_STUB("ord" #o, #o)
#include "winmm_exports.inc"
#undef X
#undef W
#undef O

/* 3) Tabla para resolver y para el log. */
typedef struct {
    uint16_t ord;
    uint8_t win_only;
    const char *name; /* NULL: solo por ordinal */
    void **slot;
} ExportInfo;

static const ExportInfo k_exports[] = {
#define X(n, o) { o, 0, #n, &aimp_p_##n },
#define W(n, o) { o, 1, #n, &aimp_p_##n },
#define O(o) { o, 0, NULL, &aimp_p_ord##o },
#include "winmm_exports.inc"
#undef X
#undef W
#undef O
};
#define N_EXPORTS (sizeof k_exports / sizeof k_exports[0])

static HMODULE g_real;

HMODULE proxy_real(void) { return g_real; }

/* Llamada a un export que la winmm real no tiene (o la real no se cargó).
 * No vuelve: el stack del llamador no se puede reparar sin conocer la firma. */
__attribute__((used, noreturn, force_align_arg_pointer)) void aimp_unresolved(uint32_t ord)
{
    char msg[256];
    const char *name = "(desconocido)";
    for (size_t i = 0; i < N_EXPORTS; i++)
        if (k_exports[i].ord == ord)
            name = k_exports[i].name ? k_exports[i].name : "(solo ordinal)";
    aimp_snprintf(msg, sizeof msg,
                  "Se llamó a winmm!%s (ordinal %u), que la winmm real no tiene o no se cargó. "
                  "Se cierra el juego. Log: %s",
                  name, (unsigned)ord, log_path_utf8());
    log_printf("FATAL: %s", msg);
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef int(WINAPI * MsgBoxA)(HWND, LPCSTR, LPCSTR, UINT);
        MsgBoxA mb = (MsgBoxA)(void *)GetProcAddress(u, "MessageBoxA");
        if (mb)
            mb(NULL, msg, "AI_Multiplayer (proxy winmm)", MB_ICONERROR | MB_OK);
    }
    TerminateProcess(GetCurrentProcess(), 0xA1A1);
    for (;;)
        Sleep(1000);
}

int proxy_init(void)
{
    wchar_t path[MAX_PATH + 32];
    char p8[MAX_PATH * 3];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        log_printf("proxy: GetSystemDirectoryW falló (%u)", (unsigned)GetLastError());
        return 0;
    }
    aimp_wcscat(path, MAX_PATH + 32, L"\\winmm.dll");
    g_real = LoadLibraryW(path);
    if (!g_real) {
        log_printf("proxy: no se pudo cargar %s (error %u)", aimp_utf8(p8, sizeof p8, path),
                   (unsigned)GetLastError());
        return 0;
    }
    if (g_real == g_aimp_self) {
        log_printf("proxy: LoadLibrary(%s) devolvió este mismo proxy", aimp_utf8(p8, sizeof p8, path));
        g_real = NULL;
        return 0;
    }
    wchar_t got[MAX_PATH];
    GetModuleFileNameW(g_real, got, MAX_PATH);
    log_printf("proxy: winmm real = %s (0x%08x)", aimp_utf8(p8, sizeof p8, got),
               (unsigned)(uintptr_t)g_real);

    unsigned ok = 0, miss = 0, miss_win = 0;
    char missing[1024];
    size_t mk = 0;
    missing[0] = 0;
    for (size_t i = 0; i < N_EXPORTS; i++) {
        const ExportInfo *e = &k_exports[i];
        FARPROC f = e->name ? GetProcAddress(g_real, e->name)
                            : GetProcAddress(g_real, (LPCSTR)(uintptr_t)e->ord);
        if (f) {
            *e->slot = (void *)f;
            ok++;
            continue;
        }
        miss++;
        if (e->win_only)
            miss_win++;
        if (mk + 40 < sizeof missing)
            mk += (size_t)aimp_snprintf(missing + mk, sizeof missing - mk, " %s", e->name ? e->name : "#ord");
    }
    log_printf("proxy: %u exports resueltos, %u ausentes en la winmm real (%u de ellos son solo de Windows)",
               ok, miss, miss_win);
    if (miss)
        log_printf("proxy: ausentes:%s", missing);
    return 1;
}
