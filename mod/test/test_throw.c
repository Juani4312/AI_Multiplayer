/* Test de los objetos arrojados (throw.c, hallazgo 17), con wine. Imagen de AI.exe falsa con
 * stubs de código máquina en las RVAs reales de FUN_007b5050 (tirar, thiscall, ret 0x14) y
 * FUN_007b5190 (colocar, thiscall, ret 0xC). Los stubs empiezan con el prólogo SEH real
 * (mov eax,fs:[0]; push -1; push imm32), que es lo que pisa el hook, y llaman a funciones de
 * prueba en C con this y los argumentos. La red es real (UDP por 127.0.0.1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "game.h"
#include "log.h"
#include "net.h"
#include "sync.h"
#include "throw.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

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

static void *obj(size_t n) { return calloc(1, n); }
static void put32(void *base, uint32_t off, uint32_t v) { memcpy((uint8_t *)base + off, &v, 4); }
static void putp(void *base, uint32_t off, void *p) { put32(base, off, (uint32_t)(uintptr_t)p); }
static uint32_t fbits(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

/* lo que recibió el "original" */
static int g_spawns, g_places;
static uint32_t g_pool_seen, g_a3, g_t;
static float g_m[12], g_v[3];
static char g_name[64];
static uintptr_t g_m_ptr, g_v_ptr, g_name_ptr;

static void __attribute__((cdecl)) fake_spawn(uint32_t self, const float *m, const float *v, uint32_t a3, const char *name,
                                              uint32_t t)
{
    g_spawns++;
    g_pool_seen = self;
    memcpy(g_m, m, sizeof g_m);
    memcpy(g_v, v, sizeof g_v);
    g_a3 = a3;
    g_t = t;
    strncpy(g_name, name, sizeof g_name - 1);
    g_m_ptr = (uintptr_t)m;
    g_v_ptr = (uintptr_t)v;
    g_name_ptr = (uintptr_t)name;
}
static void __attribute__((cdecl)) fake_place(uint32_t self, const float *m, const char *name, uint32_t t)
{
    g_places++;
    g_pool_seen = self;
    memcpy(g_m, m, sizeof g_m);
    g_t = t;
    strncpy(g_name, name, sizeof g_name - 1);
}

static void emit(uint8_t **p, const void *bytes, size_t n)
{
    memcpy(*p, bytes, n);
    *p += n;
}
static void emit_call(uint8_t **p, void *fn)
{
    uint8_t mov = 0xB8;
    uint32_t a = (uint32_t)(uintptr_t)fn;
    emit(p, &mov, 1);
    emit(p, &a, 4);
    emit(p, "\xFF\xD0", 2);
}
/* prólogo SEH real (los 9 bytes que verifica el hook + el imm32 del push) y lo deshace */
static void emit_seh(uint8_t **p)
{
    emit(p, AIMP_THROW_PROLOGUE, sizeof AIMP_THROW_PROLOGUE); /* mov eax,fs:[0]; push -1; push ... */
    emit(p, "\x00\x00\x00\x00", 4);                         /* ... imm32 */
    emit(p, "\x83\xC4\x08", 3);                             /* add esp,8 */
}

static void build_stubs(uint8_t *img)
{
    DWORD old;
    uint8_t *p = img + AIMP_RVA_THROW_SPAWN;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit_seh(&p);
    for (int i = 0; i < 5; i++)
        emit(&p, "\xFF\x74\x24\x14", 4); /* push [esp+0x14]: a5, a4, a3, a2, a1 */
    emit(&p, "\x51", 1);                 /* push ecx (this) */
    emit_call(&p, (void *)fake_spawn);
    emit(&p, "\x83\xC4\x18\xC2\x14\x00", 6); /* add esp,0x18; ret 0x14 */

    p = img + AIMP_RVA_THROW_PLACE;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit_seh(&p);
    for (int i = 0; i < 3; i++)
        emit(&p, "\xFF\x74\x24\x0C", 4); /* push [esp+0xC]: a3, a2, a1 */
    emit(&p, "\x51", 1);
    emit_call(&p, (void *)fake_place);
    emit(&p, "\x83\xC4\x10\xC2\x0C\x00", 6); /* add esp,0x10; ret 0xC */
}

