/* Trazas para la animación del avatar (fase 4b) y un segundo jugador "PLAYER_2" (NOTES, 2026-11-15).
 * Solo miran: no escriben nada del juego.
 *
 * AIMP_TRACE_CTRL=1: el controlador de jugador del jugador 0 (Character+0x31C, 0x4C bytes, lo crea
 *   FUN_007b2630 para los personajes con definición de jugador) y la acción principal
 *   (Character+0x36C: id +0x19 y los dos bloques [largo][datos] de +0x0C/+0x10), cada vez que cambian y
 *   como mucho cada AIMP_TRACE_CTRL_MS (100). Con marcas F8 al caminar, correr, agacharse y girar,
 *   alcanza para mapear qué campo es cada eje y cada botón (el plan: escribirle esa entrada a un
 *   segundo personaje-jugador). Hasta AIMP_TRACE_CTRL_MAX líneas (600).
 *
 * AIMP_TRACE_CMD=1: hook de SwitchCommand (FUN_00678e20, thiscall(controlador, comando), ret 4). Corre
 *   cada vez que un personaje cambia de comando, [TASK] (IA) o [SCRIPT] (CMD_* de los scripts). Loguea
 *   el controlador, el tipo (comando+0x28) y a qué personaje pertenece: offset del controlador desde
 *   cada Character a menos de 64 KB y si alguno de los primeros dwords del controlador apunta a un
 *   Character. Sirve para ubicar el controlador de comandos (el `this` de FUN_0067e980, MoveTowards).
 */
#include "ctrl.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define CMD_LOG_MAX 160
#define CMD_PER_CTRL 3
#define CTRLS_MAX 64
#define CHARS_MAX 96

typedef void(__attribute__((fastcall)) * SwitchFn)(void *ctrl, void *edx, void *cmd);

static AimpHook g_hk_switch;
static SwitchFn g_switch_orig;
static int g_trace_ctrl, g_trace_cmd, g_ctrl_max = 600, g_ctrl_ms = 100;
static volatile LONG g_n_switch, g_n_logged;
static CRITICAL_SECTION g_cs;
static int g_cs_ok;
static uint32_t g_ctrls[CTRLS_MAX];
static uint8_t g_ctrl_seen[CTRLS_MAX];
static int g_nctrls;

/* estado de AIMP_TRACE_CTRL (thread principal) */
static uint8_t g_last[CTRL_SIZE], g_last_act[48];
static uint32_t g_last_pl, g_last_id = 0xFFFFFFFFu;
static int g_have_last, g_ctrl_lines;
static int64_t g_last_line;

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static int chars(uint32_t *out, int cap, int *nplayers)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0;
    int k = 0;
    *nplayers = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm)
        return 0;
    if (mem_u32(cm + CM_NPLAYERS, &n) && n && n < 8 && mem_u32(cm + CM_PLAYERS, &arr) && arr)
        for (uint32_t i = 0; i < n && k < cap; i++)
            if (mem_u32(arr + 4 * i, &out[k]) && out[k])
                k++;
    *nplayers = k;
    if (mem_u32(cm + CM_NNPCS, &n) && n && n < 256 && mem_u32(cm + CM_NPCS, &arr) && arr)
        for (uint32_t i = 0; i < n && k < cap; i++)
            if (mem_u32(arr + 4 * i, &out[k]) && out[k])
                k++;
    return k;
}

static const char *cmd_name(uint32_t type, char *buf, size_t cap)
{
    uint32_t p = 0;
    if (type < COMMAND_TYPES && mem_u32(g_aimp_exe + AIMP_RVA_COMMAND_NAMES + 4 * type, &p) && p &&
        mem_cstr(p, buf, cap))
        return buf;
    aimp_snprintf(buf, cap, "?%u", type);
    return buf;
}

/* Describe a quién pertenece el controlador: offset desde un Character cercano y punteros a Characters. */
static void owner(uint32_t ctrl, char *out, size_t cap)
{
    uint32_t cs[CHARS_MAX];
    int np = 0, n = chars(cs, CHARS_MAX, &np), len = 0;
    out[0] = 0;
    for (int i = 0; i < n && len < (int)cap - 40; i++) {
        if (ctrl >= cs[i] && ctrl - cs[i] < 0x10000)
            len += aimp_snprintf(out + len, cap - (size_t)len, " %s%d+0x%x", i < np ? "J" : "N", i < np ? i : i - np,
                                 ctrl - cs[i]);
    }
    for (uint32_t off = 0; off < 0x60 && len < (int)cap - 40; off += 4) {
        uint32_t v = 0;
        if (!mem_u32(ctrl + off, &v) || !v)
            continue;
        for (int i = 0; i < n; i++)
            if (v == cs[i])
                len += aimp_snprintf(out + len, cap - (size_t)len, " [+0x%x]=%s%d", off, i < np ? "J" : "N",
                                     i < np ? i : i - np);
    }
    if (!out[0])
        aimp_snprintf(out, cap, " (no está cerca de ningún Character ni apunta a uno)");
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_switch(void *ctrl, void *edx, void *cmd)
{
    LONG n = InterlockedIncrement(&g_n_switch);
    uint32_t c = (uint32_t)(uintptr_t)ctrl, type = 0xFFFFFFFFu;
    uint8_t fl = 0;
    int show = 0, idx = -1;
    if (g_cs_ok && g_n_logged < CMD_LOG_MAX) {
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < g_nctrls; i++)
            if (g_ctrls[i] == c)
                idx = i;
        if (idx < 0 && g_nctrls < CTRLS_MAX) {
            idx = g_nctrls++;
            g_ctrls[idx] = c;
            g_ctrl_seen[idx] = 0;
        }
        if (idx >= 0 && g_ctrl_seen[idx] < CMD_PER_CTRL) {
            g_ctrl_seen[idx]++;
            show = 1;
        }
        LeaveCriticalSection(&g_cs);
    }
    if (show && InterlockedIncrement(&g_n_logged) <= CMD_LOG_MAX) {
        char nm[48], who[256];
        if (cmd)
            mem_u32((uintptr_t)cmd + CMD_TYPE, &type);
        mem_u8(c + CMDCTRL_FLAGS, &fl);
        owner(c, who, sizeof who);
        log_printf("CMD SwitchCommand %ld: controlador 0x%08x (#%d) %s comando 0x%08x tipo %s, desde rva 0x%x; dueño:%s", (long)n,
                   c, idx, (fl & 4) ? "[TASK]" : "[SCRIPT]", (unsigned)(uintptr_t)cmd, cmd_name(type, nm, sizeof nm),
                   rva((uint32_t)(uintptr_t)__builtin_return_address(0)), who);
    }
    g_switch_orig(ctrl, edx, cmd);
}

