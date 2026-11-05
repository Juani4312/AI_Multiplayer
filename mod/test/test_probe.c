/* Test de la sonda (probe.c + log.c) contra memoria simulada. Correr con wine y
 * AIMP_LOG=<archivo>. Arma una "imagen" de AI.exe falsa del tamaño real, pone los
 * objetos del juego en los offsets de game.h, simula frames y verifica el log. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "game.h"
#include "log.h"
#include "probe.h"
#include "trace.h"

/* Wrappers falsos con el mismo prólogo de 14 bytes que FUN_00a2a290/FUN_00a2a810:
 * push ebp; mov ebp,esp; mov eax,[ebp+10]; mov ecx,[ebp+0C]; mov edx,[ebp+8]; push eax; push ecx.
 * Después llaman a fake_enqueue(tipo, datos, tamaño), que cuenta. */
uint32_t g_enq_count, g_enq_last_type, g_enq_last_size;
__attribute__((cdecl, used)) void fake_enqueue(uint32_t type, const void *data, uint32_t size)
{
    g_enq_count++;
    g_enq_last_type = type & 0xFFFF;
    g_enq_last_size = size;
}
#define FAKE_WRAP(name)                                                                   \
    __asm__(".text\n.globl _" name "\n_" name ":\n"                                     \
            "  .byte 0x55,0x8B,0xEC,0x8B,0x45,0x10,0x8B,0x4D,0x0C,0x8B,0x55,0x08,0x50,0x51\n" \
            "  push %edx\n  call _fake_enqueue\n  add $12, %esp\n  pop %ebp\n  ret\n");
FAKE_WRAP("fake_wrap_a")
FAKE_WRAP("fake_wrap_b")
uint32_t fake_wrap_a(uint32_t type, const void *data, uint32_t size);
uint32_t fake_wrap_b(uint32_t type, const void *data, uint32_t size);

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

static int g_fail;

