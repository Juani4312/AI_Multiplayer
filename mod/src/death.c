/* Muerte en coop: investigación del flujo del game over (2026-11-17). Ver death.h. */
#include "death.h"
#include "game.h"
#include "hook.h"
#include "log.h"
#include "ghost.h"

#define AIMP_RVA_UI_OPEN 0x0041F8F0u
static const uint8_t UI_OPEN_PROLOGUE[7] = { 0x6A, 0xFF, 0x68, 0x48, 0x2E, 0x13, 0x01 };

typedef void(__attribute__((fastcall)) * UiOpenFn)(void *ui, void *edx, const char *name);
static UiOpenFn g_ui_orig;
static AimpHook g_hk_ui;
static volatile LONG g_n_ui;
static uint32_t g_text_lo, g_text_hi;

static void stack_line(const char *what, uint32_t sp)
{
    uint32_t st[192];
    if (!mem_read(sp, st, sizeof st))
        return;
    char line[900];
    int n = aimp_snprintf(line, sizeof line, "   %s: retornos posibles (offset:rva):", what);
    int k = 0;
    for (int i = 0; i < 192 && k < 28 && n > 0 && (size_t)n < sizeof line - 24; i++)
        if (st[i] >= g_text_lo && st[i] < g_text_hi) {
            n += aimp_snprintf(line + n, sizeof line - (size_t)n, " +%x:%x", (unsigned)(i * 4), (unsigned)(st[i] - g_aimp_exe));
            k++;
        }
    log_printf("%s", line);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_ui_open(void *ui, void *edx, const char *name)
{
    LONG n = InterlockedIncrement(&g_n_ui);
    if (n <= 200) {
        char nm[64];
        if (!name || !mem_cstr((uintptr_t)name, nm, sizeof nm))
            aimp_snprintf(nm, sizeof nm, "?");
        log_printf("UI: abre \"%s\" (ui 0x%08x, #%ld)", nm, (unsigned)(uintptr_t)ui, (long)n);
        stack_line("pila", (uint32_t)(uintptr_t)__builtin_frame_address(0) + 4);
    }
    g_ui_orig(ui, edx, name);
}

/* FUN_007caac0 (daño sobre el componente de salud, Character+0x320): si Character+0x4253 != 0 y el golpe es >= la
 * salud, lo recorta a salud-1 (también cuando el golpe no es "letal", arg 5 en 0). Salud u16 en Character+0x33C,
 * máxima en +0x33E (NOTES, 2026-11-15). */
#define CHR_NO_DEATH   0x4253
#define CHR_HEALTH     0x33C
#define CHR_HEALTH_MAX 0x33E
#define CHR_INVINC_A   0x2E8
#define CHR_INVINC_B   0x2EA

static int g_downed_mode, g_downed_cfg, g_downed, g_remote_downed, g_revive_pending;
static volatile LONG g_checkpoint_hit;
static AimpNet *g_net;
static float g_revive_pos[4] __attribute__((aligned(16)));
static float g_revive_yaw;
typedef void(__attribute__((fastcall)) * TeleportFn)(void *self, void *edx, const float *pos, float yaw, uint32_t p3);
typedef void(__attribute__((fastcall)) * KillFn)(void *chr, void *edx, uint32_t attacker);
static TeleportFn g_tp;
static KillFn g_kill;
static const uint8_t KILL_PROLOGUE[11] = { 0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x64, 0x80, 0x79, 0x1F, 0x00 };
#define AIMP_RVA_CHR_KILL 0x003D0C10u

/* AIMP_TRACE_DMG (2026-11-18, para la pantalla negra de la corrida 119): golpes sobre el jugador y muertes.
 * FUN_007caac0 thiscall(componente de salud = Character+0x320, 9 args), ret 0x24: a1 = daño (s16), a2 = atacante,
 * a5 (byte bajo) = "letal", a8/a9 = flags; el personaje es [comp+0x24] y la salud, comp+0x1C (u16). */
#define AIMP_RVA_CHR_DAMAGE 0x003CAAC0u
static const uint8_t DAMAGE_PROLOGUE[13] = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF, 0x68, 0x58, 0x97, 0x12, 0x01 };
typedef void(__attribute__((fastcall)) * DamageFn)(void *comp, void *edx, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                                                   uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9);
static DamageFn g_dmg_orig;
static KillFn g_kill_orig;
static AimpHook g_hk_dmg, g_hk_kill;
static volatile LONG g_n_dmg, g_n_kill;
static volatile uint32_t g_trace_player; /* el jugador local (lo actualiza death_frame) */

