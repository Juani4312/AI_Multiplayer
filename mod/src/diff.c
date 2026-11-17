/* Dificultad del coop: traza del cargador de la dificultad del juego (ver diff.h). */
#include "diff.h"
#include "hook.h"
#include "log.h"

#define AIMP_RVA_DIFF_LOAD 0x002F8B00u /* FUN_006f8b00 */
static const uint8_t DIFF_LOAD_PROLOGUE[13] = { 0x83, 0xEC, 0x08, 0x8B, 0x4C, 0x24, 0x0C, 0x57, 0x68, 0x04, 0xDA, 0x2E, 0x01 };

typedef void(__attribute__((fastcall)) * LoadFn)(void *self, void *edx, uint32_t a1, uint32_t a2, uint32_t out);
static LoadFn g_orig;
static AimpHook g_hk;
static volatile LONG g_n;

static const char *const ALIEN_FIELDS[] = { "decrease_sweep_duration", "increase_sweep_duration", "target_first_min", "target_first_max",
                                            "target_next_min", "target_next_max", "objective_first_min", "objective_first_max",
                                            "objective_next_min", "objective_next_max", "menace_decrease_time", "menace_cool_down",
                                            "menace_deemed_time", "max_menaces", "menace_seconds_to_fill", "sweep_half_length",
                                            "sweep_half_width", "vent_attract_min", "vent_attract_max", "bs_role_timeout",
                                            "bs_ambush_timeout", "bs_min_distance", "bs_max_distance", "bs_min_idle", "bs_max_idle",
                                            "bs_killtrap_time" };

static void dump(uint32_t out)
{
    float a[26];
    char line[1200];
    int n = 0;
    if (!mem_read(out, a, sizeof a))
        return;
    for (int i = 0; i < 26 && n < (int)sizeof line - 60; i++)
        n += aimp_snprintf(line + n, sizeof line - (size_t)n, " %s=%.3f", ALIEN_FIELDS[i], (double)a[i]);
    log_printf("DIFF: Alien:%s", line);
    uint32_t v[3] = { 0 };
    mem_read(out + 0x68, v, sizeof v);
    log_printf("DIFF: vector de plantillas: +0x68=%08x +0x6c=%08x +0x70=%08x", v[0], v[1], v[2]);
    uint32_t cnt = v[2] < 64 ? v[2] : 0;
    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t p = 0;
        float b[16];
        if (!mem_u32(v[1] + 4 * i, &p) || !p || !mem_read(p, b, sizeof b))
            continue;
        log_printf("DIFF: plantilla %u @%08x: oído %.3f arma %.3f/%.3f pasos %.3f/%.3f linterna %.3f/%.3f vista %.3f/%.3f | "
                   "daño hecho %.3f recibido %.3f ritmo %.3f/%.3f cobertura %.3f disparos %.3f ítem %.3f",
                   i, p, (double)b[0], (double)b[1], (double)b[2], (double)b[3], (double)b[4], (double)b[5], (double)b[6], (double)b[7],
                   (double)b[8], (double)b[9], (double)b[10], (double)b[11], (double)b[12], (double)b[13], (double)b[14], (double)b[15]);
    }
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_load(void *self, void *edx, uint32_t a1, uint32_t a2,
                                                                                    uint32_t out)
{
    g_orig(self, edx, a1, a2, out);
    LONG n = InterlockedIncrement(&g_n);
    if (n <= 20) {
        log_printf("DIFF: el juego cargó una dificultad (#%ld): this %p, args %08x %08x, salida %08x", (long)n, self, a1, a2, out);
        dump(out);
    }
}

void diff_install(void)
{
    if (!aimp_env_int(L"AIMP_TRACE_DIFF", 0))
        return;
    if (hook_install(&g_hk, "cargador de la dificultad (FUN_006f8b00)", (void *)(g_aimp_exe + AIMP_RVA_DIFF_LOAD), DIFF_LOAD_PROLOGUE,
                     sizeof DIFF_LOAD_PROLOGUE, 7, (void *)det_load, (void **)&g_orig))
        log_printf("DIFF: traza del cargador de la dificultad activa");
}