static void *obj(size_t n) { return calloc(1, n); }
static void put32(void *base, uint32_t off, uint32_t v) { memcpy((uint8_t *)base + off, &v, 4); }
static void putp(void *base, uint32_t off, void *p) { put32(base, off, (uint32_t)(uintptr_t)p); }
static void putf(void *base, uint32_t off, float v) { memcpy((uint8_t *)base + off, &v, 4); }
static void putv(void *base, uint32_t off, float x, float y, float z)
{
    putf(base, off, x);
    putf(base, off + 4, y);
    putf(base, off + 8, z);
}

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
    char *buf = malloc((size_t)n + 1);
    fread(buf, 1, (size_t)n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void expect(const char *log, const char *needle, int should)
{
    int has = strstr(log, needle) != NULL;
    printf("  %s %s \"%s\"\n", has == should ? "ok   " : "FALLA", should ? "contiene" : "no contiene", needle);
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
    const uint32_t b = (uint32_t)g_aimp_exe;

    void *root = obj(0x100), *mm = obj(0x300), *mode = obj(0x110), *levels = obj(0x2C00), *levels_pp = obj(4);
    void *cm = obj(0x200), *parr = obj(16), *narr = obj(16), *pl = obj(0x400), *def = obj(0x40), *ctrl = obj(0x60);
    void *npc0 = obj(0x400), *npc1 = obj(0x400), *ndef = obj(0x40);
    void *cmgr = obj(0x220), *cam = obj(0x60), *netz = obj(0x6A0), *qa = obj(0x40), *qbuf = obj(0x80), *sess = obj(0x40);

    /* Globales de .data */
    put32(img, AIMP_RVA_FRAME_COUNTER, 1000);
    double dt = 1.0 / 60.0, gt = 12.5;
    memcpy(img + AIMP_RVA_DT_REAL, &dt, 8);
    memcpy(img + AIMP_RVA_DT_SCALED, &dt, 8);
    memcpy(img + AIMP_RVA_GAME_TIME, &gt, 8);
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(img, AIMP_RVA_NET_SESSION, sess);
    putp(img, AIMP_RVA_QUEUE_A, qa); /* el global de la cola A == g_root+0x50 */
    /* Lista de endpoints de la cola A: 2 endpoints dentro de la imagen (como en el juego). */
    uint8_t *ep0 = img + 0x1236100, *ep1 = img + 0x1236120;
    putp(ep1, EP_NEXT, ep0);
    put32(ep1, EP_FN, b + 0x397490);
    put32(ep1, EP_CTX, 0x1234);
    ((uint16_t *)(ep1 + EP_TYPE))[0] = 0;
    put32(ep0, EP_FN, b + 0x7DDC60);
    ((uint16_t *)(ep0 + EP_TYPE))[0] = 1;
    putp(img, AIMP_RVA_RPC_LIST_A, ep1);
    put32(qa, QA_NHANDLERS, 2);
    putp(img, AIMP_RVA_LEVEL_LIST_PP, levels_pp); /* doble indirección */
    putp(levels_pp, 0, levels);

    strcpy((char *)levels + 0 * LEVEL_NAME_STRIDE, "FE_LEVEL");
    strcpy((char *)levels + 7 * LEVEL_NAME_STRIDE, "TECH_HUB");
    strcpy((char *)levels + 8 * LEVEL_NAME_STRIDE, "BSP_TORRENS");
    put32(levels, LEVELS_COUNT, 10);
    put32(levels, LEVELS_CURRENT, 7);

    putp(root, ROOT_NETZ, netz);
    putp(root, ROOT_CHARMGR, cm);
    putp(root, ROOT_MODEMGR, mm);
    putp(root, ROOT_QUEUE_A, qa);
    putp(root, ROOT_SESSION, sess);
    putp(root, ROOT_CAMMGR, cmgr);

    putp(mm, MM_MODE, mode);
    put32(mm, MM_INDEX, 3);
    put32(mm, MM_STATE, 5);
    put32(mode, 0, b + AIMP_RVA_VT_LEVEL_MODE);
    put32(mode, MODE_LEVEL_INDEX, 7);

    putp(cm, CM_PLAYERS, parr);
    put32(cm, CM_NPLAYERS, 1);
    putp(cm, CM_NPCS, narr);
    put32(cm, CM_NNPCS, 2);
    putp(parr, 0, pl);
    putp(narr, 0, npc0);
    putp(narr, 4, npc1);

    put32(pl, 0, b + AIMP_RVA_VT_CHARACTER);
    strcpy((char *)pl + 0x10, "Ripley");
    putv(pl, CHR_POS, 1.0f, 2.0f, 3.0f);
    putf(pl, CHR_YAW, 1.5707964f);
    putp(pl, CHR_DEF, def);
    put32(def, DEF_TYPE, 1);
    ((uint8_t *)def)[DEF_FLAG_1C] = 1;
    ((uint8_t *)def)[DEF_FLAG_1F] = 1;
    putp(pl, CHR_CTRL, ctrl);
    ((uint8_t *)ctrl)[CTRL_ACTIVE] = 1;

    put32(npc0, 0, b + AIMP_RVA_VT_CHARACTER);
    strcpy((char *)npc0 + 0x10, "Alien");
    putv(npc0, CHR_POS, 10.0f, 0.0f, -5.0f);
    putp(npc0, CHR_DEF, ndef);
    put32(ndef, DEF_TYPE, 2);
    put32(npc1, 0, 0x12345678); /* vtable rara: tiene que decir DISTINTA */
    putv(npc1, CHR_POS, -1.0f, 0.5f, 7.25f);

    putp(cmgr, CAMMGR_ACTIVE, cam);
    putf(cam, CAM_QUAT + 12, 1.0f); /* (0, 0, 0, 1) */
    putv(cam, CAM_POS, 1.0f, 3.6f, 3.0f);

    put32(netz, NETZ_MASK, 5);
    putp(qa, QA_DATA, qbuf);
    put32(qa, QA_SIZE, 24);
    ((uint8_t *)qbuf)[0] = 3; /* [u16 tipo = 3][payload] */
    put32(sess, SESSION_QSIZE, 16); /* cola B */
    putp(sess, SESSION_QDATA, qbuf);

    probe_init();
    int tr_ok = trace_install_at((void *)fake_wrap_a, (void *)fake_wrap_b);
    static const uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    const int64_t step = f.QuadPart / 60;
    int64_t now = 1000000;
    for (int i = 1; i <= 420; i++) { /* 7 s simulados */
        if (i == 70)
            putv(pl, CHR_POS, 3.0f, 2.0f, 3.0f); /* se mueve 2 m en x */
        if (i == 100) {
            put32(levels, LEVELS_CURRENT, 8);
            put32(mm, MM_STATE, 3);
        }
        if (i == 110)
            put32(mm, MM_STATE, 5);
        if (i == 200)
            put32(netz, NETZ_TRANSPORT, 0xCAFE0000u); /* inesperado */
        if (i == 300)
            probe_request_dump();
        put32(img, AIMP_RVA_FRAME_COUNTER, 1000 + (uint32_t)i);
        fake_wrap_a(3, payload, 8);
        if (i % 10 == 0)
            fake_wrap_b(0x10011, payload, 4); /* bits altos basura: el tipo es u16 */
        probe_after_run((uintptr_t)mm, 1, step / 2, now);
        now += step;
    }
    probe_after_run((uintptr_t)mm, 0, step, now);
    log_close();

    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log (¿AIMP_LOG definido?)\n");
        return 1;
    }
    printf("[probe] log de %u bytes\n", (unsigned)strlen(log));
    expect(log, "FASE 1: primer frame dentro de ModeManager::Run", 1);
    expect(log, "frame 1: Run=", 1);
    expect(log, "frame 4: Run=", 0);
    expect(log, "módulos cargados:", 1);
    expect(log, "estado 5 (corriendo) idx 3", 1);
    expect(log, "(modo-de-nivel, nivel 7)", 1);
    expect(log, "nivel 7/10 \"TECH_HUB\"", 1);
    expect(log, "lista de niveles en 0x", 1);
    expect(log, "nivel   8: BSP_TORRENS", 1);
    expect(log, "EV nivel actual 7 \"TECH_HUB\" -> 8 \"BSP_TORRENS\"", 1);
    expect(log, "EV frame 100: ModeManager estado 5 (corriendo) -> 3 (cargar)", 1);
    expect(log, "EV frame 110: ModeManager estado 3 (cargar) -> 5 (corriendo)", 1);
    expect(log, "Character(ok) def 0x", 1);
    expect(log, "tipo 1 1c=1 1f=1 ctrl 0x", 1);
    expect(log, "activo 1", 1);
    expect(log, "J0 pos=(1.000, 2.000, 3.000) yaw=1.5708 rad (90.0 grados)", 1);
    expect(log, "J0 pos=(3.000, 2.000, 3.000)", 1);
    expect(log, "se movió 2.000 desde el ST anterior", 1);
    expect(log, "|q|^2=1.0000", 1);
    expect(log, "cam - J0 = (0.000, 1.600, 0.000)", 1);
    expect(log, "volcado (automático, apareció el jugador 0)", 1);
    expect(log, "volcado (automático, 5 s después de aparecer el jugador)", 1);
    expect(log, "volcado (pedido)", 1);
    expect(log, "\"Alien\"", 1);
    expect(log, "\"Ripley\"", 1);
    expect(log, "N[1] 0x", 1);
    expect(log, "DISTINTA", 1);
    expect(log, "jugador 0 (Character) @ 0x", 1);
    expect(log, "+070: 3f800000 40000000 40400000", 1);
    expect(log, "cola A: buffer @ 0x", 1);
    expect(log, "tamaño tras Run máx 24, frames con datos 61/61", 1);
    expect(log, "(contador del juego 1062, +61)", 1);
    expect(log, "máscara 00000000:00000005", 1);
    expect(log, "EV NETZ transporte 0x00000000 -> 0xcafe0000  <-- INESPERADO: hay transporte", 1);
    expect(log, "INESPERADO: hay proveedor", 0);
    expect(log, "ModeManager::Run devolvió 0", 1);
    expect(log, "OJO:", 0);
    expect(log, "EV tablas de handlers: cola A 2, sesión 0", 1);
    expect(log, "RPC   A tipo 0x0000 handler rva 0x397490 ctx 0x00001234 endpoint rva 0x1236120", 1);
    expect(log, "RPC   A tipo 0x0001 handler rva 0x7ddc60", 1);
    expect(log, "RPC lista A: 2 endpoints", 1);
    expect(log, "RPC lista sesión: 0 endpoints", 1);
    expect(log, "cola B (sesión, se descarta sin proveedor): tamaño tras Run máx 16, frames con datos 61/61", 1);
    expect(log, "hook encolar A (FUN_00a2a290): instalado", 1);
    expect(log, "hook encolar B (FUN_00a2a810): instalado", 1);
    expect(log, "TR cola A: tipo nuevo 0x0003 (8 bytes), call site rva 0x", 1);
    expect(log, "TR cola B: tipo nuevo 0x0011 (4 bytes), call site rva 0x", 1);
    expect(log, "TR cola A: 0x0003 n=", 1);
    expect(log, "TR   A 0x0003 n=420 b=3360 max=8 p sitios 0x", 1);
    expect(log, "TR   B 0x0011 n=42 b=168 max=4 p sitios 0x", 1);
    expect(log, "TR   cola B: 1 tipos distintos", 1);
    printf("  %s trace_install_at=%d, original ejecutado %u veces (esperado 462), último tipo 0x%x tamaño %u\n",
           tr_ok && g_enq_count == 462 && g_enq_last_type == 0x11 && g_enq_last_size == 4 ? "ok   " : "FALLA", tr_ok,
           g_enq_count, g_enq_last_type, g_enq_last_size);
    if (!(tr_ok && g_enq_count == 462 && g_enq_last_type == 0x11 && g_enq_last_size == 4))
        g_fail++;
    printf("%s (%d fallas)\n", g_fail ? "TEST_PROBE CON FALLAS" : "TEST_PROBE OK", g_fail);
    return g_fail ? 1 : 0;
}
