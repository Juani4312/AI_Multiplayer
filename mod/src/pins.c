/* Traza de los pines de salida de CATHODE (hallazgo 15; fase 6, paso 1).
 *
 * Todo pin de salida que dispara una entidad de script pasa por EntityInterface::vt[+0x394]
 * = FUN_008be0d0 (777 clases con RTTI lo heredan, y también las vtables concretas sin RTTI,
 * p. ej. la de TriggerSimple 0x01261EB8). Firma (asm de FUN_008bd920 y FUN_008be0d0):
 *     thiscall(EntityInterface *this, Handle *ctx, uint32 *pin, msg, double demora, flag), ret 0x18
 * FUN_008be0d0 busca el pin en la tabla de links de la entidad (this+4) y, por cada link, dispara
 * el destino directo o lo encola en el entity manager con la demora (FUN_009271c0).
 *
 * Este módulo solo mira: por cada (entidad, pin) loguea las primeras apariciones y cuenta el
 * resto. Los ShortGuids se traducen offline con work/evidence/h15/shortguids.tsv
 * (work/scripts/pin_names.py) y work/scripts/analyze_pins.py.
 */
#include "pins.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define PK_SLOTS 16384 /* potencia de 2; claves (entidad, pin) */
#define VT_SLOTS 1024  /* vtables vistas (para el volcado de la primera instancia) */

typedef struct {
    uint32_t ent, pin;
    uint32_t vt, idx, links;
    uint32_t total, period;
} PKey;

typedef void(__attribute__((fastcall)) * FireFn)(void *self, void *edx, void *ctx, const uint32_t *pin, void *msg,
                                                 double delay, uint32_t flag);

static AimpHook g_hk;
static FireFn g_orig;
static int g_on, g_per_key = 2, g_max_lines = 20000;
static CRITICAL_SECTION g_cs;
static PKey g_keys[PK_SLOTS];
static uint32_t g_nkeys, g_lines, g_fires, g_fires_period, g_full, g_other_thread;
static uint32_t g_vts[VT_SLOTS], g_nvts;
static volatile LONG g_depth_max;
static int t_depth; /* anidamiento en el thread principal: un pin dispara a otro directamente */

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

static void note(uint32_t ent, void *ctx, const uint32_t *pinp, void *msg, double delay, uint32_t flag, uint32_t ret)
{
    uint32_t pin = 0, vt = 0, idx = 0;
    if (!pinp || !mem_u32((uintptr_t)pinp, &pin))
        return;
    mem_u32(ent, &vt);
    mem_u32(ent + ENT_INDEX, &idx);
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
    int detail = (int)k->total <= g_per_key && g_lines < (uint32_t)g_max_lines;
    int dumpvt = created && vt_first(vt);
    uint32_t total = k->total;
    if (detail)
        g_lines++;
    LeaveCriticalSection(&g_cs);
    if (!detail)
        return;
    /* Lecturas y log fuera de la sección crítica (el log tiene su propio lock). */
    uint32_t tgt = 0, links = count_links(ent, pin, &tgt);
    uint32_t h = 0, o = 0, ovt = 0;
    if (ctx && mem_u32((uintptr_t)ctx, &h) && h && mem_u32(h + 0xC, &o) && o)
        mem_u32(o, &ovt);
    k->vt = vt;
    k->idx = idx & 0xFFFFFF;
    k->links = links;
    int32_t rel = (int32_t)(o - ent);
    union {
        double d;
        uint32_t u[2];
    } dl = { delay };
    log_printf("PIN f%u ent 0x%08x vt %x idx %u fl %02x | pin %08x n%u | links %u tgt 0x%08x | demora %08x%08x flag %x msg 0x%08x | ctx 0x%08x h 0x%08x o 0x%08x (ent%+d) ovt %x | desde %x%s%s",
               frame, ent, rva(vt), idx & 0xFFFFFF, idx >> 24, pin, total, links, tgt, dl.u[1], dl.u[0], flag,
               (unsigned)(uintptr_t)msg, (unsigned)(uintptr_t)ctx, h, o, (o && rel > -0x1000 && rel < 0x1000) ? rel : 0, rva(ovt),
               rva(ret), other ? " (otro thread)" : "", !other && t_depth > 1 ? " (anidado)" : "");
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
    InitializeCriticalSection(&g_cs);
    g_per_key = aimp_env_int(L"AIMP_TRACE_PINS_N", 2);
    g_max_lines = aimp_env_int(L"AIMP_TRACE_PINS_MAX", 20000);
    if (!hook_install(&g_hk, "disparar pin (FUN_008be0d0)", target, AIMP_FIRE_PIN_PROLOGUE, sizeof AIMP_FIRE_PIN_PROLOGUE,
                      AIMP_FIRE_PIN_STEAL, (void *)det_fire, (void **)&g_orig))
        return 0;
    g_on = 1;
    log_printf("AIMP_TRACE_PINS=1 (fase 6, solo mira): se loguean los pines de salida de CATHODE, %d por (entidad, pin), tope %d líneas",
               g_per_key, g_max_lines);
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
    log_printf("PIN %s: %u disparos, %u (entidad, pin) distintos, %u líneas detalladas, %u sin lugar, %u en otro thread, anidamiento máx. %ld",
               title, fires, nkeys, g_lines, g_full, g_other_thread, (long)g_depth_max);
    for (int i = 0; i < nb; i++)
        log_printf("PIN   %6u x  ent 0x%08x vt %x idx %u pin %08x links %u", period ? best[i].period : best[i].total,
                   best[i].ent, rva(best[i].vt), best[i].idx, best[i].pin, best[i].links);
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
