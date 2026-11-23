/* Botín de las cajas: multiplicador sobre el sorteo del pool de ítems (ver loot.h). */
#include "loot.h"
#include "hook.h"
#include "log.h"

#define AIMP_RVA_GC_ALLOC 0x003A4DE0u /* FUN_007a4de0 */
/* PUSH EBX; MOV EBX,ECX; LEA EAX,[EBX+0x5C1C]; PUSH ESI; TEST AL,0xF */
static const uint8_t GC_ALLOC_PROLOGUE[12] = { 0x53, 0x8B, 0xD9, 0x8D, 0x83, 0x1C, 0x5C, 0x00, 0x00, 0x56, 0xA8, 0x0F };

typedef uint32_t(__attribute__((fastcall)) * AllocFn)(void *pool, void *edx, void *items, float bias, uint32_t force, uint8_t *out);
static AllocFn g_orig;
static AimpHook g_hk;
static volatile LONG g_pct = 100;
static int g_trace;
static volatile LONG g_n;
static uint32_t g_rng = 0x1234567u;

int loot_scale(int qty, int pct, unsigned rnd16)
{
    if (qty <= 0 || pct == 100)
        return qty;
    int64_t x = (int64_t)qty * pct; /* en centésimos */
    int q = (int)(x / 100), frac = (int)(x % 100);
    if ((int)((rnd16 & 0xFFFFu) * 100u / 65536u) < frac)
        q++;
    return q > 999 ? 999 : q;
}

void loot_set_pct(int pct)
{
    if (pct < 0)
        pct = 0;
    if (pct != (int)g_pct)
        log_printf("LOOT: botín de las cajas al %d %% (menú Coop)", pct);
    InterlockedExchange(&g_pct, pct);
}

/* Escala la cantidad de una salida de sorteo (+0 u32 ítem, +6 u16 cantidad). Devuelve el resultado para el juego. */
static uint32_t scale_out(uint32_t r, uint8_t *out, const char *what, float bias, int with_bias)
{
    LONG n = InterlockedIncrement(&g_n);
    if (!(r & 0xFF) || !out) {
        if (g_trace)
            log_printf("LOOT: %s #%ld: sin ítem", what, (long)n);
        return r;
    }
    uint32_t item = 0;
    uint16_t q = 0;
    if (!mem_read((uintptr_t)out, &item, 4) || !mem_read((uintptr_t)out + 6, &q, 2))
        return r;
    int pct = (int)g_pct;
    g_rng = g_rng * 1103515245u + 12345u;
    int nq = loot_scale(q, pct, g_rng >> 16);
    if (g_trace || n <= 40 || nq != q) {
        if (with_bias)
            log_printf("LOOT: %s #%ld: ítem %08x x%u -> x%d (botín %d %%, bias %.2f)", what, (long)n, item, q, nq, pct, (double)bias);
        else
            log_printf("LOOT: %s #%ld: ítem %08x x%u -> x%d (botín %d %%)", what, (long)n, item, q, nq, pct);
    }
    if (nq == q)
        return r;
    if (nq <= 0)
        return r & 0xFFFFFF00u; /* sin ítem: esa ranura (y las que siguen) quedan vacías */
    uint16_t w = (uint16_t)nq;
    mem_write((uintptr_t)out + 6, &w, 2);
    return r;
}

/* Variante de un ítem (AllocateGCItemFromPoolBySubset: FUN_005761b0 -> FUN_007a4de0). */
__attribute__((fastcall, force_align_arg_pointer, noinline)) static uint32_t det_alloc(void *pool, void *edx, void *items, float bias,
                                                                                        uint32_t force, uint8_t *out)
{
    (void)force;
    return scale_out(g_orig(pool, edx, items, bias, force, out), out, "sorteo de un ítem", bias, 1);
}

/* Cajas (AllocateGCItemsFromPool: FUN_00576070 -> FUN_007a4a70, que para cada ranura vacía sortea con FUN_00794960 o,
 * con cantidad fija, directo con FUN_00786aa0; FUN_00794960 también termina en FUN_00786aa0 sobre la misma salida).
 * FUN_00786aa0 thiscall(pool, salida*, n), ret 8. Corrida 181: la entidad de las cajas tiene vtable 0x1230448 y su
 * manejador (slot 0x284) es FUN_00576070. */
#define AIMP_RVA_GC_PICK 0x00386AA0u /* FUN_00786aa0 */
/* SUB ESP,8; PUSH ESI; MOV ESI,[ESP+0x14]; TEST ESI,ESI; JNZ +9 */
static const uint8_t GC_PICK_PROLOGUE[12] = { 0x83, 0xEC, 0x08, 0x56, 0x8B, 0x74, 0x24, 0x14, 0x85, 0xF6, 0x75, 0x09 };
typedef uint32_t(__attribute__((fastcall)) * PickFn)(void *pool, void *edx, uint8_t *out, uint32_t n);
static PickFn g_pick_orig;
static AimpHook g_hk_pick;

__attribute__((fastcall, force_align_arg_pointer, noinline)) static uint32_t det_pick(void *pool, void *edx, uint8_t *out, uint32_t n)
{
    return scale_out(g_pick_orig(pool, edx, out, n), out, "sorteo de caja", 0.0f, 0);
}

void loot_install(void)
{
    g_trace = aimp_env_int(L"AIMP_TRACE_LOOT", 0);
    g_pct = aimp_env_int(L"AIMP_COOP_LOOT_PCT", 100);
    g_rng ^= GetTickCount();
    int a = hook_install(&g_hk, "sorteo de un ítem del pool de botín (FUN_007a4de0)", (void *)(g_aimp_exe + AIMP_RVA_GC_ALLOC),
                         GC_ALLOC_PROLOGUE, sizeof GC_ALLOC_PROLOGUE, 9, (void *)det_alloc, (void **)&g_orig);
    int b = hook_install(&g_hk_pick, "sorteo de las cajas del pool de botín (FUN_00786aa0)", (void *)(g_aimp_exe + AIMP_RVA_GC_PICK),
                         GC_PICK_PROLOGUE, sizeof GC_PICK_PROLOGUE, 8, (void *)det_pick, (void **)&g_pick_orig);
    if (a || b)
        log_printf("LOOT: botín de las cajas al %d %%%s", (int)g_pct, b ? "" : " (solo la variante de un ítem)");
}
