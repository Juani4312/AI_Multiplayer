/* Test de AIMP_GHOST_WALK (ghost.c, fase 4b), con wine. Imagen de AI.exe falsa:
 * - stubs de los dos spawns y del teleport (los mismos que test_ghost);
 * - stubs de FUN_0067e980 (MOVE_TOWARDS: empezar) y FUN_006797a0 (actualizar destino) con sus
 *   prólogos reales y `ret 0x14`, que llaman a funciones de prueba con el controlador (ECX) y los
 *   cinco argumentos;
 * - personajes de 0x4400 bytes con el controlador de comandos en +0x3820 (+0x34 = el personaje). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "aimp.h"
#include "game.h"
#include "ghost.h"
#include "log.h"
#include "net.h"

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
static void putf(void *base, uint32_t off, float v) { memcpy((uint8_t *)base + off, &v, 4); }

static void *g_cm, *g_parr, *g_narr, *g_this, *g_sc_this;
static uint32_t g_teleports, g_starts, g_updates, g_misaligned, g_bad_ctrl;
static float g_last_target[12];
static uint8_t g_last_a[8], g_last_b[8], g_last_params[MOVE_PARAMS_SIZE];
static uint32_t g_last_flag;

static void append(uint32_t f_arr, uint32_t f_n, void *c)
{
    uint8_t *arr = (uint8_t *)(uintptr_t)get32(g_cm, f_arr);
    uint32_t n = get32(g_cm, f_n);
    putp(arr, n * 4, c);
    put32(g_cm, f_n, n + 1);
}

static void __attribute__((cdecl)) fake_auth(uint32_t self, const uint8_t *req) { (void)self, (void)req; }

static uint32_t __attribute__((cdecl)) fake_spawnchar(uint32_t self, const uint8_t *req)
{
    (void)self;
    uint8_t *c = obj(0x4400), *d = obj(0x40);
    put32(c, 0, (uint32_t)g_aimp_exe + AIMP_RVA_VT_CHARACTER);
    memcpy(c + CHR_ID, req + SREQ_ID, 8);
    memcpy(c + CHR_POS, req + SREQ_POS, 12);
    putf(c, CHR_POS + 12, 1.0f);
    uint32_t type = get32(req, SREQ_TYPE);
    put32(d, DEF_TYPE, type);
    putp(c, CHR_DEF, d);
    putp(c, CHR_CMDCTRL + CMDCTRL_OWNER, c); /* controlador de comandos: +0x34 = el personaje */
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

static void __attribute__((cdecl)) fake_teleport(uint32_t pj, const float *pos, float yaw)
{
    g_teleports++;
    if ((uintptr_t)pos & 15)
        g_misaligned++;
    memcpy((uint8_t *)(uintptr_t)pj + CHR_POS, pos, 16);
    memcpy((uint8_t *)(uintptr_t)pj + CHR_YAW, &yaw, 4);
}

static void record(uint32_t ctrl, const uint8_t *a, const uint8_t *b, const float *t, const uint8_t *params, uint32_t flag)
{
    uint32_t owner = get32((void *)(uintptr_t)ctrl, CMDCTRL_OWNER);
    if (ctrl != owner + CHR_CMDCTRL)
        g_bad_ctrl++;
    if ((uintptr_t)t & 15)
        g_misaligned++;
    memcpy(g_last_a, a, 8);
    memcpy(g_last_b, b, 8);
    memcpy(g_last_target, t, sizeof g_last_target);
    memcpy(g_last_params, params, MOVE_PARAMS_SIZE);
    g_last_flag = flag;
}

/* Como FUN_0067e980: cancela el comando anterior y pone uno nuevo de tipo 5 con los dos ids. */
static void __attribute__((cdecl)) fake_move_start(uint32_t ctrl, const uint8_t *a, const uint8_t *b, const float *t,
                                                   const uint8_t *params, uint32_t flag)
{
    g_starts++;
    record(ctrl, a, b, t, params, flag);
    uint8_t *cmd = obj(0x100);
    put32(cmd, CMD_TYPE, CMD_TYPE_MOVE_TOWARDS);
    memcpy(cmd + CMD_OWNER_A, a, 8);
    memcpy(cmd + CMD_OWNER_B, b, 8);
    putp((void *)(uintptr_t)ctrl, CMDCTRL_CURRENT, cmd);
}
static void __attribute__((cdecl)) fake_move_update(uint32_t ctrl, const uint8_t *a, const uint8_t *b, const float *t,
                                                    const uint8_t *params, uint32_t flag)
{
    g_updates++;
    record(ctrl, a, b, t, params, flag);
}

