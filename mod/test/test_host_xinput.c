/* Simula cómo AI.exe usa xinput1_3: la importa (AI.exe por ordinal: 2, 3 y 4) y llama a
 * XInputGetState/SetState/GetCapabilities. Con la variante xinput1_3.dll del mod al lado del .exe
 * (en Wine con WINEDLLOVERRIDES=xinput1_3=n,b) tiene que cargar el proxy, que reenvía a la real.
 * Después carga también la winmm.dll del mod: el mod tiene que arrancar una sola vez (el log de
 * AIMP_LOG tiene un solo encabezado, el de la variante xinput1_3) y la winmm tiene que reenviar igual. */
#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>

static int count_modules(const wchar_t *name)
{
    int count = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    MODULEENTRY32W me;
    me.dwSize = sizeof me;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        if (lstrcmpiW(me.szModule, name) == 0) {
            printf("  módulo %ls: %ls\n", name, me.szExePath);
            count++;
        }
    CloseHandle(snap);
    return count;
}

static int count_in_file(const char *path, const char *needle)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return -1;
    static char buf[1 << 20];
    DWORD got = 0;
    ReadFile(f, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    int n = 0;
    for (const char *p = buf; (p = strstr(p, needle)) != NULL; p += strlen(needle))
        n++;
    return n;
}

static int ok_xi(DWORD r) { return r == ERROR_SUCCESS || r == ERROR_DEVICE_NOT_CONNECTED; }

int main(void)
{
    int fail = 0;
    setvbuf(stdout, NULL, _IONBF, 0);

    HMODULE x = GetModuleHandleW(L"xinput1_3.dll");
    wchar_t p[MAX_PATH] = L"";
    GetModuleFileNameW(x, p, MAX_PATH);
    printf("xinput1_3.dll del import: %ls\n", p);

    XINPUT_STATE st;
    XINPUT_CAPABILITIES caps;
    XINPUT_VIBRATION vib = {0, 0};
    DWORD r1 = XInputGetState(0, &st), r2 = XInputGetCapabilities(0, XINPUT_FLAG_GAMEPAD, &caps),
          r3 = XInputSetState(0, &vib);
    printf("por nombre: GetState=%lu GetCapabilities=%lu SetState=%lu\n", (unsigned long)r1, (unsigned long)r2,
           (unsigned long)r3);
    if (!ok_xi(r1) || !ok_xi(r2) || !ok_xi(r3))
        fail = 1;

    /* Como AI.exe: por ordinal. */
    typedef DWORD(WINAPI * GetStateFn)(DWORD, XINPUT_STATE *);
    typedef DWORD(WINAPI * GetCapsFn)(DWORD, DWORD, XINPUT_CAPABILITIES *);
    GetStateFn o2 = (GetStateFn)(void *)GetProcAddress(x, MAKEINTRESOURCEA(2));
    GetCapsFn o4 = (GetCapsFn)(void *)GetProcAddress(x, MAKEINTRESOURCEA(4));
    FARPROC o3 = GetProcAddress(x, MAKEINTRESOURCEA(3)), o100 = GetProcAddress(x, MAKEINTRESOURCEA(100));
    if (!o2 || !o3 || !o4 || !o100) {
        printf("faltan ordinales: 2=%p 3=%p 4=%p 100=%p\n", (void *)o2, (void *)o3, (void *)o4, (void *)o100);
        fail = 1;
    } else {
        DWORD q2 = o2(0, &st), q4 = o4(0, XINPUT_FLAG_GAMEPAD, &caps);
        printf("por ordinal: #2=%lu #4=%lu\n", (unsigned long)q2, (unsigned long)q4);
        if (!ok_xi(q2) || !ok_xi(q4))
            fail = 1;
    }

    /* El proxy y la real. */
    if (count_modules(L"xinput1_3.dll") != 2)
        fail = 1;

    /* La winmm del mod en el mismo proceso: tiene que reenviar, pero sin arrancar el mod otra vez. */
    HMODULE w = LoadLibraryW(L"winmm.dll");
    typedef DWORD(WINAPI * TimeFn)(void);
    TimeFn tg = w ? (TimeFn)(void *)GetProcAddress(w, "timeGetTime") : NULL;
    DWORD t0 = tg ? tg() : 0;
    Sleep(50);
    DWORD t1 = tg ? tg() : 0;
    printf("winmm del mod: %p, timeGetTime dif %lu ms\n", (void *)w, (unsigned long)(t1 - t0));
    if (!tg || t1 - t0 < 30 || t1 - t0 > 2000)
        fail = 1;
    if (count_modules(L"winmm.dll") != 2)
        fail = 1;

    const char *log = getenv("AIMP_LOG");
    if (log) {
        const char *lp = log;
        int heads = count_in_file(lp, "AI_Multiplayer 0.");
        int via = count_in_file(lp, "el mod entró por xinput1_3.dll");
        printf("log %s: %d encabezado(s), %d \"entró por xinput1_3.dll\"\n", lp, heads, via);
        if (heads != 1 || via != 1)
            fail = 1;
    } else {
        printf("sin AIMP_LOG: no se verifica el arranque único\n");
        fail = 1;
    }
    printf("%s\n", fail ? "TEST_HOST_XINPUT FALLA" : "TEST_HOST_XINPUT OK");
    return fail;
}
