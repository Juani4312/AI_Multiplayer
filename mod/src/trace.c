/* Trazado de los productores de mensajes de las colas A y B (hallazgo 10).
 *
 * Se hookean los dos wrappers cdecl que usan todos los productores:
 *   A: FUN_00a2a290(tipo, datos, tamaño) -> FUN_00a2a160(cola A)  (31 callers)
 *   B: FUN_00a2a810(tipo, datos, tamaño) -> FUN_00a2a760(sesión)  (41 callers)
 * Como el detour reemplaza al wrapper, su dirección de retorno es el call site del
 * productor: eso permite mapear cada tipo a la función que lo genera.
 * Los productores pueden correr en workers (la cola A tiene lock), así que todo
 * se cuenta con operaciones Interlocked.
 */
#include "trace.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define TR_TYPES 1024 /* los ids son secuenciales y chicos; el resto va al último */
#define TR_SITES 4

typedef struct {
    volatile LONG count, bytes;   /* del período (se resetean en cada informe) */
    volatile LONG total, tbytes;  /* de toda la sesión */
    volatile LONG maxsz, threads; /* bit 0: thread principal, bit 1: otro thread */
    volatile LONG site[TR_SITES]; /* RVAs de las direcciones de retorno */
} TStat;

static TStat g_tr[2][TR_TYPES + 1];
static AimpHook g_hook_a, g_hook_b;
static int g_enabled;

typedef uint32_t(__attribute__((cdecl)) * WrapFn)(uint32_t type, const void *data, uint32_t size);
static WrapFn g_wrap_a, g_wrap_b;

int trace_enabled(void) { return g_enabled; }

static char qname(int q) { return q ? 'B' : 'A'; }

#define TR_PAYLOAD_SAMPLES 8  /* payloads logueados por tipo (los tipos raros son los interesantes) */
#define TR_PAYLOAD_MAX 64

void trace_note(int q, uint32_t type, const void *data, uint32_t size, uintptr_t ret)
{
    uint32_t t = type & 0xFFFF;
    TStat *s = &g_tr[q & 1][t < TR_TYPES ? t : TR_TYPES];
    LONG nth = InterlockedIncrement(&s->total);
    LONG first = nth == 1;
    InterlockedIncrement(&s->count);
    InterlockedExchangeAdd(&s->bytes, (LONG)size);
    InterlockedExchangeAdd(&s->tbytes, (LONG)size);
    for (;;) {
        LONG cur = s->maxsz;
        if ((LONG)size <= cur || InterlockedCompareExchange(&s->maxsz, (LONG)size, cur) == cur)
            break;
    }
    LONG bit = GetCurrentThreadId() == g_aimp_main_tid ? 1 : 2;
    for (;;) {
        LONG cur = s->threads;
        if ((cur & bit) || InterlockedCompareExchange(&s->threads, cur | bit, cur) == cur)
            break;
    }
    LONG site = (LONG)(ret - g_aimp_exe);
    int new_site = 0;
    for (int i = 0; i < TR_SITES; i++) {
        LONG v = s->site[i];
        if (v == site)
            break;
        if (v == 0) {
            LONG prev = InterlockedCompareExchange(&s->site[i], site, 0);
            if (prev == 0) {
                new_site = 1;
                break;
            }
            if (prev == site)
                break;
        }
    }
    if (first)
        log_printf("TR cola %c: tipo nuevo 0x%04x (%u bytes), call site rva 0x%x, thread %s", qname(q), t,
                   (unsigned)size, (unsigned)site, bit == 1 ? "principal" : "otro");
    else if (new_site)
        log_printf("TR cola %c: tipo 0x%04x, call site nuevo rva 0x%x", qname(q), t, (unsigned)site);
    if (nth <= TR_PAYLOAD_SAMPLES && data && size) {
        uint8_t b[TR_PAYLOAD_MAX];
        char hex[TR_PAYLOAD_MAX * 3 + 8];
        uint32_t k = size < TR_PAYLOAD_MAX ? size : TR_PAYLOAD_MAX;
        size_t n = 0;
        if (mem_read((uintptr_t)data, b, k)) {
            for (uint32_t i = 0; i < k; i++)
                n += (size_t)aimp_snprintf(hex + n, sizeof hex - n, (i && i % 4 == 0) ? " %02x" : "%02x", b[i]);
            log_printf("TR   payload %c 0x%04x #%ld (%u bytes%s): %s", qname(q), t, nth, (unsigned)size,
                       size > k ? ", recortado" : "", hex);
        }
    }
}

