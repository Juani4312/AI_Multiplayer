/* Verificación de la build y hook del loop principal. */
#include "game.h"
#include "crash.h"
#include "ghost.h"
#include "sync.h"
#include "hook.h"
#include "log.h"
#include "pins.h"
#include "probe.h"
#include "trace.h"

typedef uint32_t(__attribute__((thiscall)) * ModeRunFn)(void *self);
static ModeRunFn g_run_orig;
static AimpHook g_hook_run;

/* Reemplazo de ModeManager::Run. fastcall: ECX = this (EDX no se usa). Sin
 * argumentos en el stack, igual que el original. Devuelve EAX completo del
 * original (el caller solo mira AL). */
__attribute__((fastcall, force_align_arg_pointer)) static uint32_t detour_mode_run(void *self, void *edx_unused)
{
    LARGE_INTEGER a, b;
    (void)edx_unused;
    QueryPerformanceCounter(&a);
    uint32_t r = g_run_orig(self);
    QueryPerformanceCounter(&b);
    probe_after_run((uintptr_t)self, r, b.QuadPart - a.QuadPart, b.QuadPart);
    return r;
}

static const char *okstr(int ok) { return ok ? "OK" : "DISTINTO"; }

/* Devuelve 1 si la build es la esperada. */
static int check_build(void)
{
    const uintptr_t base = g_aimp_exe;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log_printf("build: el módulo principal no tiene header MZ");
        return 0;
    }
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(base + (uint32_t)dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        log_printf("build: el módulo principal no es PE i386");
        return 0;
    }
    uint32_t tds = nt->FileHeader.TimeDateStamp;
    uint32_t soi = nt->OptionalHeader.SizeOfImage;
    FARPROC ovr = GetProcAddress((HMODULE)base, "ovr_Initialize");
    FARPROC curl = GetProcAddress((HMODULE)base, "curl_easy_init");
    uint32_t rva_ovr = ovr ? (uint32_t)((uintptr_t)ovr - base) : 0;
    uint32_t rva_curl = curl ? (uint32_t)((uintptr_t)curl - base) : 0;
    uint8_t pro[sizeof AIMP_MODEMGR_RUN_PROLOGUE];
    int pro_ok = mem_read(base + AIMP_RVA_MODEMGR_RUN, pro, sizeof pro) &&
                 aimp_memcmp(pro, AIMP_MODEMGR_RUN_PROLOGUE, sizeof pro) == 0;

    int ok_tds = tds == AIMP_EXPECT_TIMESTAMP, ok_soi = soi == AIMP_EXPECT_SIZEOFIMAGE;
    int ok_ovr = rva_ovr == AIMP_RVA_OVR_INITIALIZE, ok_curl = rva_curl == AIMP_RVA_CURL_EASY_INIT;
    log_printf("build: TimeDateStamp 0x%08x %s | SizeOfImage 0x%08x %s", tds, okstr(ok_tds), soi, okstr(ok_soi));
    log_printf("build: RVA ovr_Initialize 0x%x %s | RVA curl_easy_init 0x%x %s", rva_ovr, okstr(ok_ovr), rva_curl,
               okstr(ok_curl));
    log_printf("build: prólogo de ModeManager::Run (RVA 0x%x) %s", AIMP_RVA_MODEMGR_RUN, okstr(pro_ok));
    return ok_tds && ok_soi && ok_ovr && ok_curl && pro_ok;
}

int game_init(void)
{
    wchar_t exe[MAX_PATH];
    char p8[MAX_PATH * 3];
    g_aimp_exe = (uintptr_t)GetModuleHandleW(NULL);
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    log_printf("EXE: %s, base 0x%08x", aimp_utf8(p8, sizeof p8, exe), (unsigned)g_aimp_exe);
    if (!check_build()) {
        log_printf("build NO reconocida: el mod queda solo como proxy de winmm (sin hooks)");
        return 0;
    }
    log_printf("build reconocida (AI.exe dde66521...19a90, buildid 4940658)");
    if (aimp_env_int(L"AIMP_NOHOOK", 0)) {
        log_printf("AIMP_NOHOOK=1: sin hooks");
        return 0;
    }
    crash_install(); /* AIMP_CRASHLOG=0 lo apaga */
    probe_init();
    if (!hook_install(&g_hook_run, "ModeManager::Run", (void *)(g_aimp_exe + AIMP_RVA_MODEMGR_RUN),
                      AIMP_MODEMGR_RUN_PROLOGUE, sizeof AIMP_MODEMGR_RUN_PROLOGUE, AIMP_MODEMGR_RUN_STEAL,
                      (void *)detour_mode_run, (void **)&g_run_orig))
        return 0;
    trace_install(); /* opcional: AIMP_TRACE_QUEUES=1 */
    ghost_install(); /* opcional: AIMP_TRACE_SPAWN=1 / AIMP_GHOST=1 (fase 4, experimental) */
    sync_install(probe_net()); /* opcional: AIMP_TRACE_USE=1 / AIMP_SYNC_USE=1 (fase 5, experimental) */
    pins_install(); /* opcional: AIMP_TRACE_PINS=1 (fase 6, solo mira) */
    return 1;
}
