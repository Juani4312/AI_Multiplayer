/* Test del mando virtual (autopad.c), con wine: parseo del guion, estado en el tiempo y reemplazo
 * de los slots de la IAT sobre una imagen PE armada en memoria que importa XINPUT1_3 por ordinal. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "aimp.h"
#include "autopad.h"
#include "log.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;
void probe_mark(const char *why) { log_printf("MARCA test %s", why ? why : ""); }

static int g_fail;
#define CHECK(cond, ...)                          \
    do {                                          \
        int ok_ = (cond);                         \
        printf(ok_ ? "  ok    " : "  FALLA ");    \
        printf(__VA_ARGS__);                      \
        printf("\n");                             \
        if (!ok_)                                 \
            g_fail++;                             \
    } while (0)

typedef struct {
    DWORD pkt;
    WORD buttons;
    BYTE lt, rt;
    SHORT lx, ly, rx, ry;
} St;
static DWORD WINAPI real_state(DWORD idx, St *s) { (void)idx; (void)s; return ERROR_DEVICE_NOT_CONNECTED; }
static DWORD WINAPI real_set(DWORD idx, void *v) { (void)idx; (void)v; return 0; }
static DWORD WINAPI real_caps(DWORD idx, DWORD f, uint8_t *c) { (void)idx; (void)f; (void)c; return ERROR_DEVICE_NOT_CONNECTED; }

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    printf("[guion]\n");
    CHECK(autopad_parse("9000-9200:A,13000-13200:a+START, P3000-8000:LY=32000+RT,P3000-3100:MARK,P5000-6000:LX=-40000") == 5,
          "5 pasos");
    CHECK(autopad_parse("9000-9200:Z") < 0, "acción desconocida -> inválido");
    CHECK(autopad_parse("9000:A") < 0, "falta el fin -> inválido");
    CHECK(autopad_parse("9000-9200:A,13000-13200:A+START,P3000-8000:LY=32000+RT,P5000-6000:LX=-40000") == 4, "reparseo");
    uint16_t b;
    int16_t th[4];
    uint8_t tr[2];
    autopad_state_at(9100, -1, &b, th, tr);
    CHECK(b == 0x1000 && !tr[1] && th[1] == 0, "t=9,1 s: A");
    autopad_state_at(13100, -1, &b, th, tr);
    CHECK(b == 0x1010, "t=13,1 s: A+START");
    autopad_state_at(20000, -1, &b, th, tr);
    CHECK(b == 0 && th[1] == 0, "sin jugador, los pasos P no corren");
    autopad_state_at(20000, 5500, &b, th, tr);
    CHECK(b == 0 && th[1] == 32000 && tr[1] == 255 && th[0] == -32768, "P+5,5 s: LY=32000, RT, LX recortado a -32768");

    CHECK(autopad_parse("0-30000:GO=12.5/-3.25,0-30000:GO=-1/2") == 2, "GO=x/z parsea (2 pasos)");
    CHECK(autopad_parse("0-30000:GO=12.5") < 0, "GO sin z -> inválido");

    printf("[IAT]\n");
    /* Imagen mínima: DOS + NT + descriptor de import de XINPUT1_3.dll con ordinales 2, 3, 4. */
    uint8_t *img = VirtualAlloc(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(img + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x400;
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(img + 0x400);
    d->Name = 0x600;
    strcpy((char *)img + 0x600, "xinput1_3.dll");
    d->OriginalFirstThunk = 0x700;
    d->FirstThunk = 0x800;
    uint32_t *ot = (uint32_t *)(img + 0x700), *ft = (uint32_t *)(img + 0x800);
    ot[0] = IMAGE_ORDINAL_FLAG32 | 2;
    ot[1] = IMAGE_ORDINAL_FLAG32 | 3;
    ot[2] = IMAGE_ORDINAL_FLAG32 | 4;
    ft[0] = (uint32_t)(uintptr_t)real_state;
    ft[1] = (uint32_t)(uintptr_t)real_set;
    ft[2] = (uint32_t)(uintptr_t)real_caps;
    CHECK(autopad_install_on((uintptr_t)img) == 2, "2 slots reemplazados (ordinales 2 y 4)");
    CHECK(ft[1] == (uint32_t)(uintptr_t)real_set, "XInputSetState queda igual");
    typedef DWORD(WINAPI * GS)(DWORD, St *);
    typedef DWORD(WINAPI * GC)(DWORD, DWORD, uint8_t *);
    St s;
    memset(&s, 0, sizeof s);
    uint8_t caps[20];
    CHECK(((GC)(uintptr_t)ft[2])(0, 1, caps) == 0 && caps[0] == 1, "mando 0 conectado (caps de gamepad)");
    CHECK(((GC)(uintptr_t)ft[2])(1, 1, caps) == ERROR_DEVICE_NOT_CONNECTED, "mando 1 sigue desconectado");
    autopad_frame(0, 1000, 0, NULL, 0); /* sin start: no pasa nada (no está activo: g_on = 0) */
    CHECK(((GS)(uintptr_t)ft[0])(0, &s) == 0 && s.buttons == 0, "estado del mando 0: conectado y en reposo");
    CHECK(((GS)(uintptr_t)ft[0])(2, &s) == ERROR_DEVICE_NOT_CONNECTED, "mando 2 desconectado");
    printf("[GO a lazo cerrado, con un jugador simulado]\n");
    /* modelo de las corridas 18 y 19: rumbo = yaw = atan2(dx, dz); RX>0 sube el yaw (~3,6 rad/s a fondo) */
    g_aimp_exe = (uintptr_t)img;
    SetEnvironmentVariableW(L"AIMP_AUTOPAD", L"0-60000:GO=5/5,0-60000:GO=5/-4+MARK,0-60000:GO=-3/-4");
    CHECK(autopad_install() == 1, "autopad_install con 3 GO");
    float pos[3] = { 0, 0, 0 }, yaw = -0.785398f;
    LARGE_INTEGER fq, c0;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&c0);
    int64_t t = c0.QuadPart;
    for (int f = 0; f < 3000; f++, t += fq.QuadPart * 16 / 1000) {
        autopad_frame(t, fq.QuadPart, 1, pos, yaw);
        St st;
        ((GS)(uintptr_t)ft[0])(0, &st);
        float dt = 0.016f;
        yaw += st.rx / 32767.0f * 3.6f * dt;
        float h = yaw, v = st.ly / 32767.0f * 3.0f * dt;
        pos[0] += sinf(h) * v;
        pos[2] += cosf(h) * v;
    }
    CHECK(fabsf(pos[0] + 3) < 0.7f && fabsf(pos[2] + 4) < 0.7f, "llegó al tercer punto (%.2f, %.2f)", pos[0], pos[2]);
    printf(g_fail ? "test_autopad: %d FALLAS\n" : "test_autopad: todo OK\n", g_fail);
    return g_fail ? 1 : 0;
}
