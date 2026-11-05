/* Fase 5 (experimental): "usar" compartido. Hallazgo 12 (parte 3) y hallazgo 11.
 *
 * Cuando el jugador usa algo (botón, caja, ítem, locker, terminal), el juego llama a
 * FUN_00795d00(this, payload, 0), con payload = objetivo[8] | personaje[8] | flag[4]
 * (FUN_007a3110 lo arma). Esa función encola el RPC A 0x26; el handler A (FUN_00418a10) lo
 * pasa a la sesión (B 0x29) y el handler de sesión (FUN_00418980) llama a ctx->vf[1]. Es el
 * mismo patrón que el spawn, donde lo que crea de verdad es un handler de sesión.
 *
 * Con AIMP_SYNC_USE=1 (y red):
 * - hook en FUN_00795d00: si el personaje del payload es el jugador local, el payload sale
 *   como evento AIMP_EVT_USE (net.c: se manda 3 veces y el receptor descarta repetidos);
 * - el receptor, si está en el mismo nivel, encola un A 0x26 (FUN_00a2a290, con el tipo que
 *   el juego guardó en [0x163652C]) con ese objetivo y su propio jugador como personaje,
 *   como si lo hubiera usado él. No vuelve a mandarse: no pasa por FUN_00795d00.
 * Los ids de entidad son iguales en las dos máquinas con el mismo nivel y los mismos datos
 * (corrida 9: el mismo NPC tenía el mismo id en papito y en la laptop).
 *
 * AIMP_TRACE_USE=1 (o AIMP_SYNC_USE=1) además hookea los dos handlers, solo para loguear
 * por dónde pasa cada uso.
 */
#include "sync.h"
#include "game.h"
#include "ghost.h"
#include "hook.h"
#include "log.h"

#define LOG_MAX 64

typedef void(__attribute__((fastcall)) * UseFn)(void *self, void *edx, void *payload, uint32_t flag);
typedef uint32_t(__attribute__((cdecl)) * HandlerFn)(void *ctx, void *payload);
typedef uint32_t(__attribute__((cdecl)) * EnqueueFn)(uint32_t type, const void *data, uint32_t size);

static AimpHook g_hk_use, g_hk_ha, g_hk_hs;
static UseFn g_use_orig;
static HandlerFn g_ha_orig, g_hs_orig;
static AimpNet *g_net;
static int g_on, g_actor = 1; /* quién usa lo que llega: 1 = el avatar (si hay), 0 = el jugador local */
static volatile uint32_t g_level;
static volatile LONG g_n_use, g_n_ha, g_n_hs;
/* Prefijos de objetivo (primeros 4 bytes del id, en hex como en el log) cuyos usos no se mandan:
 * son acciones del jugador, no estado del mundo (hallazgo 13). Lockers y terminal de guardado. */
static char g_skip[256] = "3e3ec35d,1e38b898";

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static uint32_t rd32(const uint8_t *p)
{
    uint32_t v;
    aimp_memcpy(&v, p, 4);
    return v;
}

static void hex8(char *out, const uint8_t *id)
{
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        out[2 * i] = hx[id[i] >> 4];
        out[2 * i + 1] = hx[id[i] & 15];
    }
    out[16] = 0;
}

/* ¿El prefijo del objetivo (8 caracteres hex) está en g_skip? */
static int skipped(const char *hex8chars)
{
    for (const char *q = g_skip; *q;) {
        while (*q == ',' || *q == ' ')
            q++;
        int i = 0;
        while (i < 8 && q[i] && (q[i] | 0x20) == hex8chars[i])
            i++;
        if (i == 8 && (q[8] == 0 || q[8] == ',' || q[8] == ' '))
            return 1;
        while (*q && *q != ',')
            q++;
    }
    return 0;
}