typedef void(__attribute__((fastcall)) * SpawnFn)(void *pool, void *edx, const float *m, const float *vel, uint32_t a3,
                                                  const char *name, uint32_t t);
typedef void(__attribute__((fastcall)) * PlaceFn)(void *pool, void *edx, const float *m, const char *name, uint32_t t);

static char *read_log(void)
{
    wchar_t path[MAX_PATH];
    if (!GetEnvironmentVariableW(L"AIMP_LOG", path, MAX_PATH))
        return NULL;
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    s[fread(s, 1, (size_t)n, f)] = 0;
    fclose(f);
    return s;
}
static void expect(const char *log, const char *needle, int should)
{
    int has = strstr(log, needle) != NULL;
    printf(has == should ? "  ok    " : "  FALLA ");
    printf("%s \"%s\"\n", should ? "está" : "no está", needle);
    if (has != should)
        g_fail++;
}

static void push_in(AimpNet *n, const AimpEvt *e)
{
    n->in[n->in_tail] = *e;
    n->in_tail = (n->in_tail + 1) % AIMP_EVT_QUEUE;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stubs(img);
    void *root = obj(0x100), *cm = obj(0x200), *parr = obj(64), *pl = obj(0x400), *pool = obj(4 + THROW_POOL_SLOTS * THROW_POOL_STRIDE);
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(root, ROOT_CHARMGR, cm);
    putp(cm, CM_PLAYERS, parr);
    put32(cm, CM_NPLAYERS, 1);
    putp(parr, 0, pl);
    putp(img, AIMP_RVA_THROW_POOL, pool);

    CHECK(sizeof(ThrowEvt) == 112 && sizeof(ThrowEvt) <= AIMP_EVT_DATA, "ThrowEvt = %u bytes (cabe en un evento de %u)",
          (unsigned)sizeof(ThrowEvt), AIMP_EVT_DATA);
    CHECK(sizeof(AimpEvt) == 136, "AimpEvt = %u bytes", (unsigned)sizeof(AimpEvt));

    LARGE_INTEGER f, now;
    QueryPerformanceFrequency(&f);
    AimpNet host, cli;
    SetEnvironmentVariableW(L"AIMP_NET", L"host");
    SetEnvironmentVariableW(L"AIMP_NET_PORT", L"47897");
    CHECK(net_init(&host, f.QuadPart), "host abierto");
    SetEnvironmentVariableW(L"AIMP_NET", L"join");
    SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47897");
    CHECK(net_init(&cli, f.QuadPart), "cliente abierto");
    SetEnvironmentVariableW(L"AIMP_SYNC_THROW", L"1");
    SetEnvironmentVariableW(L"AIMP_THROW_ECHO", L"40");
    CHECK(throw_install(&cli) == 1, "throw_install (los dos hooks)");
    CHECK(throw_sync_enabled(), "AIMP_SYNC_THROW activo");

    QueryPerformanceCounter(&now);
    net_poll(&host, now.QuadPart);
    AimpPkt hello;
    memset(&hello, 0, sizeof hello);
    net_send(&cli, now.QuadPart, &hello); /* el host aprende al cliente */
    Sleep(20);
    net_poll(&host, now.QuadPart);
    throw_frame(now.QuadPart, f.QuadPart, 16); /* fija el nivel actual */

    printf("[tirar local: corre el original y sale por la red]\n");
    /* como FUN_007b9500: 64 bytes = matriz 3x4 (traslación en 3, 7, 11) + velocidad + 9.81 */
    static float blk[16] __attribute__((aligned(16))) = { 1, 0, 0, 9.5f, 0, 1, 0, 15.25f, 0, 0, 1, 31.75f, 12.0f, 4.0f, -16.0f, 9.81f };
    char tmpl[] = "Thrown\\Flare";
    uint32_t esp0, esp1;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    ((SpawnFn)(void *)(img + AIMP_RVA_THROW_SPAWN))(pool, NULL, blk, blk + 12, fbits(15.0f), tmpl, fbits(30.0f));
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_spawns == 1, "tirar hookeado: corrió el original (%d) y el stack quedó balanceado", g_spawns);
    CHECK(g_pool_seen == (uint32_t)(uintptr_t)pool && g_m_ptr == (uintptr_t)blk && g_v_ptr == (uintptr_t)(blk + 12) &&
              g_name_ptr == (uintptr_t)tmpl && g_a3 == fbits(15.0f) && g_t == fbits(30.0f),
          "el original recibió los mismos argumentos");
    throw_frame(now.QuadPart, f.QuadPart, 16); /* encola el evento en la red */
    for (int i = 0; i < AIMP_EVT_REPEAT; i++)
        net_send_events(&cli);
    Sleep(30);
    net_poll(&host, now.QuadPart);
    AimpEvt e;
    int got = net_take_event(&host, &e), more = net_take_event(&host, &(AimpEvt){ 0 });
    ThrowEvt te;
    memcpy(&te, e.data, sizeof te);
    CHECK(got && !more && e.kind == AIMP_EVT_THROW && e.size == sizeof(ThrowEvt) && e.level == 16, "llegó un evento de lanzamiento (nivel 16)");
    CHECK(got && te.op == THROW_OP_THROW && memcmp(te.m, blk, 48) == 0 && memcmp(te.vel, blk + 12, 12) == 0 &&
              te.a3 == fbits(15.0f) && te.t == fbits(30.0f) && strcmp(te.name, "Thrown\\Flare") == 0,
          "payload intacto: matriz, velocidad, 15.0, 30.0 y \"%s\"", got ? te.name : "?");

    printf("[eco local (AIMP_THROW_ECHO=40): se repite corrido al costado]\n");
    Sleep(60);
    g_spawns = 0;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    throw_frame(now.QuadPart, f.QuadPart, 16);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    /* dirección horizontal (12, -16) / 20 -> costado (0.8, 0.6) * 0.75 */
    CHECK(esp0 == esp1 && g_spawns == 1, "el eco llamó al original una vez (%d)", g_spawns);
    CHECK(g_spawns == 1 && g_m[3] > 10.09f && g_m[3] < 10.11f && g_m[7] == 15.25f && g_m[11] > 32.19f && g_m[11] < 32.21f &&
              memcmp(g_v, blk + 12, 12) == 0 && strcmp(g_name, "Thrown\\Flare") == 0 && g_pool_seen == (uint32_t)(uintptr_t)pool,
          "eco en (%.3f, %.3f, %.3f) con la misma velocidad y plantilla", (double)g_m[3], (double)g_m[7], (double)g_m[11]);
    CHECK(((uintptr_t)g_m_ptr & 15) == 0 && ((uintptr_t)g_v_ptr & 15) == 0, "matriz y velocidad inyectadas alineadas a 16");
    CHECK(g_name_ptr != (uintptr_t)tmpl, "la plantilla inyectada es una copia propia (no el buffer del juego)");
    for (int i = 0; i < AIMP_EVT_REPEAT; i++)
        net_send_events(&cli);
    Sleep(30);
    net_poll(&host, now.QuadPart);
    CHECK(!net_take_event(&host, &e), "el eco no vuelve a mandarse (pasa por el trampolín)");

    printf("[tirar remoto: se aplica llamando a la función con los datos del otro]\n");
    AimpEvt in;
    memset(&in, 0, sizeof in);
    in.kind = AIMP_EVT_THROW;
    in.size = sizeof(ThrowEvt);
    in.level = 16;
    in.seq = 5;
    ThrowEvt rt = te;
    rt.m[3] = -3.0f;
    strcpy(rt.name, "Thrown\\NoiseMaker");
    memcpy(in.data, &rt, sizeof rt);
    push_in(&cli, &in);
    g_spawns = 0;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    sync_frame(&cli, 16);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_spawns == 1 && g_m[3] == -3.0f && strcmp(g_name, "Thrown\\NoiseMaker") == 0 &&
              g_a3 == fbits(15.0f) && g_t == fbits(30.0f),
          "lanzamiento remoto aplicado (%d): %s en x = %.2f", g_spawns, g_name, (double)g_m[3]);
    throw_frame(now.QuadPart, f.QuadPart, 16);
    for (int i = 0; i < AIMP_EVT_REPEAT; i++)
        net_send_events(&cli);
    Sleep(30);
    net_poll(&host, now.QuadPart);
    CHECK(!net_take_event(&host, &e), "aplicar no lo vuelve a mandar (no hay eco por la red)");
    in.level = 9;
    in.seq = 6;
    push_in(&cli, &in);
    sync_frame(&cli, 16);
    CHECK(g_spawns == 1, "un lanzamiento de otro nivel se descarta");
    putp(img, AIMP_RVA_THROW_POOL, NULL);
    in.level = 16;
    in.seq = 7;
    push_in(&cli, &in);
    sync_frame(&cli, 16);
    CHECK(g_spawns == 1, "sin pool (nivel sin cargar) no se llama");
    putp(img, AIMP_RVA_THROW_POOL, pool);

    printf("[colocar local y remoto]\n");
    static float blk2[16] __attribute__((aligned(16))) = { 1, 0, 0, 2.0f, 0, 1, 0, 0.5f, 0, 0, 1, -7.0f, 0, 0, 0, 1 };
    char tmpl2[] = "Thrown\\explosive_mine";
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    ((PlaceFn)(void *)(img + AIMP_RVA_THROW_PLACE))(pool, NULL, blk2, tmpl2, fbits(0.0f));
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_places == 1 && strcmp(g_name, "Thrown\\explosive_mine") == 0, "colocar hookeado: corrió el original");
    g_spawns = 0;
    throw_frame(now.QuadPart, f.QuadPart, 16);
    for (int i = 0; i < AIMP_EVT_REPEAT; i++)
        net_send_events(&cli);
    Sleep(30);
    net_poll(&host, now.QuadPart);
    got = net_take_event(&host, &e);
    memcpy(&te, e.data, sizeof te);
    CHECK(got && te.op == THROW_OP_PLACE && memcmp(te.m, blk2, 48) == 0 && strcmp(te.name, "Thrown\\explosive_mine") == 0,
          "evento de colocación con la matriz y la plantilla");
    Sleep(60);
    throw_frame(now.QuadPart, f.QuadPart, 16);
    CHECK(g_spawns == 0 && g_places == 1, "colocar no tiene eco");
    memcpy(in.data, &te, sizeof te);
    in.seq = 8;
    push_in(&cli, &in);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    sync_frame(&cli, 16);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_places == 2 && g_m[3] == 2.0f && g_m[11] == -7.0f, "colocación remota aplicada");

    net_close(&cli);
    net_close(&host);
    log_close();
    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "hook tirar (FUN_007b5050): instalado", 1);
    expect(log, "hook colocar (FUN_007b5190): instalado", 1);
    expect(log, "THROW local 1: tirar Thrown\\Flare desde (9.50, 15.25, 31.75), vel (12.00, 4.00, -16.00) |20.40|, a3 15.00, t 30.00", 1);
    expect(log, "THROW   mandado al otro jugador (evento 1, nivel 16)", 1);
    expect(log, "THROW eco 1: tirar Thrown\\Flare desde (10.10, 15.25, 32.20)", 1);
    expect(log, "THROW remoto 2: tirar Thrown\\NoiseMaker desde (-3.00, 15.25, 31.75)", 1);
    expect(log, "del nivel 9; acá el nivel es 16: se descarta", 1);
    expect(log, "no se aplica (pool 0x00000000", 1);
    expect(log, "THROW local 2: colocar Thrown\\explosive_mine en (2.00, 0.50, -7.00), t 0.00", 1);
    free(log);
    printf(g_fail ? "RESULTADO: %d FALLAS\n" : "RESULTADO: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
