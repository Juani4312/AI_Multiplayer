/* Fase 4 (experimental): avatar del jugador remoto ("ghost"). Hallazgo 12.
 *
 * El spawn de autoridad del engine es FUN_00797490 (thiscall(this, pedido*, flag), ret 8):
 * avisa a dos listas de observadores, llama a this->vf[1](pedido) (SpawnCharacter) y
 * encola una copia del pedido de 0x130 bytes como RPC (A 0x29, hallazgo 11).
 *
 * - AIMP_TRACE_SPAWN=1: hook en esa función. Guarda `this` y una copia de cada pedido, y
 *   los loguea decodificados (clave, tipo, id, nombres) con el volcado completo. Solo mira.
 * - AIMP_GHOST=1 (implica el hook): AIMP_GHOST_DELAY_MS después de que aparece el jugador
 *   local, clona el último pedido del jugador (tipo 0; o el de clave AIMP_GHOST_FROM) con
 *   otra clave y otro id, y llama a la función original con el mismo `this`. Cuando el
 *   personaje nuevo aparece en un vector del CharacterManager, le apaga el controlador de
 *   jugador (+0x31C -> +0x48 = 0, para que no lea el input local) y en cada frame lo
 *   teletransporta (FUN_007c6330) a la posición del otro jugador si hay red y están en el
 *   mismo nivel; si no, a 1,5 m del jugador local.
 *
 * Esto sí escribe en el juego y llama a funciones del engine. Cada paso se loguea antes de
 * hacerlo: si el juego se cae, la última línea del log dice dónde.
 */
#include "ghost.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define MAX_REQS 32  /* anillo de pedidos capturados */
#define LOG_REQS 24  /* pedidos con volcado completo por sesión */
#define MAX_CHARS 64
#define MAX_ATTEMPTS 4

typedef void(__attribute__((fastcall)) * SpawnAuthFn)(void *self, void *edx, void *req, uint32_t flag);
typedef void(__attribute__((fastcall)) * TeleportFn)(void *self, void *edx, const float *pos, float yaw, uint32_t p3);

enum { G_OFF, G_WAIT_PLAYER, G_DELAY, G_FIND, G_ACTIVE, G_DONE };

static AimpHook g_hook;
static SpawnAuthFn g_spawn_orig;
static TeleportFn g_teleport;
static volatile uintptr_t g_this; /* `this` del último spawn visto */
static uint8_t g_req[MAX_REQS][SPAWN_REQ_SIZE];
static volatile LONG g_nreq; /* pedidos vistos en la sesión */
static uint8_t g_clone[SPAWN_REQ_SIZE];
static int g_ghost, g_state = G_OFF, g_from_key = -1;
static int64_t g_freq = 1, g_delay_ticks, g_t0, g_deadline, g_last_log, g_last_tp;
static uint32_t g_player, g_chr, g_before[2 * MAX_CHARS], g_nbefore, g_attempts, g_tps;
static float g_last_pos[3], g_last_yaw;

typedef struct {
    uint32_t cm, pl[MAX_CHARS], npl, npc[MAX_CHARS], nnpc, npend;
} Chars;

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static void sanitize(char *dst, const uint8_t *src, size_t cap)
{
    size_t i = 0;
    for (; i + 1 < cap && src[i]; i++)
        dst[i] = (src[i] >= 0x20 && src[i] < 0x7F) ? (char)src[i] : '?';
    dst[i] = 0;
}

static uint32_t rd32(const uint8_t *p)
{
    uint32_t v;
    aimp_memcpy(&v, p, 4);
    return v;
}

static void id_str(char *out, const uint8_t *id) /* 8 bytes en orden de memoria, como el resto del log */
{
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        out[2 * i] = hx[id[i] >> 4];
        out[2 * i + 1] = hx[id[i] & 15];
    }
    out[16] = 0;
}