static uint32_t local_player(void)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0, pl = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm || !mem_u32(cm + CM_NPLAYERS, &n) || !n ||
        !mem_u32(cm + CM_PLAYERS, &arr) || !mem_u32(arr, &pl))
        return 0;
    return pl;
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_use(void *self, void *edx, void *payload,
                                                                               uint32_t flag)
{
    uint8_t p[20];
    if (payload && mem_read((uintptr_t)payload, p, sizeof p)) {
        LONG n = InterlockedIncrement(&g_n_use);
        uint32_t pl = local_player(), vt = 0, vf1 = 0;
        uint8_t id[8];
        int mine = pl && mem_read(pl + CHR_ID, id, sizeof id) && aimp_memcmp(id, p + 8, 8) == 0;
        char t[17], c[17];
        hex8(t, p);
        hex8(c, p + 8);
        if (mem_u32((uintptr_t)self, &vt) && vt)
            mem_u32(vt + 4, &vf1);
        if (n <= LOG_MAX)
            log_printf("SYNC uso local %ld: this 0x%08x (vtable rva 0x%x, vf[1] rva 0x%x), desde rva 0x%x | objetivo %s, personaje %s%s, flag %08x",
                       (long)n, (unsigned)(uintptr_t)self, rva(vt), rva(vf1), rva((uint32_t)(uintptr_t)__builtin_return_address(0)),
                       t, c, mine ? " (jugador local)" : "", rd32(p + 16));
        if (mine && g_on && g_net && g_net->mode && skipped(t)) {
            log_printf("SYNC   no se manda: el objetivo empieza con un prefijo de AIMP_SYNC_USE_SKIP (acción del jugador, no del mundo)");
        } else if (mine && g_on && g_net && g_net->mode) {
            if (net_event(g_net, AIMP_EVT_USE, g_level, p, sizeof p))
                log_printf("SYNC   mandado al otro jugador (evento %u, nivel %u)", g_net->evt_seq, g_level);
            else
                log_printf("SYNC   no se pudo mandar (sin par o cola llena)");
        }
    }
    g_use_orig(self, edx, payload, flag);
}

static void log_handler(const char *what, LONG n, void *ctx, void *payload)
{
    uint8_t p[20];
    char t[17] = "?", c[17] = "?";
    uint32_t vt = 0, f1 = 0, f2 = 0;
    if (payload && mem_read((uintptr_t)payload, p, sizeof p)) {
        hex8(t, p);
        hex8(c, p + 8);
    }
    if (ctx && mem_u32((uintptr_t)ctx, &vt) && vt) {
        mem_u32(vt + 4, &f1);
        mem_u32(vt + 8, &f2);
    }
    log_printf("SYNC %s %ld: ctx 0x%08x (vtable rva 0x%x, vf[1] rva 0x%x, vf[2] rva 0x%x) | objetivo %s, personaje %s", what,
               (long)n, (unsigned)(uintptr_t)ctx, rva(vt), rva(f1), rva(f2), t, c);
}

__attribute__((cdecl, force_align_arg_pointer, noinline)) static uint32_t det_ha(void *ctx, void *payload)
{
    LONG n = InterlockedIncrement(&g_n_ha);
    if (n <= LOG_MAX)
        log_handler("handler A del uso", n, ctx, payload);
    return g_ha_orig(ctx, payload);
}

__attribute__((cdecl, force_align_arg_pointer, noinline)) static uint32_t det_hs(void *ctx, void *payload)
{
    LONG n = InterlockedIncrement(&g_n_hs);
    if (n <= LOG_MAX)
        log_handler("handler de sesión del uso", n, ctx, payload);
    return g_hs_orig(ctx, payload);
}

