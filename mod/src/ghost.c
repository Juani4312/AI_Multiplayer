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
 * frame, se le da un comando MOVE_TOWARDS (CMD_MoveTowards: FUN_0067e980) en su controlador de
 * scripts (Character+0x3570; AIMP_GHOST_WALK_CTRL) hacia el objetivo, y cada AIMP_GHOST_WALK_MS (200)
 * se le actualiza el destino (FUN_006797a0). Así camina con su propia animación. Su árbol de
 * comportamiento ejecuta ese comando por la rama cinemática (ConditionHasScript + motivación
 * CINEMATIC, que el mod fuerza con AIMP_GHOST_WALK_CINEMATIC=1). Corrida 27: camina detrás del jugador;
 * corrida 28: con solo actualizaciones sigue igual. Se lo teletransporta (con una orden nueva en el
 * mismo frame) la primera vez y si queda a más de AIMP_GHOST_WALK_TP_CM (1200) u otro piso durante 1 s.
 * Con AIMP_GHOST_WALK_RESTART_CM > 0 (0 por defecto) se le da una orden nueva cuando el objetivo se movió
 * más que eso (como mucho cada AIMP_GHOST_WALK_RESTART_MS). Si la IA le cambia el comando,
 * se lo vuelve a dar (como mucho una vez por segundo). AIMP_GHOST_WALK_TYPE = move_type (1).
 * Con la caminata, AIMP_GHOST_NOAUTH queda apagado por defecto (un NPC sin autoridad no se mueve).
 * Corrida 21: camina, pero su IA (con autoridad) lo hacía huir del jugador. Con AIMP_GHOST_WALK_BLOCK=1
 * (por defecto) se bloquean los SwitchCommand de la IA sobre el controlador del avatar (filtro en
 * ctrl.c, que cancela el comando bloqueado para liberar su slot). Sin red, el objetivo queda detrás
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
typedef void(__attribute__((fastcall)) * IdleCmdFn)(void *ctrl, void *edx, const uint8_t *a, const uint8_t *b, uint32_t stance,
                                                    float f, uint32_t face, uint32_t raise, uint32_t style, uint32_t lock,
                                                    uint32_t anchor, uint32_t start);
typedef void(__attribute__((fastcall)) * MoveCmdFn)(void *ctrl, void *edx, const uint8_t *a, const uint8_t *b,
                                                    const float *target, const uint8_t *params, uint32_t never_succeed);
/* FUN_006d3630 termina con `ret 8` (dos argumentos; el segundo no se usa). Corrida 25: declarado con uno, el stack
 * quedaba corrido y el juego saltaba a 0x4. */
typedef uint32_t(__attribute__((fastcall)) * HasMotivationFn)(void *node, void *edx, void *agent, uint32_t arg2);

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
static int g_front_cm; /* AIMP_GHOST_FRONT_CM */
static int g_clone_auth;      /* AIMP_GHOST_CLONE_AUTH: clonar el pedido de autoridad (0x130) en vez del de SpawnCharacter */
static char g_clone_name[0x40]; /* AIMP_GHOST_CLONE_NAME (p. ej. PLAYER_2) */
static int g_noauth; /* AIMP_GHOST_NOAUTH=1: experimento, el avatar pasa a no autoritativo */
static int64_t g_freq = 1, g_delay_ticks, g_npc_ticks, g_t0, g_deadline, g_last_log, g_last_tp;
static uint32_t g_player, g_chr, g_before[2 * MAX_CHARS], g_nbefore, g_attempts, g_tps;
static float g_last_pos[3], g_last_yaw;
/* AIMP_GHOST_WALK */
static MoveCmdFn g_move_start, g_move_update;
static int g_walk, g_walk_type = MOVE_TYPE_WALK, g_walk_tp_cm = 600, g_walk_bad, g_walk_block = 1, g_owner_warned;
static int g_walk_type_far = MOVE_TYPE_RUN, g_walk_far_cm = 500, g_cur_type = MOVE_TYPE_WALK; /* corre si queda lejos */
/* AIMP_GHOST_WALK_FACE: con red, quieto, mira hacia donde mira el otro jugador */
static int g_walk_face = 1, g_tgt_net;
static int64_t g_still_since, g_last_face, g_settle_since;
static float g_still_t[3], g_settle_p[3];
static uint32_t g_faces;
static volatile uint32_t g_walk_ctrl; /* controlador del avatar, para el filtro (corre en otro thread) */
/* AIMP_GHOST_WALK_CINEMATIC: la motivación cinemática del avatar siempre verdadera (hook de FUN_006d3630) */
static AimpHook g_hook_motiv;
static HasMotivationFn g_motiv_orig;
static int g_walk_cine = 1;
static volatile uint32_t g_walk_chr; /* el avatar, para el hook (corre en el thread de la IA) */
static volatile LONG g_n_cine, g_cine_logged;
static volatile LONG g_n_blocked;
static int64_t g_walk_ticks, g_last_cmd, g_last_restart, g_far_since, g_prep_at;
static int g_wstate, g_walk_behind_cm = 250, g_walk_restart_cm, g_walk_restart_ms = 500, g_walk_follow_cm = 200, g_walk_prep_ms = 4000;
/* AIMP_GHOST_WALK_REARM_CM: orden nueva si está quieto lejos del objetivo */
static int g_walk_rearm_cm;
static float g_rearm_p[3];
static int64_t g_rearm_since;
static uint32_t g_rearms;
static uint32_t g_walk_off = CHR_SCRIPTCTRL;
/* Corrida 38: con el objetivo inalcanzable (contra una pared) la re-orden se repetía cada 2 s. Como mucho 3 por
 * objetivo (se renueva cuando el objetivo se mueve más de 50 cm). */
static float g_rearm_tgt[2];
static int g_rearm_n;
static int rearm_allowed(const float *t)
{
    float x = t[0] - g_rearm_tgt[0], z = t[2] - g_rearm_tgt[1];
    if (x * x + z * z > 0.25f) {
        g_rearm_tgt[0] = t[0];
        g_rearm_tgt[1] = t[2];
        g_rearm_n = 0;
    }
    return g_rearm_n++ < 3;
}
/* movimiento del otro jugador (remote_motion) */
static uint32_t g_rm_seq, g_rm_ms;
static float g_rm_p[3], g_rm_v[3], g_rm_speed, g_rm_camh = 1.57f;
static int64_t g_rm_at, g_rm_cr_since;
static IdleCmdFn g_idle_cmd;
static int g_idle_on, g_crouch_stance = 2;
static volatile int g_allow_cmd; /* el filtro deja pasar el comando que da el mod (mismo thread) */
static int g_puppet; /* AIMP_GHOST_PUPPET: 1 = SetTransform por frame, 2 = escribir la posición */
static uint32_t g_pup_frames;
static int g_rm_valid, g_rm_crouch, g_walk_lead_ms = 400, g_walk_run_mms = 3000;
/* AIMP_GHOST_INVINCIBLE */
static int g_invincible = 1, g_inv_bad;
static uint32_t g_inv_chr;
static int64_t g_inv_last;
/* plantado después de girar (no camina hasta que el otro se mueve) */
static int g_hold;
static float g_hold_p[4] __attribute__((aligned(16)));
static float g_hold_t[3]; /* objetivo cuando quedó plantado */
static float g_still_yaw;
static int64_t g_yaw_since; /* controlador al que se le dan las órdenes (AIMP_GHOST_WALK_CTRL) */
static float g_cmd_t[3]; /* objetivo de la última orden (FUN_0067e980) */

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

