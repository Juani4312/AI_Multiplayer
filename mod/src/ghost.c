/* Fase 4 (experimental): avatar del jugador remoto ("ghost"). Hallazgo 12.
 *
 * Lo que se sabe (corrida 6):
 * - El spawn de autoridad FUN_00797490 (thiscall(this, pedido*, flag), ret 8) recibe un
 *   `this` cuyo vf[1] es un `ret 4` vacío: solo avisa a dos listas de observadores (vacías
 *   en el binario) y encola la copia del pedido como RPC A 0x29. El handler del 0x29 tampoco
 *   crea nada (vf[2] = `mov al,1; ret 4`). Clonar el pedido por ahí hizo caer el juego en el
 *   frame siguiente, sin que se sepa todavía dónde.
 * - Por eso ahora también se hookea SpawnCharacter (0x7D3B50, thiscall(interfaz, pedido*),
 *   ret 4), que según el hallazgo 05 es quien crea el personaje.
 *
 * Variables:
 * - AIMP_TRACE_SPAWN=1: solo mira. Loguea los pedidos de los dos hooks ("SP" y "SC") y las
 *   cuatro listas de observadores.
 * - AIMP_GHOST (implica el trazado):
 *     1 = clon: AIMP_GHOST_DELAY_MS después de que aparece el jugador, clona su pedido (tipo 0)
 *         con otra clave (+0x00 y +0xF0) y otro id, y llama a SpawnCharacter con el mismo
 *         `this` que usó el juego. Si no se vio SpawnCharacter, no clona.
 *     2 = NPC y después clon: primero mueve un NPC existente (AIMP_GHOST_NPC_TYPE, 6 =
 *         INNOCENT) durante AIMP_GHOST_NPC_MS, y después hace el clon.
 *     3 = solo NPC.
 *   Al clon le apaga el controlador de jugador (+0x31C -> +0x48 = 0). Al personaje elegido
 *   (clon o NPC) lo teletransporta en cada frame con FUN_007c6330: a la posición del otro
 *   jugador si hay red y están en el mismo nivel, o a 1,5 m en x del jugador local.
 *
 * AIMP_GHOST_WALK=1 (fase 4b, experimental; hallazgo 19): en vez de teletransportar al NPC en cada
 * frame, se le da un comando MOVE_TOWARDS (CMD_MoveTowards: FUN_0067e980 sobre su controlador de
 * comandos, Character+0x3820) hacia el objetivo, y cada AIMP_GHOST_WALK_MS (200) se le actualiza el
 * destino (FUN_006797a0). Así camina con su propia animación. Solo se lo teletransporta la primera
 * vez o si queda a más de AIMP_GHOST_WALK_TP_CM (1200) u otro piso durante 1 s (cancelando antes el
 * comando: teletransportarlo caminando no se pega, corrida 22). Si la IA le cambia el comando,
 * se lo vuelve a dar (como mucho una vez por segundo). AIMP_GHOST_WALK_TYPE = move_type (1).
 * Con la caminata, AIMP_GHOST_NOAUTH queda apagado por defecto (un NPC sin autoridad no se mueve).
 * Corrida 21: camina, pero su IA (con autoridad) lo hacía huir del jugador. Con AIMP_GHOST_WALK_BLOCK=1
 * (por defecto) se bloquean los SwitchCommand de la IA sobre el controlador del avatar (filtro en
 * ctrl.c, que cancela el comando bloqueado para liberar su slot). Sin red, el objetivo es 1,2 m detrás
 * del jugador (AIMP_GHOST_WALK_BEHIND_CM, 250; adelante = (sin yaw, cos yaw)): "1,5 m en x" podía quedar
 * fuera del navmesh.
 *
 * Esto escribe en el juego y llama a funciones del engine. Cada paso se loguea antes de
 * hacerlo; si el juego se cae, el registro de excepciones (crash.c) dice dónde.
 */
#include "ghost.h"
#include "ctrl.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define MAX_REQS 32 /* anillo de pedidos capturados */
#define LOG_REQS 24 /* pedidos con volcado completo por sesión */
#define LOG_SC 24
#define MAX_CHARS 64
#define MAX_ATTEMPTS 4

typedef void(__attribute__((fastcall)) * SpawnAuthFn)(void *self, void *edx, void *req, uint32_t flag);
typedef uint32_t(__attribute__((fastcall)) * SpawnCharFn)(void *self, void *edx, void *req);
typedef void(__attribute__((fastcall)) * TeleportFn)(void *self, void *edx, const float *pos, float yaw, uint32_t p3);
typedef void(__attribute__((fastcall)) * MoveCmdFn)(void *ctrl, void *edx, const uint8_t *a, const uint8_t *b,
                                                    const float *target, const uint8_t *params, uint32_t never_succeed);

enum { G_OFF, G_WAIT_PLAYER, G_DELAY, G_NPC, G_FIND, G_ACTIVE, G_DONE };

