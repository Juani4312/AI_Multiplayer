/* Traza (y reinyección experimental) de los pines de salida de CATHODE (hallazgo 15; fase 6).
 *
 * Todo pin de salida que dispara una entidad de script pasa por EntityInterface::vt[+0x394]
 * = FUN_008be0d0 (777 clases con RTTI lo heredan, y también las vtables concretas sin RTTI,
 * p. ej. la de TriggerSimple 0x01261EB8). Firma (asm de FUN_008bd920 y FUN_008be0d0):
 *     thiscall(EntityInterface *this, Handle **ctx, uint32 *pin, Msg **msg, double demora, flag), ret 0x18
 * - ctx: puntero a un handle refcontado (+4 refcount atómico, +0xC objeto completo de la entidad;
 *   corrida 15: siempre la propia entidad, a 0x14..0x20 bytes antes de this).
 * - objeto completo: +0x0C ShortGuid de la entidad (está en el COMMANDS.PAK del nivel) y +0x10
 *   ShortGuid del tipo ("TriggerSimple", "Door", ...). Corrida 15: 78/78 y 77/77 en el PAK.
 * - msg: puntero a un objeto de 0x28 bytes de un pool (refcount en +0x24, FUN_0091e1e0); +0x20 flags.
 * FUN_008be0d0 busca el pin en la tabla de links de la entidad (this+4) y, por cada link, dispara
 * el destino directo o lo encola en el entity manager con la demora (FUN_009271c0).
 *
 * AIMP_TRACE_PINS=1: solo mira. Por cada (entidad, pin) loguea las primeras apariciones y cuenta el
 * resto; la tabla se vacía en cada cambio de nivel.
 * AIMP_PINS_FILE=ruta (experimento del paso 3): archivo de comandos que se relee cuando cambia.
 *     FIRE <pid o id de entidad, hex> <pin hex>   (pid = ruta de composites, único en el nivel)
 * vuelve a disparar ese pin sobre la entidad (tiene que haber disparado algún pin desde que cargó el
 * nivel), con copias propias del handle y del mensaje del primer disparo visto (refcount alto: el motor
 * nunca las libera). Corre en el thread principal, entre frames.
 *     FIRE <pid> <pin> [flags]  también funciona sin disparo previo, con el índice (abajo) y una
 *                               plantilla de mensaje (mismo pid y pin, mismo tipo y pin, mismo pin...).
 *     INDEX                     rearma el índice y lo verifica contra lo visto.
 *
 * Índice pid -> entidad (hallazgo 16): los CompositeInstance guardan sus hijos en +0x2C (refptr a
 * {+8 cantidad, +0xC handles}); se recorre el árbol desde las raíces (subiendo por +0x08 desde
 * cualquier entidad que haya disparado) y el pid se calcula con la pila de ancestros, igual que
 * path_id. El subobjeto EntityInterface (el this del disparo) es el primer puntero del objeto completo
 * a una vtable del módulo con FUN_008be0d0 en +0x394.
 *
 * AIMP_SYNC_PINS=1 (fase 6, paso 4; requiere AIMP_NET): reglas AIMP_SYNC_PINS_LIST = "tipo:pin,..."
 * (hex; pin * = todos). En el host, cada disparo natural que cumple una regla sale como evento
 * AIMP_EVT_PIN. En el cliente, todo disparo que cumple una regla se bloquea (no se llama al original)
 * salvo el que inyecta el mod al aplicar un evento del host (y solo ese: lo que dispara en cadena y
 * cumple una regla también se bloquea, porque el host lo manda aparte).
 */
#include "pins.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define PK_SLOTS 65536 /* potencia de 2; claves (entidad, pin) */
#define VT_SLOTS 2048  /* vtables vistas (para el volcado de la primera instancia) */
#define MSG_SIZE 0x28
#define H_SIZE 0x10

typedef struct {
    uint32_t ent, pin;
    uint32_t vt, idx, links;
    uint32_t total, period;
    uint32_t outer, eid, type;
    uint32_t pid, depth; /* id de la ruta de composites (estable entre máquinas) y profundidad */
    uint32_t h, mobj;
    uint32_t t_first; /* GetTickCount del primer disparo */
    uint32_t hsnap[H_SIZE / 4];
    uint32_t msnap[MSG_SIZE / 4];
    uint8_t have_snap;
} PKey;

typedef void(__attribute__((fastcall)) * FireFn)(void *self, void *edx, void *ctx, const uint32_t *pin, void *msg,
                                                 double delay, uint32_t flag);

static AimpHook g_hk;
static FireFn g_orig;
static int g_on, g_per_key = 2, g_max_lines = 20000;
static CRITICAL_SECTION g_cs;
static PKey g_keys[PK_SLOTS];
static uint32_t g_nkeys, g_lines, g_fires, g_fires_period, g_full, g_other_thread, g_level, g_injected;
static uint32_t g_vts[VT_SLOTS], g_nvts;
static volatile LONG g_depth_max;
static int t_depth;     /* anidamiento en el thread principal: un pin dispara a otro directamente */
static int g_injecting; /* el disparo actual lo hizo el mod */
static volatile uint32_t g_win_until, g_win_lines; /* ventana después de una inyección: se detalla todo */
static wchar_t g_file[MAX_PATH];
static FILETIME g_file_time;
static int64_t g_next_file_check, g_freq;

/* Inyección en curso: solo ese (entidad, pin) pasa el bloqueo del cliente. */
static volatile uint32_t g_inj_ent, g_inj_pin;

/* ---- Índice pid -> entidad (hallazgo 16) ---- */
#define IX_SLOTS 65536
#define VC_SLOTS 8192
#define MAX_DEPTH 32
typedef struct {
    uint32_t pid, outer, ent, h, vt, eid, type, depth;
} IxEnt;
static IxEnt g_ix[IX_SLOTS];
static uint32_t g_nix, g_ix_coll, g_ix_ms, g_ix_builds, g_ix_at;
static int g_ix_dirty = 1;
static uint32_t g_vc_key[VC_SLOTS];
static uint8_t g_vc_val[VC_SLOTS]; /* 1 = vtable de EntityInterface, 2 = no */
static uint32_t g_level_at;        /* GetTickCount del último cambio de nivel */
static int g_ix_auto_done;

