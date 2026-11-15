/* Objetos arrojados y colocados por el jugador (hallazgo 17).
 *
 * Tirar (bengala, noisemaker, molotov, pipe bomb, humo, flashbang...): el evento de animación
 * "spawn_projectile" (FUN_00666f90 -> FUN_0064c180 -> FUN_007d4830) llama a FUN_007b9500, que:
 *   1. arma 64 bytes con FUN_0079c910: matriz 3x4 de la mano (traslación en m[3], m[7], m[11]) y
 *      velocidad = normalizar(punto de impacto [equipo+0x90] - mano) * [0x163E964] (20), más 9.81;
 *   2. llama a FUN_007b5050(pool = [0x16F0DE4], &m, &m[12] (velocidad), 15.0, def+0x58, def+0x70).
 * FUN_007b5050 toma una entrada libre del pool y llama a FUN_007ade20, que instancia el composite
 * cuyo nombre termina con la plantilla (def+0x58 = "Thrown\%s", p. ej. REQUIRED_ASSETS\THROWN\FLARE;
 * búsqueda por sufijo sin mayúsculas en FUN_00920750), le pone start_position_matrix,
 * start_position, velocity, angular_velocity = 0, motion_duration = 10 y time_left, y le dispara
 * "spawn_explosive". El vuelo lo simula ProjectileMotion (FUN_0060b740) con esos parámetros y la
 * gravedad: no lee el estado del jugador.
 * Colocar (minas: evento "place_ied" -> FUN_007d4a00; soltar el ítem de la mano: FUN_007c7ef0):
 * FUN_007b5190(pool, &m, plantilla, time_left), que hace lo mismo con velocidad 0 y "place_item".
 *
 * Con AIMP_SYNC_THROW=1 y red, cada llamada del jugador local sale como AIMP_EVT_THROW y el otro
 * llama a la misma función (por el trampolín, así no vuelve a mandarse) con los mismos datos.
 * La plantilla es un char* que solo se usa para buscar el composite; igual se guarda en una tabla
 * estática, por si algo la retiene.
 * Pendiente: el lanzamiento local también avisa a la IA (FUN_0070f310 en FUN_007d4830); el
 * remoto no. Lo que haga el composite (ruido, luz, fuego) corre igual en las dos máquinas.
 */
#include "throw.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define LOG_MAX 64
#define NAMES_MAX 16
#define ECHO_MAX 8

typedef void(__attribute__((fastcall)) * SpawnFn)(void *pool, void *edx, const float *m, const float *vel, uint32_t a3,
                                                  const char *name, uint32_t t);
typedef void(__attribute__((fastcall)) * PlaceFn)(void *pool, void *edx, const float *m, const char *name, uint32_t t);

static AimpHook g_hk_spawn, g_hk_place;
static SpawnFn g_spawn_orig;
static PlaceFn g_place_orig;
static AimpNet *g_net;
static int g_trace, g_sync, g_echo_ms, g_echo_cm = 75;
static volatile uint32_t g_level;
static volatile LONG g_n_local, g_n_remote;

/* Los detours corren en el thread del evento de animación (no se sabe si es el principal): solo
 * encolan, con una sección crítica. throw_frame (thread principal) loguea, manda y hace los ecos. */
typedef struct {
    ThrowEvt t;
    uint32_t level;
    uint32_t from; /* rva del caller */
    DWORD due_ms;  /* ecos */
} Item;
static CRITICAL_SECTION g_cs;
static int g_cs_ok;
static Item g_local[ECHO_MAX], g_echo[ECHO_MAX];
static int g_nlocal, g_necho;
static volatile LONG g_n_lost;

static char g_names[NAMES_MAX][THROW_NAME_MAX];

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static float bits_f(uint32_t u)
{
    float f;
    aimp_memcpy(&f, &u, 4);
    return f;
}

