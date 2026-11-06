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

int pins_enabled(void) { return g_on; }

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

static void note(uint32_t ent, void *ctx, const uint32_t *pinp, void *msg, double delay, uint32_t flag, uint32_t ret)
{
    uint32_t pin = 0, vt = 0, idx = 0;
    if (!pinp || !mem_u32((uintptr_t)pinp, &pin))
        return;
    mem_u32(ent, &vt);
    mem_u32(ent + ENT_INDEX, &idx);
    uint32_t h = 0, outer = outer_of(ent, ctx, &h), eid = 0, type = 0, mobj = 0, pid = 0, depth = 0;
    if (outer) {
        mem_u32(outer + 0xC, &eid);
        mem_u32(outer + 0x10, &type);
        pid = path_id(outer, &depth);
    }
    if (msg)
        mem_u32((uintptr_t)msg, &mobj);
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
        return;
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
        k->have_snap = h && mobj && mem_read(h, k->hsnap, H_SIZE) && mem_read(mobj, k->msnap, MSG_SIZE);
    }
    int win = g_win_until && (int32_t)(GetTickCount() - g_win_until) < 0 && g_win_lines < 600;
    int detail = ((int)k->total <= g_per_key && g_lines < (uint32_t)g_max_lines) || win;
    if (win)
        g_win_lines++;
    int dumpvt = created && vt_first(vt);
    uint32_t total = k->total;
    if (detail)
        g_lines++;
    LeaveCriticalSection(&g_cs);
    if (!detail && !g_injecting)
        return;
    /* Lecturas y log fuera de la sección crítica (el log tiene su propio lock). */
    uint32_t tgt = 0, links = count_links(ent, pin, &tgt), mflags = 0, mref = 0;
    if (mobj) {
        mem_u32(mobj + 0x20, &mflags);
        mem_u32(mobj + 0x24, &mref);
    }
    k->links = links;
    union {
        double d;
        uint32_t u[2];
    } dl = { delay };
    log_printf("PIN f%u ent 0x%08x vt %x idx %u fl %02x | id %08x tipo %08x pid %08x d%u | pin %08x n%u | links %u tgt 0x%08x | demora %08x%08x flag %x | msg 0x%08x m 0x%08x mfl %08x mref %u | ctx 0x%08x h 0x%08x o %+d | desde %x%s%s%s",
               frame, ent, rva(vt), idx & 0xFFFFFF, idx >> 24, eid, type, pid, depth, pin, total, links, tgt, dl.u[1], dl.u[0], flag,
               (unsigned)(uintptr_t)msg, mobj, mflags, mref, (unsigned)(uintptr_t)ctx, h, outer ? (int32_t)(outer - ent) : 0,
               rva(ret), other ? " (otro thread)" : "", !other && t_depth > 1 ? " (anidado)" : "",
               g_injecting ? " (INYECTADO)" : win ? " (ventana)" : "");
    if (dumpvt)
        log_dump32("PIN primera instancia de la vtable (desde ent-0x20)", ent - 0x20, 0x80);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_fire(void *self, void *edx, void *ctx,
                                                                                const uint32_t *pin, void *msg,
                                                                                double delay, uint32_t flag)
{
    uint32_t ret = (uint32_t)(uintptr_t)__builtin_return_address(0);
    int main_thread = GetCurrentThreadId() == g_aimp_main_tid;
    if (main_thread && ++t_depth > g_depth_max)
        g_depth_max = t_depth;
    note((uint32_t)(uintptr_t)self, ctx, pin, msg, delay, flag, ret);
    g_orig(self, edx, ctx, pin, msg, delay, flag);
    if (main_thread)
        t_depth--;
}