/* ---- Volcado de mensajes: el primero de cada (tipo, pin) ---- */
#define MD_SLOTS 4096
static uint32_t g_md_type[MD_SLOTS], g_md_pin[MD_SLOTS], g_nmd;

/* ---- Sincronización (AIMP_SYNC_PINS) ---- */
#define RULES_MAX 32
#define OUT_MAX 128
#define PEND_MAX 128
typedef struct {
    uint32_t type, pin;
    int any;
} Rule;
typedef struct {
    uint32_t pid, pin, type, eid, mflags;
} PinEvt;
typedef struct {
    PinEvt p;
    uint32_t level, seq, at, told;
} PinPend;
static AimpNet *g_net;
static int g_sync; /* 0 apagada, 1 host (manda), 2 cliente (bloquea y aplica) */
static Rule g_rules[RULES_MAX];
static int g_nrules;
static CRITICAL_SECTION g_out_cs;
static PinEvt g_out[OUT_MAX];
static uint32_t g_nout, g_out_drop;
static PinPend g_pend[PEND_MAX];
static int g_npend;
static uint32_t g_n_sent, g_n_blocked, g_n_applied, g_n_expired, g_sync_lines;
static uint32_t g_pend_ms = 60000;

int pins_enabled(void) { return g_on; }
int pins_sync_enabled(void) { return g_on && g_sync; }
void pins_test_role(int role) { g_sync = role; }

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

/* Links del pin en la tabla de la entidad: cantidad y primer destino. */
static uint32_t count_links(uint32_t ent, uint32_t pin, uint32_t *first_target)
{
    uint32_t tab = 0, n = 0, arr = 0, cnt = 0;
    *first_target = 0;
    if (!mem_u32(ent + ENT_LINKS, &tab) || !tab || !mem_u32(tab + 8, &n) || !mem_u32(tab + 0xC, &arr) || !arr)
        return 0;
    if (n > 8192)
        return 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t k = 0, link = 0;
        if (!mem_u32(arr + i * 12, &k))
            return 0;
        if (k != pin)
            continue;
        mem_u32(arr + i * 12 + 8, &link);
        if (link)
            mem_u32(link + 8, first_target);
        while (link && cnt < 1000) {
            cnt++;
            if (!mem_u32(link + 0xC, &link))
                break;
        }
        break;
    }
    return cnt;
}

static PKey *lookup(uint32_t ent, uint32_t pin, int *created)
{
    uint32_t h = hash2(ent, pin) & (PK_SLOTS - 1);
    *created = 0;
    for (uint32_t i = 0; i < PK_SLOTS; i++, h = (h + 1) & (PK_SLOTS - 1)) {
        PKey *k = &g_keys[h];
        if (k->ent == ent && k->pin == pin && k->total)
            return k;
        if (!k->total) {
            if (g_nkeys >= PK_SLOTS * 3 / 4)
                return NULL;
            k->ent = ent;
            k->pin = pin;
            g_nkeys++;
            *created = 1;
            return k;
        }
    }
    return NULL;
}

static int vt_first(uint32_t vt)
{
    for (uint32_t i = 0; i < g_nvts; i++)
        if (g_vts[i] == vt)
            return 0;
    if (g_nvts < VT_SLOTS)
        g_vts[g_nvts++] = vt;
    return 1;
}

/* Objeto completo de la entidad a partir del ctx (handle -> +0xC), si está cerca de this. */
static uint32_t outer_of(uint32_t ent, void *ctx, uint32_t *h_out)
{
    uint32_t h = 0, o = 0;
    *h_out = 0;
    if (!ctx || !mem_u32((uintptr_t)ctx, &h) || !h || !mem_u32(h + 0xC, &o) || !o)
        return 0;
    *h_out = h;
    int32_t rel = (int32_t)(o - ent);
    return (rel >= -0x40 && rel <= 0) ? o : 0;
}

/* Identidad estable de una instancia: hash de los ShortGuids de la entidad y de cada composite
 * padre (objeto completo +0x08 = handle al CompositeInstance padre; corrida 16: 60 de 124 volcados
 * apuntan a un handle de CompositeInstance conocido). */
static uint32_t path_id(uint32_t outer, uint32_t *depth)
{
    uint32_t x = 0x811C9DC5u, d = 0;
    for (; outer && d < 32; d++) {
        uint32_t eid = 0, h = 0, up = 0;
        if (!mem_u32(outer + 0xC, &eid))
            break;
        x = hash2(x, eid);
        if (!mem_u32(outer + 0x8, &h) || !h || !mem_u32(h + 0xC, &up) || up == outer)
            break;
        outer = up;
    }
    *depth = d + (outer ? 1 : 0);
    return x;
}

static int rule_match(uint32_t type, uint32_t pin)
{
    for (int i = 0; i < g_nrules; i++)
        if (g_rules[i].type == type && (g_rules[i].any || g_rules[i].pin == pin))
            return 1;
    return 0;
}

/* Primer mensaje de cada (tipo, pin): para aprender el layout (fase 6, paso 4). Bajo g_cs. */
static int msg_first(uint32_t type, uint32_t pin)
{
    if (g_nmd >= MD_SLOTS)
        return 0;
    for (uint32_t i = 0; i < g_nmd; i++)
        if (g_md_type[i] == type && g_md_pin[i] == pin)
            return 0;
    g_md_type[g_nmd] = type;
    g_md_pin[g_nmd] = pin;
    g_nmd++;
    return 1;
}