static float sqrt_f(float x)
{
    float r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

int throw_sync_enabled(void) { return g_sync; }

static void log_evt(const char *what, LONG n, const ThrowEvt *t, uint32_t from)
{
    float v = sqrt_f(t->vel[0] * t->vel[0] + t->vel[1] * t->vel[1] + t->vel[2] * t->vel[2]);
    if (t->op == THROW_OP_THROW)
        log_printf("THROW %s %ld: tirar %s desde (%.2f, %.2f, %.2f), vel (%.2f, %.2f, %.2f) |%.2f|, a3 %.2f, t %.2f%s%x", what,
                   (long)n, t->name, (double)t->m[3], (double)t->m[7], (double)t->m[11], (double)t->vel[0], (double)t->vel[1],
                   (double)t->vel[2], (double)v, (double)bits_f(t->a3), (double)bits_f(t->t), from ? ", desde rva 0x" : "", from);
    else
        log_printf("THROW %s %ld: colocar %s en (%.2f, %.2f, %.2f), t %.2f%s%x", what, (long)n, t->name, (double)t->m[3],
                   (double)t->m[7], (double)t->m[11], (double)bits_f(t->t), from ? ", desde rva 0x" : "", from);
}

/* Lo que hizo el jugador local (en el detour): se encola para throw_frame. */
static void local_event(const ThrowEvt *t, uint32_t from)
{
    if (!g_cs_ok)
        return;
    EnterCriticalSection(&g_cs);
    if (g_nlocal < ECHO_MAX) {
        g_local[g_nlocal].t = *t;
        g_local[g_nlocal].level = g_level;
        g_local[g_nlocal].from = from;
        g_nlocal++;
    } else {
        InterlockedIncrement(&g_n_lost);
    }
    LeaveCriticalSection(&g_cs);
}

/* throw_frame: log, red y eco de un evento local. */
static void process_local(const Item *it)
{
    LONG n = InterlockedIncrement(&g_n_local);
    if (n <= LOG_MAX)
        log_evt("local", n, &it->t, it->from);
    if (g_sync && g_net && g_net->mode) {
        if (net_event(g_net, AIMP_EVT_THROW, it->level, &it->t, sizeof it->t)) {
            if (n <= LOG_MAX)
                log_printf("THROW   mandado al otro jugador (evento %u, nivel %u)", g_net->evt_seq, it->level);
        } else if (n <= LOG_MAX) {
            log_printf("THROW   no se pudo mandar (sin par o cola llena)");
        }
    }
    if (g_echo_ms > 0 && it->t.op == THROW_OP_THROW && g_necho < ECHO_MAX) {
        g_echo[g_necho] = *it;
        g_echo[g_necho].due_ms = GetTickCount() + (DWORD)g_echo_ms;
        g_necho++;
    }
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_spawn(void *pool, void *edx, const float *m,
                                                                                 const float *vel, uint32_t a3,
                                                                                 const char *name, uint32_t t)
{
    ThrowEvt e;
    aimp_memset(&e, 0, sizeof e);
    e.op = THROW_OP_THROW;
    if (m && vel && name && mem_read((uintptr_t)m, e.m, sizeof e.m) && mem_read((uintptr_t)vel, e.vel, sizeof e.vel) &&
        mem_cstr((uintptr_t)name, e.name, sizeof e.name)) {
        e.a3 = a3;
        e.t = t;
        local_event(&e, rva((uint32_t)(uintptr_t)__builtin_return_address(0)));
    }
    g_spawn_orig(pool, edx, m, vel, a3, name, t);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_place(void *pool, void *edx, const float *m,
                                                                                 const char *name, uint32_t t)
{
    ThrowEvt e;
    aimp_memset(&e, 0, sizeof e);
    e.op = THROW_OP_PLACE;
    if (m && name && mem_read((uintptr_t)m, e.m, sizeof e.m) && mem_cstr((uintptr_t)name, e.name, sizeof e.name)) {
        e.t = t;
        local_event(&e, rva((uint32_t)(uintptr_t)__builtin_return_address(0)));
    }
    g_place_orig(pool, edx, m, name, t);
}

int throw_install(AimpNet *net)
{
    g_trace = aimp_env_int(L"AIMP_TRACE_THROW", 0);
    g_sync = aimp_env_int(L"AIMP_SYNC_THROW", 0);
    g_echo_ms = aimp_env_int(L"AIMP_THROW_ECHO", 0);
    g_echo_cm = aimp_env_int(L"AIMP_THROW_ECHO_CM", 75);
    if (!g_trace && !g_sync && g_echo_ms <= 0)
        return 0;
    g_net = net;
    if (!g_cs_ok) {
        InitializeCriticalSection(&g_cs);
        g_cs_ok = 1;
    }
    int ok = hook_install(&g_hk_spawn, "tirar (FUN_007b5050)", (void *)(g_aimp_exe + AIMP_RVA_THROW_SPAWN), AIMP_THROW_PROLOGUE,
                          sizeof AIMP_THROW_PROLOGUE, AIMP_THROW_STEAL, (void *)det_spawn, (void **)&g_spawn_orig);
    int ok2 = hook_install(&g_hk_place, "colocar (FUN_007b5190)", (void *)(g_aimp_exe + AIMP_RVA_THROW_PLACE),
                           AIMP_THROW_PROLOGUE, sizeof AIMP_THROW_PROLOGUE, AIMP_THROW_STEAL, (void *)det_place,
                           (void **)&g_place_orig);
    if (!ok || !ok2) {
        g_sync = 0;
        g_echo_ms = 0;
    }
    log_printf("THROW: AIMP_TRACE_THROW=%d AIMP_SYNC_THROW=%d%s AIMP_THROW_ECHO=%d ms (%d cm); hooks %s/%s", g_trace, g_sync,
               g_sync && !(net && net->mode) ? " (sin red: solo se loguea)" : "", g_echo_ms, g_echo_cm, ok ? "ok" : "NO",
               ok2 ? "ok" : "NO");
    return ok && ok2;
}

static const char *intern(const char *name)
{
    for (int i = 0; i < NAMES_MAX; i++) {
        if (!g_names[i][0]) {
            aimp_memcpy(g_names[i], name, THROW_NAME_MAX);
            g_names[i][THROW_NAME_MAX - 1] = 0;
            return g_names[i];
        }
        if (aimp_memcmp(g_names[i], name, aimp_strlen(g_names[i]) + 1) == 0)
            return g_names[i];
    }
    return NULL;
}

static uint32_t local_player(void)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0, pl = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm || !mem_u32(cm + CM_NPLAYERS, &n) || !n ||
        !mem_u32(cm + CM_PLAYERS, &arr) || !mem_u32(arr, &pl))
        return 0;
    return pl;
}

/* Llama a la función original (por el trampolín: no pasa por el detour). 1 si se llamó. */
static int apply(const ThrowEvt *t, const char *who)
{
    static float s_m[16] __attribute__((aligned(16)));
    static float s_v[4] __attribute__((aligned(16)));
    uint32_t pool = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_THROW_POOL);
    LONG n = InterlockedIncrement(&g_n_remote);
    char name[THROW_NAME_MAX];
    aimp_memcpy(name, t->name, sizeof name);
    name[sizeof name - 1] = 0;
    const char *nm = intern(name);
    if (!pool || !nm || !local_player() || (t->op == THROW_OP_THROW && !g_spawn_orig) ||
        (t->op == THROW_OP_PLACE && !g_place_orig) || (t->op != THROW_OP_THROW && t->op != THROW_OP_PLACE)) {
        if (n <= LOG_MAX)
            log_printf("THROW %s %ld: no se aplica (pool 0x%08x, plantilla %s, jugador %s, op %u)", who, (long)n, pool,
                       nm ? "ok" : "sin lugar", local_player() ? "sí" : "no", t->op);
        return 0;
    }
    if (n <= LOG_MAX)
        log_evt(who, n, t, 0);
    aimp_memcpy(s_m, t->m, sizeof t->m);
    aimp_memcpy(s_m + 12, t->vel, sizeof t->vel); /* como en FUN_007b9500: la velocidad sigue a la matriz */
    s_m[15] = 9.81f;
    aimp_memcpy(s_v, t->vel, sizeof t->vel);
    s_v[3] = 9.81f;
    if (t->op == THROW_OP_THROW)
        g_spawn_orig((void *)(uintptr_t)pool, NULL, s_m, s_v, t->a3, nm, t->t);
    else
        g_place_orig((void *)(uintptr_t)pool, NULL, s_m, nm, t->t);
    return 1;
}

