/* Comando PLAY_ANIMATION para el avatar y traza de las animaciones que pide el juego (hallazgo 20).
 *
 * Layout de los parámetros (0x58 bytes; FUN_0052fa10 los arma desde la entidad CMD_PlayAnimation, cada
 * getter FUN_008e1b80 + 0x30*i lee el parámetro i desde AnimationSet, ShortGuids en 0x0175087c + 4*i):
 *   +0x00..+0x1F  cuatro referencias a entidades de 8 bytes (marcadores; 0 = ninguna)
 *   +0x20 float   (de una referencia; 0)
 *   +0x24 u32     AnimationSet (FNV-1a 32 del texto)       +0x28 u32  Animation (FNV-1a 32)
 *   +0x2C i32     StartFrame (-1)   +0x30 i32 EndFrame (-1)   +0x34 i32 PlayCount (1)
 *   +0x38 float   PlaySpeed (1,0)   +0x3C float ConvergenceTime (1,0)   +0x40 float BlendInTime (0,3)
 *   +0x44 float   ExertionFactor (0)   +0x48 float (-1,0, DAT_011bb280)   +0x4C float ExitConvergenceTime (1,0)
 *   +0x50 u32     (de una referencia; 0)
 *   +0x54 bits    3 AllowGravity (1), 4 AllowCollision (1), 5 AllowInterruption, 6 RemoveMotion, 7 LocationConvergence
 *   +0x55 bits    0 OrientationConvergence, 1 FullCinematic, 2 RagdollEnabled, 3 NoIK, 4 NoFootIK, 5 NoLayers,
 *                 6 PlayerAnimDrivenView, 7 GaitSyncStart
 *   +0x56 bits    1 Mirror, 3 lo pone FUN_00579b50 al validar el set, 4 UseExitConvergence, 6 InitiallyBackstage,
 *                 7 IsCrouchedAnim
 *   +0x57 bits    0 Death_by_ragdoll_only, 1 DisableGunLayer
 * Los valores entre paréntesis son los defaults de los getters. El comando (FUN_0067df70) copia los parámetros a
 * cmd+0x50 y los ids a cmd+0x34 (b) y cmd+0x3C (a). */
#include "anim.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define AIMP_RVA_CMD_PLAY_ANIM 0x0027E3B0u
#define AIMP_RVA_STRING_DB     0x00635B90u
#define AIMP_RVA_CTRL_TICK     0x00279150u
#define ANIM_PARAMS            0x58
#define CMD_ANIM_PARAMS        0x50

static const uint8_t PLAY_PROLOGUE[10] = { 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x04, 0x85, 0xC9, 0x74, 0x0E };
static const uint8_t TICK_PROLOGUE[11] = { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x57, 0xBF, 0x04, 0x00, 0x00, 0x00 };
static const uint8_t STRDB_PROLOGUE[8] = { 0x55, 0x8B, 0xEC, 0xA1, 0x40, 0x1C, 0xBE, 0x01 };

typedef void(__attribute__((fastcall)) * PlayFn)(void *ctrl, void *edx, const uint8_t *a, const uint8_t *b, uint32_t start,
                                                  const uint8_t *params);
typedef const char *(__attribute__((cdecl)) * StrFn)(uint32_t hash);

typedef void(__attribute__((fastcall)) * TickFn)(void *ctrl, void *edx, uint32_t arg);
static TickFn g_tick;
static PlayFn g_play, g_play_orig;
static StrFn g_str;
static AimpHook g_hk;
static int g_trace, g_trace_max = 80;
static volatile LONG g_n;
static uint8_t g_params[0x60] __attribute__((aligned(16)));

uint32_t anim_hash(const char *s)
{
    if (!s || !*s)
        return 0;
    uint32_t h = 0x811C9DC5u;
    for (; *s; s++)
        h = (h ^ (uint32_t)(int32_t)(signed char)*s) * 0x01000193u;
    return h;
}