/* Devuelve 1 si el disparo se bloquea (cliente con AIMP_SYNC_PINS). */
static int note(uint32_t ent, void *ctx, const uint32_t *pinp, void *msg, double delay, uint32_t flag, uint32_t ret,
                int injected)
{
    uint32_t pin = 0, vt = 0, idx = 0;
    if (!pinp || !mem_u32((uintptr_t)pinp, &pin))
        return 0;
    mem_u32(ent, &vt);
    mem_u32(ent + ENT_INDEX, &idx);
    uint32_t h = 0, outer = outer_of(ent, ctx, &h), eid = 0, type = 0, mobj = 0, pid = 0, depth = 0;
    if (outer) {
        mem_u32(outer + OUT_ID, &eid);
        mem_u32(outer + OUT_TYPE, &type);
        pid = path_id(outer, &depth);
    }
    if (msg)
        mem_u32((uintptr_t)msg, &mobj);
    uint32_t mflags = 0;
    if (mobj)
        mem_u32(mobj + 0x20, &mflags);
    int block = 0, send = 0;
    if (g_sync && outer && !injected && rule_match(type, pin)) {
        if (g_sync == 2)
            block = 1;
        else
            send = 1;
    }
    uint32_t frame = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_FRAME_COUNTER);
    int other = GetCurrentThreadId() != g_aimp_main_tid;
    EnterCriticalSection(&g_cs);
    g_fires++;
    g_fires_period++;
    if (other)
        g_other_thread++;
    int created;
    PKey *k = lookup(ent, pin, &created);
    if (!k) {
        g_full++;
        LeaveCriticalSection(&g_cs);
        goto sync;
    }
    k->total++;
    k->period++;
    if (created) {
        k->vt = vt;
        k->idx = idx & 0xFFFFFF;
        k->outer = outer;
        k->eid = eid;
        k->type = type;
        k->pid = pid;
        k->depth = depth;
        k->h = h;
        k->mobj = mobj;
        k->t_first = GetTickCount();
    }
    /* Plantilla (handle y mensaje) del primer disparo natural: los inyectados llevan copias del mod. */
    int snapped = 0;
    if (!k->have_snap && !injected && h && mobj) {
        k->h = h;
        k->mobj = mobj;
        k->have_snap = snapped = mem_read(h, k->hsnap, H_SIZE) && mem_read(mobj, k->msnap, MSG_SIZE);
    }
    int win = g_win_until && (int32_t)(GetTickCount() - g_win_until) < 0 && g_win_lines < 600;
    int detail = ((int)k->total <= g_per_key && g_lines < (uint32_t)g_max_lines) || win;
    if (win)
        g_win_lines++;
    int dumpvt = created && g_per_key > 0 && vt_first(vt);
    int dumpmsg = snapped && g_per_key > 0 && outer && msg_first(type, pin);
    uint32_t total = k->total;
    if (detail)
        g_lines++;
    LeaveCriticalSection(&g_cs);
    if (detail || injected) {
        /* Lecturas y log fuera de la sección crítica (el log tiene su propio lock). */
        uint32_t tgt = 0, links = count_links(ent, pin, &tgt), mref = 0;
        if (mobj)
            mem_u32(mobj + 0x24, &mref);
        k->links = links;
        union {
            double d;
            uint32_t u[2];
        } dl = { delay };
        log_printf("PIN f%u ent 0x%08x vt %x idx %u fl %02x | id %08x tipo %08x pid %08x d%u | pin %08x n%u | links %u tgt 0x%08x | demora %08x%08x flag %x | msg 0x%08x m 0x%08x mfl %08x mref %u | ctx 0x%08x h 0x%08x o %+d | desde %x%s%s%s%s",
                   frame, ent, rva(vt), idx & 0xFFFFFF, idx >> 24, eid, type, pid, depth, pin, total, links, tgt, dl.u[1], dl.u[0],
                   flag, (unsigned)(uintptr_t)msg, mobj, mflags, mref, (unsigned)(uintptr_t)ctx, h,
                   outer ? (int32_t)(outer - ent) : 0, rva(ret), other ? " (otro thread)" : "",
                   !other && t_depth > 1 ? " (anidado)" : "", injected ? " (INYECTADO)" : win ? " (ventana)" : "",
                   block ? " (BLOQUEADO)" : send ? " (al otro)" : "");
    }
    if (dumpvt)
        log_dump32("PIN primera instancia de la vtable (desde ent-0x20)", ent - 0x20, 0x80);
    if (dumpmsg) {
        char t[96];
        aimp_snprintf(t, sizeof t, "PIN mensaje del primer %08x.%08x (mfl %08x)", type, pin, mflags);
        log_dump32(t, mobj, MSG_SIZE);
    }
sync:
    if (send) {
        EnterCriticalSection(&g_out_cs);
        if (g_nout < OUT_MAX) {
            PinEvt *o = &g_out[g_nout++];
            o->pid = pid;
            o->pin = pin;
            o->type = type;
            o->eid = eid;
            o->mflags = mflags;
        } else
            g_out_drop++;
        LeaveCriticalSection(&g_out_cs);
    }
    if (block) {
        g_n_blocked++;
        if (g_sync_lines < 400) {
            g_sync_lines++;
            log_printf("PINSYNC bloqueado (lo decide el host): tipo %08x id %08x pid %08x pin %08x mfl %08x%s", type, eid, pid, pin,
                       mflags, other ? " (otro thread)" : "");
        }
    }
    return block;
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_fire(void *self, void *edx, void *ctx,
                                                                                const uint32_t *pin, void *msg,
                                                                                double delay, uint32_t flag)
{
    uint32_t ret = (uint32_t)(uintptr_t)__builtin_return_address(0);
    int main_thread = GetCurrentThreadId() == g_aimp_main_tid;
    /* Solo el disparo de arriba de una inyección cuenta como inyectado (el anidado se trata como natural). */
    int injected = 0;
    if (g_inj_ent && main_thread && (uint32_t)(uintptr_t)self == g_inj_ent && pin && *pin == g_inj_pin) {
        injected = 1;
        g_inj_ent = 0;
    }
    if (main_thread && ++t_depth > g_depth_max)
        g_depth_max = t_depth;
    if (!note((uint32_t)(uintptr_t)self, ctx, pin, msg, delay, flag, ret, injected))
        g_orig(self, edx, ctx, pin, msg, delay, flag);
    if (main_thread)
        t_depth--;
}