int pins_install_at(void *target)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart;
    InitializeCriticalSection(&g_cs);
    g_per_key = aimp_env_int(L"AIMP_TRACE_PINS_N", 2);
    g_max_lines = aimp_env_int(L"AIMP_TRACE_PINS_MAX", 20000);
    int have_file = aimp_env_str(L"AIMP_PINS_FILE", g_file, MAX_PATH);
    if (!hook_install(&g_hk, "disparar pin (FUN_008be0d0)", target, AIMP_FIRE_PIN_PROLOGUE, sizeof AIMP_FIRE_PIN_PROLOGUE,
                      AIMP_FIRE_PIN_STEAL, (void *)det_fire, (void **)&g_orig))
        return 0;
    g_on = 1;
    char f8[MAX_PATH * 3];
    log_printf("AIMP_TRACE_PINS=1 (fase 6): se loguean los pines de salida de CATHODE, %d por (entidad, pin), tope %d líneas%s%s",
               g_per_key, g_max_lines, have_file ? "; comandos FIRE en " : "", have_file ? aimp_utf8(f8, sizeof f8, g_file) : "");
    return 1;
}

int pins_install(void)
{
    if (!aimp_env_int(L"AIMP_TRACE_PINS", 0))
        return 0;
    return pins_install_at((void *)(g_aimp_exe + AIMP_RVA_FIRE_PIN));
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
}

/* ---- Reinyección (experimento del paso 3) ---- */

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

int pins_fire(uint32_t eid, uint32_t pin)
{
    PKey src;
    int found = 0;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < PK_SLOTS && found < 2; i++) {
        PKey *k = &g_keys[i];
        if (!k->total || (k->pid != eid && k->eid != eid) || !k->have_snap)
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
    if (!found) {
        log_printf("PIN FIRE %08x %08x: no hay ninguna entidad con ese id que haya disparado en este nivel", eid, pin);
        return 0;
    }
    /* La entidad tiene que seguir viva: su handle original sigue apuntando al mismo objeto. */
    uint32_t o = 0, vt = 0;
    if (!mem_u32(src.h + 0xC, &o) || o != src.outer || !mem_u32(src.ent, &vt) || vt != src.vt) {
        log_printf("PIN FIRE %08x %08x: la entidad 0x%08x ya no está (handle 0x%08x -> 0x%08x)", eid, pin, src.ent, src.h, o);
        return 0;
    }
    /* Copias propias con refcount alto: el motor suma y resta, pero nunca llega a 0. */
    uint32_t *hc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    uint32_t *mc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    if (!hc || !mc)
        return 0;
    aimp_memcpy(hc, src.hsnap, H_SIZE);
    hc[1] = 0x10000000;
    hc[3] = src.outer;
    aimp_memcpy(mc, src.msnap, MSG_SIZE);
    mc[9] = 0x10000000;
    void *ctx_slot = hc, *msg_slot = mc;
    uint32_t pin_v = pin;
    log_printf("PIN FIRE %08x %08x: ent 0x%08x (vt %x, tipo %08x), con el handle y el mensaje del primer disparo de %08x%s",
               eid, pin, src.ent, rva(src.vt), src.type, src.pin, found == 2 ? "" : " (otro pin de la misma entidad)");
    g_win_lines = 0;
    g_win_until = GetTickCount() + 1500;
    g_injecting = 1;
    det_fire((void *)(uintptr_t)src.ent, NULL, &ctx_slot, &pin_v, &msg_slot, 0.0, 0);
    g_injecting = 0;
    g_injected++;
    log_printf("PIN FIRE %08x %08x: hecho (inyecciones %u)", eid, pin, g_injected);
    return 1;
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
        if ((p[0] | 0x20) == 'f' && (p[1] | 0x20) == 'i' && (p[2] | 0x20) == 'r' && (p[3] | 0x20) == 'e' && p[4] == ' ') {
            p += 5;
            uint32_t eid = parse_hex(&p), pin = parse_hex(&p);
            pins_fire(eid, pin);
        }
        while (*p && *p != '\n')
            p++;
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
    }
    if (g_file[0] && now >= g_next_file_check) {
        g_next_file_check = now + g_freq / 4;
        check_file();
    }
}
