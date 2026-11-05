/* Test de la fase 5 (sync.c), con wine. Imagen de AI.exe falsa con stubs de código máquina en
 * las RVAs reales del productor del "usar" (FUN_00795d00, ret 8), los dos handlers (cdecl,
 * ret) y el wrapper de encolado de la cola A (FUN_00a2a290, cdecl). Los stubs tienen los
 * prólogos reales y llaman a funciones de prueba en C. La red es real (UDP por 127.0.0.1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "game.h"
#include "log.h"
#include "net.h"
#include "sync.h"

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

static uint32_t g_uses, g_enq, g_enq_type, g_enq_size, g_ha, g_hs;
static uint8_t g_enq_data[32];
static void *g_use_this;

static void __attribute__((cdecl)) fake_use(uint32_t self, const uint8_t *payload)
{
    g_uses++;
    (void)self;
    (void)payload;
}
static uint32_t __attribute__((cdecl)) fake_enqueue(uint32_t type, const uint8_t *data, uint32_t size)
{
    g_enq++;
    g_enq_type = type;
    g_enq_size = size;
    memcpy(g_enq_data, data, size < sizeof g_enq_data ? size : sizeof g_enq_data);
    return 0;
}
static void __attribute__((cdecl)) fake_ha(void) { g_ha++; }
static void __attribute__((cdecl)) fake_hs(void) { g_hs++; }

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

static void build_stubs(uint8_t *img)
{
    DWORD old;
    uint8_t *p = img + AIMP_RVA_USE_PRODUCER;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_USE_PRODUCER_PROLOGUE, sizeof AIMP_USE_PRODUCER_PROLOGUE); /* push ebp/esi/edi; edi = this */
    emit(&p, "\xFF\x75\x08\x57", 4);                                         /* push payload; push this */
    emit_call(&p, (void *)fake_use);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5D\xC2\x08\x00", 9); /* add esp,8; pop edi/esi/ebp; ret 8 */

    p = img + AIMP_RVA_USE_HANDLER_A;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_USE_HANDLER_A_PROLOGUE, sizeof AIMP_USE_HANDLER_A_PROLOGUE); /* push ebp/ebx/esi/edi; mov eax,0x24 */
    emit_call(&p, (void *)fake_ha);
    emit(&p, "\x5F\x5E\x5B\x5D\xB8\x14\x00\x00\x00\xC3", 10); /* pop edi/esi/ebx/ebp; mov eax,0x14; ret */

    p = img + AIMP_RVA_USE_HANDLER_S;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_USE_HANDLER_S_PROLOGUE, sizeof AIMP_USE_HANDLER_S_PROLOGUE); /* push ebp/esi/edi; mov eax,0x24 */
    emit_call(&p, (void *)fake_hs);
    emit(&p, "\x5F\x5E\x5D\xB8\x14\x00\x00\x00\xC3", 9); /* pop edi/esi/ebp; mov eax,0x14; ret */

    p = img + AIMP_RVA_ENQUEUE_A;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_ENQUEUE_PROLOGUE, sizeof AIMP_ENQUEUE_PROLOGUE); /* push ebp; ...; push size; push data */
    emit(&p, "\x52", 1);                                           /* push type */
    emit_call(&p, (void *)fake_enqueue);
    emit(&p, "\x83\xC4\x0C\x5D\xC3", 5); /* add esp,0xC; pop ebp; ret */
}