static uint32_t parse_hex(const char **pp);

static void parse_rules(const char *s)
{
    g_nrules = 0;
    for (const char *p = s; *p && g_nrules < RULES_MAX;) {
        while (*p == ' ' || *p == ',')
            p++;
        if (!*p)
            break;
        Rule r = { 0, 0, 0 };
        r.type = parse_hex(&p);
        if (*p == ':') {
            p++;
            if (*p == '*') {
                r.any = 1;
                p++;
            } else
                r.pin = parse_hex(&p);
        } else
            r.any = 1;
        g_rules[g_nrules++] = r;
        while (*p && *p != ',')
            p++;
    }
}

/* Reglas por defecto: las raíces de zona de la corrida 19.
 *   PlayerTriggerBox (2f3efad3): on_entered (af66ad73), on_exited (b357f479)
 *   TriggerVolumeFilter (f86133d4): on_event_entered (98678b2b), on_event_exited (4dace76a), triggered (0ff02876) */
static const char k_default_rules[] = "2f3efad3:af66ad73,2f3efad3:b357f479,f86133d4:98678b2b,f86133d4:4dace76a,f86133d4:0ff02876";

int pins_install_at(void *target, AimpNet *net)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart;
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_out_cs);
    int trace = aimp_env_int(L"AIMP_TRACE_PINS", 0);
    int sync = aimp_env_int(L"AIMP_SYNC_PINS", 0);
    /* Solo sincronización: sin líneas de detalle (sí los bloqueos, envíos y aplicaciones). */
    g_per_key = aimp_env_int(L"AIMP_TRACE_PINS_N", trace || !sync ? 2 : 0);
    g_max_lines = aimp_env_int(L"AIMP_TRACE_PINS_MAX", 20000);
    int ms = aimp_env_int(L"AIMP_SYNC_PENDING_MS", 60000);
    g_pend_ms = ms < 0 ? 0 : (uint32_t)ms;
    int have_file = aimp_env_str(L"AIMP_PINS_FILE", g_file, MAX_PATH);
    if (!hook_install(&g_hk, "disparar pin (FUN_008be0d0)", target, AIMP_FIRE_PIN_PROLOGUE, sizeof AIMP_FIRE_PIN_PROLOGUE,
                      AIMP_FIRE_PIN_STEAL, (void *)det_fire, (void **)&g_orig))
        return 0;
    g_on = 1;
    char f8[MAX_PATH * 3];
    log_printf("AIMP_TRACE_PINS (fase 6): pines de salida de CATHODE, %d líneas por (entidad, pin), tope %d%s%s", g_per_key,
               g_max_lines, have_file ? "; comandos FIRE/INDEX en " : "", have_file ? aimp_utf8(f8, sizeof f8, g_file) : "");
    g_net = net;
    if (sync) {
        wchar_t wl[512];
        char l8[1024];
        if (aimp_env_str(L"AIMP_SYNC_PINS_LIST", wl, 512))
            aimp_utf8(l8, sizeof l8, wl);
        else
            aimp_snprintf(l8, sizeof l8, "%s", k_default_rules);
        parse_rules(l8);
        if (!net || !net->mode)
            log_printf("AIMP_SYNC_PINS=1 sin red (AIMP_NET): no se sincroniza nada");
        else {
            g_sync = net->mode == 1 ? 1 : 2;
            log_printf("AIMP_SYNC_PINS=1 (fase 6): %s; %d reglas [%s]; pendientes hasta %u ms",
                       g_sync == 1 ? "host: manda los disparos de las reglas" : "cliente: bloquea los suyos y aplica los del host",
                       g_nrules, l8, g_pend_ms);
        }
    }
    return 1;
}

int pins_install(AimpNet *net)
{
    if (!aimp_env_int(L"AIMP_TRACE_PINS", 0) && !aimp_env_int(L"AIMP_SYNC_PINS", 0))
        return 0;
    return pins_install_at((void *)(g_aimp_exe + AIMP_RVA_FIRE_PIN), net);
}

/* Los 12 (entidad, pin) con más disparos en el período (o en total). */
static void top(int period, const char *title)
{
    enum { TOPN = 12 };
    PKey best[TOPN];
    int nb = 0;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < PK_SLOTS; i++) {
        PKey *k = &g_keys[i];
        uint32_t v = period ? k->period : k->total;
        if (!k->total || !v)
            continue;
        int pos = nb;
        while (pos > 0 && (period ? best[pos - 1].period : best[pos - 1].total) < v)
            pos--;
        if (pos >= TOPN)
            continue;
        if (nb < TOPN)
            nb++;
        for (int j = nb - 1; j > pos; j--)
            best[j] = best[j - 1];
        best[pos] = *k;
    }
    uint32_t fires = period ? g_fires_period : g_fires, nkeys = g_nkeys;
    if (period) {
        for (uint32_t i = 0; i < PK_SLOTS; i++)
            g_keys[i].period = 0;
        g_fires_period = 0;
    }
    LeaveCriticalSection(&g_cs);
    if (!fires && period)
        return;
    log_printf("PIN %s: %u disparos, %u (entidad, pin) distintos, %u líneas detalladas, %u sin lugar, %u en otro thread, anidamiento máx. %ld, nivel %u",
               title, fires, nkeys, g_lines, g_full, g_other_thread, (long)g_depth_max, g_level);
    for (int i = 0; i < nb; i++)
        log_printf("PIN   %6u x  ent 0x%08x vt %x idx %u id %08x tipo %08x pid %08x pin %08x links %u",
                   period ? best[i].period : best[i].total, best[i].ent, rva(best[i].vt), best[i].idx, best[i].eid,
                   best[i].type, best[i].pid, best[i].pin, best[i].links);
}

void pins_report_period(void)
{
    if (g_on)
        top(1, "período");
}