static void log_request(const char *tag, long n, uintptr_t self, const uint8_t *r, uintptr_t from)
{
    char name[0x41], comp[0x41], id[17];
    uint32_t vt = 0, vf1 = 0;
    sanitize(name, r + SREQ_NAME, sizeof name);
    sanitize(comp, r + SREQ_COMPOSITE, sizeof comp);
    id_str(id, r + SREQ_ID);
    if (self && mem_u32(self, &vt) && vt)
        mem_u32(vt + 4, &vf1);
    log_printf("SP %s %ld: this 0x%08x (vtable rva 0x%x, vf[1] rva 0x%x), desde rva 0x%x | clave %u tipo %u id %s | \"%s\" / \"%s\"",
               tag, n, (unsigned)self, rva(vt), rva(vf1), rva((uint32_t)from), rd32(r + SREQ_KEY), rd32(r + SREQ_TYPE),
               id, name, comp);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_spawn(void *self, void *edx, void *req,
                                                                                 uint32_t flag)
{
    if (req) {
        LONG n = InterlockedIncrement(&g_nreq);
        uint8_t *slot = g_req[(n - 1) % MAX_REQS];
        aimp_memcpy(slot, req, SPAWN_REQ_SIZE);
        g_this = (uintptr_t)self;
        if (n <= LOG_REQS) {
            log_request("pedido", n, (uintptr_t)self, slot, (uintptr_t)__builtin_return_address(0));
            log_hexdump("SP   volcado", (uintptr_t)slot, SPAWN_REQ_SIZE);
        }
    }
    g_spawn_orig(self, edx, req, flag);
}

int ghost_install(void)
{
    g_ghost = aimp_env_int(L"AIMP_GHOST", 0);
    if (!g_ghost && !aimp_env_int(L"AIMP_TRACE_SPAWN", 0))
        return 0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    uint8_t got[sizeof AIMP_CHR_TELEPORT_PROLOGUE];
    uintptr_t tp = g_aimp_exe + AIMP_RVA_CHR_TELEPORT;
    if (g_ghost && (!mem_read(tp, got, sizeof got) || aimp_memcmp(got, AIMP_CHR_TELEPORT_PROLOGUE, sizeof got) != 0)) {
        log_printf("GHOST: el prólogo de FUN_007c6330 (teleport) no coincide; AIMP_GHOST queda apagado");
        g_ghost = 0;
    }
    g_teleport = (TeleportFn)(void *)tp;
    if (!hook_install(&g_hook, "spawn de autoridad (FUN_00797490)", (void *)(g_aimp_exe + AIMP_RVA_SPAWN_AUTH),
                      AIMP_SPAWN_AUTH_PROLOGUE, sizeof AIMP_SPAWN_AUTH_PROLOGUE, AIMP_SPAWN_AUTH_STEAL,
                      (void *)det_spawn, (void **)&g_spawn_orig)) {
        g_ghost = 0;
        return 0;
    }
    int delay = aimp_env_int(L"AIMP_GHOST_DELAY_MS", 8000);
    g_delay_ticks = g_freq * (delay < 0 ? 0 : delay) / 1000;
    g_from_key = aimp_env_int(L"AIMP_GHOST_FROM", -1);
    if (g_ghost) {
        g_state = G_WAIT_PLAYER;
        if (g_from_key < 0)
            log_printf("AIMP_GHOST=1 (fase 4, experimental): %d ms después de que aparece el jugador se clona su pedido de spawn", delay);
        else
            log_printf("AIMP_GHOST=1 (fase 4, experimental): %d ms después de que aparece el jugador se clona el pedido de clave %d",
                       delay, g_from_key);
    } else {
        log_printf("AIMP_TRACE_SPAWN=1: se loguean los pedidos de spawn (solo mira)");
    }
    return 1;
}

static int read_vec(uint32_t cm, uint32_t f_arr, uint32_t f_n, uint32_t *out, uint32_t *n)
{
    uint32_t arr = 0, cnt = 0;
    if (!mem_u32(cm + f_arr, &arr) || !mem_u32(cm + f_n, &cnt) || cnt > MAX_CHARS)
        return 0;
    if (cnt && !mem_read(arr, out, cnt * 4))
        return 0;
    *n = cnt;
    return 1;
}

static int read_chars(Chars *c)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT);
    c->cm = c->npl = c->nnpc = c->npend = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &c->cm) || !c->cm)
        return 0;
    if (!read_vec(c->cm, CM_PLAYERS, CM_NPLAYERS, c->pl, &c->npl) || !read_vec(c->cm, CM_NPCS, CM_NNPCS, c->npc, &c->nnpc))
        return 0;
    mem_u32(c->cm + CM_NPENDING, &c->npend);
    return 1;
}

static const char *find_chr(const Chars *c, uint32_t p, uint32_t *idx)
{
    for (uint32_t i = 0; i < c->npl; i++)
        if (c->pl[i] == p) {
            *idx = i;
            return "jugadores";
        }
    for (uint32_t i = 0; i < c->nnpc; i++)
        if (c->npc[i] == p) {
            *idx = i;
            return "NPCs";
        }
    return NULL;
}