/* ConditionHasMotivation del árbol de comportamiento: para el avatar, CINEMATIC_MOTIVATION es siempre verdadera,
 * así el árbol entra en la rama de los comandos de script (ConditionHasScript -> ActionScript) y ejecuta el
 * MOVE_TOWARDS del mod en vez de sus propias ramas (huir, acercarse al jugador). El agente se identifica por
 * rango: está dentro del Character del avatar (los Character ocupan CHR_STRIDE bytes). */
__attribute__((fastcall, force_align_arg_pointer, noinline)) static uint32_t det_has_motivation(void *node, void *edx, void *agent,
                                                                                            uint32_t arg2)
{
    uint32_t chr = g_walk_chr, a = (uint32_t)(uintptr_t)agent, type = 0;
    if (chr && a >= chr && a - chr < CHR_STRIDE && node && mem_u32((uintptr_t)node + BTNODE_MOTIVATION_TYPE, &type) &&
        type == MOTIVATION_CINEMATIC) {
        InterlockedIncrement(&g_n_cine);
        if (InterlockedIncrement(&g_cine_logged) == 1)
            log_printf("GHOST: caminata: motivación cinemática forzada para el avatar 0x%08x (agente = personaje + 0x%x)", chr,
                       a - chr);
        return 1;
    }
    return g_motiv_orig(node, edx, agent, arg2);
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
    g_clone_auth = aimp_env_int(L"AIMP_GHOST_CLONE_AUTH", 0);
    g_front_cm = aimp_env_int(L"AIMP_GHOST_FRONT_CM", 0);
    g_walk_lead_ms = aimp_env_int(L"AIMP_GHOST_WALK_LEAD_MS", 400);
    g_puppet = aimp_env_int(L"AIMP_GHOST_PUPPET", 1); /* corrida 46: 1 (SetTransform) da 0,00 m; 2 (escribir +0x70) no se pega (corrida 47) */
    g_walk_run_mms = aimp_env_int(L"AIMP_GHOST_WALK_RUN_MMS", 3000);
    {
        wchar_t wn[0x40];
        g_clone_name[0] = 0;
        if (aimp_env_str(L"AIMP_GHOST_CLONE_NAME", wn, 0x40)) {
            int i = 0;
            for (; i < 0x3F && wn[i]; i++)
                g_clone_name[i] = (char)(wn[i] < 128 ? wn[i] : '?');
            g_clone_name[i] = 0;
        }
    }
    g_npc_type = aimp_env_int(L"AIMP_GHOST_NPC_TYPE", 6);
    g_walk = g_mode >= 2 && aimp_env_int(L"AIMP_GHOST_WALK", 0);
    if (g_walk) {
        uint8_t a[sizeof AIMP_CMD_MOVE_START_PROLOGUE], b[sizeof AIMP_CMD_MOVE_UPDATE_PROLOGUE];
        uintptr_t ps = g_aimp_exe + AIMP_RVA_CMD_MOVE_START, pu = g_aimp_exe + AIMP_RVA_CMD_MOVE_UPDATE;
        if (!mem_read(ps, a, sizeof a) || aimp_memcmp(a, AIMP_CMD_MOVE_START_PROLOGUE, sizeof a) != 0 || !mem_read(pu, b, sizeof b) ||
            aimp_memcmp(b, AIMP_CMD_MOVE_UPDATE_PROLOGUE, sizeof b) != 0) {
            log_printf("GHOST: los prólogos de FUN_0067e980/FUN_006797a0 (MOVE_TOWARDS) no coinciden; AIMP_GHOST_WALK queda apagado");
            g_walk = 0;
        } else {
            g_move_start = (MoveCmdFn)(void *)ps;
            uint8_t c[sizeof AIMP_CMD_MOVE_START_PROLOGUE];
            uintptr_t pi = g_aimp_exe + AIMP_RVA_CMD_IDLE;
            if (mem_read(pi, c, sizeof c) && aimp_memcmp(c, AIMP_CMD_MOVE_START_PROLOGUE, sizeof c) == 0)
                g_idle_cmd = (IdleCmdFn)(void *)pi;
            else
                log_printf("GHOST: el prólogo de FUN_0067db10 (IDLE) no coincide; el avatar no se agacha");
            g_crouch_stance = aimp_env_int(L"AIMP_GHOST_CROUCH_STANCE", 2);
            g_move_update = (MoveCmdFn)(void *)pu;
            g_walk_type = aimp_env_int(L"AIMP_GHOST_WALK_TYPE", MOVE_TYPE_WALK);
            g_walk_type_far = aimp_env_int(L"AIMP_GHOST_WALK_TYPE_FAR", MOVE_TYPE_RUN); /* igual a TYPE: no cambia */
            g_walk_far_cm = aimp_env_int(L"AIMP_GHOST_WALK_FAR_CM", 500);
            g_walk_face = aimp_env_int(L"AIMP_GHOST_WALK_FACE", 1);
            int ms = aimp_env_int(L"AIMP_GHOST_WALK_MS", 200);
            g_walk_ticks = g_freq * (ms < 20 ? 20 : ms) / 1000;
            g_walk_tp_cm = aimp_env_int(L"AIMP_GHOST_WALK_TP_CM", 1200);
            g_walk_behind_cm = aimp_env_int(L"AIMP_GHOST_WALK_BEHIND_CM", 250);
            g_walk_restart_cm = aimp_env_int(L"AIMP_GHOST_WALK_RESTART_CM", 0); /* 0: solo actualizaciones (corrida 28) */
            g_walk_restart_ms = aimp_env_int(L"AIMP_GHOST_WALK_RESTART_MS", 500);
            g_walk_follow_cm = aimp_env_int(L"AIMP_GHOST_WALK_FOLLOW_CM", 200); /* 0: punto fijo detrás del jugador */
            g_walk_prep_ms = aimp_env_int(L"AIMP_GHOST_WALK_PREP_MS", 4000);
            g_walk_off = (uint32_t)aimp_env_int(L"AIMP_GHOST_WALK_CTRL", CHR_SCRIPTCTRL); /* 0x3820 = el de tareas */
            aimp_memset(g_params, 0, sizeof g_params);
            uint32_t mt = (uint32_t)g_walk_type;
            /* arrived_radius: 0,1 es el valor por defecto de FUN_0067e980 (DAT_011bb6ac); los scripts usan 0,6 */
            float f18 = (float)aimp_env_int(L"AIMP_GHOST_WALK_RADIUS_CM", 10) / 100.0f;
            aimp_memcpy(g_params, &mt, 4);
            aimp_memcpy(g_params + MOVE_PARAMS_F18, &f18, 4);
            /* AIMP_GHOST_WALK_FLAGS: los bool de los parámetros (bit i = byte +0x04+i; 2048 = start_instantly,
             * 8 = continue_from_previous_move, 4 = use_stopping_anim; game.h) */
            int fl = aimp_env_int(L"AIMP_GHOST_WALK_FLAGS", 2048) /* corrida 35 */;
            for (int i = 0; i < MOVE_PARAMS_NFLAGS; i++)
                g_params[MOVE_PARAMS_FLAGS + i] = (uint8_t)((fl >> i) & 1);
            g_walk_rearm_cm = aimp_env_int(L"AIMP_GHOST_WALK_REARM_CM", 100) /* corrida 35 */;
            if (fl || g_walk_rearm_cm)
                log_printf("AIMP_GHOST_WALK: flags de MOVE_TOWARDS 0x%03x (bytes +0x04..+0x0F), arrived_radius %.2f m; orden nueva si está quieto a más de %d cm del objetivo",
                           fl, (double)f18, g_walk_rearm_cm);
            g_cur_type = g_walk_type;
            g_walk_block = aimp_env_int(L"AIMP_GHOST_WALK_BLOCK", 1);
            if (g_walk_block)
                ctrl_set_switch_filter(ghost_switch_filter); /* ctrl_install (después) instala el hook */
            /* Corrida 24: sin motivación cinemática, el árbol del civil ignora los comandos de script. */
            g_walk_cine = aimp_env_int(L"AIMP_GHOST_WALK_CINEMATIC", 1);
            if (g_walk_cine && !hook_install(&g_hook_motiv, "ConditionHasMotivation (FUN_006d3630)",
                                             (void *)(g_aimp_exe + AIMP_RVA_HAS_MOTIVATION), AIMP_HAS_MOTIVATION_PROLOGUE,
                                             sizeof AIMP_HAS_MOTIVATION_PROLOGUE, AIMP_HAS_MOTIVATION_STEAL,
                                             (void *)det_has_motivation, (void **)&g_motiv_orig))
                g_walk_cine = 0;
        }
    }
    /* Corrida 14: con el avatar no autoritativo su IA no reacciona (no huye del fuego) y no se mueve
     * solo. Por defecto en el modo 3, salvo con la caminata (que necesita que el NPC procese comandos);
     * AIMP_GHOST_NOAUTH=0/1 lo fuerza. */
    g_noauth = aimp_env_int(L"AIMP_GHOST_NOAUTH", g_mode == 3 && !g_walk);
    g_invincible = aimp_env_int(L"AIMP_GHOST_INVINCIBLE", 1);
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
        if (g_walk) {
            char rs[96];
            if (g_walk_restart_cm > 0)
                aimp_snprintf(rs, sizeof rs, "orden nueva si el objetivo se movió más de %d cm (como mucho cada %d ms)", g_walk_restart_cm,
                              g_walk_restart_ms);
            else
                aimp_snprintf(rs, sizeof rs, "sin órdenes nuevas cuando el objetivo se mueve");
            log_printf("AIMP_GHOST_WALK=1 (fase 4b, experimental): MOVE_TOWARDS en el controlador +0x%x con move_type %d (%d a más de %d cm del objetivo), destino cada %d ms, %s, teletransporte a más de %d cm; NOAUTH %d; bloqueo de los comandos de la IA %d; motivación cinemática %d",
                       g_walk_off, g_walk_type, g_walk_type_far, g_walk_far_cm, (int)(g_walk_ticks * 1000 / g_freq), rs, g_walk_tp_cm,
                       g_noauth, g_walk_block, g_walk_cine);
        }
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
    g_wstate = 0; /* W_PREP */
    g_prep_at = 0;
    g_still_since = g_last_face = g_settle_since = 0;
    g_starts = g_updates = 0;
    g_walk_bad = 0;
    g_rearm_since = 0;
    g_hold = 0;
    g_yaw_since = 0;
    g_idle_on = 0;
    g_walk_ctrl = g_walk ? p + g_walk_off : 0;
    g_walk_chr = g_walk && g_walk_cine ? p : 0;
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

static void do_spawn_auth(const Chars *c, int64_t now);

static void do_spawn(const Chars *c, int64_t now)
{
    if (g_clone_auth) {
        do_spawn_auth(c, now);
        return;
    }
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


/* AIMP_GHOST_CLONE_AUTH=1 ("dos Amandas", 2026-11-15): la corrida 8 clonó el mensaje de SpawnCharacter y se cayó en
 * RequestCharacter porque el slot de la clave nueva (CM+0x230 + i*0x1D0) no tenía recursos. Esos los prepara
 * FUN_007b7a10 a partir del pedido de autoridad de 0x130 bytes (instancia el composite del personaje con las
 * referencias de +0xFC..+0x114). Acá se clona ese pedido (clave nueva en +0x00 y +0xF0, id nuevo, nombre opcional)
 * y se lo manda por FUN_00797490, la misma vía que el juego. */
static void do_spawn_auth(const Chars *c, int64_t now)
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
        log_printf("GHOST: no se clona (autoridad): pedido %s, spawn de autoridad %s (%ld pedidos)", found < 0 ? "no encontrado" : "ok",
                   g_this ? "visto" : "no visto", (long)total);
        g_state = G_DONE;
        return;
    }
    if (g_attempts >= MAX_ATTEMPTS) {
        log_printf("GHOST: ya se hicieron %u intentos en la sesión; no se clona más", g_attempts);
        g_state = G_DONE;
        return;
    }
    aimp_memset(g_clone, 0, sizeof g_clone);
    aimp_memcpy(g_clone, g_req[found], SPAWN_REQ_SIZE);
    uint32_t old_key = rd32(g_clone + SREQ_KEY), key = 0x40 + g_attempts, id1 = 0x504D4941u + g_attempts;
    char id_old[17], id_new[17];
    id_str(id_old, g_clone + SREQ_ID);
    aimp_memcpy(g_clone + SREQ_KEY, &key, 4);
    aimp_memcpy(g_clone + SREQ_KEY2, &key, 4);
    aimp_memcpy(g_clone + SREQ_ID + 4, &id1, 4);
    /* Corrida 40: con +0x4C = 1 RequestCharacter llama a FUN_007a37e0, que ata el personaje a la plaza de jugador
     * con el id de +0x48 (la del jugador 0): el clon se llevó la cámara y el input. AIMP_GHOST_CLONE_UNBIND=1
     * (por defecto) pone +0x4C = 0: personaje sin plaza de jugador. */
    /* Corrida 41: sin plaza (+0x4C = 0) el clon THE_PLAYER queda como NPC y el juego se cae (árbol de IA, lectura
     * en 0x10). Plazas de jugador: **(SpawnCharacter.this + 0x34): +0xAC0 arreglo de entradas de 0x158 bytes,
     * +0xAC4 cantidad; por entrada +0xF4 id, +0xC8 flags (bit 1: remota, FUN_007a37e0 no le conecta input ni
     * cámara), +0xD0 personaje. Se loguean y el clon va a una plaza libre (sin personaje) o, si no hay, a un id
     * que no existe (AIMP_GHOST_CLONE_PLAYER_ID lo fuerza). UNBIND=1 vuelve a lo de la corrida 41. */
    {
        uint32_t m = 0, base = 0, arr = 0, cnt = 0, free_id = 0xFFFFFFFFu;
        if (mem_u32(g_sc_this + 0x34, &m) && m && mem_u32(m, &base) && base && mem_u32(base + 0xAC0, &arr) && mem_u32(base + 0xAC4, &cnt)) {
            log_printf("GHOST: plazas de jugador: %u (arreglo 0x%08x)", cnt, arr);
            for (uint32_t i = 0; i < cnt && i < 8; i++) {
                uint32_t e = arr + i * 0x158, id = 0, fl = 0, ch = 0;
                mem_u32(e + 0xF4, &id);
                mem_u32(e + 0xC8, &fl);
                mem_u32(e + 0xD0, &ch);
                log_printf("GHOST:   plaza %u: id 0x%08x flags 0x%08x personaje 0x%08x", i, id, fl, ch);
                if (!ch && free_id == 0xFFFFFFFFu)
                    free_id = id;
            }
        } else {
            log_printf("GHOST: no se pudieron leer las plazas de jugador (this 0x%08x)", (unsigned)g_sc_this);
        }
        int forced = aimp_env_int(L"AIMP_GHOST_CLONE_PLAYER_ID", -1);
        uint32_t pid = forced >= 0 ? (uint32_t)forced : free_id != 0xFFFFFFFFu ? free_id : 0x4149u;
        aimp_memcpy(g_clone + SREQ_PLAYER_ID, &pid, 4);
        log_printf("GHOST: el clon va con +0x48 = 0x%08x (%s)", pid,
                   forced >= 0 ? "forzado" : free_id != 0xFFFFFFFFu ? "plaza libre" : "id que no existe");
    }
    if (aimp_env_int(L"AIMP_GHOST_CLONE_UNBIND", 0))
        g_clone[SREQ_PLAYER_FLAG] = 0;
    if (g_clone_name[0]) {
        aimp_memset(g_clone + SREQ_NAME, 0, 0x40);
        aimp_memcpy(g_clone + SREQ_NAME, g_clone_name, aimp_strlen(g_clone_name));
    }
    id_str(id_new, g_clone + SREQ_ID);
    g_nbefore = 0;
    for (uint32_t i = 0; i < c->npl; i++)
        g_before[g_nbefore++] = c->pl[i];
    for (uint32_t i = 0; i < c->nnpc; i++)
        g_before[g_nbefore++] = c->npc[i];
    g_attempts++;
    log_printf("GHOST: clonando el pedido de AUTORIDAD (clave %u -> %u, id %s -> %s, nombre \"%s\"), this 0x%08x; antes: %u jugadores, %u NPCs, %u pendientes",
               old_key, key, id_old, id_new, g_clone_name, (unsigned)g_this, c->npl, c->nnpc, c->npend);
    log_request("GHOST clon auth", (long)g_attempts, g_this, g_clone, 0);
    log_hexdump("GHOST   volcado", (uintptr_t)g_clone, SPAWN_REQ_SIZE);
    g_auth_orig((void *)g_this, NULL, g_clone, 0);
    Chars a;
    if (read_chars(&a))
        log_printf("GHOST: spawn de autoridad hecho; ahora %u jugadores, %u NPCs, %u pendientes", a.npl, a.nnpc, a.npend);
    g_chr = 0;
    g_state = G_FIND;
    g_deadline = now + 10 * g_freq;
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

static float aimp_atan2f(float y, float x)
{
    float r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
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
    if (!mine || ctrl != mine || !cmd || g_allow_cmd || cmd_is_ours(cmd, &type))
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
 * Corrida 22: teletransportarlo mientras camina, sin orden nueva, no se pega (la locomoción lo devuelve en
 * ~4 frames). Por eso cada teletransporte va con una orden nueva en el mismo frame: al ubicarlo y si queda
 * lejos 1 s (como mucho cada 3 s). Corrida 23 (en el controlador de tareas) las actualizaciones no lo hacían
 * caminar; en el de scripts sí (corrida 28), así que por defecto no se reinicia la orden. Nunca
 * SwitchCommand(ctrl, NULL): el juego se cae. */
enum { W_PREP, W_PLACE, W_WALK };
static const char *const g_wnames[] = { "preparando", "ubicando", "caminando" };

static void cmd_name(uint32_t type, char *nm, size_t cap)
{
    uint32_t p = 0;
    if (!(type < COMMAND_TYPES && mem_u32(g_aimp_exe + AIMP_RVA_COMMAND_NAMES + 4 * type, &p) && p && mem_cstr(p, nm, cap)))
        aimp_snprintf(nm, cap, "%s", type == 0xFFFFFFFFu ? "ninguno" : "?");
}

/* ¿El otro controlador del avatar (el de tareas, si la orden va al de scripts) tiene un PLAY_ANIMATION en
 * ejecución o pendiente? Devuelve en nm el nombre del comando en ejecución (o del pendiente). */
static int task_playing_anim(char *nm, size_t cap)
{
    uint32_t c[2] = { 0, 0 }, type, anim = 0, shown = 0xFFFFFFFFu;
    if (g_walk_off == CHR_CMDCTRL) {
        aimp_snprintf(nm, cap, "-");
        return 0;
    }
    mem_u32(g_chr + CHR_CMDCTRL + CMDCTRL_EXEC, &c[0]);
    mem_u32(g_chr + CHR_CMDCTRL + CMDCTRL_CURRENT, &c[1]);
    for (int i = 0; i < 2; i++) {
        if (!c[i] || !mem_u32(c[i] + CMD_TYPE, &type))
            continue;
        if (shown == 0xFFFFFFFFu)
            shown = type;
        anim |= type == CMD_TYPE_PLAY_ANIMATION;
    }
    cmd_name(shown, nm, cap);
    return (int)anim;
}

static float wrap_pi(float a)
{
    while (a > 3.14159265f)
        a -= 6.28318531f;
    while (a < -3.14159265f)
        a += 6.28318531f;
    return a;
}

static void set_move_type(int type)
{
    uint32_t mt = (uint32_t)type;
    aimp_memcpy(g_params, &mt, 4);
    g_cur_type = type;
}

static void walk_start(uint32_t ctrl, const float *t, int64_t now, const char *why)
{
    g_cmd_t[0] = t[0];
    g_cmd_t[1] = t[1];
    g_cmd_t[2] = t[2];
    if (g_starts < 20)
        log_printf("GHOST: caminata: MOVE_TOWARDS a (%.2f, %.2f, %.2f) para 0x%08x (controlador 0x%08x; %s)", (double)t[0],
                   (double)t[1], (double)t[2], g_chr, ctrl, why);
    g_move_start((void *)ctrl, NULL, g_cmd_a, g_cmd_b, g_target, g_params, 1);
    g_last_restart = g_last_cmd = now;
    g_starts++;
}

static int walk_chr(const float *t, float tyaw, const char *src, const char *where, uint32_t idx, int64_t now)
{
    uint32_t ctrl = g_chr + g_walk_off, owner = 0, type = 0xFFFFFFFFu;
    if (!mem_u32(ctrl + CMDCTRL_OWNER, &owner)) {
        log_printf("GHOST: no se pudo leer el controlador de comandos del avatar 0x%08x (+0x%x); se vuelve al teletransporte", g_chr,
                   g_walk_off);
        g_walk_bad = 1;
        return 0;
    }
    if (owner != g_chr && !g_owner_warned) {
        log_printf("GHOST: caminata: el controlador +0x%x del avatar 0x%08x tiene +0x%x = 0x%08x (no es el personaje); se sigue igual",
                   g_walk_off, g_chr, CMDCTRL_OWNER, owner);
        g_owner_warned = 1;
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
    case W_PREP: {
        /* Corrida 29: al recibir la orden, la IA del avatar arranca un PLAY_ANIMATION en su controlador de tareas,
         * y si se lo teletransporta mientras dura, cae del piso (3 de 3 corridas; la reubicación 3 s después
         * funcionó). Primero la orden desde donde está; se lo ubica cuando no hay PLAY_ANIMATION (al menos
         * 500 ms, como mucho AIMP_GHOST_WALK_PREP_MS). */
        char nm[32];
        int anim = task_playing_anim(nm, sizeof nm);
        if (!g_prep_at) {
            log_printf("GHOST: caminata: orden antes de ubicarlo; controlador de tareas: %s", nm);
            set_move_type(g_walk_type);
            walk_start(ctrl, t, now, "antes de ubicarlo");
            g_prep_at = now;
            break;
        }
        int64_t el = now - g_prep_at;
        if (el < g_freq / 2 || (anim && el < g_freq * g_walk_prep_ms / 1000))
            break;
        log_printf("GHOST: caminata: se lo ubica a los %d ms de la orden (controlador de tareas: %s%s)", (int)(el * 1000 / g_freq), nm,
                   anim ? "; se agotó la espera" : "");
        g_wstate = W_PLACE;
    }
        __attribute__((fallthrough));
    case W_PLACE:
        log_printf("GHOST: caminata: teletransporte de 0x%08x a (%.2f, %.2f, %.2f) [%s] (%s; estaba a %.2f m en horizontal, %.2f m en vertical)",
                   g_chr, (double)t[0], (double)t[1], (double)t[2], src, g_last_tp ? "lejos o en otro piso" : "primera vez",
                   (double)aimp_sqrtf(d2), (double)dy);
        /* Corridas 26-28: ubicarlo y esperar 400 ms sin orden lo dejaba en manos de su IA (un PLAY_ANIMATION) y
         * caía del piso; teletransporte y orden en el mismo frame (como la reubicación) sí se pega. */
        teleport_to(t, tyaw);
        g_last_tp = now;
        set_move_type(g_walk_type);
        walk_start(ctrl, t, now, "después de ubicarlo");
        g_wstate = W_WALK;
        g_far_since = 0;
        break;
    case W_WALK: {
        /* AIMP_GHOST_PUPPET (pedido del usuario 2026-11-15: "movimientos exactos, en tiempo real"): con red, el
         * avatar va cada frame a la posición del otro (extrapolada con su velocidad desde el último paquete, como
         * mucho 150 ms) y su locomoción recibe un MOVE_TOWARDS hacia un punto 0,6 s adelante, con el paso según la
         * velocidad del otro, para que las piernas animen. 1 = SetTransform (teleport) por frame; 2 = escribir
         * Character+0x70/+0x174 directo (SetTransform pone en 0 +0x184/+0x1A0, que podría cortar la animación). */
        if (g_puppet && g_tgt_net && g_rm_valid) {
            float el = (float)(now - g_rm_at) / (float)g_freq;
            if (el > 0.15f)
                el = 0.15f;
            if (el < 0.0f)
                el = 0.0f;
            static float s_p[4] __attribute__((aligned(16)));
            s_p[0] = g_rm_p[0] + g_rm_v[0] * el;
            s_p[1] = g_rm_p[1];
            s_p[2] = g_rm_p[2] + g_rm_v[2] * el;
            float pyaw = tyaw;
            if (g_rm_speed > 0.5f)
                pyaw = aimp_atan2f(g_rm_v[0], g_rm_v[2]);
            if (g_puppet == 2) {
                mem_write(g_chr + CHR_POS, s_p, 12);
                mem_write(g_chr + CHR_YAW, &pyaw, 4);
            } else {
                teleport_to(s_p, pyaw);
            }
            g_pup_frames++;
            float lk = g_rm_speed > 0.3f ? 0.6f : 0.0f;
            g_target[3] = s_p[0] + g_rm_v[0] * lk;
            g_target[7] = s_p[1];
            g_target[11] = s_p[2] + g_rm_v[2] * lk;
            float lt[3] = { g_target[3], g_target[7], g_target[11] };
            /* agachado y quieto: comando IDLE con la postura (AIMP_GHOST_CROUCH_STANCE); al moverse o pararse, MOVE */
            int want_idle = g_idle_cmd && g_rm_crouch && g_rm_speed < 0.3f;
            if (want_idle && !g_idle_on && now - g_last_restart >= g_freq / 2) {
                log_printf("GHOST: títere: el otro está agachado y quieto; IDLE con postura %d", g_crouch_stance);
                g_allow_cmd = 1;
                g_idle_cmd((void *)ctrl, NULL, g_cmd_a, g_cmd_b, (uint32_t)g_crouch_stance, 1.0f, 0, 0, 0, 0, 0, 1);
                g_allow_cmd = 0;
                g_idle_on = 1;
                g_last_restart = now;
                break;
            }
            if (g_idle_on) {
                /* corrida 48: sin histéresis salía del IDLE por un pico de velocidad; sale con 0,6 m/s o al pararse */
                if (g_rm_crouch && g_rm_speed < 0.6f)
                    break;
                g_idle_on = 0;
                walk_start(ctrl, lt, now, "títere: deja el IDLE");
                break;
            }
            int want = g_cur_type; /* histéresis: corre por encima del umbral, camina por debajo del 80 % (corrida 46) */
            if (g_rm_speed * 1000.0f > (float)g_walk_run_mms)
                want = g_walk_type_far;
            else if (g_rm_speed * 1000.0f < (float)g_walk_run_mms * 0.8f)
                want = g_walk_type;
            ours = walk_is_ours(ctrl, &type);
            if (want != g_cur_type && now - g_last_restart >= g_freq * 3 / 10) {
                set_move_type(want);
                walk_start(ctrl, lt, now, want == g_walk_type_far ? "títere: corre" : "títere: camina");
            } else if (!ours && now - g_last_restart >= g_freq) {
                walk_start(ctrl, lt, now, "títere: el comando no era el nuestro");
            } else if (ours && now - g_last_cmd >= g_walk_ticks / 2) {
                g_move_update((void *)ctrl, NULL, g_cmd_a, g_cmd_b, g_target, g_params, 1);
                g_last_cmd = now;
                g_updates++;
            }
            break;
        }
        int is_far = d2 > tp * tp || dy > 2.0f || dy < -2.0f;
        if (!is_far)
            g_far_since = 0;
        else if (!g_far_since)
            g_far_since = now;
        if (is_far && now - g_far_since >= g_freq && now - g_last_tp >= 3 * g_freq) {
            /* Corrida 23: SwitchCommand(ctrl, NULL) hace caer el juego (lee cmd+0x28). Se lo teletransporta
             * y en el mismo frame se le da una orden nueva desde ahí. */
            log_printf("GHOST: caminata: teletransporte de 0x%08x a (%.2f, %.2f, %.2f) [%s] (lejos o en otro piso hace 1 s; estaba a %.2f m en horizontal, %.2f m en vertical)",
                       g_chr, (double)t[0], (double)t[1], (double)t[2], src, (double)aimp_sqrtf(d2), (double)dy);
            teleport_to(t, tyaw);
            g_last_tp = now;
            g_far_since = 0;
            set_move_type(g_walk_type);
            walk_start(ctrl, t, now, "después de reubicarlo");
            break;
        }
        ours = walk_is_ours(ctrl, &type);
        /* Prueba con dos máquinas (2026-11-15): con el otro quieto girando la cámara, el avatar giraba y caminaba
         * (la orden del giro iba al objetivo, a 1-3 m, y la re-orden en quieto lo volvía a mover). Después de girar
         * queda "plantado": la orden y las actualizaciones van a donde está parado, sin re-orden ni cambio de
         * paso, hasta que el objetivo se aleja más de 50 cm de donde estaba quieto. */
        if (g_hold && !g_tgt_net)
            g_hold = 0;
        if (g_hold) {
            float hx = t[0] - g_hold_t[0], hz = t[2] - g_hold_t[2]; /* corrida 37: g_still_t sigue al objetivo */
            if (hx * hx + hz * hz > 0.25f) {
                g_hold = 0;
                log_printf("GHOST: caminata: el otro se movió; deja de estar plantado");
                walk_start(ctrl, t, now, "deja de estar plantado");
                break;
            } else {
                g_target[3] = g_hold_p[0];
                g_target[7] = g_hold_p[1];
                g_target[11] = g_hold_p[2];
            }
        }
        /* AIMP_GHOST_WALK_FACE: con red, si el otro está quieto hace 1 s y el avatar se asentó (no se movió más de
         * 10 cm en 1 s, a menos de 3 m: corrida 33, el punto exacto puede no ser alcanzable), se lo gira hacia donde
         * mira el otro. MOVE_TOWARDS no guarda la rotación del destino (FUN_0067e660 copia solo la traslación), así
         * que: teletransporte en su lugar con ese yaw y orden nueva en el mismo frame (como mucho cada 2 s y si la
         * diferencia pasa de 20 grados). */
        if (g_walk_face && g_tgt_net) {
            float sx = t[0] - g_still_t[0], sz = t[2] - g_still_t[2], px = cur[0] - g_settle_p[0], pz = cur[2] - g_settle_p[2], yaw;
            if (px * px + pz * pz > 0.01f || !g_settle_since) {
                g_settle_p[0] = cur[0];
                g_settle_p[1] = cur[1];
                g_settle_p[2] = cur[2];
                g_settle_since = now;
            }
            float dty = wrap_pi(tyaw - g_still_yaw);
            if (dty > 0.1f || dty < -0.1f || !g_yaw_since) { /* el giro espera a que el otro deje de rotar 500 ms */
                g_still_yaw = tyaw;
                g_yaw_since = now;
            }
            if (sx * sx + sz * sz > 0.0025f || !g_still_since) {
                g_still_t[0] = t[0];
                g_still_t[1] = t[1];
                g_still_t[2] = t[2];
                g_still_since = now;
            } else if (now - g_still_since >= g_freq && (g_hold || now - g_settle_since >= g_freq) && (g_hold || d2 < 9.0f) &&
                       now - g_yaw_since >= g_freq / 2 && now - g_last_face >= g_freq &&
                       mem_f32(g_chr + CHR_YAW, &yaw)) {
                float dyaw = wrap_pi(tyaw - yaw);
                if (dyaw > 0.35f || dyaw < -0.35f) {
                    g_faces++;
                    if (g_faces <= 10 || g_faces % 50 == 0)
                        log_printf("GHOST: caminata: gira de %.2f a %.2f rad, hacia donde mira el otro (%u)", (double)yaw, (double)tyaw,
                                   g_faces);
                    if (!g_hold) {
                        g_hold = 1;
                        g_hold_t[0] = t[0];
                        g_hold_t[2] = t[2];
                        g_hold_p[0] = cur[0];
                        g_hold_p[1] = cur[1];
                        g_hold_p[2] = cur[2];
                    }
                    g_target[3] = g_hold_p[0];
                    g_target[7] = g_hold_p[1];
                    g_target[11] = g_hold_p[2];
                    teleport_to(g_hold_p, tyaw);
                    g_last_tp = g_last_face = now;
                    walk_start(ctrl, g_hold_p, now, "gira en el lugar hacia donde mira el otro");
                    break;
                }
            }
        }
        /* AIMP_GHOST_WALK_REARM_CM: después de llegar no vuelve a arrancar hasta que el objetivo se aleja ~1-3 m
         * (corridas 31-34). Si está quieto (no se movió más de 10 cm en 1 s) y a más de eso del objetivo, orden
         * nueva (como mucho cada 2 s). */
        if (g_walk_rearm_cm > 0 && !g_hold) {
            float qx = cur[0] - g_rearm_p[0], qz = cur[2] - g_rearm_p[2], ra = (float)g_walk_rearm_cm / 100.0f;
            if (qx * qx + qz * qz > 0.01f || !g_rearm_since) {
                g_rearm_p[0] = cur[0];
                g_rearm_p[2] = cur[2];
                g_rearm_since = now;
            } else if (now - g_rearm_since >= g_freq && d2 > ra * ra && now - g_last_restart >= 2 * g_freq && rearm_allowed(t)) {
                char why[48];
                g_rearms++;
                aimp_snprintf(why, sizeof why, "quieto a %.1f m del objetivo (%u)", (double)aimp_sqrtf(d2), g_rearms);
                walk_start(ctrl, t, now, why);
                g_rearm_since = now;
                break;
            }
        }
        /* Corrida 30: caminando (~1,2 m/s) quedaba hasta 8,8 m atrás del jugador (~2,2 m/s). Corre (MOVE RUN) si
         * queda a más de AIMP_GHOST_WALK_FAR_CM del objetivo y vuelve a caminar 2,5 m más cerca; el tipo se
         * fija al dar la orden, así que va con una orden nueva (como mucho una por segundo). */
        float run_at = (float)g_walk_far_cm / 100.0f, walk_at = run_at - 2.5f, dd = aimp_sqrtf(d2);
        int want = g_cur_type;
        if (g_tgt_net && g_rm_valid && g_walk_run_mms > 0 && !g_hold) {
            /* con red: el paso sigue a la velocidad del otro (corre si el otro corre) o corre si quedó lejos */
            if (g_rm_speed * 1000.0f > (float)g_walk_run_mms || dd > run_at)
                want = g_walk_type_far;
            else if (g_rm_speed * 1000.0f < (float)g_walk_run_mms * 0.8f && dd < walk_at)
                want = g_walk_type;
        } else if (g_walk_type_far != g_walk_type && !g_hold) {
            if (g_cur_type == g_walk_type && dd > run_at)
                want = g_walk_type_far;
            else if (g_cur_type == g_walk_type_far && dd < walk_at)
                want = g_walk_type;
        }
        if (want != g_cur_type && now - g_last_restart >= (g_tgt_net ? g_freq / 2 : g_freq)) {
            char why[64];
            aimp_snprintf(why, sizeof why, "%s: a %.1f m del objetivo, el otro a %.1f m/s", want == g_walk_type_far ? "corre" : "camina",
                          (double)dd, (double)g_rm_speed);
            set_move_type(want);
            walk_start(ctrl, t, now, why);
            break;
        }
        float mx = t[0] - g_cmd_t[0], mz = t[2] - g_cmd_t[2], rs = (float)g_walk_restart_cm / 100.0f;
        int moved = g_walk_restart_cm > 0 && mx * mx + mz * mz > rs * rs;
        if (!ours && now - g_last_restart >= g_freq) {
            char nm[32], why[48];
            cmd_name(type, nm, sizeof nm);
            aimp_snprintf(why, sizeof why, "el comando actual era %s", nm);
            walk_start(ctrl, g_hold ? g_hold_p : t, now, why);
        } else if (moved && now - g_last_restart >= g_freq * g_walk_restart_ms / 1000) {
            /* Corrida 23: actualizar el destino (FUN_006797a0) no lo hace caminar; una orden nueva sí (corrida 21). */
            walk_start(ctrl, t, now, "el objetivo se movió");
        } else if (ours && now - g_last_cmd >= g_walk_ticks) {
            g_move_update((void *)ctrl, NULL, g_cmd_a, g_cmd_b, g_target, g_params, 1);
            g_last_cmd = now;
            g_updates++;
        }
        break;
    }
    }
    (void)tyaw;
    if (now - g_last_log >= g_freq) {
        float cyaw = 0.0f;
        mem_f32(g_chr + CHR_YAW, &cyaw);
        log_printf("GHOST: caminata (%s): personaje en (%.2f, %.2f, %.2f) yaw %.2f, objetivo (%.2f, %.2f, %.2f) yaw %.2f [%s] a %.2f m; comando %s tipo %d, move_type %d; %u inicios, %u actualizaciones, %u teletransportes, %ld cinemáticas; %s[%u]",
                   g_wnames[g_wstate], (double)cur[0], (double)cur[1], (double)cur[2], (double)cyaw, (double)t[0], (double)t[1], (double)t[2],
                   (double)tyaw, src, (double)aimp_sqrtf(d2), ours ? "propio" : "ajeno", (int)type, g_cur_type, g_starts, g_updates, g_tps,
                   (long)InterlockedExchange(&g_n_cine, 0), where, idx);
        g_last_log = now;
        g_tps = 0;
    }
    return 1;
}

/* AIMP_GHOST_INVINCIBLE (1 por defecto): el avatar no recibe daño (prueba con dos máquinas, 2026-11-15: el
 * usuario mató al avatar y el juego dio game over por lastimar civiles). Se le pone el flag de
 * CHR_SetInvincibility(INVINCIBLE) (Character+0x2E8 |= 8, +0x2EA |= 8; game.h), con la misma verificación
 * que hace el engine: el componente de salud de Character+0x320 tiene que apuntar al personaje. Se repite cada
 * segundo por si un script lo cambia. */
static void invincible(int64_t now)
{
    if (!g_invincible || g_inv_bad || (g_inv_chr == g_chr && now - g_inv_last < g_freq))
        return;
    uint32_t owner = 0;
    uint8_t a = 0, b = 0;
    uint16_t hp = 0;
    if (!mem_u32(g_chr + CHR_HEALTHCOMP + HEALTHCOMP_OWNER, &owner) || owner != g_chr || !mem_u8(g_chr + CHR_INVINCIBLE, &a) ||
        !mem_u8(g_chr + CHR_PLAYER_ONLY_DAMAGE, &b)) {
        log_printf("GHOST: invencible: el componente de salud de 0x%08x (+0x%x) apunta a 0x%08x, no al personaje; no se toca",
                   g_chr, CHR_HEALTHCOMP + HEALTHCOMP_OWNER, owner);
        g_inv_bad = 1;
        return;
    }
    if (!(a & 8) || !(b & 8)) {
        uint8_t na = a | 8, nb = b | 8;
        mem_write(g_chr + CHR_INVINCIBLE, &na, 1);
        mem_write(g_chr + CHR_PLAYER_ONLY_DAMAGE, &nb, 1);
        mem_read(g_chr + CHR_HEALTH, &hp, 2);
        log_printf("GHOST: invencible: avatar 0x%08x, +0x2e8 %02x -> %02x, +0x2ea %02x -> %02x (salud %u)%s", g_chr, a, na, b, nb, hp,
                   g_inv_chr == g_chr ? "; alguien lo había cambiado" : "");
    }
    g_inv_chr = g_chr;
    g_inv_last = now;
}


/* Movimiento del otro jugador (prueba con dos máquinas 2026-11-15: el avatar reaccionaba tarde, caminaba cuando
 * el otro corría una distancia corta y no se agachaba). Con los paquetes de estado (time_ms del emisor) se estima
 * la velocidad (media exponencial) y la altura de la cámara sobre los pies (parado ~1,57 m). El objetivo se
 * adelanta AIMP_GHOST_WALK_LEAD_MS (400) en la dirección del movimiento, y el paso sale de la velocidad real:
 * RUN a más de AIMP_GHOST_WALK_RUN_MMS (3000 mm/s). Agachado = cámara a menos de 1,25 m (por ahora solo se loguea). */
static void remote_motion(const AimpPkt *r, float *t, int64_t now)
{
    if (r->seq != g_rm_seq) {
        g_rm_at = now;
        if (g_rm_valid && r->seq > g_rm_seq && r->time_ms != g_rm_ms) {
            float dt = (float)(uint32_t)(r->time_ms - g_rm_ms) / 1000.0f;
            if (dt > 0.0f && dt < 1.0f) {
                for (int k = 0; k < 3; k++) {
                    float v = (r->pos[k] - g_rm_p[k]) / dt;
                    g_rm_v[k] = g_rm_v[k] * 0.5f + v * 0.5f;
                }
            } else {
                g_rm_v[0] = g_rm_v[1] = g_rm_v[2] = 0.0f;
            }
        }
        g_rm_seq = r->seq;
        g_rm_ms = r->time_ms;
        g_rm_p[0] = r->pos[0];
        g_rm_p[1] = r->pos[1];
        g_rm_p[2] = r->pos[2];
        g_rm_valid = 1;
        g_rm_speed = aimp_sqrtf(g_rm_v[0] * g_rm_v[0] + g_rm_v[2] * g_rm_v[2]);
        float h = r->cam_pos[1] - r->pos[1];
        if (h > 0.3f && h < 2.5f)
            g_rm_camh = h;
        /* corrida 49: la cámara del paquete a veces es otra (alterna 1,0-1,2 y 1,57 m cuadro a cuadro aunque el
         * jugador esté parado): el estado cambia solo si la lectura se mantiene 300 ms seguidos */
        int cr = g_rm_camh < 1.25f ? 1 : g_rm_camh > 1.40f ? 0 : g_rm_crouch;
        if (cr == g_rm_crouch || !g_rm_cr_since)
            g_rm_cr_since = now;
        if (cr != g_rm_crouch && now - g_rm_cr_since >= g_freq * 3 / 10) {
            g_rm_crouch = cr;
            log_printf("GHOST: el otro %s (cámara a %.2f m de los pies)", cr ? "se agachó" : "se paró", (double)g_rm_camh);
        }
    }
    if (g_walk_lead_ms > 0 && g_rm_speed > 0.3f) {
        float k = (float)g_walk_lead_ms / 1000.0f;
        t[0] += g_rm_v[0] * k;
        t[2] += g_rm_v[2] * k;
    }
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
    invincible(now);
    float lpos[3], lyaw, t[3], tyaw;
    if (!mem_read(g_player + CHR_POS, lpos, sizeof lpos) || !mem_f32(g_player + CHR_YAW, &lyaw))
        return;
    const char *src;
    g_tgt_net = 0;
    if (net && net->have_remote && (net->remote.flags & AIMP_FLAG_PLAYER) && net->remote.level == level) {
        t[0] = net->remote.pos[0];
        t[1] = net->remote.pos[1];
        t[2] = net->remote.pos[2];
        tyaw = net->remote.yaw;
        src = "red";
        g_tgt_net = 1;
        remote_motion(&net->remote, t, now);
    } else if (g_walk && !g_walk_bad) {
        /* AIMP_GHOST_WALK_BEHIND_CM (250) detrás del jugador: adelante = (sin yaw, cos yaw) (corrida 20). Con
         * 1,2 m, caminar hacia atrás metía al jugador contra el NPC (corrida 22). Corrida 29: si el jugador se da
         * vuelta, ese punto queda del otro lado y el NPC se le pega al ir en línea recta. Con
         * AIMP_GHOST_WALK_FOLLOW_CM (200) el objetivo es el punto a esa distancia del jugador del lado donde ya
         * está el NPC (como un acompañante); la primera vez y si quedó lejos, detrás. */
        float b = (float)g_walk_behind_cm / 100.0f, f = (float)g_walk_follow_cm / 100.0f, cur[3], vx = 0.0f, vz = 0.0f, n2 = 0.0f;
        if (f > 0.0f && g_wstate == W_WALK && mem_read(g_chr + CHR_POS, cur, sizeof cur)) {
            vx = cur[0] - lpos[0];
            vz = cur[2] - lpos[2];
            n2 = vx * vx + vz * vz;
        }
        float tp = (float)g_walk_tp_cm / 100.0f;
        if (n2 > 0.01f && n2 < tp * tp) { /* lejos: se lo reubica detrás (del otro lado podría haber una pared) */
            float k = f / aimp_sqrtf(n2);
            t[0] = lpos[0] + vx * k;
            t[2] = lpos[2] + vz * k;
            src = "local a distancia";
        } else {
            t[0] = lpos[0] - b * aimp_sinf(lyaw);
            t[2] = lpos[2] - b * aimp_cosf(lyaw);
            src = "local detrás";
        }
        t[1] = lpos[1];
        tyaw = lyaw;
    } else if (g_front_cm > 0) { /* AIMP_GHOST_FRONT_CM: delante del jugador, mirándolo (para verlo en las capturas) */
        float d = (float)g_front_cm / 100.0f;
        t[0] = lpos[0] + d * aimp_sinf(lyaw);
        t[1] = lpos[1];
        t[2] = lpos[2] + d * aimp_cosf(lyaw);
        tyaw = lyaw > 0.0f ? lyaw - 3.14159265f : lyaw + 3.14159265f;
        src = "local delante";
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