const char *anim_name(uint32_t hash)
{
    if (!g_str || !hash)
        return NULL;
    const char *p = g_str(hash);
    if (!p)
        return NULL;
    char probe[20];
    if (!mem_cstr((uintptr_t)p, probe, sizeof probe) || aimp_memcmp(probe, "BAD_STRING_ENTRY", 16) == 0)
        return NULL;
    return p;
}

static uint32_t rd32(const uint8_t *p, int off)
{
    uint32_t v;
    aimp_memcpy(&v, p + off, 4);
    return v;
}

static float rdf(const uint8_t *p, int off)
{
    float v;
    aimp_memcpy(&v, p + off, 4);
    return v;
}

static void __attribute__((fastcall)) detour_play(void *ctrl, void *edx, const uint8_t *a, const uint8_t *b, uint32_t start,
                                                  const uint8_t *params)
{
    LONG n = InterlockedIncrement(&g_n);
    if (n <= g_trace_max && params) {
        uint8_t p[ANIM_PARAMS];
        uint32_t owner = 0;
        mem_u32((uintptr_t)ctrl + CMDCTRL_OWNER, &owner);
        if (mem_read((uintptr_t)params, p, sizeof p)) {
            uint32_t set = rd32(p, 0x24), an = rd32(p, 0x28);
            const char *ns = anim_name(set), *na = anim_name(an);
            char bs[96], ba[96];
            if (!ns || !mem_cstr((uintptr_t)ns, bs, sizeof bs))
                aimp_snprintf(bs, sizeof bs, "?");
            if (!na || !mem_cstr((uintptr_t)na, ba, sizeof ba))
                aimp_snprintf(ba, sizeof ba, "?");
            uint32_t refs = 0;
            for (int i = 0; i < 0x20; i += 4)
                refs |= rd32(p, i);
            log_printf("ANIM: PLAY_ANIMATION #%ld chr 0x%08x ctrl +0x%x start %u: set 0x%08x \"%s\" anim 0x%08x \"%s\" frames %d..%d x%d "
                       "vel %.2f blend %.2f conv %.2f bits %02x %02x %02x %02x marcadores %s +0x20 %.2f +0x48 %.2f +0x50 0x%08x",
                       (long)n, owner, owner ? (unsigned)((uint32_t)(uintptr_t)ctrl - owner) : 0u, start, set, bs, an, ba,
                       (int)rd32(p, 0x2C), (int)rd32(p, 0x30), (int)rd32(p, 0x34), (double)rdf(p, 0x38), (double)rdf(p, 0x40),
                       (double)rdf(p, 0x3C), p[0x54], p[0x55], p[0x56], p[0x57], refs ? "sí" : "no", (double)rdf(p, 0x20),
                       (double)rdf(p, 0x48), rd32(p, 0x50));
        }
    }
    g_play_orig(ctrl, edx, a, b, start, params);
}

int anim_install(void)
{
    uint8_t c[sizeof PLAY_PROLOGUE], s[sizeof STRDB_PROLOGUE];
    uintptr_t pp = g_aimp_exe + AIMP_RVA_CMD_PLAY_ANIM, ps = g_aimp_exe + AIMP_RVA_STRING_DB;
    if (mem_read(ps, s, sizeof s) && aimp_code_cmp(s, STRDB_PROLOGUE, sizeof s) == 0)
        g_str = (StrFn)(void *)ps;
    {
        uint8_t t[sizeof TICK_PROLOGUE];
        uintptr_t pt = g_aimp_exe + AIMP_RVA_CTRL_TICK;
        if (mem_read(pt, t, sizeof t) && aimp_code_cmp(t, TICK_PROLOGUE, sizeof t) == 0)
            g_tick = (TickFn)(void *)pt;
        else
            log_printf("ANIM: el prólogo de FUN_00679150 (tick del controlador) no coincide");
    }
    if (!mem_read(pp, c, sizeof c) || aimp_code_cmp(c, PLAY_PROLOGUE, sizeof c) != 0) {
        log_printf("ANIM: el prólogo de FUN_0067e3b0 (PLAY_ANIMATION) no coincide; sin animaciones del mod");
        return 0;
    }
    g_play = g_play_orig = (PlayFn)(void *)pp;
    g_trace = aimp_env_int(L"AIMP_TRACE_ANIM", 0);
    g_trace_max = aimp_env_int(L"AIMP_TRACE_ANIM_MAX", 80);
    if (g_trace) {
        /* steal 6: push esi; mov esi,ecx; mov ecx,[esi+4] (sin direccionamiento relativo) */
        if (!hook_install(&g_hk, "PLAY_ANIMATION (FUN_0067e3b0)", (void *)pp, PLAY_PROLOGUE, sizeof PLAY_PROLOGUE, 6,
                          (void *)detour_play, (void **)&g_play_orig)) {
            g_play_orig = g_play;
            log_printf("ANIM: no se pudo instalar la traza de PLAY_ANIMATION");
        } else
            log_printf("ANIM: traza de PLAY_ANIMATION activa (hasta %d); tabla de textos %s", g_trace_max, g_str ? "sí" : "no");
    }
    return 1;
}