static int was_before(uint32_t p)
{
    for (uint32_t i = 0; i < g_nbefore; i++)
        if (g_before[i] == p)
            return 1;
    return 0;
}

static void do_spawn(const Chars *c, int64_t now)
{
    LONG total = g_nreq;
    int found = -1;
    for (LONG k = total; k > 0 && k > total - MAX_REQS; k--) {
        const uint8_t *r = g_req[(k - 1) % MAX_REQS];
        uint32_t key = rd32(r + SREQ_KEY), type = rd32(r + SREQ_TYPE);
        if ((g_from_key < 0 && type == 0) || (g_from_key >= 0 && key == (uint32_t)g_from_key)) {
            found = (int)((k - 1) % MAX_REQS);
            break;
        }
    }
    if (found < 0 || !g_this) {
        log_printf("GHOST: no hay pedido para clonar (pedidos vistos %ld, this 0x%08x): no se clona en este nivel", (long)total,
                   (unsigned)g_this);
        g_state = G_DONE;
        return;
    }
    if (g_attempts >= MAX_ATTEMPTS) {
        log_printf("GHOST: ya se hicieron %u intentos en la sesión; no se clona más", g_attempts);
        g_state = G_DONE;
        return;
    }
    aimp_memcpy(g_clone, g_req[found], SPAWN_REQ_SIZE);
    uint32_t old_key = rd32(g_clone + SREQ_KEY), key = 0x40 + g_attempts, id1 = 0x504D4941u + g_attempts; /* "AIMP" */
    char id_old[17], id_new[17];
    id_str(id_old, g_clone + SREQ_ID);
    aimp_memcpy(g_clone + SREQ_KEY, &key, 4);
    aimp_memcpy(g_clone + SREQ_ID + 4, &id1, 4);
    id_str(id_new, g_clone + SREQ_ID);
    g_nbefore = 0;
    for (uint32_t i = 0; i < c->npl; i++)
        g_before[g_nbefore++] = c->pl[i];
    for (uint32_t i = 0; i < c->nnpc; i++)
        g_before[g_nbefore++] = c->npc[i];
    g_attempts++;
    log_printf("GHOST: clonando el pedido (clave %u -> %u, id %s -> %s) con this 0x%08x; antes: %u jugadores, %u NPCs, %u pendientes",
               old_key, key, id_old, id_new, (unsigned)g_this, c->npl, c->nnpc, c->npend);
    log_request("clon", (long)g_attempts, g_this, g_clone, 0);
    g_spawn_orig((void *)g_this, NULL, g_clone, 0);
    Chars a;
    if (read_chars(&a))
        log_printf("GHOST: el spawn volvió; ahora %u jugadores, %u NPCs, %u pendientes", a.npl, a.nnpc, a.npend);
    else
        log_printf("GHOST: el spawn volvió; no se pudo leer el CharacterManager");
    g_state = G_FIND;
    g_deadline = now + 5 * g_freq;
}

static void on_found(uint32_t p, const char *where, uint32_t idx, int64_t now)
{
    uint32_t vt = 0, def = 0, type = 0, ctrl = 0;
    uint8_t f1c = 0, f1f = 0, act = 0, id[8] = { 0 };
    float pos[3] = { 0 };
    char ids[17];
    mem_u32(p, &vt);
    mem_u32(p + CHR_DEF, &def);
    if (def) {
        mem_u32(def + DEF_TYPE, &type);
        mem_u8(def + DEF_FLAG_1C, &f1c);
        mem_u8(def + DEF_FLAG_1F, &f1f);
    }
    mem_u32(p + CHR_CTRL, &ctrl);
    if (ctrl)
        mem_u8(ctrl + CTRL_ACTIVE, &act);
    mem_read(p + CHR_POS, pos, sizeof pos);
    mem_read(p + CHR_ID, id, sizeof id);
    id_str(ids, id);
    log_printf("GHOST: apareció el clon: %s[%u] = 0x%08x, vtable rva 0x%x, def 0x%08x tipo %u flags 1c=%u 1f=%u, controlador 0x%08x (activo %u), pos=(%.2f, %.2f, %.2f), id %s",
               where, idx, p, rva(vt), def, type, f1c, f1f, ctrl, act, (double)pos[0], (double)pos[1], (double)pos[2], ids);
    if (ctrl) {
        uint8_t zero = 0;
        log_printf("GHOST: desactivando el controlador del clon (+0x%x = 0)", CTRL_ACTIVE);
        if (!mem_write(ctrl + CTRL_ACTIVE, &zero, 1))
            log_printf("GHOST: no se pudo escribir en el controlador");
    }
    g_chr = p;
    g_state = G_ACTIVE;
    g_last_log = now;
    g_last_tp = 0;
    g_tps = 0;
}

