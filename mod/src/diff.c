/* Dificultad del coop (ver diff.h). */
#include "diff.h"
#include "hook.h"
#include "log.h"
#include "game.h"

#define AIMP_RVA_DIFF_LOAD 0x002F8B00u /* FUN_006f8b00 */
static const uint8_t DIFF_LOAD_PROLOGUE[13] = { 0x83, 0xEC, 0x08, 0x8B, 0x4C, 0x24, 0x0C, 0x57, 0x68, 0x04, 0xDA, 0x2E, 0x01 };
#define ALIEN_FLOATS 26
#define MAX_SETS 8

typedef void(__attribute__((fastcall)) * LoadFn)(void *self, void *edx, uint32_t a1, uint32_t a2, uint32_t out);
static LoadFn g_orig;
static AimpHook g_hk;
static volatile LONG g_n;
static int g_trace;
static AimpNet *g_net;
static uint32_t g_sets[MAX_SETS];
static float g_base[MAX_SETS][ALIEN_FLOATS];
static int g_nsets;
static int g_local, g_host_level = -1, g_applied = -1, g_sent = -1, g_had_remote;

/* ---- Humanos y androides (menú Coop, "Humanos y androides"; menu.c) ----
 * Estructura de dificultad +0x6C = arreglo de punteros a bloques de 0x40 por plantilla de NPC, +0x70 = cantidad (el
 * cargador FUN_006f8b00 los usa como param_3[0x1b]/[0x1c] y agranda con FUN_006d0ff0; corrida 175: leerlos como
 * inicio/fin en +0x68/+0x6C daba basura y escribirla colgó el juego). El índice es la posición del nombre en el registro de plantillas: [[g_root+0x98]+4] = nombres de
 * 0x40 bytes, [[g_root+0x98]+8] = cantidad (FUN_006d8d90 busca así; si no está, FUN_006d2020 lo agrega).
 * Un escalón = el salto de Medium a Hard en los sentidos y el de Hard a Nightmare en el daño (BML de
 * DIFFICULTYSETTINGS, 2026-11-23): oído +5 m, sonido de armas +0,4/+0,15, de pasos +0,15/+0,15, linterna +0,3/+0,3,
 * vista +0,15/+0,15 (sumas) y daño que hacen ×1,25. Plantillas: las de humanos y androides hostiles. */
#define NPC_FLOATS 16
#define MAX_TPL 32
static const float NPC_ADD[NPC_FLOATS] = { 5.0f, 0.4f, 0.15f, 0.15f, 0.15f, 0.3f, 0.3f, 0.15f, 0.15f, 0, 0, 0, 0, 0, 0, 0 };
#define NPC_DAMAGE_DEALT 9 /* +0x24 */
static const char *const HUMAN_TPL[] = { "ANDROID", "ANDROID_HEAVY", "SECURITY_GUARD", "RIOT_GUARD", "MELEE_HUMAN", "CIVILIAN" };
static uint32_t g_npc[MAX_SETS][MAX_TPL];
static float g_npc_base[MAX_SETS][MAX_TPL][NPC_FLOATS];
static int g_humans, g_humans_applied = -1, g_nhuman_tpl;

void diff_apply_npc(float *dst, const float *base, int level)
{
    float mul = 1.0f;
    for (int i = 0; i < level; i++)
        mul *= 1.25f;
    for (int i = 0; i < NPC_FLOATS; i++)
        dst[i] = base[i] + NPC_ADD[i] * (float)level;
    dst[NPC_DAMAGE_DEALT] = base[NPC_DAMAGE_DEALT] * mul;
}

static int streq(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;
    return *a == *b;
}