int anim_ready(void) { return g_play != NULL; }

void anim_req_default(AnimReq *r)
{
    aimp_memset(r, 0, sizeof *r);
    r->play_count = 1;
    r->speed = 1.0f;
    r->blend_in = 0.3f;
    r->gravity = r->collision = 1;
    r->start_instantly = 1;
}

void anim_play(uint32_t ctrl, const uint8_t *a, const uint8_t *b, const AnimReq *r)
{
    if (!g_play_orig || !ctrl)
        return;
    uint8_t *p = g_params;
    aimp_memset(p, 0, sizeof g_params);
    int32_t m1 = -1;
    float one = 1.0f, mone = -1.0f;
    aimp_memcpy(p + 0x24, &r->set, 4);
    aimp_memcpy(p + 0x28, &r->anim, 4);
    aimp_memcpy(p + 0x2C, &m1, 4);
    aimp_memcpy(p + 0x30, &m1, 4);
    aimp_memcpy(p + 0x34, &r->play_count, 4);
    aimp_memcpy(p + 0x38, &r->speed, 4);
    aimp_memcpy(p + 0x3C, &one, 4);
    aimp_memcpy(p + 0x40, &r->blend_in, 4);
    aimp_memcpy(p + 0x48, &mone, 4);
    aimp_memcpy(p + 0x4C, &one, 4);
    p[0x54] = (uint8_t)((r->gravity ? 0x08 : 0) | (r->collision ? 0x10 : 0) | (r->interruption ? 0x20 : 0) | (r->remove_motion ? 0x40 : 0));
    p[0x55] = (uint8_t)((r->no_ik ? 0x08 : 0) | (r->no_foot_ik ? 0x10 : 0));
    /* corrida 83: en los PLAY_ANIMATION de la IA +0x56 trae siempre los bits 0, 2 y 3 (0x0D) */
    p[0x56] = (uint8_t)(0x0D | (r->crouched_anim ? 0x80 : 0));
    p[0x57] = 0;
    /* lo llama la entidad del thread de scripts; el mod, desde el principal (igual que MOVE_TOWARDS) */
    g_play_orig((void *)ctrl, NULL, a, b, r->start_instantly ? 1u : 0u, p);
}

int anim_cmd_is(uint32_t cmd, const uint8_t *a, const uint8_t *b)
{
    uint32_t type = 0;
    uint8_t x[8], y[8];
    if (!cmd || !mem_u32(cmd + CMD_TYPE, &type) || type != CMD_TYPE_PLAY_ANIMATION)
        return 0;
    if (!mem_read(cmd + CMD_OWNER_A, x, 8) || !mem_read(cmd + CMD_OWNER_B, y, 8))
        return 0;
    return (aimp_memcmp(x, a, 8) == 0 && aimp_memcmp(y, b, 8) == 0) || (aimp_memcmp(x, b, 8) == 0 && aimp_memcmp(y, a, 8) == 0);
}

int anim_ctrl_tick(uint32_t ctrl)
{
    if (!g_tick || !ctrl)
        return 0;
    g_tick((void *)ctrl, NULL, 1);
    return 1;
}