void pins_report_totals(const char *why)
{
    if (!g_on)
        return;
    char t[64];
    aimp_snprintf(t, sizeof t, "totales (%s)", why);
    top(0, t);
    if (g_sync)
        log_printf("PINSYNC totales (%s): %u mandados, %u bloqueados, %u aplicados, %u vencidos, %d pendientes; índice: %u armados, %u entidades",
                   why, g_n_sent, g_n_blocked, g_n_applied, g_n_expired, g_npend, g_ix_builds, g_nix);
}

/* ---- Reinyección ---- */

static uint32_t parse_hex(const char **pp)
{
    const char *p = *pp;
    uint32_t v = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p[0] == '0' && (p[1] | 0x20) == 'x')
        p += 2;
    for (;; p++) {
        char c = *p;
        int d = c >= '0' && c <= '9' ? c - '0' : (c | 0x20) >= 'a' && (c | 0x20) <= 'f' ? (c | 0x20) - 'a' + 10 : -1;
        if (d < 0)
            break;
        v = v << 4 | (uint32_t)d;
    }
    *pp = p;
    return v;
}

/* ---- Índice pid -> entidad ---- */

static int in_image(uint32_t p, uint32_t n) { return p >= (uint32_t)g_aimp_exe && p + n <= (uint32_t)g_aimp_exe + AIMP_EXPECT_SIZEOFIMAGE; }

/* ¿vt es una vtable de EntityInterface (FUN_008be0d0 en +0x394)? Con caché. */
static int is_iface_vt(uint32_t vt)
{
    if (!vt || !in_image(vt, ENT_VT_FIRE + 4))
        return 0;
    uint32_t hsh = (vt * 0x9E3779B1u) >> 19; /* 13 bits */
    for (uint32_t i = 0; i < VC_SLOTS; i++, hsh = (hsh + 1) & (VC_SLOTS - 1)) {
        if (g_vc_key[hsh] == vt)
            return g_vc_val[hsh] == 1;
        if (!g_vc_key[hsh]) {
            uint32_t f = 0;
            int yes = mem_u32(vt + ENT_VT_FIRE, &f) && f == (uint32_t)g_aimp_exe + AIMP_RVA_FIRE_PIN;
            g_vc_key[hsh] = vt;
            g_vc_val[hsh] = yes ? 1 : 2;
            return yes;
        }
    }
    uint32_t f = 0;
    return mem_u32(vt + ENT_VT_FIRE, &f) && f == (uint32_t)g_aimp_exe + AIMP_RVA_FIRE_PIN;
}

/* Subobjeto EntityInterface del objeto completo: primer puntero (desde +4) a una vtable de entidad. */
static uint32_t iface_of(uint32_t outer, const uint32_t *w, int nw)
{
    for (int i = 1; i < nw; i++)
        if (is_iface_vt(w[i]))
            return outer + (uint32_t)i * 4;
    return is_iface_vt(w[0]) ? outer : 0;
}

static IxEnt *ix_find(uint32_t pid)
{
    uint32_t hsh = hash2(pid, 0x1D) & (IX_SLOTS - 1);
    for (uint32_t i = 0; i < IX_SLOTS; i++, hsh = (hsh + 1) & (IX_SLOTS - 1)) {
        if (!g_ix[hsh].outer)
            return NULL;
        if (g_ix[hsh].pid == pid)
            return &g_ix[hsh];
    }
    return NULL;
}

static void ix_add(const IxEnt *e)
{
    if (g_nix >= IX_SLOTS * 3 / 4)
        return;
    uint32_t hsh = hash2(e->pid, 0x1D) & (IX_SLOTS - 1);
    for (uint32_t i = 0; i < IX_SLOTS; i++, hsh = (hsh + 1) & (IX_SLOTS - 1)) {
        IxEnt *s = &g_ix[hsh];
        if (!s->outer) {
            *s = *e;
            g_nix++;
            return;
        }
        if (s->pid == e->pid) {
            if (s->outer != e->outer)
                g_ix_coll++;
            return;
        }
    }
}

/* pid de un nodo con su pila de ancestros (anc[0] = raíz): igual que path_id, que va de la entidad
 * hacia arriba. */
static uint32_t pid_of(uint32_t eid, const uint32_t *anc, uint32_t nanc)
{
    uint32_t x = hash2(0x811C9DC5u, eid);
    for (uint32_t j = nanc; j-- > 0;)
        x = hash2(x, anc[j]);
    return x;
}

typedef struct {
    uint32_t comp, items, n, i;
} Frame;

/* Agrega un nodo (objeto completo `o`, handle `h`) con sus ancestros. Devuelve 1 si es un composite. */
static int visit(uint32_t o, uint32_t h, const uint32_t *anc, uint32_t nanc, uint32_t *eid_out, uint32_t *bad_parent,
                 uint32_t parent_comp)
{
    uint32_t w[0x44 / 4];
    if (!mem_read(o, w, sizeof w))
        return 0;
    IxEnt e;
    e.outer = o;
    e.h = h;
    e.eid = w[OUT_ID / 4];
    e.type = w[OUT_TYPE / 4];
    e.pid = pid_of(e.eid, anc, nanc);
    e.depth = nanc + 1;
    e.ent = iface_of(o, w, (int)(sizeof w / 4));
    e.vt = 0;
    if (e.ent)
        mem_u32(e.ent, &e.vt);
    if (parent_comp) {
        uint32_t ph = w[OUT_PARENT / 4], po = 0;
        if (!ph || !mem_u32(ph + HANDLE_OBJ, &po) || po != parent_comp)
            (*bad_parent)++;
    }
    ix_add(&e);
    *eid_out = e.eid;
    return w[0] == (uint32_t)g_aimp_exe + AIMP_RVA_VT_COMPOSITE;
}