/* guarda los bloques de humanos y androides de una estructura recién cargada (con su copia base) */
static int capture_npcs(int set, uint32_t st)
{
    uint32_t root = 0, reg = 0, names = 0, n = 0, data = 0, cnt = 0;
    int found = 0;
    aimp_memset(g_npc[set], 0, sizeof g_npc[set]);
    if (!mem_u32(g_aimp_exe + AIMP_RVA_G_ROOT, &root) || !root || !mem_u32(root + 0x98, &reg) || !reg || !mem_u32(reg + 4, &names) ||
        !mem_u32(reg + 8, &n) || !mem_u32(st + 0x6C, &data) || !mem_u32(st + 0x70, &cnt) || data < 0x100000 || cnt > 256) {
        log_printf("DIFF: no se encontraron los bloques por plantilla (registro %08x, arreglo %08x, %u): humanos y androides sin efecto",
                   reg, data, cnt);
        return 0;
    }
    for (uint32_t j = 0; j < cnt && j < n && j < MAX_TPL; j++) {
        char nm[64];
        uint32_t blk = 0;
        if (!mem_cstr(names + j * 0x40, nm, sizeof nm) || !mem_u32(data + 4 * j, &blk) || !blk)
            continue;
        for (unsigned k = 0; k < sizeof HUMAN_TPL / sizeof HUMAN_TPL[0]; k++)
            if (streq(nm, HUMAN_TPL[k]) && mem_read(blk, g_npc_base[set][j], sizeof g_npc_base[set][j])) {
                float dd = g_npc_base[set][j][9], dr = g_npc_base[set][j][10];
                if (blk < 0x100000 || !(dd > 0.0f && dd < 10.0f) || !(dr > 0.0f && dr < 10.0f)) { /* defaults del cargador: 1,0 */
                    log_printf("DIFF:   %s (plantilla %u) en %08x no parece un bloque (daño x%.2f/x%.2f): no se toca", nm, j, blk, (double)dd,
                               (double)dr);
                    continue;
                }
                g_npc[set][j] = blk;
                found++;
                if (g_trace)
                    log_printf("DIFF:   %s (plantilla %u) en %08x: oído %+.1f, vista %+.2f, daño que hace x%.2f, que recibe x%.2f", nm, j, blk,
                               (double)g_npc_base[set][j][0], (double)g_npc_base[set][j][7], (double)g_npc_base[set][j][9],
                               (double)g_npc_base[set][j][10]);
            }
    }
    return found;
}

static void apply_humans(const char *why)
{
    int n = 0;
    for (int i = 0; i < g_nsets; i++)
        for (int j = 0; j < MAX_TPL; j++)
            if (g_npc[i][j]) {
                float v[NPC_FLOATS];
                diff_apply_npc(v, g_npc_base[i][j], g_humans);
                mem_write(g_npc[i][j], v, sizeof v);
                n++;
            }
    if (g_humans != g_humans_applied || g_trace)
        log_printf("DIFF: humanos y androides %+d aplicado a %d bloques (%s)", g_humans, n, why);
    g_humans_applied = g_humans;
}

void diff_set_humans(int level)
{
    g_humans = level < 0 ? 0 : level > 3 ? 3 : level;
    apply_humans("menú Coop");
}

/* Un escalón = lo que suma Nightmare sobre Hard (corridas 124–125), con los máximos del radio también hacia adentro. */
static const float STEP[ALIEN_FLOATS] = {
    0, 0,                       /* decrease/increase_sweep_duration */
    -5, -5, -5, -5,             /* near_target_exclusion_radius: primer acecho mín/máx, siguientes mín/máx */
    -5, -5, -5, -5,             /* near_objective_exclusion_radius */
    -3, -3,                     /* menace_gauge_decrease_time, menace_cool_down_time */
    0, 1, 0,                    /* meance_deemed_time, max_menaces, menace_gauge_seconds_to_fill */
    0, 0,                       /* sweep_box */
    -10, -20,                   /* Vent_Attract_Time min/máx */
    0, 0, -5, -10, 0, 0, 0      /* backstage: role_timeout, ambush_timeout, min/max_distance, min/max_idle, killtrap */
};

void diff_apply_block(float *dst, const float *base, int level)
{
    for (int i = 0; i < ALIEN_FLOATS; i++)
        dst[i] = base[i] + STEP[i] * (float)level;
}

