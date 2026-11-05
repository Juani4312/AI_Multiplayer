/* Test de la fase 4 (ghost.c), con wine. Arma una imagen de AI.exe falsa con stubs de
 * código máquina en las RVAs reales de:
 *   - el spawn de autoridad (FUN_00797490, ret 8): solo registra el pedido (en retail no crea nada);
 *   - SpawnCharacter (FUN_007d3b50, ret 4): crea el personaje y lo agrega al vector;
 *   - el teleport (FUN_007c6330, ret 0xC): escribe posición y yaw.
 * Los stubs tienen los mismos prólogos y el mismo `ret N` que los originales y llaman a
 * funciones de prueba en C. Si las convenciones de llamada no coincidieran, el stack se
 * desbalancea y el test se cae. Prueba el modo 2 (NPC existente y después clon). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "crash.h"
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
static void *g_cm, *g_parr, *g_narr, *g_this, *g_sc_this;
static uint32_t g_auths, g_spawns, g_bad_this, g_teleports, g_last_tp_chr, g_misaligned;
static uint8_t g_last_sc_req[SPAWN_REQ_SIZE];

static void append(uint32_t f_arr, uint32_t f_n, void *c)
{
    uint8_t *arr = (uint8_t *)(uintptr_t)get32(g_cm, f_arr);
    uint32_t n = get32(g_cm, f_n);
    putp(arr, n * 4, c);
    put32(g_cm, f_n, n + 1);
}

static void __attribute__((cdecl)) fake_auth(uint32_t self, const uint8_t *req)
{
    g_auths++;
    if (self != (uint32_t)(uintptr_t)g_this)
        g_bad_this++;
    (void)req;
}

/* SpawnCharacter + FinishSpawning en uno: crea el personaje y lo agrega al vector. */
static uint32_t __attribute__((cdecl)) fake_spawnchar(uint32_t self, const uint8_t *req)
{
    g_spawns++;
    if (self != (uint32_t)(uintptr_t)g_sc_this)
        g_bad_this++;
    if ((uintptr_t)req & 15) /* el handler de sesión 0x2E lo pasa alineado a 16 */
        g_misaligned++;
    memcpy(g_last_sc_req, req, SPAWN_REQ_SIZE);
    uint8_t *c = obj(0x400), *d = obj(0x40);
    put32(c, 0, (uint32_t)g_aimp_exe + AIMP_RVA_VT_CHARACTER);
    memcpy(c + CHR_ID, req + SREQ_ID, 8);
    memcpy(c + CHR_POS, req + SREQ_POS, 12);
    float w = 1.0f; /* cuarto componente de la posición: el teleport tiene que conservarlo */
    memcpy(c + CHR_POS + 12, &w, 4);
    strcpy((char *)c + 0x10, (const char *)req + SREQ_COMPOSITE);
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
    return (uint32_t)(uintptr_t)c;
}

/* Como SetTransform: copia 16 bytes con MOVAPS, así que exige pos alineado a 16. */
static void __attribute__((cdecl)) fake_teleport(uint32_t pj, const float *pos, float yaw)
{
    g_teleports++;
    g_last_tp_chr = pj;
    if ((uintptr_t)pos & 15)
        g_misaligned++;
    memcpy((uint8_t *)(uintptr_t)pj + CHR_POS, pos, 16);
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
    emit_call(&p, (void *)fake_auth);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5B\x5D\xC2\x08\x00", 10); /* add esp,8; pop edi/esi/ebx/ebp; ret 8 */

    p = img + AIMP_RVA_SPAWN_CHAR;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SPAWN_CHAR_PROLOGUE, sizeof AIMP_SPAWN_CHAR_PROLOGUE); /* push ecx/ebx/ebp/esi/edi; mov edi,[esp+18] */
    emit(&p, "\x8B\x4C\x24\x10\x57\x51", 6);                             /* mov ecx,[esp+10] (this); push edi; push ecx */
    emit_call(&p, (void *)fake_spawnchar);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5D\x5B\x59\xC2\x04\x00", 11); /* add esp,8; pop edi/esi/ebp/ebx/ecx; ret 4 */

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

static volatile int g_fixed;
static LONG CALLBACK fixer(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    ep->ContextRecord->Eip += 6;
    g_fixed++;
    return EXCEPTION_CONTINUE_EXECUTION;
}