static void chr_desc(uint32_t c, char *out, size_t cap)
{
    char nm[33] = "";
    uint32_t def = 0, type = 0xFFFFFFFFu;
    if (!c) {
        aimp_snprintf(out, cap, "ninguno");
        return;
    }
    mem_cstr(c + 0x10, nm, sizeof nm);
    if (mem_u32(c + CHR_DEF, &def) && def)
        mem_u32(def + 4, &type);
    aimp_snprintf(out, cap, "0x%08x \"%s\" tipo %d", c, nm, (int)type);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_damage(void *comp, void *edx, uint32_t a1, uint32_t a2,
                                                                                      uint32_t a3, uint32_t a4, uint32_t a5,
                                                                                      uint32_t a6, uint32_t a7, uint32_t a8,
                                                                                      uint32_t a9)
{
    uint32_t c = (uint32_t)(uintptr_t)comp, chr = 0;
    uint16_t before = 0, after = 0;
    mem_u32(c + 0x24, &chr);
    mem_read(c + 0x1C, &before, 2);
    int mine = chr && chr == g_trace_player;
    g_dmg_orig(comp, edx, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    if (!mine)
        return;
    mem_read(c + 0x1C, &after, 2);
    LONG n = InterlockedIncrement(&g_n_dmg);
    if (n > 400)
        return;
    char at[80];
    chr_desc(a2, at, sizeof at);
    log_printf("DMG: jugador golpeado #%ld: daño %d, salud %u -> %u, atacante %s, a3 0x%x a4 0x%x letal %u a6 0x%x a7 0x%x a8 0x%x a9 0x%x, desde rva 0x%x",
               (long)n, (int)(int16_t)a1, before, after, at, a3, a4, a5 & 0xFF, a6, a7, a8 & 0xFF, a9 & 0xFF,
               (unsigned)((uintptr_t)__builtin_return_address(0) - g_aimp_exe));
    if (n <= 40)
        stack_line("pila del golpe", (uint32_t)(uintptr_t)__builtin_frame_address(0) + 4);
}

static int g_trace_dmg;
static void note_player_killed(uint32_t player, uint32_t attacker);
__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_kill(void *chr, void *edx, uint32_t attacker)
{
    if ((uint32_t)(uintptr_t)chr == g_trace_player && g_trace_player)
        note_player_killed(g_trace_player, attacker);
    LONG n = g_trace_dmg ? InterlockedIncrement(&g_n_kill) : 1000;
    if (n <= 100) {
        char who[80], at[80];
        chr_desc((uint32_t)(uintptr_t)chr, who, sizeof who);
        chr_desc(attacker, at, sizeof at);
        log_printf("DMG: matar (FUN_007d0c10) #%ld: %s%s, atacante %s, desde rva 0x%x", (long)n, who,
                   (uint32_t)(uintptr_t)chr == g_trace_player ? " (el jugador)" : "", at,
                   (unsigned)((uintptr_t)__builtin_return_address(0) - g_aimp_exe));
        stack_line("pila", (uint32_t)(uintptr_t)__builtin_frame_address(0) + 4);
    }
    g_kill_orig(chr, edx, attacker);
}

/* Acción de muerte (corridas 134-135): los remates (el agarre del Alien, y probablemente los de humanos y androides)
 * construyen en el slot de acción del personaje (Character+0x36C) la acción de vtable rva 0xEAE690
 * (FUN_0065c450 -> FUN_0065b4c0 + vt+0x60 = FUN_00675f10). El arranque saca la invencibilidad, mata (FUN_007d0c10),
 * prende Character+0x298/299/29C/2A9/2BC y apaga [0x1766A2C]+0xA8; con el modo caído la salud queda en 1 y el
 * jugador se queda en esa acción para siempre: cámara congelada y pantalla negra (lo que vio el usuario en la 119).
 * FUN_00658080 thiscall(acción), ret: "volver a la locomoción del jugador": destruye la acción en curso (vt[0](1); el
 * destructor de la de muerte, FUN_0065b630, apaga esos flags y repone la animación) y construye y arranca la locomoción
 * (vtable rva 0xEADC18). La llama FUN_0065bdd0 (acción por defecto) cuando el personaje tiene controlador de jugador. */
#define ACT_DEATH_VT_RVA 0x00EAE690u
#define ACT_LOCO_VT_RVA  0x00EADC18u
#define AIMP_RVA_ACT_TO_LOCO 0x00258080u
static const uint8_t ACT_TO_LOCO_PROLOGUE[12] = { 0x56, 0x8B, 0xF1, 0x8A, 0x46, 0x1A, 0x8B, 0x4E, 0x04, 0x88, 0x46, 0x1B };
typedef void(__attribute__((fastcall)) * ActToLocoFn)(void *act, void *edx);
static ActToLocoFn g_to_loco;
static int g_death_act_ms;   /* AIMP_COOP_DEATH_ACT_MS: cuánto se deja correr el remate antes de devolver el control */

/* Fundido de pantalla (corrida 135): singleton en VA 0x019D0970 (FUN_009f19c0 lo devuelve). +0x1B44 estado (0 nada,
 * 1 negro sostenido, 2/3 en transición; FUN_009f2360/FUN_009f2390), +0x1B48 opacidad del negro [0, 1], +0x1B4C/+0x1B50,
 * +0x1B54/+0x1B58 contadores (FUN_009f2300/9f2320 y 9f2330/9f2350), +0x1B5C dueño actual (id que sube en cada toma).
 * Lo usan las entidades ScreenFade* (0x611xxx), el Checkpoint (0x5A24xx) y las acciones de muerte (0x64A0xx, 0x64B2xx).
 * Después del remate queda en estado 1 con opacidad 1.0: la pantalla negra. */
#define FADE_RVA        0x015D0970u
#define FADE_STATE      0x1B44
#define FADE_ALPHA      0x1B48
static void clear_fade(void)
{
    uintptr_t f = g_aimp_exe + FADE_RVA;
    uint32_t st = 0, z = 0;
    float a = 0;
    if (!mem_u32(f + FADE_STATE, &st) || !mem_f32(f + FADE_ALPHA, &a))
        return;
    log_printf("DEATH: fundido de pantalla: estado %u, opacidad %.2f -> 0 (pantalla visible)", st, (double)a);
    mem_write(f + FADE_ALPHA, &z, 4);
    mem_write(f + FADE_STATE, &z, 4);
}

int death_player_to_loco(uint32_t player, const char *why)
{
    uint32_t act = 0, vt = 0;
    if (!g_to_loco || !player || !mem_u32(player + CHR_ACTION, &act) || !act || !mem_u32(act, &vt))
        return 0;
    log_printf("DEATH: %s: acción del jugador 0x%08x (vtable rva 0x%x) -> locomoción (FUN_00658080)", why, act,
               (unsigned)(vt - g_aimp_exe));
    g_to_loco((void *)(uintptr_t)act, NULL);
    mem_u32(act, &vt);
    log_printf("DEATH: ahora la acción tiene vtable rva 0x%x%s", (unsigned)(vt - g_aimp_exe),
               vt - g_aimp_exe == ACT_LOCO_VT_RVA ? " (locomoción)" : " (inesperada)");
    return 1;
}

static volatile DWORD g_kill_at; /* GetTickCount() del último remate al jugador local (0 = ninguno pendiente) */
static uint32_t g_kill_attacker, g_kill_attacker_vt;
static int g_withdraw_cfg;

/* FUN_0065bdd0 thiscall(acción), ret: vuelve a la acción por defecto del personaje (vt[0](1) destruye la actual; con
 * controlador de jugador, FUN_00658080; si no, la de su tipo). Corrida 140: el Alien queda trabado en su mitad del
 * remate (vtable rva 0xEAE5D0) esperando que el jugador termine de morir. */
#define AIMP_RVA_ACT_DEFAULT 0x0025BDD0u
static const uint8_t ACT_DEFAULT_PROLOGUE[13] = { 0x6A, 0xFF, 0x68, 0x32, 0xD9, 0x12, 0x01, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
int death_char_default_action(uint32_t chr, const char *why)
{
    uint32_t act = 0, vt = 0;
    uint8_t pr[sizeof ACT_DEFAULT_PROLOGUE];
    if (!chr || !mem_u32(chr + CHR_ACTION, &act) || !act || !mem_u32(act, &vt) ||
        !mem_read(g_aimp_exe + AIMP_RVA_ACT_DEFAULT, pr, sizeof pr) || aimp_code_cmp(pr, ACT_DEFAULT_PROLOGUE, sizeof pr) != 0)
        return 0;
    ((ActToLocoFn)(void *)(g_aimp_exe + AIMP_RVA_ACT_DEFAULT))((void *)(uintptr_t)act, NULL);
    uint32_t vt2 = 0;
    mem_u32(act, &vt2);
    log_printf("DEATH: %s: personaje 0x%08x, acción vtable rva 0x%x -> 0x%x (por defecto, FUN_0065bdd0)", why, chr,
               (unsigned)(vt - g_aimp_exe), (unsigned)(vt2 - g_aimp_exe));
    return 1;
}

static void note_player_killed(uint32_t player, uint32_t attacker)
{
    uint32_t act = 0, vt = 0;
    if (!g_downed_mode || !mem_u32(player + CHR_ACTION, &act) || !act || !mem_u32(act, &vt))
        return;
    if (vt - g_aimp_exe == ACT_LOCO_VT_RVA)
        return; /* muerte sin animación (KILL, daño): el modo caído ya alcanza */
    g_kill_at = GetTickCount() | 1;
    g_kill_attacker = attacker;
    g_kill_attacker_vt = 0;
    if (attacker && mem_u32(attacker + CHR_ACTION, &act) && act)
        mem_u32(act, &g_kill_attacker_vt);
    log_printf("DEATH: remate al jugador en la acción de vtable rva 0x%x%s (atacante 0x%08x en la acción 0x%x); se le devuelve el control en %d ms",
               (unsigned)(vt - g_aimp_exe), vt - g_aimp_exe == ACT_DEATH_VT_RVA ? " (muerte por el Alien)" : "", attacker,
               g_kill_attacker_vt ? (unsigned)(g_kill_attacker_vt - g_aimp_exe) : 0u, g_death_act_ms);
}

/* Retirada del Alien (2026-11-18), como la entidad NPC_WithdrawAlien (manejador de arranque FUN_00496da0, sin posición):
 * w = [Alien+0x34F0], c = [w+0x18] (c+0x14 = el Alien; [c] = modo); FUN_007349d0(c) (reinicia: bandera 0x13 del árbol
 * en 0), FUN_006c5ef0(c, 0) (bandera 0x13), si [c] == 0, FUN_0072fc20(c, 1) (modo 1 = retirarse: arma el director del
 * Alien en +0x34D0) y [w+0xC] = 0 (FUN_00821d20). Solo para personajes de tipo 1 (el Alien). */
#define CHR_ALIEN_WITHDRAW 0x34F0
static const uint8_t WD_RESET_PROLOGUE[10] = { 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x14, 0x6A, 0x00, 0x6A, 0x13 };
static const uint8_t WD_FLAG_PROLOGUE[10] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x49, 0x14, 0x50, 0x6A, 0x13 };
static const uint8_t WD_MODE_PROLOGUE[10] = { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x56, 0x8B, 0xF1, 0x33, 0xDB };
typedef void(__attribute__((fastcall)) * WdResetFn)(void *c, void *edx);
typedef void(__attribute__((fastcall)) * WdArgFn)(void *c, void *edx, uint32_t v);

int death_alien_withdraw(uint32_t alien, const char *why)
{
    uint32_t def = 0, type = 0, w = 0, c = 0, mode = 0;
    uint8_t a[10], b[10], d[10], z = 0;
    if (!alien || !mem_u32(alien + CHR_DEF, &def) || !def || !mem_u32(def + 4, &type) || type != 1 ||
        !mem_u32(alien + CHR_ALIEN_WITHDRAW, &w) || !w || !mem_u32(w + 0x18, &c) || !c)
        return 0;
    if (!mem_read(g_aimp_exe + 0x003349D0u, a, 10) || aimp_code_cmp(a, WD_RESET_PROLOGUE, 10) != 0 ||
        !mem_read(g_aimp_exe + 0x002C5EF0u, b, 10) || aimp_code_cmp(b, WD_FLAG_PROLOGUE, 10) != 0 ||
        !mem_read(g_aimp_exe + 0x0032FC20u, d, 10) || aimp_code_cmp(d, WD_MODE_PROLOGUE, 10) != 0) {
        log_printf("DEATH: los prólogos de la retirada del Alien no coinciden");
        return 0;
    }
    ((WdResetFn)(void *)(g_aimp_exe + 0x003349D0u))((void *)(uintptr_t)c, NULL);
    ((WdArgFn)(void *)(g_aimp_exe + 0x002C5EF0u))((void *)(uintptr_t)c, NULL, 0);
    mem_u32(c, &mode);
    if (mode == 0)
        ((WdArgFn)(void *)(g_aimp_exe + 0x0032FC20u))((void *)(uintptr_t)c, NULL, 1);
    mem_write(w + 0x0C, &z, 1);
    mem_u32(c, &mode);
    log_printf("DEATH: %s: el Alien 0x%08x se retira (modo %u)", why, alien, mode);
    return 1;
}

/* El atacante queda en su mitad del remate (el Alien: acción rva 0xEAE5D0) mientras el jugador está caído: si se lo
 * suelta, vuelve a rematar al caído cada ~4,5 s (corrida 141). Como cada máquina tiene su propio Alien, que solo persigue
 * a su jugador, retenerlo no afecta al compañero. Al revivir se lo suelta (FUN_0065bdd0) y, con
 * AIMP_COOP_ALIEN_WITHDRAW=1 (experimental, apagado), se lo manda a retirarse. */
static uint32_t g_held_attacker, g_held_vt;
static void hold_attacker(void)
{
    g_held_attacker = g_kill_attacker;
    g_held_vt = g_kill_attacker_vt;
    g_kill_attacker = 0;
    if (g_held_attacker)
        log_printf("DEATH: el atacante 0x%08x queda retenido en su acción (rva 0x%x) mientras el jugador esté caído",
                   g_held_attacker, g_held_vt ? (unsigned)(g_held_vt - g_aimp_exe) : 0u);
}
static void release_attacker(const char *why)
{
    uint32_t a = g_held_attacker, act = 0, vt = 0;
    g_held_attacker = 0;
    if (!a || !mem_u32(a + CHR_ACTION, &act) || !act || !mem_u32(act, &vt))
        return;
    if (vt == g_held_vt)
        death_char_default_action(a, why);
    if (g_withdraw_cfg)
        death_alien_withdraw(a, why);
}

int death_coop_enabled(void) { return g_downed_mode; }
int death_remote_is_downed(void) { return g_remote_downed; }

static void send(uint32_t kind, uint32_t player)
{
    CoopEvt ev;
    aimp_memset(&ev, 0, sizeof ev);
    ev.kind = kind;
    if (player) {
        mem_read(player + CHR_POS, ev.pos, sizeof ev.pos);
        mem_f32(player + CHR_YAW, &ev.yaw);
    }
    if (g_net && g_net->mode && net_event(g_net, AIMP_EVT_COOP, 0, &ev, sizeof ev))
        log_printf("DEATH: avisado al otro: %s", kind == COOP_DOWNED ? "caído" : kind == COOP_REVIVED ? "revivido" : "revivir");
}

/* Facción del caído (corrida 146): Character+0x2E4 es la facción (FUN_007c4780 la cruza con la máscara de relaciones
 * de [g_root+0x34]; jugador 1, Alien 3). Con la del Alien, el Alien deja de tomar al caído como enemigo: suelto
 * después del remate se fue a los conductos sin volver a rematarlo. Se pone al caer y se restaura al revivir.
 * AIMP_COOP_DOWNED_FACTION (3 por defecto; -1 = no tocarla: entonces el atacante queda retenido, corrida 142). */
#define CHR_FACTION 0x2E4
static int g_downed_faction;
static uint32_t g_saved_faction, g_faction_player;
static void faction_downed(uint32_t player)
{
    uint32_t f = 0, nf = (uint32_t)g_downed_faction;
    if (g_downed_faction < 0 || g_faction_player || !mem_u32(player + CHR_FACTION, &f))
        return;
    g_saved_faction = f;
    g_faction_player = player;
    mem_write(player + CHR_FACTION, &nf, 4);
    log_printf("DEATH: facción del caído %u -> %u (la del Alien: deja de ser su enemigo)", f, nf);
}
static void faction_restore(const char *why)
{
    if (!g_faction_player)
        return;
    mem_write(g_faction_player + CHR_FACTION, &g_saved_faction, 4);
    log_printf("DEATH: %s: facción del jugador restaurada (%u)", why, g_saved_faction);
    g_faction_player = 0;
}

/* Los dos caídos: game over normal. Se saca el "no puede morir" y se lo mata como lo hace el juego. */
static void both_down(uint32_t player)
{
    uint8_t z = 0;
    log_printf("DEATH: los dos jugadores están caídos: game over");
    faction_restore("game over");
    mem_write(player + CHR_NO_DEATH, &z, 1);
    uint8_t a = 0, b = 0;
    mem_u8(player + CHR_INVINC_A, &a);
    mem_u8(player + CHR_INVINC_B, &b);
    a &= (uint8_t)~8;
    b &= (uint8_t)~8;
    mem_write(player + CHR_INVINC_A, &a, 1);
    mem_write(player + CHR_INVINC_B, &b, 1);
    g_downed_mode = 0; /* que no lo vuelva a marcar; vuelve con el jugador de la próxima carga (death_frame) */
    if (g_kill)
        g_kill((void *)player, NULL, 0);
}
static uint32_t g_last_player;
static int64_t g_last_chk;
static wchar_t g_revive_file[MAX_PATH];
static FILETIME g_revive_time;

int death_is_downed(void) { return g_downed; }
static DWORD g_revived_at;
int death_push_ok(void) { return g_downed || (g_revived_at && GetTickCount() - g_revived_at < 120000); }

static void set_flags(uint32_t chr, int downed)
{
    uint8_t a = 0, b = 0;
    mem_u8(chr + CHR_INVINC_A, &a);
    mem_u8(chr + CHR_INVINC_B, &b);
    a = downed ? (uint8_t)(a | 8) : (uint8_t)(a & ~8);
    b = downed ? (uint8_t)(b | 8) : (uint8_t)(b & ~8);
    mem_write(chr + CHR_INVINC_A, &a, 1);
    mem_write(chr + CHR_INVINC_B, &b, 1);
}

void death_frame(int64_t now, int64_t freq, uint32_t player)
{
    g_trace_player = player;
    /* Jugador nuevo (carga después del game over, o cambio de nivel): los dos recargan, así que el estado de caídos
     * vuelve a cero y el modo caído se vuelve a activar (corrida 115: después del game over de los dos quedaba apagado
     * y el compañero seguía "caído", así que el próximo caído daba game over al instante). */
    if (g_downed_cfg && player && player != g_last_player && g_last_player) {
        if (!g_downed_mode || g_remote_downed || g_downed)
            log_printf("DEATH: carga nueva: se reinicia el estado de caídos (modo caído activo)");
        g_downed_mode = 1;
        if (g_remote_downed) {
            g_remote_downed = 0;
            ghost_set_remote_downed(0);
        }
        g_revive_pending = 0;
        g_downed = 0;
        g_held_attacker = 0;
        g_kill_at = 0;
        g_faction_player = 0; /* el personaje es otro: no hay nada que restaurar */
    }
    if (!g_downed_mode || !player || now - g_last_chk < freq / 10)
        return;
    if (InterlockedExchange(&g_checkpoint_hit, 0)) {
        log_printf("DEATH: checkpoint guardado%s", g_remote_downed ? ": se pide revivir al compañero caído" : "");
        if (g_remote_downed && !g_downed)
            send(COOP_REVIVE, player);
    }
    g_last_chk = now;
    if (player != g_last_player) {
        g_last_player = player;
        g_downed = 0;
    }
    uint8_t nd = 0;
    if (mem_u8(player + CHR_NO_DEATH, &nd) && !nd) {
        uint8_t one = 1;
        mem_write(player + CHR_NO_DEATH, &one, 1);
        log_printf("DEATH: jugador 0x%08x: no puede morir (+0x4253 = 1)", player);
    }
    uint16_t hp = 0, mx = 0;
    if (!mem_read(player + CHR_HEALTH, &hp, 2))
        return;
    mem_read(player + CHR_HEALTH_MAX, &mx, 2);
    if (!g_downed && hp <= 1) {
        g_downed = 1;
        set_flags(player, 1);
        faction_downed(player);
        log_printf("DEATH: el jugador quedó caído (salud %u de %u): invencible; en coop sigue el compañero", hp, mx);
        send(COOP_DOWNED, player);
        if (g_remote_downed) {
            both_down(player);
            return;
        }
    }
    /* Remate con el modo caído (corridas 134-136): si al jugador lo mataron (FUN_007d0c10) estando en una acción que no
     * es la locomoción, a los AIMP_COOP_DEATH_ACT_MS se le devuelve la locomoción y se borra el fundido: control y
     * cámara de vuelta, sigue caído. */
    if (g_kill_at && g_death_act_ms >= 0 && GetTickCount() - g_kill_at >= (DWORD)g_death_act_ms) {
        uint32_t act = 0, vt = 0;
        g_kill_at = 0;
        if (mem_u32(player + CHR_ACTION, &act) && act && mem_u32(act, &vt) && vt - g_aimp_exe != ACT_LOCO_VT_RVA &&
            death_player_to_loco(player, "fin del remate")) {
            clear_fade();
            hold_attacker();
            if (g_faction_player) /* con la facción del Alien, el atacante se suelta y se va (corrida 146) */
                release_attacker("fin del remate (atacante)");
            if (g_downed)
                set_flags(player, 1);
        } else
            log_printf("DEATH: fin del remate: el jugador ya está en la locomoción; no se toca");
    }
    if (g_downed && g_revive_pending) {
        g_revive_pending = 0;
        g_downed = 0;
        release_attacker("revivido (atacante)");
        faction_restore("revivido");
        set_flags(player, 0);
        if (mx)
            mem_write(player + CHR_HEALTH, &mx, 2);
        if (g_tp) {
            mem_f32(player + CHR_POS + 12, &g_revive_pos[3]);
            g_tp((void *)player, NULL, g_revive_pos, g_revive_yaw, 0);
        }
        g_revived_at = GetTickCount() | 1;
        log_printf("DEATH: revivido por el compañero (salud %u), junto a él en (%.2f, %.2f, %.2f)", mx, (double)g_revive_pos[0],
                   (double)g_revive_pos[1], (double)g_revive_pos[2]);
        send(COOP_REVIVED, player);
    }
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (g_downed && g_revive_file[0] && GetFileAttributesExW(g_revive_file, GetFileExInfoStandard, &fa) &&
        (fa.ftLastWriteTime.dwLowDateTime != g_revive_time.dwLowDateTime ||
         fa.ftLastWriteTime.dwHighDateTime != g_revive_time.dwHighDateTime)) {
        g_revive_time = fa.ftLastWriteTime;
        g_downed = 0;
        release_attacker("revivido (atacante)");
        faction_restore("revivido");
        set_flags(player, 0);
        if (mx)
            mem_write(player + CHR_HEALTH, &mx, 2);
        log_printf("DEATH: el jugador revive (salud %u)", mx);
        send(COOP_REVIVED, player);
    }
}

void death_remote_event(const AimpEvt *e)
{
    CoopEvt ev;
    if (!g_downed_mode || e->size < sizeof ev)
        return;
    aimp_memcpy(&ev, e->data, sizeof ev);
    if (ev.kind == COOP_DOWNED) {
        log_printf("DEATH: el otro jugador quedó caído%s", g_downed ? "; acá también: game over" : "");
        g_remote_downed = 1;
        ghost_set_remote_downed(1);
        if (g_downed && g_last_player)
            both_down(g_last_player);
    } else if (ev.kind == COOP_REVIVED) {
        log_printf("DEATH: el otro jugador revivió");
        g_remote_downed = 0;
        ghost_set_remote_downed(0);
    } else if (ev.kind == COOP_REVIVE) {
        if (!g_downed) {
            log_printf("DEATH: pedido de revivir, pero acá no estoy caído: se ignora");
            return;
        }
        g_revive_pos[0] = ev.pos[0] + 0.8f; /* al lado, no encima */
        g_revive_pos[1] = ev.pos[1];
        g_revive_pos[2] = ev.pos[2];
        g_revive_yaw = ev.yaw;
        g_revive_pending = 1;
        log_printf("DEATH: el compañero usó una terminal de guardado: revivir junto a él");
    }
}

void death_note_checkpoint(void) { InterlockedExchange(&g_checkpoint_hit, 1); }

void death_on_save_terminal(void)
{
    if (!g_downed_mode)
        return;
    log_printf("DEATH: terminal de guardado usada%s", g_remote_downed ? ": se pide revivir al compañero caído" : "");
    if (g_remote_downed)
        send(COOP_REVIVE, g_last_player);
}

void death_install(AimpNet *net)
{
    g_net = net;
    g_downed_mode = g_downed_cfg = aimp_env_int(L"AIMP_COOP_DOWNED", 0);
    {
        uint8_t t[sizeof AIMP_CHR_TELEPORT_PROLOGUE], k[sizeof KILL_PROLOGUE];
        if (mem_read(g_aimp_exe + AIMP_RVA_CHR_TELEPORT, t, sizeof t) && aimp_code_cmp(t, AIMP_CHR_TELEPORT_PROLOGUE, sizeof t) == 0)
            g_tp = (TeleportFn)(void *)(g_aimp_exe + AIMP_RVA_CHR_TELEPORT);
        if (mem_read(g_aimp_exe + AIMP_RVA_CHR_KILL, k, sizeof k) && aimp_code_cmp(k, KILL_PROLOGUE, sizeof k) == 0)
            g_kill = (KillFn)(void *)(g_aimp_exe + AIMP_RVA_CHR_KILL);
    }
    {
        uint8_t pr[sizeof ACT_TO_LOCO_PROLOGUE];
        if (mem_read(g_aimp_exe + AIMP_RVA_ACT_TO_LOCO, pr, sizeof pr) && aimp_code_cmp(pr, ACT_TO_LOCO_PROLOGUE, sizeof pr) == 0)
            g_to_loco = (ActToLocoFn)(void *)(g_aimp_exe + AIMP_RVA_ACT_TO_LOCO);
        else
            log_printf("DEATH: el prólogo de FUN_00658080 no coincide: el remate deja la pantalla negra");
    }
    g_death_act_ms = aimp_env_int(L"AIMP_COOP_DEATH_ACT_MS", 4000);
    g_withdraw_cfg = aimp_env_int(L"AIMP_COOP_ALIEN_WITHDRAW", 0);
    g_downed_faction = aimp_env_int(L"AIMP_COOP_DOWNED_FACTION", 3);
    if (aimp_env_str(L"AIMP_COOP_REVIVE_FILE", g_revive_file, MAX_PATH)) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_revive_file, GetFileExInfoStandard, &fa))
            g_revive_time = fa.ftLastWriteTime;
    }
    if (g_downed_mode)
        log_printf("DEATH: AIMP_COOP_DOWNED=1: el jugador local no muere, queda caído");
    /* .text de AI.exe: RVA 0x1000, 0xD9D13B bytes (objdump -h) */
    g_text_lo = (uint32_t)g_aimp_exe + 0x1000;
    g_text_hi = g_text_lo + 0x00D9D13Bu;
    if (aimp_env_int(L"AIMP_TRACE_UI", 0)) {
        if (hook_install(&g_hk_ui, "abrir pantalla de UI (FUN_0081f8f0)", (void *)(g_aimp_exe + AIMP_RVA_UI_OPEN), UI_OPEN_PROLOGUE,
                         sizeof UI_OPEN_PROLOGUE, 7, (void *)det_ui_open, (void **)&g_ui_orig))
            log_printf("DEATH: traza de pantallas de UI activa");
    }
    g_trace_dmg = aimp_env_int(L"AIMP_TRACE_DMG", 0);
    if (g_trace_dmg && hook_install(&g_hk_dmg, "daño (FUN_007caac0)", (void *)(g_aimp_exe + AIMP_RVA_CHR_DAMAGE), DAMAGE_PROLOGUE,
                                    sizeof DAMAGE_PROLOGUE, 13, (void *)det_damage, (void **)&g_dmg_orig))
        log_printf("DEATH: traza de daño al jugador activa");
    /* matar: con el modo caído siempre (remates), y con la traza */
    if ((g_trace_dmg || g_downed_cfg) && g_kill &&
        hook_install(&g_hk_kill, "matar (FUN_007d0c10)", (void *)(g_aimp_exe + AIMP_RVA_CHR_KILL), KILL_PROLOGUE, sizeof KILL_PROLOGUE, 7,
                     (void *)det_kill, (void **)&g_kill_orig))
        log_printf("DEATH: hook de matar activo (%s)", g_downed_cfg ? "remates con el modo caído" : "traza");
}