typedef void(__attribute__((fastcall)) * UseFn)(void *self, void *edx, void *payload, uint32_t flag);
typedef uint32_t(__attribute__((cdecl)) * HandlerFn)(void *ctx, void *payload);

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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stubs(img);
    void *root = obj(0x100), *cm = obj(0x200), *parr = obj(64), *pl = obj(0x400);
    g_use_this = obj(0x40);
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(root, ROOT_CHARMGR, cm);
    putp(cm, CM_PLAYERS, parr);
    put32(cm, CM_NPLAYERS, 1);
    putp(parr, 0, pl);
    memcpy((uint8_t *)pl + CHR_ID, "\x2f\xf7\x22\xb2\x09\x9d\x5b\x9b", 8);

    /* red: este proceso hace de join (manda sus usos) y abre un host para recibirlos */
    LARGE_INTEGER f, now;
    QueryPerformanceFrequency(&f);
    AimpNet host, cli;
    SetEnvironmentVariableW(L"AIMP_NET", L"host");
    SetEnvironmentVariableW(L"AIMP_NET_PORT", L"47895");
    CHECK(net_init(&host, f.QuadPart), "host abierto");
    SetEnvironmentVariableW(L"AIMP_NET", L"join");
    SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47895");
    CHECK(net_init(&cli, f.QuadPart), "cliente abierto");
    SetEnvironmentVariableW(L"AIMP_SYNC_USE", L"1");
    CHECK(sync_install(&cli) == 1, "sync_install");

    printf("[uso local: sale por la red]\n");
    static const uint8_t use_local[20] = { 0x04, 0x41, 0x06, 0xb1, 0x6f, 0x52, 0x99, 0x59, /* botón del transit */
                                           0x2f, 0xf7, 0x22, 0xb2, 0x09, 0x9d, 0x5b, 0x9b, /* el jugador local */
                                           0x00, 0x7c, 0x58, 0x01 };
    static const uint8_t use_npc[20] = { 0xee, 0xd1, 0x79, 0xbb, 0x3c, 0x73, 0x01, 0xa5, 0x1c, 0x34, 0x60, 0xb3,
                                         0x7e, 0x03, 0xa5, 0xdc, 0x00, 0x00, 0x00, 0x00 };
    uint8_t payload[20] __attribute__((aligned(16)));
    QueryPerformanceCounter(&now);
    net_poll(&host, now.QuadPart);
    AimpPkt hello;
    memset(&hello, 0, sizeof hello);
    net_send(&cli, now.QuadPart, &hello); /* el host aprende al cliente */
    Sleep(20);
    net_poll(&host, now.QuadPart);
    sync_frame(&cli, 16); /* fija el nivel actual en sync.c */
    uint32_t esp0, esp1;
    memcpy(payload, use_local, 20);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    ((UseFn)(void *)(img + AIMP_RVA_USE_PRODUCER))(g_use_this, NULL, payload, 0);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_uses == 1, "productor hookeado: corrió el original y el stack quedó balanceado (%u)", g_uses);
    memcpy(payload, use_npc, 20);
    ((UseFn)(void *)(img + AIMP_RVA_USE_PRODUCER))(g_use_this, NULL, payload, 0);
    CHECK(g_uses == 2, "el uso de un NPC también pasa por el original");
    static const uint8_t use_locker[20] = { 0x3e, 0x3e, 0xc3, 0x5d, 0xbe, 0xbf, 0xb3, 0xd2, /* locker: no se manda */
                                            0x2f, 0xf7, 0x22, 0xb2, 0x09, 0x9d, 0x5b, 0x9b, 0x00, 0x7c, 0x58, 0x01 };
    memcpy(payload, use_locker, 20);
    ((UseFn)(void *)(img + AIMP_RVA_USE_PRODUCER))(g_use_this, NULL, payload, 0);
    CHECK(g_uses == 3, "el uso del locker corre igual en esta máquina");
    for (int i = 0; i < AIMP_EVT_REPEAT; i++)
        sync_frame(&cli, 16);
    Sleep(30);
    net_poll(&host, now.QuadPart);
    AimpEvt e;
    int got = net_take_event(&host, &e), more = net_take_event(&host, &(AimpEvt){ 0 });
    CHECK(got && !more, "llegó un solo evento (el del botón; ni el del NPC ni el del locker se mandan)");
    CHECK(got && e.kind == AIMP_EVT_USE && e.size == 20 && e.level == 16 && memcmp(e.data, use_local, 20) == 0,
          "evento de uso con el payload intacto y el nivel 16");

    printf("[uso remoto: se aplica encolando un A 0x26 con el jugador local]\n");
    put32(img, AIMP_RVA_USE_TYPE_A, 0x26); /* el registro de RPCs ya corrió */
    AimpEvt in;
    memset(&in, 0, sizeof in);
    in.kind = AIMP_EVT_USE;
    in.size = 20;
    in.level = 16;
    in.seq = 7;
    memcpy(in.data, use_local, 20);
    memcpy(in.data + 8, "\x11\x22\x33\x44\x55\x66\x77\x88", 8); /* el personaje del otro, que acá no existe */
    cli.in[cli.in_tail] = in;
    cli.in_tail = (cli.in_tail + 1) % AIMP_EVT_QUEUE;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    sync_frame(&cli, 16);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_enq == 1 && g_enq_type == 0x26 && g_enq_size == 20, "encolado A tipo 0x%x, %u bytes (llamadas %u)",
          g_enq_type, g_enq_size, g_enq);
    CHECK(memcmp(g_enq_data, use_local, 8) == 0 && memcmp(g_enq_data + 8, "\x2f\xf7\x22\xb2\x09\x9d\x5b\x9b", 8) == 0 &&
              memcmp(g_enq_data + 16, use_local + 16, 4) == 0,
          "objetivo y flag del otro, personaje = jugador local");
    CHECK(g_uses == 3, "aplicar no vuelve a pasar por el productor (no hay eco)");
    in.level = 9;
    in.seq = 8;
    cli.in[cli.in_tail] = in;
    cli.in_tail = (cli.in_tail + 1) % AIMP_EVT_QUEUE;
    sync_frame(&cli, 16);
    CHECK(g_enq == 1, "un uso de otro nivel no se aplica");
    put32(img, AIMP_RVA_USE_TYPE_A, 0);
    in.level = 16;
    in.seq = 9;
    cli.in[cli.in_tail] = in;
    cli.in_tail = (cli.in_tail + 1) % AIMP_EVT_QUEUE;
    sync_frame(&cli, 16);
    CHECK(g_enq == 1, "sin id de RPC asignado no se encola nada");

    printf("[handlers hookeados (solo traza)]\n");
    memcpy(payload, use_local, 20);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    uint32_t ra = ((HandlerFn)(void *)(img + AIMP_RVA_USE_HANDLER_A))(g_use_this, payload);
    uint32_t rs = ((HandlerFn)(void *)(img + AIMP_RVA_USE_HANDLER_S))(g_use_this, payload);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && ra == 0x14 && rs == 0x14 && g_ha == 1 && g_hs == 1, "los dos handlers corrieron y devolvieron 0x14");
    net_close(&cli);
    net_close(&host);
    log_close();

    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "hook usar (FUN_00795d00): instalado", 1);
    expect(log, "hook handler A del uso (FUN_00418a10): instalado", 1);
    expect(log, "hook handler de sesión del uso (FUN_00418980): instalado", 1);
    expect(log, "AIMP_SYNC_USE=1 (fase 5, experimental)", 1);
    expect(log, "objetivo 044106b16f529959, personaje 2ff722b2099d5b9b (jugador local), flag 01587c00", 1);
    expect(log, "SYNC   mandado al otro jugador (evento 1, nivel 16)", 1);
    expect(log, "no se mandan los objetivos con prefijo [3e3ec35d,1e38b898]", 1);
    expect(log, "SYNC   no se manda: el objetivo empieza con un prefijo de AIMP_SYNC_USE_SKIP", 1);
    expect(log, "SYNC   mandado al otro jugador (evento 2", 0);
    expect(log, "objetivo eed179bb3c7301a5, personaje 1c3460b37e03a5dc, flag 00000000", 1);
    expect(log, "SYNC: aplicando uso remoto (evento 7): objetivo 044106b16f529959, RPC A 0x26, personaje = el jugador local (2ff722b2099d5b9b)", 1);
    expect(log, "del nivel 9; acá el nivel es 16: se ignora", 1);
    expect(log, "todavía no está asignado: se ignora el evento 9", 1);
    expect(log, "SYNC handler A del uso 1: ctx 0x", 1);
    expect(log, "SYNC handler de sesión del uso 1: ctx 0x", 1);
    printf("%s (%d fallas)\n", g_fail ? "TEST_SYNC CON FALLAS" : "TEST_SYNC OK", g_fail);
    return g_fail ? 1 : 0;
}