static uint32_t walk(uint32_t root, uint32_t *ncomp, uint32_t *maxd, uint32_t *bad_parent)
{
    Frame st[MAX_DEPTH];
    uint32_t anc[MAX_DEPTH], nodes = 0, eid = 0;
    if (!visit(root, 0, anc, 0, &eid, bad_parent, 0))
        return 1;
    int sp = 0;
    st[0].comp = root;
    st[0].i = 0;
    anc[0] = eid;
    for (;;) {
        Frame *f = &st[sp];
        if (f->i == 0) { /* entrar al composite: leer su arreglo de hijos */
            uint32_t arr = 0;
            f->n = 0;
            f->items = 0;
            (*ncomp)++;
            if (mem_u32(f->comp + COMP_CHILDREN, &arr) && arr && mem_u32(arr + 8, &f->n) && mem_u32(arr + 0xC, &f->items) &&
                f->n > 0x100000)
                f->n = 0;
        }
        if (f->i >= f->n || !f->items) {
            if (sp == 0)
                break;
            sp--;
            continue;
        }
        uint32_t hh = 0, o = 0;
        uint32_t i = f->i++;
        nodes++;
        if (!mem_u32(f->items + i * 4, &hh) || !hh || !mem_u32(hh + HANDLE_OBJ, &o) || !o)
            continue;
        if (visit(o, hh, anc, (uint32_t)sp + 1, &eid, bad_parent, f->comp) && sp + 1 < MAX_DEPTH) {
            sp++;
            st[sp].comp = o;
            st[sp].i = 0;
            anc[sp] = eid;
            if ((uint32_t)sp + 1 > *maxd)
                *maxd = (uint32_t)sp + 1;
        }
    }
    return nodes + 1;
}

uint32_t pins_index_build(const char *why)
{
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    /* Raíces: subir por el padre desde las entidades que dispararon. */
    enum { ROOTS = 16 };
    uint32_t roots[ROOTS], nroots = 0, samples = 0;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < PK_SLOTS && samples < 4000; i++) {
        PKey *k = &g_keys[i];
        if (!k->total || !k->outer)
            continue;
        samples++;
        uint32_t o = k->outer, up = 0, ph = 0, d = 0;
        while (d++ < MAX_DEPTH && mem_u32(o + OUT_PARENT, &ph) && ph && mem_u32(ph + HANDLE_OBJ, &up) && up && up != o)
            o = up;
        int seen = 0;
        for (uint32_t r = 0; r < nroots; r++)
            seen |= roots[r] == o;
        if (!seen && nroots < ROOTS)
            roots[nroots++] = o;
    }
    LeaveCriticalSection(&g_cs);
    aimp_memset(g_ix, 0, sizeof g_ix);
    g_nix = 0;
    g_ix_coll = 0;
    uint32_t ncomp = 0, maxd = 1, bad_parent = 0, nodes = 0;
    for (uint32_t r = 0; r < nroots; r++)
        nodes += walk(roots[r], &ncomp, &maxd, &bad_parent);
    QueryPerformanceCounter(&t1);
    g_ix_ms = g_freq ? (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000 / g_freq) : 0;
    g_ix_dirty = 0;
    g_ix_builds++;
    g_ix_at = GetTickCount();
    /* Verificación contra lo visto: mismo objeto y mismo subobjeto que en el disparo. */
    uint32_t ok = 0, bad_ent = 0, missing = 0, shown = 0;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < PK_SLOTS; i++) {
        PKey *k = &g_keys[i];
        if (!k->total || !k->outer)
            continue;
        IxEnt *e = ix_find(k->pid);
        if (!e || e->outer != k->outer) {
            missing++;
            if (shown++ < 8) {
                uint32_t ph = 0, po = 0, pvt = 0;
                mem_u32(k->outer + OUT_PARENT, &ph);
                if (ph)
                    mem_u32(ph + HANDLE_OBJ, &po);
                if (po)
                    mem_u32(po, &pvt);
                log_printf("PINIX   falta: pid %08x id %08x tipo %08x d%u ent 0x%08x; padre 0x%08x vt %x%s", k->pid, k->eid, k->type,
                           k->depth, k->ent, po, rva(pvt), e ? " (el pid apunta a otro objeto)" : "");
            }
        } else if (e->ent != k->ent)
            bad_ent++;
        else
            ok++;
    }
    LeaveCriticalSection(&g_cs);
    log_printf("PINIX índice (%s, nivel %u): %u entidades (%u nodos, %u composites, %u raíces, prof. máx. %u) en %u ms; %u colisiones de pid, %u con otro padre; contra lo visto: %u ok, %u con otro subobjeto, %u faltan",
               why, g_level, g_nix, nodes, ncomp, nroots, maxd, g_ix_ms, g_ix_coll, bad_parent, ok, bad_ent, missing);
    return g_nix;
}

/* Plantilla de mensaje: mismo pid y pin > mismo tipo y pin > mismo pin > mismo tipo. Bajo g_cs. */
static int find_template(uint32_t pid, uint32_t type, uint32_t pin, uint32_t *msnap, const char **from)
{
    static const char *names[] = { "mismo pid y pin", "mismo tipo y pin", "mismo pin", "mismo tipo" };
    int best = 99;
    for (uint32_t i = 0; i < PK_SLOTS && best > 0; i++) {
        PKey *k = &g_keys[i];
        if (!k->total || !k->have_snap)
            continue;
        int lv = k->pid == pid && k->pin == pin ? 0 : k->type == type && k->pin == pin ? 1 : k->pin == pin ? 2 : k->type == type ? 3 : 99;
        if (lv < best) {
            best = lv;
            aimp_memcpy(msnap, k->msnap, MSG_SIZE);
        }
    }
    if (best == 99)
        return 0;
    *from = names[best];
    return 1;
}