static void move_clone(const Chars *c, const AimpNet *net, uint32_t level, int64_t now)
{
    uint32_t idx;
    const char *where = find_chr(c, g_chr, &idx);
    if (!where) {
        log_printf("GHOST: el clon 0x%08x ya no está en los vectores; se deja de mover", g_chr);
        g_state = G_DONE;
        return;
    }
    float lpos[3], lyaw, t[3], tyaw;
    if (!mem_read(g_player + CHR_POS, lpos, sizeof lpos) || !mem_f32(g_player + CHR_YAW, &lyaw))
        return;
    const char *src;
    if (net && net->have_remote && (net->remote.flags & AIMP_FLAG_PLAYER) && net->remote.level == level) {
        t[0] = net->remote.pos[0];
        t[1] = net->remote.pos[1];
        t[2] = net->remote.pos[2];
        tyaw = net->remote.yaw;
        src = "red";
    } else {
        t[0] = lpos[0] + 1.5f;
        t[1] = lpos[1];
        t[2] = lpos[2];
        tyaw = lyaw;
        src = "local +1,5 m en x";
    }
    float dx = t[0] - g_last_pos[0], dy = t[1] - g_last_pos[1], dz = t[2] - g_last_pos[2], dyaw = tyaw - g_last_yaw;
    int moved = dx * dx + dy * dy + dz * dz > 0.0004f || dyaw > 0.01f || dyaw < -0.01f;
    if (moved || !g_last_tp || now - g_last_tp > g_freq / 2) {
        if (!g_last_tp)
            log_printf("GHOST: primer teletransporte a (%.2f, %.2f, %.2f) yaw %.3f [%s]", (double)t[0], (double)t[1],
                       (double)t[2], (double)tyaw, src);
        g_teleport((void *)g_chr, NULL, t, tyaw, 0);
        g_last_pos[0] = t[0];
        g_last_pos[1] = t[1];
        g_last_pos[2] = t[2];
        g_last_yaw = tyaw;
        g_last_tp = now;
        g_tps++;
    }
    if (now - g_last_log >= g_freq) {
        float cp[3] = { 0 };
        mem_read(g_chr + CHR_POS, cp, sizeof cp);
        log_printf("GHOST: clon en (%.2f, %.2f, %.2f), objetivo (%.2f, %.2f, %.2f) [%s], %u teletransportes; %s[%u]",
                   (double)cp[0], (double)cp[1], (double)cp[2], (double)t[0], (double)t[1], (double)t[2], src, g_tps, where,
                   idx);
        g_last_log = now;
        g_tps = 0;
    }
}

void ghost_frame(int64_t now, const AimpNet *net, uint32_t level)
{
    if (!g_ghost || g_state == G_OFF)
        return;
    Chars c;
    if (!read_chars(&c))
        return;
    uint32_t local = c.npl ? c.pl[0] : 0;
    if (local != g_player) { /* nivel nuevo, o salió del nivel */
        if (g_state != G_WAIT_PLAYER)
            log_printf("GHOST: cambió el jugador local (0x%08x -> 0x%08x): se reinicia", g_player, local);
        g_player = local;
        g_chr = 0;
        g_state = G_WAIT_PLAYER;
    }
    switch (g_state) {
    case G_WAIT_PLAYER:
        if (local) {
            g_t0 = now;
            g_state = G_DELAY;
        }
        break;
    case G_DELAY:
        if (now - g_t0 >= g_delay_ticks)
            do_spawn(&c, now);
        break;
    case G_FIND: {
        for (uint32_t i = 0; i < c.npl; i++)
            if (c.pl[i] && !was_before(c.pl[i])) {
                on_found(c.pl[i], "jugadores", i, now);
                return;
            }
        for (uint32_t i = 0; i < c.nnpc; i++)
            if (c.npc[i] && !was_before(c.npc[i])) {
                on_found(c.npc[i], "NPCs", i, now);
                return;
            }
        if (now > g_deadline) {
            log_printf("GHOST: en 5 s no apareció ningún personaje nuevo (%u jugadores, %u NPCs, %u pendientes)", c.npl,
                       c.nnpc, c.npend);
            g_state = G_DONE;
        }
        break;
    }
    case G_ACTIVE:
        move_clone(&c, net, level, now);
        break;
    default:
        break;
    }
}
