/* Test de la fase 4 (ghost.c), con wine. Arma una imagen de AI.exe falsa con dos stubs
 * de código máquina en las RVAs reales del spawn de autoridad (FUN_00797490) y del
 * teleport (FUN_007c6330). Los stubs tienen los mismos prólogos y el mismo `ret N` que
 * los originales, y llaman a funciones de prueba en C. Así se prueba el hook, la captura
 * de pedidos, la máquina de estados, el controlador y, sobre todo, las convenciones de
 * llamada: si no coincidieran, el stack se desbalancea y el test se cae. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "game.h"
#include "ghost.h"
#include "log.h"

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
static uint32_t get32(const void *base, uint32_t off)
{
    uint32_t v;
    memcpy(&v, (const uint8_t *)base + off, 4);
    return v;
}
static float getf(const void *base, uint32_t off)
{
    float v;
    memcpy(&v, (const uint8_t *)base + off, 4);
    return v;
}

/* CharacterManager falso */
static void *g_cm, *g_parr, *g_narr, *g_this;
static uint32_t g_spawns, g_bad_this, g_teleports, g_last_tp_chr;

static void append(uint32_t f_arr, uint32_t f_n, void *c)
{
    uint8_t *arr = (uint8_t *)(uintptr_t)get32(g_cm, f_arr);
    uint32_t n = get32(g_cm, f_n);
    putp(arr, n * 4, c);
    put32(g_cm, f_n, n + 1);
}

/* Lo llama el stub del spawn: crea el personaje y lo agrega al vector que corresponde
 * (hace de SpawnCharacter + FinishSpawning). */
static void __attribute__((cdecl)) fake_spawn(uint32_t self, const uint8_t *req)
{
    g_spawns++;
    if (self != (uint32_t)(uintptr_t)g_this)
        g_bad_this++;
    uint8_t *c = obj(0x400), *d = obj(0x40);
    put32(c, 0, (uint32_t)g_aimp_exe + AIMP_RVA_VT_CHARACTER);
    memcpy(c + CHR_ID, req + SREQ_ID, 8);
    uint32_t type = get32(req, SREQ_TYPE);
    put32(d, DEF_TYPE, type);
    putp(c, CHR_DEF, d);
    if (type == 0) {
        uint8_t *ctrl = obj(0x60);
        d[DEF_FLAG_1C] = d[DEF_FLAG_1F] = 1;
        ctrl[CTRL_ACTIVE] = 1;
        putp(c, CHR_CTRL, ctrl);
        append(CM_PLAYERS, CM_NPLAYERS, c);
    } else {
        append(CM_NPCS, CM_NNPCS, c);
    }
}

static void __attribute__((cdecl)) fake_teleport(uint32_t pj, const float *pos, float yaw)
{
    g_teleports++;
    g_last_tp_chr = pj;
    memcpy((uint8_t *)(uintptr_t)pj + CHR_POS, pos, 12);
    memcpy((uint8_t *)(uintptr_t)pj + CHR_YAW, &yaw, 4);
}

static void emit(uint8_t **p, const void *bytes, size_t n)
{
    memcpy(*p, bytes, n);
    *p += n;
}

static void emit_call(uint8_t **p, void *fn)
{
    uint8_t mov = 0xB8; /* mov eax, imm32 */
    uint32_t a = (uint32_t)(uintptr_t)fn;
    emit(p, &mov, 1);
    emit(p, &a, 4);
    emit(p, "\xFF\xD0", 2); /* call eax */
}

static void build_stubs(uint8_t *img)
{
    DWORD old;
    uint8_t *p = img + AIMP_RVA_SPAWN_AUTH;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SPAWN_AUTH_PROLOGUE, sizeof AIMP_SPAWN_AUTH_PROLOGUE); /* push ebp ... mov edi,ecx */
    emit(&p, "\xFF\x75\x08\x57", 4);                                     /* push [ebp+8] (pedido); push edi (this) */
    emit_call(&p, (void *)fake_spawn);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5B\x5D\xC2\x08\x00", 10); /* add esp,8; pop edi/esi/ebx/ebp; ret 8 */

    p = img + AIMP_RVA_CHR_TELEPORT;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_CHR_TELEPORT_PROLOGUE, sizeof AIMP_CHR_TELEPORT_PROLOGUE); /* ebx=pos*, ebp=p3, edi=pj */
    emit(&p, "\xFF\x74\x24\x14\x53\x57", 6);                                  /* push yaw; push pos*; push pj */
    emit_call(&p, (void *)fake_teleport);
    emit(&p, "\x83\xC4\x0C\x5F\x5D\x5B\xC2\x0C\x00", 9); /* add esp,0xC; pop edi/ebp/ebx; ret 0xC */
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