/* Dispara `pin` sobre la entidad (ent, objeto completo outer, handle de 0x10 bytes hsnap) con el mensaje msnap. */
static void inject(uint32_t ent, uint32_t outer, const uint32_t *hsnap, const uint32_t *msnap, uint32_t pin)
{
    /* Copias propias con refcount alto: el motor suma y resta, pero nunca llega a 0 (corrida 17). */
    uint32_t *hc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    uint32_t *mc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    if (!hc || !mc)
        return;
    aimp_memcpy(hc, hsnap, H_SIZE);
    hc[1] = 0x10000000;
    hc[3] = outer;
    aimp_memcpy(mc, msnap, MSG_SIZE);
    mc[9] = 0x10000000;
    void *ctx_slot = hc, *msg_slot = mc;
    uint32_t pin_v = pin;
    g_win_lines = 0;
    g_win_until = GetTickCount() + 1500;
    g_inj_pin = pin;
    g_inj_ent = ent;
    g_injecting = 1;
    det_fire((void *)(uintptr_t)ent, NULL, &ctx_slot, &pin_v, &msg_slot, 0.0, 0);
    g_injecting = 0;
    g_inj_ent = 0;
    g_injected++;
}

/* 1 = disparó; 0 = no se puede (todavía). why: motivo, para el log. */
static int fire_ex(uint32_t id, uint32_t pin, uint32_t mflags, const char **why)
{
    PKey src;
    int found = 0;
    uint32_t msnap[MSG_SIZE / 4];
    const char *tfrom = "mensaje del primer disparo";
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < PK_SLOTS && found < 2; i++) {
        PKey *k = &g_keys[i];
        if (!k->total || (k->pid != id && k->eid != id) || !k->have_snap)
            continue;
        if (k->pin == pin) {
            src = *k;
            found = 2;
        } else if (!found) {
            src = *k;
            found = 1;
        }
    }
    LeaveCriticalSection(&g_cs);
    uint32_t ent, outer, vt, hsnap[H_SIZE / 4], type, pid;
    const char *via;
    if (found) {
        /* La entidad tiene que seguir viva: su handle original sigue apuntando al mismo objeto. */
        uint32_t o = 0, v = 0;
        if (!mem_u32(src.h + HANDLE_OBJ, &o) || o != src.outer || !mem_u32(src.ent, &v) || v != src.vt) {
            *why = "la entidad ya no está";
            return 0;
        }
        ent = src.ent;
        outer = src.outer;
        vt = src.vt;
        type = src.type;
        pid = src.pid;
        aimp_memcpy(hsnap, src.hsnap, H_SIZE);
        aimp_memcpy(msnap, src.msnap, MSG_SIZE);
        if (found == 1)
            tfrom = "mensaje de otro pin de la misma entidad";
        via = "visto";
    } else {
        IxEnt *e = ix_find(id);
        /* Rearmar si el índice quedó viejo; si solo falta el pid, como mucho cada 2 s. */
        if (g_on && (g_ix_dirty || (!e && GetTickCount() - g_ix_at > 2000))) {
            pins_index_build("FIRE");
            e = ix_find(id);
        }
        if (!e) {
            *why = "el pid no está en el índice";
            return 0;
        }
        uint32_t o = 0, v = 0;
        if (!e->ent || !e->h || !mem_u32(e->h + HANDLE_OBJ, &o) || o != e->outer || !mem_u32(e->ent, &v) || v != e->vt ||
            !mem_read(e->h, hsnap, H_SIZE)) {
            g_ix_dirty = 1;
            *why = e->ent ? "la entidad del índice ya no está" : "sin subobjeto EntityInterface";
            return 0;
        }
        ent = e->ent;
        outer = e->outer;
        vt = e->vt;
        type = e->type;
        pid = e->pid;
        EnterCriticalSection(&g_cs);
        int t = find_template(pid, type, pin, msnap, &tfrom);
        LeaveCriticalSection(&g_cs);
        if (!t) {
            /* Sin ningún mensaje visto: uno vacío con la clase pedida. */
            aimp_memset(msnap, 0, sizeof msnap);
            msnap[8] = mflags;
            tfrom = "mensaje vacío";
        }
        via = "índice";
    }
    uint32_t mfl0 = msnap[8];
    if (mflags)
        msnap[8] = (msnap[8] & ~0x0F000000u) | (mflags & 0x0F000000u);
    log_printf("PIN FIRE %08x %08x: ent 0x%08x (vt %x, tipo %08x, pid %08x, por %s), %s, mfl %08x -> %08x", id, pin, ent, rva(vt), type,
               pid, via, tfrom, mfl0, msnap[8]);
    inject(ent, outer, hsnap, msnap, pin);
    log_printf("PIN FIRE %08x %08x: hecho (inyecciones %u)", id, pin, g_injected);
    *why = "";
    return 1;
}

int pins_fire(uint32_t id, uint32_t pin, uint32_t mflags)
{
    const char *why = "";
    if (fire_ex(id, pin, mflags, &why))
        return 1;
    log_printf("PIN FIRE %08x %08x: no se disparó (%s)", id, pin, why);
    return 0;
}

static int word_is(const char *p, const char *w)
{
    for (; *w; p++, w++)
        if ((*p | 0x20) != *w)
            return 0;
    return *p == ' ' || *p == '\r' || *p == '\n' || *p == 0;
}