static AimpHook g_hook_auth, g_hook_sc;
static SpawnAuthFn g_auth_orig;
static SpawnCharFn g_sc_orig;
static TeleportFn g_teleport;
static volatile uintptr_t g_this, g_sc_this; /* `this` del último spawn de autoridad / SpawnCharacter */
static uint8_t g_req[MAX_REQS][SPAWN_REQ_SIZE];
static volatile LONG g_nreq, g_nsc;
/* Alineados a 16: el engine lee estos datos con SSE (MOVAPS). */
static uint8_t g_clone[SPAWN_REQ_SIZE] __attribute__((aligned(16)));
static float g_vec[4] __attribute__((aligned(16)));
/* Pedidos que recibió SpawnCharacter (0xF0 bytes: el mensaje de sesión 0x2E, hallazgo 12). */
#define SC_REQ_SIZE 0xF0
static uint8_t g_scq[MAX_REQS][SC_REQ_SIZE];
static int g_mode, g_state = G_OFF, g_from_key = -1, g_npc_type = 6, g_obs_logged, g_warned_npc, g_chosen_once;
static int64_t g_last_pick;
static int g_noauth; /* AIMP_GHOST_NOAUTH=1: experimento, el avatar pasa a no autoritativo */
static int64_t g_freq = 1, g_delay_ticks, g_npc_ticks, g_t0, g_deadline, g_last_log, g_last_tp;
static uint32_t g_player, g_chr, g_before[2 * MAX_CHARS], g_nbefore, g_attempts, g_tps;
static float g_last_pos[3], g_last_yaw;
/* AIMP_GHOST_WALK */
static MoveCmdFn g_move_start, g_move_update;
static int g_walk, g_walk_type = 1, g_walk_tp_cm = 600, g_walk_bad, g_walk_block = 1;
static volatile uint32_t g_walk_ctrl; /* controlador del avatar, para el filtro (corre en otro thread) */
static volatile LONG g_n_blocked;
static int64_t g_walk_ticks, g_last_cmd, g_last_restart, g_wstate_at, g_far_since;
static int g_wstate, g_walk_behind_cm = 250;
typedef void(__attribute__((fastcall)) * SwitchCmdFn)(void *ctrl, void *edx, void *cmd);
static SwitchCmdFn g_switch_cmd;
static uint32_t g_starts, g_updates;
static uint8_t g_cmd_a[8] = { 'A', 'I', 'M', 'P', 'W', 'A', 'L', 'K' }, g_cmd_b[8];
static float g_target[12] __attribute__((aligned(16)));
static uint8_t g_params[32] __attribute__((aligned(16)));

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
    char name[0x41], def[0x41], id[17];
    uint32_t vt = 0, vf1 = 0;
    float pos[3], yaw;
    sanitize(name, r + SREQ_NAME, sizeof name);
    sanitize(def, r + SREQ_COMPOSITE, sizeof def);
    id_str(id, r + SREQ_ID);
    aimp_memcpy(pos, r + SREQ_POS, sizeof pos);
    aimp_memcpy(&yaw, r + SREQ_YAW, sizeof yaw);
    if (self && mem_u32(self, &vt) && vt)
        mem_u32(vt + 4, &vf1);
    log_printf("%s %ld: this 0x%08x (vtable rva 0x%x, vf[1] rva 0x%x), desde rva 0x%x | clave %u/%u tipo %u id %s | \"%s\" / \"%s\" | pos=(%.2f, %.2f, %.2f) yaw=%.3f",
               tag, n, (unsigned)self, rva(vt), rva(vf1), rva((uint32_t)from), rd32(r + SREQ_KEY), rd32(r + SREQ_KEY2),
               rd32(r + SREQ_TYPE), id, name, def, (double)pos[0], (double)pos[1], (double)pos[2], (double)yaw);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_auth(void *self, void *edx, void *req,
                                                                                uint32_t flag)
{
    if (req) {
        LONG n = InterlockedIncrement(&g_nreq);
        uint8_t *slot = g_req[(n - 1) % MAX_REQS];
        aimp_memcpy(slot, req, SPAWN_REQ_SIZE);
        g_this = (uintptr_t)self;
        if (n <= LOG_REQS) {
            log_request("SP pedido", n, (uintptr_t)self, slot, (uintptr_t)__builtin_return_address(0));
            log_hexdump("SP   volcado", (uintptr_t)slot, SPAWN_REQ_SIZE);
        }
    }
    g_auth_orig(self, edx, req, flag);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static uint32_t det_sc(void *self, void *edx, void *req)
{
    LONG n = InterlockedIncrement(&g_nsc);
    g_sc_this = (uintptr_t)self;
    if (req)
        mem_read((uintptr_t)req, g_scq[(n - 1) % MAX_REQS], SC_REQ_SIZE);
    if (req && n <= LOG_SC) {
        uint8_t r[SPAWN_REQ_SIZE];
        aimp_memset(r, 0, sizeof r);
        if (mem_read((uintptr_t)req, r, SC_REQ_SIZE)) {
            /* ¿Es el mismo pedido que el último del spawn de autoridad? */
            LONG k = g_nreq;
            int same = k > 0 && aimp_memcmp(r, g_req[(k - 1) % MAX_REQS], 0xE4) == 0;
            log_request("SC SpawnCharacter", n, (uintptr_t)self, r, (uintptr_t)__builtin_return_address(0));
            log_printf("SC   pedido en 0x%08x; %s", (unsigned)(uintptr_t)req,
                       same ? "igual al último del spawn de autoridad (0xE4 bytes)" : "distinto del último del spawn de autoridad");
        } else {
            log_printf("SC SpawnCharacter %ld: this 0x%08x, pedido 0x%08x ilegible", (long)n, (unsigned)(uintptr_t)self,
                       (unsigned)(uintptr_t)req);
        }
    }
    uint32_t r = g_sc_orig(self, edx, req);
    if (n <= LOG_SC)
        log_printf("SC   devolvió 0x%08x", r);
    return r;
}

int ghost_install(void)
{
    g_mode = aimp_env_int(L"AIMP_GHOST", 0);
    if (g_mode < 0 || g_mode > 3)
        g_mode = 0;
    if (!g_mode && !aimp_env_int(L"AIMP_TRACE_SPAWN", 0))
        return 0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    uint8_t got[sizeof AIMP_CHR_TELEPORT_PROLOGUE];
    uintptr_t tp = g_aimp_exe + AIMP_RVA_CHR_TELEPORT;
    if (g_mode && (!mem_read(tp, got, sizeof got) || aimp_memcmp(got, AIMP_CHR_TELEPORT_PROLOGUE, sizeof got) != 0)) {
        log_printf("GHOST: el prólogo de FUN_007c6330 (teleport) no coincide; AIMP_GHOST queda apagado");
        g_mode = 0;
    }
    g_teleport = (TeleportFn)(void *)tp;
    int ok = hook_install(&g_hook_auth, "spawn de autoridad (FUN_00797490)", (void *)(g_aimp_exe + AIMP_RVA_SPAWN_AUTH),
                          AIMP_SPAWN_AUTH_PROLOGUE, sizeof AIMP_SPAWN_AUTH_PROLOGUE, AIMP_SPAWN_AUTH_STEAL, (void *)det_auth,
                          (void **)&g_auth_orig);
    int ok_sc = hook_install(&g_hook_sc, "SpawnCharacter (FUN_007d3b50)", (void *)(g_aimp_exe + AIMP_RVA_SPAWN_CHAR),
                             AIMP_SPAWN_CHAR_PROLOGUE, sizeof AIMP_SPAWN_CHAR_PROLOGUE, AIMP_SPAWN_CHAR_STEAL, (void *)det_sc,
                             (void **)&g_sc_orig);
    if (!ok || !ok_sc) {
        log_printf("GHOST: falta un hook del spawn (autoridad %d, SpawnCharacter %d); AIMP_GHOST queda apagado", ok, ok_sc);
        g_mode = 0;
        return ok || ok_sc;
    }
    /* AIMP_GHOST_NPC_MS <= 0: sin límite (por defecto en el modo 3, el avatar de la fase 4). */
    int delay = aimp_env_int(L"AIMP_GHOST_DELAY_MS", 8000), npc_ms = aimp_env_int(L"AIMP_GHOST_NPC_MS", g_mode == 3 ? 0 : 20000);
    g_delay_ticks = g_freq * (delay < 0 ? 0 : delay) / 1000;
    g_npc_ticks = npc_ms > 0 ? g_freq * npc_ms / 1000 : 0;
    g_from_key = aimp_env_int(L"AIMP_GHOST_FROM", -1);
    g_npc_type = aimp_env_int(L"AIMP_GHOST_NPC_TYPE", 6);
    g_walk = g_mode >= 2 && aimp_env_int(L"AIMP_GHOST_WALK", 0);
    if (g_walk) {
        uint8_t a[sizeof AIMP_CMD_MOVE_START_PROLOGUE], b[sizeof AIMP_CMD_MOVE_UPDATE_PROLOGUE];
        uintptr_t ps = g_aimp_exe + AIMP_RVA_CMD_MOVE_START, pu = g_aimp_exe + AIMP_RVA_CMD_MOVE_UPDATE;
        uint8_t c[sizeof AIMP_SWITCH_COMMAND_PROLOGUE];
        uintptr_t pc = g_aimp_exe + AIMP_RVA_SWITCH_COMMAND; /* todavía sin hookear: ctrl_install corre después */
        if (!mem_read(ps, a, sizeof a) || aimp_memcmp(a, AIMP_CMD_MOVE_START_PROLOGUE, sizeof a) != 0 || !mem_read(pu, b, sizeof b) ||
            aimp_memcmp(b, AIMP_CMD_MOVE_UPDATE_PROLOGUE, sizeof b) != 0 || !mem_read(pc, c, sizeof c) ||
            aimp_memcmp(c, AIMP_SWITCH_COMMAND_PROLOGUE, sizeof c) != 0) {
            log_printf("GHOST: los prólogos de FUN_0067e980/FUN_006797a0/FUN_00678e20 (comandos) no coinciden; AIMP_GHOST_WALK queda apagado");
            g_walk = 0;
        } else {
            g_switch_cmd = (SwitchCmdFn)(void *)pc;
            g_move_start = (MoveCmdFn)(void *)ps;
            g_move_update = (MoveCmdFn)(void *)pu;
            g_walk_type = aimp_env_int(L"AIMP_GHOST_WALK_TYPE", 1);
            int ms = aimp_env_int(L"AIMP_GHOST_WALK_MS", 200);
            g_walk_ticks = g_freq * (ms < 20 ? 20 : ms) / 1000;
            g_walk_tp_cm = aimp_env_int(L"AIMP_GHOST_WALK_TP_CM", 1200);
            g_walk_behind_cm = aimp_env_int(L"AIMP_GHOST_WALK_BEHIND_CM", 250);
            aimp_memset(g_params, 0, sizeof g_params);
            uint32_t mt = (uint32_t)g_walk_type;
            float f18 = 0.1f; /* el valor por defecto del engine (DAT_011bb6ac) */
            aimp_memcpy(g_params, &mt, 4);
            aimp_memcpy(g_params + MOVE_PARAMS_F18, &f18, 4);
            g_walk_block = aimp_env_int(L"AIMP_GHOST_WALK_BLOCK", 1);
            if (g_walk_block)
                ctrl_set_switch_filter(ghost_switch_filter); /* ctrl_install (después) instala el hook */
        }
    }
    /* Corrida 14: con el avatar no autoritativo su IA no reacciona (no huye del fuego) y no se mueve
     * solo. Por defecto en el modo 3, salvo con la caminata (que necesita que el NPC procese comandos);
     * AIMP_GHOST_NOAUTH=0/1 lo fuerza. */
    g_noauth = aimp_env_int(L"AIMP_GHOST_NOAUTH", g_mode == 3 && !g_walk);
    static const char *const names[] = { "", "clon del jugador", "NPC existente y después clon", "solo NPC existente" };
    if (g_mode) {
        g_state = G_WAIT_PLAYER;
        char dur[32];
        if (npc_ms > 0)
            aimp_snprintf(dur, sizeof dur, "durante %d ms", npc_ms);
        else
            aimp_snprintf(dur, sizeof dur, "sin límite");
        log_printf("AIMP_GHOST=%d (fase 4, experimental: %s): empieza %d ms después de que aparece el jugador; NPC de tipo %d %s",
                   g_mode, names[g_mode], delay, g_npc_type, dur);
        if (g_walk)
            log_printf("AIMP_GHOST_WALK=1 (fase 4b, experimental): MOVE_TOWARDS con move_type %d, destino cada %d ms, teletransporte a más de %d cm; NOAUTH %d; bloqueo de los comandos de la IA %d",
                       g_walk_type, (int)(g_walk_ticks * 1000 / g_freq), g_walk_tp_cm, g_noauth, g_walk_block);
    } else {
        log_printf("AIMP_TRACE_SPAWN=1: se loguean los pedidos de spawn (solo mira)");
    }
    return 1;
}

static void log_observers(void)
{
    static const struct {
        uint32_t rva;
        const char *name;
    } lists[] = { { AIMP_RVA_OBS_AUTH_PRE, "autoridad antes" },
                  { AIMP_RVA_OBS_AUTH_POST, "autoridad después" },
                  { AIMP_RVA_OBS_RPC_PRE, "RPC 0x29 antes" },
                  { AIMP_RVA_OBS_RPC_POST, "RPC 0x29 después" } };
    for (unsigned i = 0; i < sizeof lists / sizeof lists[0]; i++) {
        uint32_t node = *(volatile uint32_t *)(g_aimp_exe + lists[i].rva), n = 0;
        char line[400];
        int len = aimp_snprintf(line, sizeof line, "SP observadores %s (rva 0x%x):", lists[i].name, lists[i].rva);
        while (node && n < 8 && len > 0 && (size_t)len < sizeof line - 40) {
            uint32_t vt = 0, vf1 = 0, next = 0;
            mem_u32(node, &vt);
            if (vt)
                mem_u32(vt + 4, &vf1);
            len += aimp_snprintf(line + len, sizeof line - (size_t)len, " [0x%08x vt rva 0x%x vf1 rva 0x%x]", node, rva(vt),
                                 rva(vf1));
            if (!mem_u32(node + 4, &next))
                break;
            node = next;
            n++;
        }
        log_printf("%s%s", line, n ? "" : " vacía");
    }
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

static void describe(const char *what, uint32_t p, const char *where, uint32_t idx)
{
    uint32_t vt = 0, def = 0, type = 0, ctrl = 0;
    uint8_t f1c = 0, f1f = 0, act = 0, id[8] = { 0 };
    float pos[3] = { 0 };
    char ids[17], name[33] = "";
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
    mem_cstr(p + 0x10, name, sizeof name);
    id_str(ids, id);
    log_printf("GHOST: %s: %s[%u] = 0x%08x \"%s\", vtable rva 0x%x, def 0x%08x tipo %u flags 1c=%u 1f=%u, controlador 0x%08x (activo %u), pos=(%.2f, %.2f, %.2f), id %s",
               what, where, idx, p, name, rva(vt), def, type, f1c, f1f, ctrl, act, (double)pos[0], (double)pos[1], (double)pos[2],
               ids);
}

static void start_moving(uint32_t p, int64_t now)
{
    g_chr = p;
    g_state = g_state == G_NPC ? G_NPC : G_ACTIVE;
    g_last_log = now;
    g_last_tp = 0;
    g_tps = 0;
    g_last_restart = g_last_cmd = g_far_since = 0;
    g_wstate = 0; /* W_PLACE */
    g_starts = g_updates = 0;
    g_walk_bad = 0;
    g_walk_ctrl = g_walk ? p + CHR_CMDCTRL : 0;
    aimp_memcpy(g_cmd_b, &p, 4); /* el segundo id del comando: el avatar y "GH" */
    g_cmd_b[4] = 'G';
    g_cmd_b[5] = 'H';
    g_cmd_b[6] = g_cmd_b[7] = 0;
}

/* Experimento (AIMP_GHOST_NOAUTH=1): marcar al avatar como no autoritativo, como un "display
 * character" del diseño de red original (hallazgo 10). Si la IA respeta el flag, el NPC deja de
 * moverse solo y bajan los teleports por segundo. Se loguea antes y después. */
static void set_noauth(const Chars *c, uint32_t p)
{
    for (uint32_t i = 0; i < CM_BLOCK_MAX; i++) {
        uint32_t blk = c->cm + CM_BLOCKS + i * CM_BLOCK_SIZE, who = 0;
        uint8_t auth = 0, zero = 0;
        if (!mem_u32(blk + BLOCK_CHARACTER, &who) || who != p)
            continue;
        mem_u8(blk + BLOCK_AUTHORITY, &auth);
        log_printf("GHOST: bloque del avatar = CM+0x%x (índice %u), autoridad %u; se pone en 0 (AIMP_GHOST_NOAUTH)",
                   CM_BLOCKS + i * CM_BLOCK_SIZE, i, auth);
        if (!mem_write(blk + BLOCK_AUTHORITY, &zero, 1))
            log_printf("GHOST: no se pudo escribir la autoridad");
        return;
    }
    log_printf("GHOST: no se encontró el bloque del avatar (AIMP_GHOST_NOAUTH no se aplica)");
}

static void pick_npc(const Chars *c, int64_t now)
{
    uint32_t best = 0, best_i = 0;
    for (uint32_t i = 0; i < c->nnpc && !best; i++) {
        uint32_t def = 0, type = 0;
        if (c->npc[i] && mem_u32(c->npc[i] + CHR_DEF, &def) && def && mem_u32(def + DEF_TYPE, &type) && (int)type == g_npc_type) {
            best = c->npc[i];
            best_i = i;
        }
    }
    if (!best) {
        if (g_mode == 2) {
            log_printf("GHOST: no hay NPC de tipo %d en el nivel (%u NPCs); se pasa al clon", g_npc_type, c->nnpc);
            g_t0 = now - g_npc_ticks; /* para que siga con el clon */
        } else if (!g_warned_npc) {
            log_printf("GHOST: no hay NPC de tipo %d en el nivel (%u NPCs); se reintenta cada 1 s", g_npc_type, c->nnpc);
            g_warned_npc = 1;
        }
        return;
    }
    describe("NPC elegido para mover", best, "NPCs", best_i);
    if (g_noauth)
        set_noauth(c, best);
    g_warned_npc = 0;
    if (!g_chosen_once)
        g_t0 = now;
    g_chosen_once = 1;
    start_moving(best, now);
}

static void do_spawn(const Chars *c, int64_t now)
{
    /* Se clona lo que recibió SpawnCharacter (el mensaje de sesión 0x2E de 0xF0 bytes), no el
     * pedido de 0x130 del spawn de autoridad: es otro formato (corrida 7). */
    LONG total = g_nsc;
    int found = -1;
    for (LONG k = total; k > 0 && k > total - MAX_REQS; k--) {
        const uint8_t *r = g_scq[(k - 1) % MAX_REQS];
        uint32_t key = rd32(r + SREQ_KEY), type = rd32(r + SREQ_TYPE);
        if ((g_from_key < 0 && type == 0) || (g_from_key >= 0 && key == (uint32_t)g_from_key)) {
            found = (int)((k - 1) % MAX_REQS);
            break;
        }
    }
    if (found < 0 || !g_sc_this) {
        log_printf("GHOST: no se clona: pedido %s, SpawnCharacter %s (llamadas a SpawnCharacter %ld)",
                   found < 0 ? "no encontrado" : "ok", g_sc_this ? "visto" : "no visto", (long)total);
        g_state = G_DONE;
        return;
    }
    if (g_attempts >= MAX_ATTEMPTS) {
        log_printf("GHOST: ya se hicieron %u intentos en la sesión; no se clona más", g_attempts);
        g_state = G_DONE;
        return;
    }
    aimp_memset(g_clone, 0, sizeof g_clone);
    aimp_memcpy(g_clone, g_scq[found], SC_REQ_SIZE);
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
    log_printf("GHOST: clonando el pedido de SpawnCharacter (clave %u -> %u, id %s -> %s), this 0x%08x; antes: %u jugadores, %u NPCs, %u pendientes",
               old_key, key, id_old, id_new, (unsigned)g_sc_this, c->npl, c->nnpc, c->npend);
    log_request("GHOST clon", (long)g_attempts, g_sc_this, g_clone, 0);
    uint32_t ret = g_sc_orig((void *)g_sc_this, NULL, g_clone);
    Chars a;
    if (read_chars(&a))
        log_printf("GHOST: SpawnCharacter devolvió 0x%08x; ahora %u jugadores, %u NPCs, %u pendientes", ret, a.npl, a.nnpc, a.npend);
    else
        log_printf("GHOST: SpawnCharacter devolvió 0x%08x; no se pudo leer el CharacterManager", ret);
    g_chr = 0;
    g_state = G_FIND;
    g_deadline = now + 5 * g_freq;
}

static void on_found(uint32_t p, const char *where, uint32_t idx, int64_t now)
{
    describe("apareció el clon", p, where, idx);
    uint32_t ctrl = 0;
    mem_u32(p + CHR_CTRL, &ctrl);
    if (ctrl) {
        uint8_t zero = 0;
        log_printf("GHOST: desactivando el controlador del clon (+0x%x = 0)", CTRL_ACTIVE);
        if (!mem_write(ctrl + CTRL_ACTIVE, &zero, 1))
            log_printf("GHOST: no se pudo escribir en el controlador");
    }
    start_moving(p, now);
}

static float aimp_sinf(float x)
{
    float r;
    __asm__("fsin" : "=t"(r) : "0"(x));
    return r;
}

static float aimp_cosf(float x)
{
    float r;
    __asm__("fcos" : "=t"(r) : "0"(x));
    return r;
}

static float aimp_sqrtf(float x)
{
    float r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

static void teleport_to(const float *t, float tyaw)
{
    /* SetTransform copia la posición con MOVAPS (FUN_00401410): vector de 16 bytes alineado a 16;
     * el cuarto componente (+0x7C) se conserva el del personaje. */
    g_vec[0] = t[0];
    g_vec[1] = t[1];
    g_vec[2] = t[2];
    if (!mem_f32(g_chr + CHR_POS + 12, &g_vec[3]))
        g_vec[3] = 0.0f;
    g_teleport((void *)g_chr, NULL, g_vec, tyaw, 0);
    g_tps++;
}

static int cmd_is_ours(uint32_t cmd, uint32_t *type)
{
    uint8_t a[8], b[8];
    if (!cmd || !mem_u32(cmd + CMD_TYPE, type))
        return 0;
    return *type == CMD_TYPE_MOVE_TOWARDS && mem_read(cmd + CMD_OWNER_A, a, 8) && mem_read(cmd + CMD_OWNER_B, b, 8) &&
           aimp_memcmp(a, g_cmd_a, 8) == 0 && aimp_memcmp(b, g_cmd_b, 8) == 0;
}

/* ¿El MOVE_TOWARDS del mod está en el controlador, en ejecución (+0) o pendiente (+4)? (corrida 21: el
 * engine lo pasa de +4 a +0; mirando solo +4 se lo volvía a dar cada segundo). Devuelve en *type el tipo
 * del comando en ejecución (o el pendiente, si no hay). */
static int walk_is_ours(uint32_t ctrl, uint32_t *type)
{
    uint32_t ex = 0, pe = 0, t2 = 0xFFFFFFFFu;
    *type = 0xFFFFFFFFu;
    mem_u32(ctrl + CMDCTRL_EXEC, &ex);
    mem_u32(ctrl + CMDCTRL_CURRENT, &pe);
    int ours = cmd_is_ours(ex, type) || cmd_is_ours(pe, &t2);
    if (*type == 0xFFFFFFFFu)
        *type = t2;
    return ours;
}

/* Filtro de SwitchCommand (ctrl.c; corre en el thread de la IA): sobre el controlador del avatar solo
 * pasan el MOVE_TOWARDS del mod y el cambio a ningún comando. */
int ghost_switch_filter(uint32_t ctrl, uint32_t cmd)
{
    uint32_t mine = g_walk_ctrl, type = 0xFFFFFFFFu;
    if (!mine || ctrl != mine || !cmd || cmd_is_ours(cmd, &type))
        return 1;
    LONG n = InterlockedIncrement(&g_n_blocked);
    if (n <= 20 || (n & 255) == 0) {
        char nm[32];
        uint32_t p = 0;
        if (!(type < COMMAND_TYPES && mem_u32(g_aimp_exe + AIMP_RVA_COMMAND_NAMES + 4 * type, &p) && p && mem_cstr(p, nm, sizeof nm)))
            aimp_snprintf(nm, sizeof nm, "?%d", (int)type);
        log_printf("GHOST: caminata: bloqueado el comando %s (0x%08x) que la IA le quería dar al avatar (%ld bloqueados)", nm, cmd,
                   (long)n);
    }
    return 0;
}

/* AIMP_GHOST_WALK: el NPC camina hacia el objetivo con MOVE_TOWARDS. 0 si no se pudo (se usa el teleport).
 * Corrida 22: teletransportarlo mientras camina no se pega (la locomoción lo devuelve en ~4 frames y el NPC
 * "corre" hacia donde cree estar). Por eso: ubicar -> esperar -> caminar; y si queda lejos, cancelar el
 * comando (SwitchCommand(ctrl, NULL)), esperar, y recién ahí volver a ubicarlo. */
enum { W_PLACE, W_SETTLE, W_WALK, W_CANCEL };
static const char *const g_wnames[] = { "ubicando", "esperando", "caminando", "cancelando" };

static void walk_start(uint32_t ctrl, const float *t, int64_t now, const char *why)
{
    if (g_starts < 20)
        log_printf("GHOST: caminata: MOVE_TOWARDS a (%.2f, %.2f, %.2f) para 0x%08x (controlador 0x%08x; %s)", (double)t[0],
                   (double)t[1], (double)t[2], g_chr, ctrl, why);
    g_move_start((void *)ctrl, NULL, g_cmd_a, g_cmd_b, g_target, g_params, 1);
    g_last_restart = g_last_cmd = now;
    g_starts++;
}

static int walk_chr(const float *t, float tyaw, const char *src, const char *where, uint32_t idx, int64_t now)
{
    uint32_t ctrl = g_chr + CHR_CMDCTRL, owner = 0, type = 0xFFFFFFFFu;
    if (!mem_u32(ctrl + CMDCTRL_OWNER, &owner) || owner != g_chr) {
        log_printf("GHOST: el avatar 0x%08x no tiene su controlador de comandos en +0x%x (+0x%x = 0x%08x); se vuelve al teletransporte",
                   g_chr, CHR_CMDCTRL, CMDCTRL_OWNER, owner);
        g_walk_bad = 1;
        return 0;
    }
    float cur[3];
    if (!mem_read(g_chr + CHR_POS, cur, sizeof cur))
        return 1;
    float dx = t[0] - cur[0], dy = t[1] - cur[1], dz = t[2] - cur[2], d2 = dx * dx + dz * dz;
    float tp = (float)g_walk_tp_cm / 100.0f;
    /* destino: rotación identidad y la traslación del objetivo (m[3], m[7], m[11]) */
    aimp_memset(g_target, 0, sizeof g_target);
    g_target[0] = g_target[5] = g_target[10] = 1.0f;
    g_target[3] = t[0];
    g_target[7] = t[1];
    g_target[11] = t[2];
    int ours = 0;
    switch (g_wstate) {
    case W_PLACE:
        log_printf("GHOST: caminata: teletransporte de 0x%08x a (%.2f, %.2f, %.2f) [%s] (%s; estaba a %.2f m en horizontal, %.2f m en vertical)",
                   g_chr, (double)t[0], (double)t[1], (double)t[2], src, g_last_tp ? "lejos o en otro piso" : "primera vez",
                   (double)aimp_sqrtf(d2), (double)dy);
        teleport_to(t, tyaw);
        g_last_tp = now;
        g_wstate = W_SETTLE;
        g_wstate_at = now;
        break;
    case W_SETTLE:
        if (now - g_wstate_at >= g_freq * 2 / 5) { /* 400 ms quieto después del teletransporte */
            walk_start(ctrl, t, now, "después de ubicarlo");
            g_wstate = W_WALK;
            g_far_since = 0;
        }
        break;
    case W_WALK: {
        int is_far = d2 > tp * tp || dy > 2.0f || dy < -2.0f;
        if (!is_far)
            g_far_since = 0;
        else if (!g_far_since)
            g_far_since = now;
        if (is_far && now - g_far_since >= g_freq && now - g_last_tp >= 2 * g_freq) {
            log_printf("GHOST: caminata: a %.2f m (vertical %.2f) hace 1 s: se cancela el comando para reubicarlo", (double)aimp_sqrtf(d2),
                       (double)dy);
            g_switch_cmd((void *)ctrl, NULL, NULL); /* el filtro deja pasar el cambio a ningún comando */
            g_wstate = W_CANCEL;
            g_wstate_at = now;
            break;
        }
        ours = walk_is_ours(ctrl, &type);
        if (!ours && now - g_last_restart >= g_freq) {
            char nm[32], why[48];
            uint32_t p = 0;
            if (!(type < COMMAND_TYPES && mem_u32(g_aimp_exe + AIMP_RVA_COMMAND_NAMES + 4 * type, &p) && p && mem_cstr(p, nm, sizeof nm)))
                aimp_snprintf(nm, sizeof nm, "%s", type == 0xFFFFFFFFu ? "ninguno" : "?");
            aimp_snprintf(why, sizeof why, "el comando actual era %s", nm);
            walk_start(ctrl, t, now, why);
        } else if (ours && now - g_last_cmd >= g_walk_ticks) {
            g_move_update((void *)ctrl, NULL, g_cmd_a, g_cmd_b, g_target, g_params, 1);
            g_last_cmd = now;
            g_updates++;
        }
        break;
    }
    case W_CANCEL:
        if (now - g_wstate_at >= g_freq * 3 / 10) /* 300 ms para que se frene */
            g_wstate = W_PLACE;
        break;
    }
    (void)tyaw;
    if (now - g_last_log >= g_freq) {
        log_printf("GHOST: caminata (%s): personaje en (%.2f, %.2f, %.2f), objetivo (%.2f, %.2f, %.2f) [%s] a %.2f m; comando %s tipo %d; %u inicios, %u actualizaciones, %u teletransportes; %s[%u]",
                   g_wnames[g_wstate], (double)cur[0], (double)cur[1], (double)cur[2], (double)t[0], (double)t[1], (double)t[2], src,
                   (double)aimp_sqrtf(d2), ours ? "propio" : "ajeno", (int)type, g_starts, g_updates, g_tps, where, idx);
        g_last_log = now;
        g_tps = 0;
    }
    return 1;
}

static void move_chr(const Chars *c, const AimpNet *net, uint32_t level, int64_t now)
{
    uint32_t idx;
    const char *where = find_chr(c, g_chr, &idx);
    if (!where) {
        if (g_state == G_NPC) { /* murió o lo liberó el juego: se elige otro */
            log_printf("GHOST: el NPC 0x%08x ya no está en los vectores; se elige otro", g_chr);
            g_chr = 0;
            return;
        }
        log_printf("GHOST: el personaje 0x%08x ya no está en los vectores; se deja de mover", g_chr);
        g_chr = 0;
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
    } else if (g_walk && !g_walk_bad) {
        /* AIMP_GHOST_WALK_BEHIND_CM (250) detrás del jugador: adelante = (sin yaw, cos yaw) (corrida 20). Con
         * 1,2 m, caminar hacia atrás metía al jugador contra el NPC (corrida 22). */
        float b = (float)g_walk_behind_cm / 100.0f;
        t[0] = lpos[0] - b * aimp_sinf(lyaw);
        t[1] = lpos[1];
        t[2] = lpos[2] - b * aimp_cosf(lyaw);
        tyaw = lyaw;
        src = "local detrás";
    } else {
        t[0] = lpos[0] + 1.5f;
        t[1] = lpos[1];
        t[2] = lpos[2];
        tyaw = lyaw;
        src = "local +1,5 m en x";
    }
    if (g_walk && !g_walk_bad && walk_chr(t, tyaw, src, where, idx, now))
        return;
    float dx = t[0] - g_last_pos[0], dy = t[1] - g_last_pos[1], dz = t[2] - g_last_pos[2], dyaw = tyaw - g_last_yaw;
    int moved = dx * dx + dy * dy + dz * dz > 0.0004f || dyaw > 0.01f || dyaw < -0.01f;
    /* La IA del NPC lo sigue moviendo entre teleports (corrida 9: se corría ~27 cm en 500 ms
     * con el objetivo quieto). Si se alejó más de 2 cm del objetivo, se lo vuelve a poner. */
    float cur[3];
    int drift = 0;
    if (mem_read(g_chr + CHR_POS, cur, sizeof cur)) {
        float ex = cur[0] - t[0], ey = cur[1] - t[1], ez = cur[2] - t[2];
        drift = ex * ex + ey * ey + ez * ez > 0.0004f;
    }
    if (moved || drift || !g_last_tp || now - g_last_tp > g_freq / 2) {
        /* SetTransform copia la posición con MOVAPS (FUN_00401410): tiene que ser un vector
         * de 16 bytes alineado a 16. Con un float[3] en el stack el juego se cayó (corrida 7:
         * acceso en 0xffffffff, que es como Windows reporta un MOVAPS desalineado). El cuarto
         * componente se copia a +0x7C: se conserva el del personaje. */
        g_vec[0] = t[0];
        g_vec[1] = t[1];
        g_vec[2] = t[2];
        if (!mem_f32(g_chr + CHR_POS + 12, &g_vec[3]))
            g_vec[3] = 0.0f;
        if (!g_last_tp)
            log_printf("GHOST: primer teletransporte de 0x%08x a (%.2f, %.2f, %.2f; w %.2f) yaw %.3f [%s]", g_chr, (double)t[0],
                       (double)t[1], (double)t[2], (double)g_vec[3], (double)tyaw, src);
        g_teleport((void *)g_chr, NULL, g_vec, tyaw, 0);
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
        log_printf("GHOST: personaje en (%.2f, %.2f, %.2f), objetivo (%.2f, %.2f, %.2f) [%s], %u teletransportes; %s[%u]",
                   (double)cp[0], (double)cp[1], (double)cp[2], (double)t[0], (double)t[1], (double)t[2], src, g_tps, where,
                   idx);
        g_last_log = now;
        g_tps = 0;
    }
}

int ghost_enabled(void) { return g_mode != 0; }

uint32_t ghost_avatar(void) { return g_mode && (g_state == G_NPC || g_state == G_ACTIVE) ? g_chr : 0; }

void ghost_frame(int64_t now, const AimpNet *net, uint32_t level)
{
    if (!g_mode || g_state == G_OFF)
        return;
    if (!g_obs_logged) {
        g_obs_logged = 1;
        log_observers();
    }
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
            g_chosen_once = 0;
            g_state = G_DELAY;
        }
        break;
    case G_DELAY:
        if (now - g_t0 < g_delay_ticks)
            break;
        if (g_mode == 1) {
            do_spawn(&c, now);
        } else {
            g_state = G_NPC;
            g_last_pick = now;
            pick_npc(&c, now);
        }
        break;
    case G_NPC:
        if (g_chr)
            move_chr(&c, net, level, now);
        else if (now - g_last_pick >= g_freq) {
            g_last_pick = now;
            pick_npc(&c, now);
        }
        if (g_state == G_NPC && g_npc_ticks && now - g_t0 >= g_npc_ticks) {
            log_printf("GHOST: fin del modo NPC%s", g_mode == 2 ? "; se intenta el clon" : "");
            g_chr = 0;
            if (g_mode == 2)
                do_spawn(&c, now);
            else
                g_state = G_DONE;
        }
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
        move_chr(&c, net, level, now);
        break;
    default:
        break;
    }
}