__attribute__((cdecl, force_align_arg_pointer, noinline)) static uint32_t det_a(uint32_t type, const void *data,
                                                                              uint32_t size)
{
    trace_note(0, type, data, size, (uintptr_t)__builtin_return_address(0));
    return g_wrap_a(type, data, size);
}

__attribute__((cdecl, force_align_arg_pointer, noinline)) static uint32_t det_b(uint32_t type, const void *data,
                                                                              uint32_t size)
{
    trace_note(1, type, data, size, (uintptr_t)__builtin_return_address(0));
    return g_wrap_b(type, data, size);
}

int trace_install_at(void *wrap_a, void *wrap_b)
{
    int ok = hook_install(&g_hook_a, "encolar A (FUN_00a2a290)", wrap_a, AIMP_ENQUEUE_PROLOGUE,
                          sizeof AIMP_ENQUEUE_PROLOGUE, AIMP_ENQUEUE_STEAL, (void *)det_a, (void **)&g_wrap_a);
    ok &= hook_install(&g_hook_b, "encolar B (FUN_00a2a810)", wrap_b, AIMP_ENQUEUE_PROLOGUE,
                       sizeof AIMP_ENQUEUE_PROLOGUE, AIMP_ENQUEUE_STEAL, (void *)det_b, (void **)&g_wrap_b);
    g_enabled = g_hook_a.installed || g_hook_b.installed;
    return ok;
}

int trace_install(void)
{
    if (!aimp_env_int(L"AIMP_TRACE_QUEUES", 0))
        return 0;
    log_printf("AIMP_TRACE_QUEUES=1: trazado de productores de las colas A y B");
    return trace_install_at((void *)(g_aimp_exe + AIMP_RVA_ENQUEUE_A), (void *)(g_aimp_exe + AIMP_RVA_ENQUEUE_B));
}

static const char *thr(LONG t)
{
    switch (t & 3) {
    case 1: return "p";
    case 2: return "o";
    case 3: return "p+o";
    default: return "-";
    }
}

void trace_report_period(void)
{
    char line[1024];
    if (!g_enabled)
        return;
    for (int q = 0; q < 2; q++) {
        int n = 0, k = 0;
        LONG tot_n = 0, tot_b = 0;
        for (int t = 0; t <= TR_TYPES; t++) {
            TStat *s = &g_tr[q][t];
            if (!s->count)
                continue;
            LONG c = InterlockedExchange(&s->count, 0);
            LONG b = InterlockedExchange(&s->bytes, 0);
            tot_n += c;
            tot_b += b;
            n += aimp_snprintf(line + n, sizeof line - (size_t)n, " %s0x%04x n=%ld b=%ld",
                               t == TR_TYPES ? ">" : "", t == TR_TYPES ? TR_TYPES : t, c, b);
            if (++k == 8 || n > (int)sizeof line - 64) {
                log_printf("TR cola %c:%s", qname(q), line);
                n = k = 0;
            }
        }
        if (k)
            log_printf("TR cola %c:%s", qname(q), line);
        if (tot_n)
            log_printf("TR cola %c: período con %ld mensajes, %ld bytes de payload", qname(q), tot_n, tot_b);
    }
}

void trace_report_totals(const char *why)
{
    if (!g_enabled)
        return;
    log_printf("TR totales (%s): tipo, mensajes, bytes, tamaño máx, threads (p = principal, o = otro), call sites (rva)",
               why);
    for (int q = 0; q < 2; q++) {
        int types = 0;
        for (int t = 0; t <= TR_TYPES; t++) {
            TStat *s = &g_tr[q][t];
            if (!s->total)
                continue;
            types++;
            log_printf("TR   %c %s0x%04x n=%ld b=%ld max=%ld %s sitios 0x%lx 0x%lx 0x%lx 0x%lx", qname(q),
                       t == TR_TYPES ? ">" : "", t, s->total, s->tbytes, s->maxsz, thr(s->threads), s->site[0],
                       s->site[1], s->site[2], s->site[3]);
        }
        log_printf("TR   cola %c: %d tipos distintos", qname(q), types);
    }
}
