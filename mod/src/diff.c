/* Dificultad del coop (ver diff.h). */
#include "diff.h"
#include "hook.h"
#include "log.h"

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
        if (diff_effective())
            apply_all("carga");
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