static void check_file(void)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!g_file[0] || !GetFileAttributesExW(g_file, GetFileExInfoStandard, &fa))
        return;
    if (fa.ftLastWriteTime.dwLowDateTime == g_file_time.dwLowDateTime &&
        fa.ftLastWriteTime.dwHighDateTime == g_file_time.dwHighDateTime)
        return;
    int first = !g_file_time.dwLowDateTime && !g_file_time.dwHighDateTime;
    g_file_time = fa.ftLastWriteTime;
    if (first)
        return; /* lo que había antes de arrancar no se ejecuta */
    HANDLE h = CreateFileW(g_file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char buf[2048];
    DWORD got = 0;
    ReadFile(h, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    for (const char *p = buf; *p;) {
        while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
            p++;
        if (word_is(p, "fire")) {
            p += 4;
            uint32_t id = parse_hex(&p), pin = parse_hex(&p), mfl = parse_hex(&p);
            pins_fire(id, pin, mfl);
        } else if (word_is(p, "index")) {
            p += 5;
            pins_index_build("comando INDEX");
        }
        while (*p && *p != '\n')
            p++;
    }
}

/* ---- Sincronización ---- */

void pins_remote_event(const AimpEvt *e)
{
    if (!g_sync)
        return;
    if (g_sync != 2 || e->size != sizeof(PinEvt)) {
        log_printf("PINSYNC: evento de pin %u ignorado (%s, %u bytes)", e->seq, g_sync == 1 ? "el host no aplica pines" : "tamaño",
                   e->size);
        return;
    }
    if (g_npend == PEND_MAX) {
        log_printf("PINSYNC: demasiados pines pendientes: se descarta el evento %u", g_pend[0].seq);
        for (int k = 1; k < g_npend; k++)
            g_pend[k - 1] = g_pend[k];
        g_npend--;
    }
    PinPend *q = &g_pend[g_npend++];
    aimp_memcpy(&q->p, e->data, sizeof q->p);
    q->level = e->level;
    q->seq = e->seq;
    q->at = GetTickCount();
    q->told = 0;
}

static void sync_frame_pins(uint32_t level)
{
    if (g_sync == 1 && g_net) {
        PinEvt tmp[OUT_MAX];
        uint32_t n, drop;
        EnterCriticalSection(&g_out_cs);
        n = g_nout;
        drop = g_out_drop;
        aimp_memcpy(tmp, g_out, n * sizeof tmp[0]);
        g_out_drop = 0;
        LeaveCriticalSection(&g_out_cs);
        uint32_t sent = 0;
        for (; sent < n; sent++) {
            if (!net_event(g_net, AIMP_EVT_PIN, level, &tmp[sent], sizeof tmp[sent]))
                break; /* cola llena o sin par: lo que falta sale en el próximo frame */
            g_n_sent++;
            if (g_sync_lines < 400) {
                g_sync_lines++;
                log_printf("PINSYNC al cliente: tipo %08x id %08x pid %08x pin %08x mfl %08x (nivel %u)", tmp[sent].type, tmp[sent].eid,
                           tmp[sent].pid, tmp[sent].pin, tmp[sent].mflags, level);
            }
        }
        EnterCriticalSection(&g_out_cs);
        /* Sacar los mandados (pudieron llegar más mientras tanto). */
        for (uint32_t i = sent; i < g_nout; i++)
            g_out[i - sent] = g_out[i];
        g_nout -= sent;
        LeaveCriticalSection(&g_out_cs);
        if (drop)
            log_printf("PINSYNC: %u disparos no se mandaron (cola llena)", drop);
    }
    if (g_sync == 2 && g_npend) {
        int w = 0, r = 0;
        uint32_t now = GetTickCount();
        for (; r < g_npend; r++) {
            PinPend *q = &g_pend[r];
            const char *why = "otro nivel";
            int done = q->level == level && fire_ex(q->p.pid, q->p.pin, q->p.mflags, &why);
            if (done) {
                g_n_applied++;
                log_printf("PINSYNC aplicado (evento %u): tipo %08x id %08x pid %08x pin %08x", q->seq, q->p.type, q->p.eid, q->p.pid,
                           q->p.pin);
                continue;
            }
            if (now - q->at > g_pend_ms) {
                g_n_expired++;
                log_printf("PINSYNC vencido sin aplicar (evento %u, pid %08x pin %08x, nivel %u, acá %u): %s", q->seq, q->p.pid, q->p.pin,
                           q->level, level, why);
                continue;
            }
            if (!q->told) {
                q->told = 1;
                log_printf("PINSYNC pendiente (evento %u, pid %08x pin %08x, nivel %u, acá %u): %s", q->seq, q->p.pid, q->p.pin, q->level,
                           level, why);
            }
            g_pend[w++] = *q;
            if (q->level == level) {
                r++;
                break; /* en orden: si este no se pudo, los siguientes esperan */
            }
        }
        for (; r < g_npend; r++)
            g_pend[w++] = g_pend[r];
        g_npend = w;
    }
}

void pins_frame(int64_t now, uint32_t level)
{
    if (!g_on)
        return;
    if (level != g_level) {
        /* El nivel nuevo empieza a disparar ~1 s antes de que la sonda vea el cambio (corrida 15):
         * se conservan las claves de los últimos 3 s y se descarta el resto. */
        if (g_nkeys)
            pins_report_totals("cambio de nivel");
        uint32_t now_ms = GetTickCount(), kept = 0;
        PKey *tmp = HeapAlloc(GetProcessHeap(), 0, sizeof(PKey) * 8192);
        EnterCriticalSection(&g_cs);
        for (uint32_t i = 0; tmp && i < PK_SLOTS; i++)
            if (g_keys[i].total && now_ms - g_keys[i].t_first <= 3000 && kept < 8192)
                tmp[kept++] = g_keys[i];
        aimp_memset(g_keys, 0, sizeof g_keys);
        g_nkeys = 0;
        g_full = 0;
        for (uint32_t i = 0; i < kept; i++) {
            int created;
            PKey *k = lookup(tmp[i].ent, tmp[i].pin, &created);
            if (k)
                *k = tmp[i];
        }
        LeaveCriticalSection(&g_cs);
        if (tmp)
            HeapFree(GetProcessHeap(), 0, tmp);
        log_printf("PIN nivel %u -> %u: tabla de (entidad, pin) renovada (%u claves de los últimos 3 s conservadas)", g_level,
                   level, kept);
        g_level = level;
        g_level_at = now_ms;
        g_ix_dirty = 1;
        g_ix_auto_done = 0;
    }
    /* Con traza: un índice de verificación 10 s después de entrar al nivel (la carga ya pasó). */
    if (!g_ix_auto_done && g_per_key > 0 && g_level && GetTickCount() - g_level_at > 10000) {
        g_ix_auto_done = 1;
        pins_index_build("automático, 10 s después del cambio de nivel");
    }
    if (g_file[0] && now >= g_next_file_check) {
        g_next_file_check = now + g_freq / 4;
        check_file();
    }
    if (g_sync)
        sync_frame_pins(level);
}
