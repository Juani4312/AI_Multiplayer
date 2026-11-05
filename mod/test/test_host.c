/* Simula cómo AI.exe usa winmm: la importa por nombre y llama a timeGetTime,
 * timeBeginPeriod y timeEndPeriod. Con el proxy al lado del .exe (y en Wine con
 * WINEDLLOVERRIDES=winmm=n,b) tiene que cargar el proxy, que reenvía a la real. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <tlhelp32.h>

int main(void)
{
    int fail = 0;
    setvbuf(stdout, NULL, _IONBF, 0);

    HMODULE w = GetModuleHandleW(L"winmm.dll");
    wchar_t p[MAX_PATH] = L"";
    GetModuleFileNameW(w, p, MAX_PATH);
    printf("winmm.dll del import: %ls\n", p);

    DWORD t0 = timeGetTime();
    Sleep(50);
    DWORD t1 = timeGetTime();
    MMRESULT r1 = timeBeginPeriod(1), r2 = timeEndPeriod(1);
    UINT nwave = waveOutGetNumDevs();
    printf("timeGetTime %lu -> %lu (dif %lu ms) | timeBeginPeriod=%u timeEndPeriod=%u | waveOutGetNumDevs=%u\n",
           (unsigned long)t0, (unsigned long)t1, (unsigned long)(t1 - t0), r1, r2, nwave);
    if (t1 - t0 < 30 || t1 - t0 > 2000 || r1 != TIMERR_NOERROR || r2 != TIMERR_NOERROR)
        fail = 1;

    /* Tiene que haber dos módulos llamados winmm.dll: el proxy y la real. */
    int count = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    MODULEENTRY32W me;
    me.dwSize = sizeof me;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        if (lstrcmpiW(me.szModule, L"winmm.dll") == 0) {
            printf("  módulo winmm.dll: %ls\n", me.szExePath);
            count++;
        }
    CloseHandle(snap);
    if (count != 2)
        fail = 1;
    printf("%s\n", fail ? "TEST_HOST FALLA" : "TEST_HOST OK");
    return fail;
}
