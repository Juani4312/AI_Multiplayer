/* Entry point de la DLL (sin CRT). */
#include "githash.h" /* generado en cada build: AIMP_GIT_HASH (antes de aimp.h, que trae un valor por defecto) */
#include "aimp.h"
#include "log.h"
#include "proxy.h"

int game_init(void);
int cfg_load(HMODULE self);
const char *cfg_applied(void);

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;
static int g_aimp_second; /* otra DLL del mod ya lo había arrancado */

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

/* El mod se puede cargar por dos DLL (winmm.dll y xinput1_3.dll). Si están las dos en la
 * carpeta del juego, solo la primera que carga arranca el mod; la otra queda como proxy puro
 * (sin log, sin hooks). Se marca con un mutex con el pid en el nombre. */
static int first_instance(void)
{
    wchar_t name[64] = L"Local\\AIMP_mod_";
    wchar_t dig[16];
    int nd = 0;
    DWORD pid = GetCurrentProcessId();
    do {
        dig[nd++] = (wchar_t)(L'0' + pid % 10);
        pid /= 10;
    } while (pid && nd < 15);
    size_t n = aimp_wcslen(name);
    while (nd && n + 1 < 64)
        name[n++] = dig[--nd];
    name[n] = 0;
    HANDLE m = CreateMutexW(NULL, FALSE, name); /* queda abierto hasta el fin del proceso */
    if (!m)
        return 1; /* sin mutex no se puede saber: se arranca */
    return GetLastError() != ERROR_ALREADY_EXISTS;
}

BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t self[MAX_PATH];
        char p8[MAX_PATH * 3];
        g_aimp_self = inst;
        g_aimp_main_tid = GetCurrentThreadId();
        DisableThreadLibraryCalls(inst);
        if (!first_instance()) {
            cfg_load(inst); /* por AIMP_PROXY_REAL */
            proxy_init();   /* sin log abierto: no escribe nada */
            g_aimp_second = 1;
            return TRUE;
        }
        int ncfg = cfg_load(inst); /* antes del log: puede definir AIMP_LOG */
        log_open();
        GetModuleFileNameW(inst, self, MAX_PATH);
        log_printf("AI_Multiplayer %s, commit %s, compilado %s %s", AIMP_VERSION, AIMP_GIT_HASH, __DATE__, __TIME__);
        const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)((uintptr_t)inst + (uint32_t)((const IMAGE_DOS_HEADER *)inst)->e_lfanew);
        log_printf("DLL: %s (0x%08x), link 0x%08x, pid %u", aimp_utf8(p8, sizeof p8, self), (unsigned)(uintptr_t)inst,
                   (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)GetCurrentProcessId());
        log_printf("log: %s", log_path_utf8());
        log_printf("config: %d archivo(s) aimp.cfg leídos; aplicadas: %s", ncfg, cfg_applied());
        log_platform();
        log_printf("proxy: el mod entró por %s.dll", proxy_name());
        if (!proxy_init())
            log_printf("ERROR: sin la %s real el juego no va a poder usarla", proxy_name());
        game_init();
    } else if (reason == DLL_PROCESS_DETACH && !g_aimp_second) {
        log_printf("DLL_PROCESS_DETACH (%s)", reserved ? "fin del proceso" : "FreeLibrary");
        log_close();
    }
    return TRUE;
}