int sync_install(AimpNet *net)
{
    int trace = aimp_env_int(L"AIMP_TRACE_USE", 0);
    g_on = aimp_env_int(L"AIMP_SYNC_USE", 0);
    if (!trace && !g_on)
        return 0;
    g_net = net;
    /* Corrida 11: con el jugador local como personaje, un botón remoto lo teletransporta al botón.
     * Con el avatar (el NPC que ya está donde está el otro), el que se mueve es el avatar. */
    g_actor = aimp_env_int(L"AIMP_SYNC_USE_ACTOR", 1);
    wchar_t wskip[128];
    if (aimp_env_str(L"AIMP_SYNC_USE_SKIP", wskip, 128))
        aimp_utf8(g_skip, sizeof g_skip, wskip);
    else if (aimp_env_int(L"AIMP_SYNC_USE_SKIP", 1) == 0)
        g_skip[0] = 0;
    int ok = hook_install(&g_hk_use, "usar (FUN_00795d00)", (void *)(g_aimp_exe + AIMP_RVA_USE_PRODUCER),
                          AIMP_USE_PRODUCER_PROLOGUE, sizeof AIMP_USE_PRODUCER_PROLOGUE, AIMP_USE_PRODUCER_STEAL,
                          (void *)det_use, (void **)&g_use_orig);
    hook_install(&g_hk_ha, "handler A del uso (FUN_00418a10)", (void *)(g_aimp_exe + AIMP_RVA_USE_HANDLER_A),
                 AIMP_USE_HANDLER_A_PROLOGUE, sizeof AIMP_USE_HANDLER_A_PROLOGUE, AIMP_USE_HANDLER_A_STEAL, (void *)det_ha,
                 (void **)&g_ha_orig);
    hook_install(&g_hk_hs, "handler de sesión del uso (FUN_00418980)", (void *)(g_aimp_exe + AIMP_RVA_USE_HANDLER_S),
                 AIMP_USE_HANDLER_S_PROLOGUE, sizeof AIMP_USE_HANDLER_S_PROLOGUE, AIMP_USE_HANDLER_S_STEAL, (void *)det_hs,
                 (void **)&g_hs_orig);
    if (!ok)
        g_on = 0;
    if (g_on)
        log_printf("AIMP_SYNC_USE=1 (fase 5, experimental): los usos del jugador local se mandan al otro y los del otro se aplican acá%s; no se mandan los objetivos con prefijo [%s]",
                   net && net->mode ? "" : " (sin red: solo se loguea)", g_skip);
    else
        log_printf("AIMP_TRACE_USE=1: se loguean los usos y sus handlers (solo mira)");
    return ok;
}

void sync_frame(AimpNet *net, uint32_t level)
{
    g_level = level;
    if (!g_on || !net || !net->mode)
        return;
    AimpEvt e;
    while (net_take_event(net, &e)) {
        if (e.kind != AIMP_EVT_USE || e.size != 20) {
            log_printf("SYNC: evento remoto desconocido (tipo %u, %u bytes): se ignora", e.kind, e.size);
            continue;
        }
        char t[17];
        hex8(t, e.data);
        if (e.level != level) {
            log_printf("SYNC: uso remoto (evento %u, objetivo %s) del nivel %u; acá el nivel es %u: se ignora", e.seq, t, e.level,
                       level);
            continue;
        }
        uint32_t pl = g_actor ? ghost_avatar() : 0;
        const char *who = "el avatar";
        if (!pl) {
            pl = local_player();
            who = "el jugador local";
        }
        uint8_t p[20];
        aimp_memcpy(p, e.data, sizeof p);
        if (!pl || !mem_read(pl + CHR_ID, p + 8, 8)) {
            log_printf("SYNC: uso remoto (evento %u) sin jugador local ni avatar: se ignora", e.seq);
            continue;
        }
        uint16_t type = *(volatile uint16_t *)(g_aimp_exe + AIMP_RVA_USE_TYPE_A);
        if (!type) {
            log_printf("SYNC: el id del RPC de uso todavía no está asignado: se ignora el evento %u", e.seq);
            continue;
        }
        char cid[17];
        hex8(cid, p + 8);
        log_printf("SYNC: aplicando uso remoto (evento %u): objetivo %s, RPC A 0x%x, personaje = %s (%s), flag %08x", e.seq, t,
                   type, who, cid, rd32(p + 16));
        ((EnqueueFn)(void *)(g_aimp_exe + AIMP_RVA_ENQUEUE_A))(type, p, sizeof p);
    }
    net_send_events(net);
}