/* Como SwitchCommand con comando NULL: el pendiente queda vacío y el que se ejecutaba termina. */
static uint32_t g_null_switches;
static void __attribute__((cdecl)) fake_switch(uint32_t ctrl, uint32_t cmd)
{
    if (!cmd) {
        g_null_switches++;
        put32((void *)(uintptr_t)ctrl, CMDCTRL_CURRENT, 0);
        put32((void *)(uintptr_t)ctrl, CMDCTRL_EXEC, 0);
    }
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

static void build_stubs(uint8_t *img)
{
    DWORD old;
    uint8_t *p = img + AIMP_RVA_SPAWN_AUTH;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SPAWN_AUTH_PROLOGUE, sizeof AIMP_SPAWN_AUTH_PROLOGUE);
    emit(&p, "\xFF\x75\x08\x57", 4);
    emit_call(&p, (void *)fake_auth);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5B\x5D\xC2\x08\x00", 10);

    p = img + AIMP_RVA_SPAWN_CHAR;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SPAWN_CHAR_PROLOGUE, sizeof AIMP_SPAWN_CHAR_PROLOGUE);
    emit(&p, "\x8B\x4C\x24\x10\x57\x51", 6);
    emit_call(&p, (void *)fake_spawnchar);
    emit(&p, "\x83\xC4\x08\x5F\x5E\x5D\x5B\x59\xC2\x04\x00", 11);

    p = img + AIMP_RVA_CHR_TELEPORT;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_CHR_TELEPORT_PROLOGUE, sizeof AIMP_CHR_TELEPORT_PROLOGUE);
    emit(&p, "\xFF\x74\x24\x14\x53\x57", 6);
    emit_call(&p, (void *)fake_teleport);
    emit(&p, "\x83\xC4\x0C\x5F\x5D\x5B\xC2\x0C\x00", 9);

    /* FUN_0067e980: push esi; mov esi,ecx; mov ecx,[esi+4]; test ecx,ecx; jz +0x0e */
    p = img + AIMP_RVA_CMD_MOVE_START;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_CMD_MOVE_START_PROLOGUE, sizeof AIMP_CMD_MOVE_START_PROLOGUE);
    for (int i = 0; i < 0x0e; i++)
        emit(&p, "\x90", 1); /* lo que salta el jz */
    for (int i = 0; i < 5; i++)
        emit(&p, "\xFF\x74\x24\x18", 4); /* push [esp+0x18]: flag, params, destino, b, a */
    emit(&p, "\x56", 1);                 /* push esi (ctrl) */
    emit_call(&p, (void *)fake_move_start);
    emit(&p, "\x83\xC4\x18\x5E\xC2\x14\x00", 7); /* add esp,0x18; pop esi; ret 0x14 */

    /* FUN_006d3630 (ConditionHasMotivation): mov eax,[esp+4]; mov eax,[eax+0x31EC] | xor eax,eax; ret 8 (falso;
     * el `ret 8` real: dos argumentos) */
    p = img + AIMP_RVA_HAS_MOTIVATION;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_HAS_MOTIVATION_PROLOGUE, sizeof AIMP_HAS_MOTIVATION_PROLOGUE);
    emit(&p, "\x31\xC0\xC2\x08\x00", 5);

    /* SwitchCommand: sub esp,0x100; push esi; mov esi,ecx | ret 4 */
    p = img + AIMP_RVA_SWITCH_COMMAND;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SWITCH_COMMAND_PROLOGUE, sizeof AIMP_SWITCH_COMMAND_PROLOGUE);
    emit(&p, "\xFF\xB4\x24\x08\x01\x00\x00\x56", 8); /* push [esp+0x108] (cmd); push esi (ctrl) */
    emit_call(&p, (void *)fake_switch);
    emit(&p, "\x83\xC4\x08\x5E\x81\xC4\x00\x01\x00\x00\xC2\x04\x00", 13); /* add esp,8; pop esi; add esp,0x100; ret 4 */

    /* FUN_006797a0: mov eax,[esp+8]; mov edx,[esp+4]; push eax; push edx (ecx intacto = ctrl) */
    p = img + AIMP_RVA_CMD_MOVE_UPDATE;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_CMD_MOVE_UPDATE_PROLOGUE, sizeof AIMP_CMD_MOVE_UPDATE_PROLOGUE);
    emit(&p, "\x83\xC4\x08", 3); /* add esp,8 */
    for (int i = 0; i < 5; i++)
        emit(&p, "\xFF\x74\x24\x14", 4); /* push [esp+0x14] */
    emit(&p, "\x51", 1);                 /* push ecx (ctrl) */
    emit_call(&p, (void *)fake_move_update);
    emit(&p, "\x83\xC4\x18\xC2\x14\x00", 6); /* add esp,0x18; ret 0x14 */
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