void throw_remote_event(const AimpEvt *e, uint32_t level)
{
    ThrowEvt t;
    if (e->size != sizeof t) {
        log_printf("THROW: evento remoto de %u bytes (se esperaban %u): se ignora", e->size, (unsigned)sizeof t);
        return;
    }
    aimp_memcpy(&t, e->data, sizeof t);
    if (e->level != level) {
        log_printf("THROW: lanzamiento remoto (evento %u, %s) del nivel %u; acá el nivel es %u: se descarta", e->seq,
                   t.name[0] ? "con plantilla" : "sin plantilla", e->level, level);
        return;
    }
    apply(&t, "remoto");
}

void throw_frame(int64_t now, int64_t freq, uint32_t level)
{
    (void)now;
    (void)freq;
    g_level = level;
    if (!g_cs_ok)
        return;
    Item loc[ECHO_MAX];
    int nl;
    EnterCriticalSection(&g_cs);
    nl = g_nlocal;
    aimp_memcpy(loc, g_local, sizeof(Item) * (size_t)nl);
    g_nlocal = 0;
    LeaveCriticalSection(&g_cs);
    for (int i = 0; i < nl; i++)
        process_local(&loc[i]);
    LONG lost = InterlockedExchange(&g_n_lost, 0);
    if (lost)
        log_printf("THROW: %ld eventos locales descartados en un frame (cola llena)", (long)lost);
    if (g_echo_ms <= 0)
        return;
    DWORD ms = GetTickCount();
    int w = 0, nd = 0;
    Item due[ECHO_MAX];
    for (int r = 0; r < g_necho; r++) {
        if ((LONG)(ms - g_echo[r].due_ms) >= 0)
            due[nd++] = g_echo[r];
        else
            g_echo[w++] = g_echo[r];
    }
    g_necho = w;
    for (int i = 0; i < nd; i++) {
        ThrowEvt t = due[i].t;
        if (due[i].level != level)
            continue;
        /* al costado de la dirección horizontal (y es la vertical); sin velocidad, en +x */
        float dx = 1.0f, dz = 0.0f, h = sqrt_f(t.vel[0] * t.vel[0] + t.vel[2] * t.vel[2]);
        if (h > 0.001f) {
            dx = -t.vel[2] / h;
            dz = t.vel[0] / h;
        }
        float d = (float)g_echo_cm / 100.0f;
        t.m[3] += dx * d;
        t.m[11] += dz * d;
        apply(&t, "eco");
    }
}