int ctrl_install(void)
{
    g_trace_ctrl = aimp_env_int(L"AIMP_TRACE_CTRL", 0);
    g_trace_cmd = aimp_env_int(L"AIMP_TRACE_CMD", 0);
    g_ctrl_max = aimp_env_int(L"AIMP_TRACE_CTRL_MAX", 600);
    g_ctrl_ms = aimp_env_int(L"AIMP_TRACE_CTRL_MS", 100);
    if (!g_trace_ctrl && !g_trace_cmd)
        return 0;
    if (!g_cs_ok) {
        InitializeCriticalSection(&g_cs);
        g_cs_ok = 1;
    }
    int ok = 1;
    if (g_trace_cmd)
        ok = hook_install(&g_hk_switch, "SwitchCommand (FUN_00678e20)", (void *)(g_aimp_exe + AIMP_RVA_SWITCH_COMMAND),
                          AIMP_SWITCH_COMMAND_PROLOGUE, sizeof AIMP_SWITCH_COMMAND_PROLOGUE, AIMP_SWITCH_COMMAND_STEAL,
                          (void *)det_switch, (void **)&g_switch_orig);
    log_printf("CTRL: AIMP_TRACE_CTRL=%d (cada %d ms, hasta %d líneas) AIMP_TRACE_CMD=%d%s", g_trace_ctrl, g_ctrl_ms, g_ctrl_max,
               g_trace_cmd, g_trace_cmd && !ok ? " (hook NO instalado)" : "");
    return ok;
}

static void hex(char *out, const uint8_t *b, int n)
{
    static const char hx[] = "0123456789abcdef";
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (i && (i & 3) == 0)
            out[k++] = ' ';
        out[k++] = hx[b[i] >> 4];
        out[k++] = hx[b[i] & 15];
    }
    out[k] = 0;
}

void ctrl_frame(int64_t now, int64_t freq)
{
    if (!g_trace_ctrl || g_ctrl_lines >= g_ctrl_max)
        return;
    uint32_t cs[2];
    int np = 0, n = chars(cs, 1, &np);
    if (n < 1 || np < 1) {
        g_have_last = 0;
        return;
    }
    uint32_t pl = cs[0], ctrl = 0, act = 0, pa = 0, pb = 0, idx = 0;
    uint8_t buf[CTRL_SIZE], ab[48], la = 0, lb = 0, id = 0;
    if (!mem_u32(pl + CHR_CTRL, &ctrl) || !ctrl || !mem_read(ctrl, buf, sizeof buf))
        return;
    aimp_memset(ab, 0, sizeof ab);
    if (mem_u32(pl + CHR_ACTION, &act) && act) {
        mem_u8(act + ACTION_ID, &id);
        if (mem_u32(act + ACTION_BLOCK_A, &pa) && pa)
            mem_u8(pa, &la);
        if (mem_u32(act + ACTION_BLOCK_B, &pb) && pb && mem_u8(pb, &lb) && lb)
            mem_read(pb, ab, lb < sizeof ab ? lb : sizeof ab);
    }
    if (pl != g_last_pl) {
        mem_u32(pl + CHR_PLAYER_INDEX, &idx);
        log_printf("CTRL jugador 0 = 0x%08x, controlador 0x%08x, acción 0x%08x, índice de jugador %u", pl, ctrl, act, idx);
        g_last_pl = pl;
        g_have_last = 0;
    }
    int changed = !g_have_last || aimp_memcmp(buf, g_last, sizeof buf) || id != g_last_id || aimp_memcmp(ab, g_last_act, sizeof ab);
    if (!changed || (g_have_last && freq && (now - g_last_line) * 1000 < (int64_t)g_ctrl_ms * freq))
        return;
    char h[3 * CTRL_SIZE + 8], ha[3 * sizeof ab + 8];
    hex(h, buf, CTRL_SIZE);
    hex(ha, ab, lb < sizeof ab ? lb : (int)sizeof ab);
    log_printf("CTRL %s", h);
    log_printf("CTRL   acción id %u, bloque A %u bytes, bloque B %u bytes: %s", id, la, lb, ha);
    aimp_memcpy(g_last, buf, sizeof buf);
    aimp_memcpy(g_last_act, ab, sizeof ab);
    g_last_id = id;
    g_have_last = 1;
    g_last_line = now;
    g_ctrl_lines += 2;
    if (g_ctrl_lines >= g_ctrl_max)
        log_printf("CTRL: tope de AIMP_TRACE_CTRL_MAX (%d líneas)", g_ctrl_max);
}