typedef void(__attribute__((fastcall)) * AuthFn)(void *self, void *edx, void *req, uint32_t flag);
typedef uint32_t(__attribute__((fastcall)) * ScFn)(void *self, void *edx, void *req);

static void make_req(uint8_t *r, uint32_t key, uint32_t type, const char *id8, const char *def, float x)
{
    memset(r, 0, SPAWN_REQ_SIZE);
    put32(r, SREQ_KEY, key);
    put32(r, SREQ_KEY2, key);
    put32(r, SREQ_TYPE, type);
    memcpy(r + SREQ_ID, id8, 8);
    strcpy((char *)r + SREQ_COMPOSITE, def);
    float pos[3] = { x, 13.0f, 30.0f }, yaw = -3.0f;
    memcpy(r + SREQ_POS, pos, 12);
    memcpy(r + SREQ_YAW, &yaw, 4);
}

/* Lo que hace el juego al cargar un nivel, según lo que se sabe: spawn de autoridad
 * (replicación) y SpawnCharacter (creación) con el mismo pedido. */
static void game_spawn(uint8_t *img, uint8_t *r)
{
    ((AuthFn)(void *)(img + AIMP_RVA_SPAWN_AUTH))(g_this, NULL, r, 0);
    ((ScFn)(void *)(img + AIMP_RVA_SPAWN_CHAR))(g_sc_this, NULL, r);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    SetEnvironmentVariableW(L"AIMP_GHOST", L"2");
    SetEnvironmentVariableW(L"AIMP_GHOST_DELAY_MS", L"100");
    SetEnvironmentVariableW(L"AIMP_GHOST_NPC_MS", L"300");
    SetEnvironmentVariableW(L"AIMP_GHOST_NOAUTH", L"1");
    log_open();

    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stubs(img);
    void *root = obj(0x100), *obs_vt = obj(16), *obs = obj(16);
    g_cm = obj(0x5000);
    g_parr = obj(64 * 4);
    g_narr = obj(64 * 4);
    g_this = obj(0x40);
    g_sc_this = obj(0x40);
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(root, ROOT_CHARMGR, g_cm);
    putp(g_cm, CM_PLAYERS, g_parr);
    putp(g_cm, CM_NPCS, g_narr);
    /* un observador en la lista del RPC (en el juego están vacías) */
    putp(obs, 0, obs_vt);
    put32(obs_vt, 4, (uint32_t)g_aimp_exe + 0x123456);
    putp(img, AIMP_RVA_OBS_RPC_PRE, obs);

    printf("[instalación]\n");
    CHECK(ghost_install() == 1, "ghost_install");

    printf("[carga del nivel: 3 pedidos por los dos hooks]\n");
    uint8_t r[SPAWN_REQ_SIZE] __attribute__((aligned(16)));
    uint32_t esp0, esp1;
    make_req(r, 1, 0, "\x2f\xf7\x22\xb2\x09\x9d\x5b\x9b", "THE_PLAYER", 4.5f);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    game_spawn(img, r);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1, "stack balanceado después de los dos spawns hookeados (0x%08x / 0x%08x)", esp0, esp1);
    make_req(r, 2, 6, "\x1c\x34\x60\xb3\x7e\x03\xa5\xdc", "INNOCENT", 29.0f);
    game_spawn(img, r);
    make_req(r, 6, 1, "\x1c\x34\x60\xb3\x21\x30\x69\x3e", "ALIEN", 5.0f);
    game_spawn(img, r);
    CHECK(g_auths == 3 && g_spawns == 3 && g_bad_this == 0, "originales: autoridad %u, SpawnCharacter %u, this malos %u", g_auths,
          g_spawns, g_bad_this);
    CHECK(get32(g_cm, CM_NPLAYERS) == 1 && get32(g_cm, CM_NNPCS) == 2, "1 jugador y 2 NPCs");
    uint8_t *local = (uint8_t *)(uintptr_t)get32(g_parr, 0), *innocent = (uint8_t *)(uintptr_t)get32(g_narr, 0);
    /* bloque 3 del CharacterManager = el del INNOCENT, autoritativo */
    uint8_t *blk3 = (uint8_t *)g_cm + CM_BLOCKS + 3 * CM_BLOCK_SIZE;
    putp(blk3, BLOCK_CHARACTER, innocent);
    blk3[BLOCK_AUTHORITY] = 1;
    float lp[3] = { 1.0f, 2.0f, 3.0f }, lyaw = 0.25f;
    memcpy(local + CHR_POS, lp, 12);
    memcpy(local + CHR_YAW, &lyaw, 4);

    printf("[modo NPC: a los 100 ms se mueve el INNOCENT]\n");
    AimpNet net;
    memset(&net, 0, sizeof net);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    int64_t now = 1000000, step = f.QuadPart / 60;
    for (int i = 0; i < 12; i++, now += step) /* 200 ms */
        ghost_frame(now, &net, 16);
    CHECK(getf(innocent, CHR_POS) == 2.5f && getf(innocent, CHR_YAW) == 0.25f, "el INNOCENT a 1,5 m en x del jugador (x=%.2f)",
          (double)getf(innocent, CHR_POS));
    CHECK(g_spawns == 3, "todavía no se clonó (SpawnCharacter %u)", g_spawns);
    CHECK(blk3[BLOCK_AUTHORITY] == 0, "AIMP_GHOST_NOAUTH: el bloque del avatar quedó no autoritativo (%u)", blk3[BLOCK_AUTHORITY]);

    printf("[fin del modo NPC (300 ms): clon por SpawnCharacter]\n");
    for (int i = 0; i < 24; i++, now += step) /* 400 ms */
        ghost_frame(now, &net, 16);
    CHECK(g_spawns == 4 && g_auths == 3, "el clon pasó por SpawnCharacter y no por la autoridad (SC %u, autoridad %u)", g_spawns,
          g_auths);
    CHECK(get32(g_last_sc_req, SREQ_KEY) == 64 && get32(g_last_sc_req, SREQ_KEY2) == 0,
          "clave nueva en +0x00; nada más allá de los 0xF0 bytes del mensaje (%u / +0xF0 = %u)", get32(g_last_sc_req, SREQ_KEY),
          get32(g_last_sc_req, SREQ_KEY2));
    CHECK(get32(g_cm, CM_NPLAYERS) == 2, "el clon es el jugador 1 (%u jugadores)", get32(g_cm, CM_NPLAYERS));
    uint8_t *clone = (uint8_t *)(uintptr_t)get32(g_parr, 4);
    uint8_t *cctrl = clone ? (uint8_t *)(uintptr_t)get32(clone, CHR_CTRL) : NULL;
    CHECK(clone && memcmp(clone + CHR_ID, "\x2f\xf7\x22\xb2\x41\x49\x4d\x50", 8) == 0, "id del clon: el del jugador con 'AIMP'");
    CHECK(cctrl && cctrl[CTRL_ACTIVE] == 0, "controlador del clon desactivado");
    CHECK(((uint8_t *)(uintptr_t)get32(local, CHR_CTRL))[CTRL_ACTIVE] == 1, "el controlador del jugador local sigue activo");
    CHECK(clone && getf(clone, CHR_POS) == 2.5f && getf(clone, CHR_POS + 12) == 1.0f, "el clon a 1,5 m en x, con w conservado (x=%.2f w=%.2f)",
          clone ? (double)getf(clone, CHR_POS) : 0.0, clone ? (double)getf(clone, CHR_POS + 12) : 0.0);
    CHECK(getf(innocent, CHR_POS + 12) == 1.0f, "el INNOCENT también conserva w (%.2f)", (double)getf(innocent, CHR_POS + 12));
    float inn_x = getf(innocent, CHR_POS);
    memcpy(local + CHR_POS, (float[]){ 5.0f, 2.0f, 3.0f }, 12); /* el jugador se mueve: el INNOCENT ya no lo sigue */
    for (int i = 0; i < 3; i++, now += step)
        ghost_frame(now, &net, 16);
    CHECK(getf(innocent, CHR_POS) == inn_x && getf(clone, CHR_POS) == 6.5f, "terminado el modo NPC, solo se mueve el clon (NPC %.2f, clon %.2f)",
          (double)getf(innocent, CHR_POS), (double)getf(clone, CHR_POS));

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

    printf("[cambio de nivel]\n");
    put32(g_cm, CM_NPLAYERS, 0);
    put32(g_cm, CM_NNPCS, 0);
    ghost_frame(now += step, &net, 16);
    make_req(r, 1, 0, "\x2f\xf7\x22\xb2\x01\x02\x03\x04", "THE_PLAYER", 4.5f);
    game_spawn(img, r);
    for (int i = 0; i < 50; i++, now += step) /* 100 ms + 300 ms sin NPC de tipo 6 + clon */
        ghost_frame(now, &net, 17);
    CHECK(g_spawns == 6 && get32(g_cm, CM_NPLAYERS) == 2, "en el nivel nuevo se clona otra vez (SC %u, jugadores %u)", g_spawns,
          get32(g_cm, CM_NPLAYERS));

    CHECK(g_misaligned == 0, "posiciones y pedidos alineados a 16 en todas las llamadas (%u desalineados)", g_misaligned);

    printf("[registro de excepciones]\n");
    crash_install();
    AddVectoredExceptionHandler(0, fixer); /* después del nuestro: salta la instrucción que falla */
    __asm__ volatile(".byte 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00" ::: "eax", "memory"); /* mov eax, [0x10] */
    CHECK(g_fixed == 1, "la lectura inválida pasó por los dos VEH y siguió (%d)", g_fixed);
    log_close();

    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "hook spawn de autoridad (FUN_00797490): instalado", 1);
    expect(log, "hook SpawnCharacter (FUN_007d3b50): instalado", 1);
    expect(log, "AIMP_GHOST=2 (fase 4, experimental: NPC existente y después clon)", 1);
    expect(log, "SP observadores autoridad antes (rva 0x12f322c): vacía", 1);
    expect(log, "SP observadores RPC 0x29 antes (rva 0x12a6728): [0x", 1);
    expect(log, "vf1 rva 0x123456]", 1);
    expect(log, "| clave 1/1 tipo 0 id 2ff722b2099d5b9b | \"\" / \"THE_PLAYER\" | pos=(4.50, 13.00, 30.00) yaw=-3.000", 1);
    expect(log, "SC SpawnCharacter 2: this 0x", 1);
    expect(log, "igual al último del spawn de autoridad", 1);
    expect(log, "SC   devolvió 0x", 1);
    expect(log, "GHOST: NPC elegido para mover: NPCs[0]", 1);
    expect(log, "GHOST: bloque del avatar = CM+0x7a0 (índice 3), autoridad 1; se pone en 0", 1);
    expect(log, "\"INNOCENT\"", 1);
    expect(log, "GHOST: primer teletransporte de 0x", 1);
    expect(log, "GHOST: fin del modo NPC; se intenta el clon", 1);
    expect(log, "GHOST: clonando el pedido de SpawnCharacter (clave 1 -> 64, id 2ff722b2099d5b9b -> 2ff722b241494d50)", 1);
    expect(log, "GHOST: SpawnCharacter devolvió 0x", 1);
    expect(log, "ahora 2 jugadores, 2 NPCs", 1);
    expect(log, "GHOST: apareció el clon: jugadores[1]", 1);
    expect(log, "GHOST: desactivando el controlador del clon", 1);
    expect(log, "objetivo (50.00, 1.00, -20.00) [red]", 1);
    expect(log, "GHOST: cambió el jugador local", 1);
    expect(log, "GHOST: no hay NPC de tipo 6 en el nivel (0 NPCs); se pasa al clon", 1);
    expect(log, "GHOST: clonando el pedido de SpawnCharacter (clave 1 -> 65, id 2ff722b201020304 -> 2ff722b242494d50)", 1);
    expect(log, "registro de excepciones (VEH) instalado", 1);
    expect(log, "EXCEPCIÓN 0xc0000005 en ", 1);
    expect(log, "thread ", 1);
    expect(log, "(principal)", 1);
    expect(log, "   lectura en 0x00000010", 1);
    expect(log, "   eax ", 1);
    expect(log, "   stack, retornos posibles a AI.exe", 1);
    printf("%s (%d fallas)\n", g_fail ? "TEST_GHOST CON FALLAS" : "TEST_GHOST OK", g_fail);
    return g_fail ? 1 : 0;
}