int diff_from_host(void) { return g_net && g_net->mode == 2 && g_net->have_remote && g_host_level >= 0; }
int diff_effective(void) { return diff_from_host() ? g_host_level : g_local; }

static void apply_all(const char *why)
{
    int lvl = diff_effective();
    for (int i = 0; i < g_nsets; i++) {
        float v[ALIEN_FLOATS];
        diff_apply_block(v, g_base[i], lvl);
        mem_write(g_sets[i], v, sizeof v);
    }
    if (lvl != g_applied || g_trace)
        log_printf("DIFF: dificultad del coop %d aplicada a %d estructuras (%s)", lvl, g_nsets, why);
    g_applied = lvl;
}

void diff_set_local(int level)
{
    g_local = level < 0 ? 0 : level > 3 ? 3 : level; /* 3: Personalizada, escala del Alien +3 (menu.c) */
    apply_all(diff_from_host() ? "elección local, pero manda el host" : "menú");
}

void diff_remote_event(const AimpEvt *e)
{
    OptionsEvt o;
    if (e->size < sizeof o)
        return;
    aimp_memcpy(&o, e->data, sizeof o);
    if (o.kind != 1 || !g_net || g_net->mode != 2)
        return;
    int lvl = (int)(o.level > 3 ? 3 : o.level);
    if (lvl != g_host_level)
        log_printf("DIFF: el host eligió la dificultad del coop %d", lvl);
    g_host_level = lvl;
    apply_all("la manda el host");
}

void diff_frame(int64_t now, int64_t freq)
{
    (void)now;
    (void)freq;
    if (!g_net || !g_net->mode)
        return;
    int rem = g_net->have_remote;
    if (g_net->mode == 1 && rem && (!g_had_remote || g_sent != g_local)) {
        OptionsEvt o = { 1, (uint32_t)g_local };
        if (net_event(g_net, AIMP_EVT_OPTIONS, 0, &o, sizeof o)) {
            g_sent = g_local;
            log_printf("DIFF: dificultad del coop %d mandada al cliente", g_local);
        }
    }
    if (g_net->mode == 2 && !rem && g_had_remote && g_host_level >= 0) {
        g_host_level = -1; /* sin host: vuelve la local */
        apply_all("se fue el host");
    }
    g_had_remote = rem;
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_load(void *self, void *edx, uint32_t a1, uint32_t a2,
                                                                                    uint32_t out)
{
    g_orig(self, edx, a1, a2, out);
    LONG n = InterlockedIncrement(&g_n);
    if (g_nsets < MAX_SETS && mem_read(out, g_base[g_nsets], sizeof g_base[0])) {
        g_sets[g_nsets++] = out;
        if (g_trace)
            log_printf("DIFF: el juego cargó una dificultad (#%ld) en %08x: radio %.1f/%.1f, amenazas %+.0f, conductos %+.0f/%+.0f", (long)n, out,
                       (double)g_base[g_nsets - 1][2], (double)g_base[g_nsets - 1][3], (double)g_base[g_nsets - 1][13],
                       (double)g_base[g_nsets - 1][17], (double)g_base[g_nsets - 1][18]);
        int nh = capture_npcs(g_nsets - 1, out);
        if (nh > g_nhuman_tpl)
            g_nhuman_tpl = nh;
        if (diff_effective())
            apply_all("carga");
        if (g_humans)
            apply_humans("carga");
    }
}

void diff_install(AimpNet *net)
{
    g_net = net;
    g_trace = aimp_env_int(L"AIMP_TRACE_DIFF", 0);
    if (!aimp_env_int(L"AIMP_COOP_DIFF", 1))
        return;
    if (hook_install(&g_hk, "cargador de la dificultad (FUN_006f8b00)", (void *)(g_aimp_exe + AIMP_RVA_DIFF_LOAD), DIFF_LOAD_PROLOGUE,
                     sizeof DIFF_LOAD_PROLOGUE, 7, (void *)det_load, (void **)&g_orig))
        log_printf("DIFF: dificultad del coop activa (menú Coop; la decide el host)");
}