typedef void(__attribute__((fastcall)) * AuthFn)(void *self, void *edx, void *req, uint32_t flag);
typedef uint32_t(__attribute__((fastcall)) * ScFn)(void *self, void *edx, void *req);

static void make_req(uint8_t *r, uint32_t key, uint32_t type, const char *id8, float x)
{
    memset(r, 0, SPAWN_REQ_SIZE);
    put32(r, SREQ_KEY, key);
    put32(r, SREQ_KEY2, key);
    put32(r, SREQ_TYPE, type);
    memcpy(r + SREQ_ID, id8, 8);
    float pos[3] = { x, 13.0f, 30.0f };
    memcpy(r + SREQ_POS, pos, 12);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    SetEnvironmentVariableW(L"AIMP_GHOST", L"3");
    SetEnvironmentVariableW(L"AIMP_GHOST_DELAY_MS", L"100");
    SetEnvironmentVariableW(L"AIMP_GHOST_WALK", L"1");
    log_open();
    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stubs(img);
    void *root = obj(0x100);
    g_cm = obj(0x5000);
    g_parr = obj(64 * 4);
    g_narr = obj(64 * 4);
    g_this = obj(0x40);
    g_sc_this = obj(0x40);
    putp(img, AIMP_RVA_G_ROOT, root);
    static const char *names[COMMAND_TYPES] = { "NONE", "IDLE", "DIE", "GOTO", "GOTO_COVER", "MOVE_TOWARDS" };
    for (int i = 0; i < COMMAND_TYPES; i++)
        putp(img, AIMP_RVA_COMMAND_NAMES + 4 * i, (void *)(names[i] ? names[i] : "X"));
    putp(root, ROOT_CHARMGR, g_cm);
    putp(g_cm, CM_PLAYERS, g_parr);
    putp(g_cm, CM_NPCS, g_narr);
    CHECK(ghost_install() == 1, "ghost_install con AIMP_GHOST=3 y AIMP_GHOST_WALK=1");

    uint8_t r[SPAWN_REQ_SIZE] __attribute__((aligned(16)));
    make_req(r, 1, 0, "\x2f\xf7\x22\xb2\x09\x9d\x5b\x9b", 4.5f);
    ((AuthFn)(void *)(img + AIMP_RVA_SPAWN_AUTH))(g_this, NULL, r, 0);
    ((ScFn)(void *)(img + AIMP_RVA_SPAWN_CHAR))(g_sc_this, NULL, r);
    make_req(r, 2, 6, "\x1c\x34\x60\xb3\x7e\x03\xa5\xdc", 29.0f);
    ((AuthFn)(void *)(img + AIMP_RVA_SPAWN_AUTH))(g_this, NULL, r, 0);
    ((ScFn)(void *)(img + AIMP_RVA_SPAWN_CHAR))(g_sc_this, NULL, r);
    uint8_t *local = (uint8_t *)(uintptr_t)get32(g_parr, 0), *npc = (uint8_t *)(uintptr_t)get32(g_narr, 0);
    uint8_t *blk = (uint8_t *)g_cm + CM_BLOCKS + 2 * CM_BLOCK_SIZE; /* el bloque del NPC, autoritativo */
    putp(blk, BLOCK_CHARACTER, npc);
    blk[BLOCK_AUTHORITY] = 1;
    float lp[3] = { 1.0f, 2.0f, 3.0f };
    memcpy(local + CHR_POS, lp, 12);
    putf(local, CHR_YAW, 0.25f);

    AimpNet net;
    memset(&net, 0, sizeof net);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    int64_t now = 1000000, step = f.QuadPart / 60;
    uint32_t esp0, esp1;

    /* objetivo sin red con la caminata: 2,5 m detrás del jugador (adelante = (sin yaw, cos yaw)) */
    const float bx = -2.5f * (float)sin(0.25), bz = -2.5f * (float)cos(0.25);
#define CLOSE(a, b) ((a) > (b) - 0.01f && (a) < (b) + 0.01f)
    printf("[el NPC elegido: se lo ubica, se espera 400 ms y recién ahí MOVE_TOWARDS]\n");
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    for (int i = 0; i < 12; i++, now += step) /* 200 ms: elegido y ubicado */
        ghost_frame(now, &net, 16);
    CHECK(g_teleports == 1 && g_starts == 0, "ubicado (teletransportes %u) y todavía sin comando (inicios %u)", g_teleports, g_starts);
    for (int i = 0; i < 30; i++, now += step) /* 500 ms más */
        ghost_frame(now, &net, 16);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1, "stack balanceado después de los frames (con las llamadas al engine)");
    CHECK(g_teleports == 1 && CLOSE(getf(npc, CHR_POS), 1.0f + bx) && CLOSE(getf(npc, CHR_POS + 8), 3.0f + bz),
          "teletransporte al objetivo, 2,5 m detrás del jugador (%.2f, %.2f; %u)", (double)getf(npc, CHR_POS),
          (double)getf(npc, CHR_POS + 8), g_teleports);
    CHECK(g_starts == 1 && g_bad_ctrl == 0, "MOVE_TOWARDS sobre el controlador del NPC (%u inicios, %u controladores malos)", g_starts,
          g_bad_ctrl);
    CHECK(memcmp(g_last_a, "AIMPWALK", 8) == 0 && get32(g_last_b, 0) == (uint32_t)(uintptr_t)npc && g_last_b[4] == 'G' &&
              g_last_b[5] == 'H',
          "ids del comando: \"AIMPWALK\" y el avatar + \"GH\"");
    CHECK(CLOSE(g_last_target[3], 1.0f + bx) && g_last_target[7] == 2.0f && CLOSE(g_last_target[11], 3.0f + bz) &&
              g_last_target[0] == 1.0f && g_last_target[5] == 1.0f && g_last_target[10] == 1.0f,
          "destino: identidad con la traslación del objetivo (%.2f, %.2f, %.2f)", (double)g_last_target[3],
          (double)g_last_target[7], (double)g_last_target[11]);
    CHECK(get32(g_last_params, 0) == 1 && getf(g_last_params, MOVE_PARAMS_F18) == 0.1f && g_last_flag == 1,
          "parámetros: move_type 1, +0x18 = 0,1; never_succeed = 1");
    CHECK(blk[BLOCK_AUTHORITY] == 1, "con la caminata el NPC conserva la autoridad (NOAUTH apagado)");

    printf("[siguiendo: se actualiza el destino cada 200 ms, sin teletransportes]\n");
    for (int i = 0; i < 60; i++, now += step) { /* 1 s; el jugador se corre 1 m */
        lp[0] = 1.0f + (float)i / 60.0f;
        memcpy(local + CHR_POS, lp, 12);
        ghost_frame(now, &net, 16);
    }
    CHECK(g_starts == 2 && g_updates >= 2, "una orden nueva cuando el objetivo se movió más de 75 cm (inicios %u; actualizaciones %u)",
          g_starts, g_updates);
    CHECK(g_teleports == 1, "sin teletransportes mientras camina (%u)", g_teleports);
    CHECK(g_last_target[3] > 1.4f + bx && g_last_target[3] < 2.0f + bx, "el destino sigue al objetivo (x=%.2f)", (double)g_last_target[3]);

    printf("[el engine pasa el comando de pendiente (+4) a en ejecución (+0): sigue siendo el del mod]\n");
    uint32_t mine = get32(npc, CHR_CMDCTRL + CMDCTRL_CURRENT);
    put32(npc, CHR_CMDCTRL + CMDCTRL_EXEC, mine);
    put32(npc, CHR_CMDCTRL + CMDCTRL_CURRENT, 0);
    uint32_t up0 = g_updates;
    for (int i = 0; i < 70; i++, now += step)
        ghost_frame(now, &net, 16);
    CHECK(g_starts == 2 && g_updates > up0, "no se lo vuelve a dar (inicios %u) y se le sigue actualizando el destino (%u)", g_starts,
          g_updates - up0);

    printf("[la IA cambió el comando: se lo vuelve a dar]\n");
    uint8_t *other = obj(0x100);
    put32(other, CMD_TYPE, 3);
    putp(npc, CHR_CMDCTRL + CMDCTRL_EXEC, other);
    for (int i = 0; i < 70; i++, now += step)
        ghost_frame(now, &net, 16);
    CHECK(g_starts == 3, "otro MOVE_TOWARDS (inicios %u)", g_starts);

    printf("[motivación cinemática: verdadera solo para el agente del avatar]\n");
    {
        typedef uint32_t(__attribute__((fastcall)) * MotivFn)(void *node, void *edx, void *agent, uint32_t x);
        MotivFn motiv = (MotivFn)(void *)(img + AIMP_RVA_HAS_MOTIVATION);
        uint8_t *cine = obj(0x40), *follow = obj(0x40);
        put32(cine, BTNODE_MOTIVATION_TYPE, MOTIVATION_CINEMATIC);
        put32(follow, BTNODE_MOTIVATION_TYPE, 5);
        uint32_t r1, r2, r3;
        __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
        r1 = motiv(cine, NULL, npc + 0x2B0, 7);
        r2 = motiv(cine, NULL, local + 0x2B0, 7);
        r3 = motiv(follow, NULL, npc + 0x2B0, 7);
        __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
        CHECK(esp0 == esp1 && (r1 & 0xFF) == 1 && (r2 & 0xFF) == 0 && (r3 & 0xFF) == 0,
              "avatar + CINEMATIC: %u; jugador + CINEMATIC: %u; avatar + FOLLOW: %u (stack balanceado)", r1 & 0xFF, r2 & 0xFF,
              r3 & 0xFF);
    }

    printf("[filtro de SwitchCommand: la IA no puede cambiarle el comando al avatar]\n");
    uint32_t actrl = (uint32_t)(uintptr_t)npc + CHR_CMDCTRL, ours = get32(npc, CHR_CMDCTRL + CMDCTRL_CURRENT);
    CHECK(ghost_switch_filter(actrl, (uint32_t)(uintptr_t)other) == 0, "GOTO de la IA sobre el avatar: bloqueado");
    CHECK(ghost_switch_filter(actrl, ours) == 1, "el MOVE_TOWARDS del mod: pasa");
    CHECK(ghost_switch_filter(actrl, 0) == 1, "cambiar a ningún comando: pasa");
    CHECK(ghost_switch_filter((uint32_t)(uintptr_t)local + CHR_CMDCTRL, (uint32_t)(uintptr_t)other) == 1, "otro personaje: pasa");

    printf("[lejos más de 1 s: teletransporte y orden nueva en el mismo frame, nunca SwitchCommand(NULL)]\n");
    lp[0] = 20.0f;
    memcpy(local + CHR_POS, lp, 12);
    for (int i = 0; i < 30; i++, now += step) /* 500 ms: todavía no */
        ghost_frame(now, &net, 16);
    CHECK(g_teleports == 1, "a los 500 ms lejos todavía no se lo teletransporta (%u)", g_teleports);
    uint32_t st0 = g_starts;
    for (int i = 0; i < 60; i++, now += step) /* 1 s más */
        ghost_frame(now, &net, 16);
    CHECK(g_teleports == 2 && CLOSE(getf(npc, CHR_POS), 20.0f + bx), "reubicado (x=%.2f, %u)", (double)getf(npc, CHR_POS), g_teleports);
    CHECK(g_starts > st0 && CLOSE(g_last_target[3], 20.0f + bx), "con una orden nueva al objetivo (inicios %u, destino x=%.2f)", g_starts,
          (double)g_last_target[3]);
    CHECK(g_null_switches == 0, "nunca SwitchCommand(ctrl, NULL), que hace caer el juego (%u)", g_null_switches);
    CHECK(g_misaligned == 0, "posiciones y destinos alineados a 16 (%u)", g_misaligned);

    log_close();
    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "AIMP_GHOST_WALK=1 (fase 4b, experimental): MOVE_TOWARDS con move_type 1", 1);
    expect(log, "GHOST: caminata: teletransporte de", 1);
    expect(log, "(primera vez;", 1);
    expect(log, "(controlador 0x", 1);
    expect(log, "después de ubicarlo", 1);
    expect(log, "después de reubicarlo", 1);
    expect(log, "el objetivo se movió", 1);
    expect(log, "el comando actual era GOTO", 1);
    expect(log, "(lejos o en otro piso hace 1 s;", 1);
    expect(log, "caminata (caminando)", 1);
    expect(log, "comando propio tipo 5", 1);
    expect(log, "local detrás", 1);
    expect(log, "bloqueado el comando GOTO", 1);
    expect(log, "bloqueo de los comandos de la IA 1; motivación cinemática 1", 1);
    expect(log, "hook ConditionHasMotivation (FUN_006d3630): instalado", 1);
    expect(log, "motivación cinemática forzada para el avatar", 1);
    expect(log, "(agente = personaje + 0x2b0)", 1);
    expect(log, "AIMP_GHOST_NOAUTH", 0);
    free(log);
    printf(g_fail ? "RESULTADO: %d FALLAS\n" : "RESULTADO: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