typedef void(__attribute__((fastcall)) * SpawnFn)(void *self, void *edx, void *req, uint32_t flag);

static void make_req(uint8_t *r, uint32_t key, uint32_t type, const char *id8, const char *name, const char *comp)
{
    memset(r, 0, SPAWN_REQ_SIZE);
    put32(r, SREQ_KEY, key);
    put32(r, SREQ_TYPE, type);
    memcpy(r + SREQ_ID, id8, 8);
    strcpy((char *)r + SREQ_NAME, name);
    strcpy((char *)r + SREQ_COMPOSITE, comp);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    SetEnvironmentVariableW(L"AIMP_GHOST", L"1");
    SetEnvironmentVariableW(L"AIMP_GHOST_DELAY_MS", L"100");
    log_open();

    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stubs(img);
    void *root = obj(0x100);
    g_cm = obj(0x200);
    g_parr = obj(64 * 4);
    g_narr = obj(64 * 4);
    g_this = obj(0x40);
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(root, ROOT_CHARMGR, g_cm);
    putp(g_cm, CM_PLAYERS, g_parr);
    putp(g_cm, CM_NPCS, g_narr);

    printf("[instalación]\n");
    CHECK(ghost_install() == 1, "ghost_install");

    printf("[carga del nivel: 3 pedidos por la función hookeada]\n");
    SpawnFn spawn = (SpawnFn)(void *)(img + AIMP_RVA_SPAWN_AUTH);
    uint8_t r[SPAWN_REQ_SIZE];
    uint32_t esp0, esp1;
    make_req(r, 1, 3, "\x1c\x34\x60\xb3\x0e\x41\xd9\x44", "NPC_HUMANO", "HAB_A");
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    spawn(g_this, NULL, r, 0);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1, "stack balanceado después del spawn hookeado (0x%08x / 0x%08x)", esp0, esp1);
    make_req(r, 2, 0, "\x2f\xf7\x22\xb2\xb8\x9a\x51\x72", "Ripley", "CUTSCENE");
    spawn(g_this, NULL, r, 0);
    make_req(r, 3, 6, "\x1c\x34\x60\xb3\x7e\xd2\xd9\x3d", "ANDROIDE", "HAB_B");
    spawn(g_this, NULL, r, 0);
    CHECK(g_spawns == 3 && g_bad_this == 0, "el original corrió 3 veces con el this correcto (%u, malos %u)", g_spawns,
          g_bad_this);
    CHECK(get32(g_cm, CM_NPLAYERS) == 1 && get32(g_cm, CM_NNPCS) == 2, "1 jugador y 2 NPCs");
    uint8_t *local = (uint8_t *)(uintptr_t)get32(g_parr, 0);
    float lp[3] = { 1.0f, 2.0f, 3.0f }, lyaw = 0.25f;
    memcpy(local + CHR_POS, lp, 12);
    memcpy(local + CHR_YAW, &lyaw, 4);

    printf("[frames: espera, clon, controlador, teleport]\n");
    AimpNet net;
    memset(&net, 0, sizeof net);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    int64_t now = 1000000, step = f.QuadPart / 60;
    for (int i = 0; i < 30; i++, now += step) /* 0,5 s: el clon se pide a los 100 ms */
        ghost_frame(now, &net, 16);
    CHECK(g_spawns == 4, "se pidió el clon (spawns %u)", g_spawns);
    CHECK(get32(g_cm, CM_NPLAYERS) == 2, "el clon es el jugador 1 (%u jugadores)", get32(g_cm, CM_NPLAYERS));
    uint8_t *clone = (uint8_t *)(uintptr_t)get32(g_parr, 4);
    uint8_t *cctrl = clone ? (uint8_t *)(uintptr_t)get32(clone, CHR_CTRL) : NULL;
    CHECK(clone && memcmp(clone + CHR_ID, "\x2f\xf7\x22\xb2\x41\x49\x4d\x50", 8) == 0, "id del clon: el del jugador con 'AIMP'");
    CHECK(cctrl && cctrl[CTRL_ACTIVE] == 0, "controlador del clon desactivado");
    uint8_t *lctrl = (uint8_t *)(uintptr_t)get32(local, CHR_CTRL);
    CHECK(lctrl[CTRL_ACTIVE] == 1, "el controlador del jugador local sigue activo");
    CHECK(g_teleports > 0 && g_last_tp_chr == (uint32_t)(uintptr_t)clone, "teleports al clon: %u", g_teleports);
    CHECK(clone && getf(clone, CHR_POS) == 2.5f && getf(clone, CHR_POS + 4) == 2.0f && getf(clone, CHR_YAW) == 0.25f,
          "sin red: el clon a 1,5 m en x del jugador (x=%.2f yaw=%.2f)", clone ? (double)getf(clone, CHR_POS) : 0.0,
          clone ? (double)getf(clone, CHR_YAW) : 0.0);

    printf("[con estado remoto del mismo nivel]\n");
    net.have_remote = 1;
    net.remote.flags = AIMP_FLAG_PLAYER;
    net.remote.level = 16;
    net.remote.pos[0] = 50.0f;
    net.remote.pos[1] = 1.0f;
    net.remote.pos[2] = -20.0f;
    net.remote.yaw = -1.0f;
    for (int i = 0; i < 70; i++, now += step) /* > 1 s: sale una línea de resumen con [red] */
        ghost_frame(now, &net, 16);
    CHECK(getf(clone, CHR_POS) == 50.0f && getf(clone, CHR_POS + 8) == -20.0f && getf(clone, CHR_YAW) == -1.0f,
          "el clon sigue al remoto: (%.1f, %.1f, %.1f) yaw %.1f", (double)getf(clone, CHR_POS), (double)getf(clone, CHR_POS + 4),
          (double)getf(clone, CHR_POS + 8), (double)getf(clone, CHR_YAW));
    net.remote.level = 9; /* otro nivel: vuelve al modo local */
    for (int i = 0; i < 5; i++, now += step)
        ghost_frame(now, &net, 16);
    CHECK(getf(clone, CHR_POS) == 2.5f, "remoto en otro nivel: vuelve al modo local (x=%.2f)", (double)getf(clone, CHR_POS));
    uint32_t tp_before = g_teleports;
    for (int i = 0; i < 20; i++, now += step) /* quieto: sin teleports hasta los 500 ms */
        ghost_frame(now, &net, 16);
    CHECK(g_teleports - tp_before <= 1, "quieto: %u teleports en 333 ms", g_teleports - tp_before);

    printf("[cambio de nivel]\n");
    put32(g_cm, CM_NPLAYERS, 0);
    put32(g_cm, CM_NNPCS, 0);
    ghost_frame(now += step, &net, 16);
    make_req(r, 2, 0, "\x2f\xf7\x22\xb2\x01\x02\x03\x04", "Ripley", "CUTSCENE");
    spawn(g_this, NULL, r, 0);
    for (int i = 0; i < 30; i++, now += step)
        ghost_frame(now, &net, 17);
    CHECK(g_spawns == 6 && get32(g_cm, CM_NPLAYERS) == 2, "en el nivel nuevo se clona otra vez (spawns %u, jugadores %u)", g_spawns,
          get32(g_cm, CM_NPLAYERS));
    log_close();

    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "hook spawn de autoridad (FUN_00797490): instalado", 1);
    expect(log, "AIMP_GHOST=1 (fase 4, experimental): 100 ms", 1);
    expect(log, "| clave 1 tipo 3 id 1c3460b30e41d944 | \"NPC_HUMANO\" / \"HAB_A\"", 1);
    expect(log, "| clave 2 tipo 0 id 2ff722b2b89a5172 | \"Ripley\" / \"CUTSCENE\"", 1);
    expect(log, "SP   volcado @ 0x", 1);
    expect(log, "GHOST: clonando el pedido (clave 2 -> 64, id 2ff722b2b89a5172 -> 2ff722b241494d50)", 1);
    expect(log, "GHOST: el spawn volvió; ahora 2 jugadores, 2 NPCs", 1);
    expect(log, "GHOST: apareció el clon: jugadores[1]", 1);
    expect(log, "tipo 0 flags 1c=1 1f=1", 1);
    expect(log, "(activo 1)", 1);
    expect(log, "GHOST: desactivando el controlador del clon", 1);
    expect(log, "GHOST: primer teletransporte a (2.50, 2.00, 3.00) yaw 0.250 [local +1,5 m en x]", 1);
    expect(log, "objetivo (50.00, 1.00, -20.00) [red]", 1);
    expect(log, "GHOST: cambió el jugador local", 1);
    expect(log, "GHOST: clonando el pedido (clave 2 -> 65, id 2ff722b201020304 -> 2ff722b242494d50)", 1);
    printf("%s (%d fallas)\n", g_fail ? "TEST_GHOST CON FALLAS" : "TEST_GHOST OK", g_fail);
    return g_fail ? 1 : 0;
}
