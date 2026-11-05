/* Entry point de la DLL (sin CRT). */
#include "aimp.h"
#include "log.h"
#include "proxy.h"

int game_init(void);

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

static void log_platform(void)
{
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    typedef const char *(CDECL * WineVer)(void);
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW *);
    WineVer wv = nt ? (WineVer)(void *)GetProcAddress(nt, "wine_get_version") : NULL;
    RtlGetVersionFn rv = nt ? (RtlGetVersionFn)(void *)GetProcAddress(nt, "RtlGetVersion") : NULL;
    OSVERSIONINFOW vi;
    aimp_memset(&vi, 0, sizeof vi);
    vi.dwOSVersionInfoSize = sizeof vi;
    if (rv)
        rv(&vi);
    log_printf("plataforma: Windows %u.%u.%u%s%s", (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
               (unsigned)vi.dwBuildNumber, wv ? ", Wine " : "", wv ? wv() : "");
}

BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved)
    __attribute__((force_align_arg_pointer));

BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t self[MAX_PATH];
        char p8[MAX_PATH * 3];
        g_aimp_self = inst;
        g_aimp_main_tid = GetCurrentThreadId();
        DisableThreadLibraryCalls(inst);
        log_open();
        GetModuleFileNameW(inst, self, MAX_PATH);
        log_printf("AI_Multiplayer %s, commit %s, compilado %s %s", AIMP_VERSION, AIMP_GIT_HASH, __DATE__, __TIME__);
        log_printf("DLL: %s (0x%08x), pid %u", aimp_utf8(p8, sizeof p8, self), (unsigned)(uintptr_t)inst,
                   (unsigned)GetCurrentProcessId());
        log_printf("log: %s", log_path_utf8());
        log_platform();
        if (!proxy_init())
            log_printf("ERROR: sin la winmm real el juego no va a poder usar winmm");
        game_init();
    } else if (reason == DLL_PROCESS_DETACH) {
        log_printf("DLL_PROCESS_DETACH (%s)", reserved ? "fin del proceso" : "FreeLibrary");
        log_close();
    }
    return TRUE;
}
