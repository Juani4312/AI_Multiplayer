/* Inventario compartido (ver inv.h). Hallazgo 21 (2026-11-17):
 *   inventario = [g_root+0x44]: 34 slots de 0x14 bytes desde +0x68: +0 definición del ítem, +4 instancia (armas),
 *     +8 cantidad. FUN_0077f330 busca el slot de una definición; FUN_0078ddf0 thiscall(inv, slot), ret 4, lo vacía.
 *   agregar = FUN_0079bb20 thiscall(inv, def, cantidad, flag), ret 0xC (lo usa AddToInventory por FUN_007aa4e0;
 *     respeta el máximo de la definición).
 *   base de ítems = [g_root+0x40] (FUN_007bcec0 la carga de DATA\gbl_item.bml): +0x04 cabeza de una lista de todas las
 *     definiciones (nodos {def, anterior, siguiente}), +0x48 cantidad (45).
 *   definición: +0x08 nombre (char*), +0x10 id secuencial, +0x14 ShortGuid del nombre, +0x18 cantidad por defecto,
 *     +0x1C máximo, +0x30 categoría (char*: Component, Ammo, Tool, Health, Holdable).
 * Cada 100 ms se leen los slots y se comparan con la lectura anterior; los cambios salen como (ShortGuid, delta). Lo que
 * llega del otro se aplica con las funciones del juego y se suma a la lectura anterior, así no vuelve como eco. */
#include "inv.h"
#include "game.h"
#include "log.h"

#define ROOT_ITEMDB    0x40
#define ROOT_INV       0x44
#define INV_SLOTS      0x68
#define INV_NSLOTS     0x22
#define INV_SLOT_SIZE  0x14
#define DB_LIST        0x04
#define DB_COUNT       0x48
#define DEF_NAME       0x08
#define DEF_GUID       0x14
#define DEF_MAX        0x1C
#define DEF_CATEGORY   0x30
#define AIMP_RVA_INV_ADD   0x0039BB20u
#define AIMP_RVA_INV_CLEAR 0x0038DDF0u
#define MAX_ITEMS 64

static const uint8_t ADD_PROLOGUE[8] = { 0x51, 0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x57, 0x8B };
static const uint8_t CLEAR_PROLOGUE[8] = { 0x8B, 0x44, 0x24, 0x04, 0x8D, 0x04, 0x80, 0x8D };
typedef uint32_t(__attribute__((fastcall)) * AddFn)(void *inv, void *edx, uint32_t def, int32_t count, uint32_t flag);
typedef void(__attribute__((fastcall)) * ClearFn)(void *inv, void *edx, uint32_t slot);

typedef struct {
    uint32_t guid;
    int32_t count;
} Item;

static int g_trace, g_sync;
static AimpNet *g_net;
static AddFn g_add;
static ClearFn g_clear;
static Item g_base[MAX_ITEMS];
static int g_nbase, g_have_base;
static uint32_t g_player, g_level;
static int64_t g_last, g_settle_until;
static uint32_t g_changes, g_sent, g_applied;
/* inventario completo: host -> cliente */
static int g_full_due, g_had_remote;
static uint32_t g_full_serial;
static Item g_full[MAX_ITEMS];
static int g_nfull;
static uint32_t g_full_rx_serial;
static uint16_t g_full_rx_parts;

int inv_sync_enabled(void) { return g_sync; }

static uint32_t inv_ptr(void)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), inv = 0;
    return root && mem_u32(root + ROOT_INV, &inv) ? inv : 0;
}

static void def_name(uint32_t def, char *out, size_t cap)
{
    uint32_t p = 0;
    if (!def || !mem_u32(def + DEF_NAME, &p) || !p || !mem_cstr(p, out, cap))
        aimp_snprintf(out, cap, "?");
}

/* Lee los slots y suma por ítem. Devuelve la cantidad de ítems distintos o -1 si no se pudo. */
static int scan(Item *out, uint32_t *defs)
{
    uint32_t inv = inv_ptr();
    uint8_t b[INV_NSLOTS * INV_SLOT_SIZE];
    if (!inv || !mem_read(inv + INV_SLOTS, b, sizeof b))
        return -1;
    int n = 0;
    for (int i = 0; i < INV_NSLOTS; i++) {
        uint32_t def, guid = 0;
        int32_t cnt;
        aimp_memcpy(&def, b + i * INV_SLOT_SIZE, 4);
        aimp_memcpy(&cnt, b + i * INV_SLOT_SIZE + 8, 4);
        if (!def || !mem_u32(def + DEF_GUID, &guid) || !guid)
            continue;
        int k = 0;
        while (k < n && out[k].guid != guid)
            k++;
        if (k == n) {
            if (n == MAX_ITEMS)
                continue;
            out[n].guid = guid;
            out[n].count = 0;
            if (defs)
                defs[n] = def;
            n++;
        }
        out[k].count += cnt;
    }
    return n;
}

static int32_t count_of(const Item *it, int n, uint32_t guid)
{
    for (int i = 0; i < n; i++)
        if (it[i].guid == guid)
            return it[i].count;
    return 0;
}

static void set_base(uint32_t guid, int32_t count)
{
    for (int i = 0; i < g_nbase; i++)
        if (g_base[i].guid == guid) {
            g_base[i].count = count;
            return;
        }
    if (g_nbase < MAX_ITEMS) {
        g_base[g_nbase].guid = guid;
        g_base[g_nbase].count = count;
        g_nbase++;
    }
}

/* Busca la definición de un ítem por ShortGuid en la lista de la base de ítems. */
static uint32_t find_def(uint32_t guid, char *name, size_t cap)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), db = 0, node = 0;
    if (!root || !mem_u32(root + ROOT_ITEMDB, &db) || !db || !mem_u32(db + DB_LIST, &node))
        return 0;
    for (int i = 0; node && i < 256; i++) {
        uint32_t def = 0, g = 0, next = 0;
        if (!mem_u32(node, &def) || !mem_u32(node + 8, &next))
            return 0;
        if (def && mem_u32(def + DEF_GUID, &g) && g == guid) {
            def_name(def, name, cap);
            return def;
        }
        node = next;
    }
    return 0;
}

/* Detecta y manda los cambios locales desde la lectura anterior. */
static void detect(void)
{
    Item cur[MAX_ITEMS];
    uint32_t defs[MAX_ITEMS];
    int n = scan(cur, defs);
    if (n < 0)
        return;
    if (!g_have_base) {
        aimp_memcpy(g_base, cur, sizeof(Item) * (size_t)n);
        g_nbase = n;
        g_have_base = 1;
        log_printf("INV: inventario inicial: %d ítems distintos", n);
        return;
    }
    InvEvt ev;
    aimp_memset(&ev, 0, sizeof ev);
    /* ítems que cambiaron o aparecieron */
    for (int i = 0; i < n; i++) {
        int32_t d = cur[i].count - count_of(g_base, g_nbase, cur[i].guid);
        if (!d)
            continue;
        char nm[40];
        def_name(defs[i], nm, sizeof nm);
        g_changes++;
        if (g_trace || g_sync)
            log_printf("INV: local: %s %+d (ahora %d)", nm, (int)d, (int)cur[i].count);
        if (ev.n < INV_EVT_MAX) {
            ev.d[ev.n].guid = cur[i].guid;
            ev.d[ev.n].delta = d;
            ev.n++;
        }
    }
    /* ítems que desaparecieron (slot vaciado) */
    for (int i = 0; i < g_nbase; i++) {
        if (!g_base[i].count || count_of(cur, n, g_base[i].guid))
            continue;
        int found = 0;
        for (int k = 0; k < n; k++)
            found |= cur[k].guid == g_base[i].guid;
        if (found)
            continue;
        g_changes++;
        if (g_trace || g_sync)
            log_printf("INV: local: ítem 0x%08x %+d (ya no está)", g_base[i].guid, (int)-g_base[i].count);
        if (ev.n < INV_EVT_MAX) {
            ev.d[ev.n].guid = g_base[i].guid;
            ev.d[ev.n].delta = -g_base[i].count;
            ev.n++;
        }
    }
    aimp_memcpy(g_base, cur, sizeof(Item) * (size_t)n);
    g_nbase = n;
    if (ev.n && g_sync && g_net && g_net->mode) {
        if (net_event(g_net, AIMP_EVT_INV, g_level, &ev, (uint16_t)(4 + 8 * ev.n)))
            g_sent++;
        else
            log_printf("INV: no se pudo encolar el evento (%u cambios)", ev.n);
    }
}

/* Host: manda el inventario completo, en partes. */
static void send_full(void)
{
    Item cur[MAX_ITEMS];
    int n = scan(cur, NULL);
    if (n < 0)
        return;
    g_full_due = 0;
    g_full_serial++;
    int nparts = n ? (n + INV_FULL_MAX - 1) / INV_FULL_MAX : 1;
    for (int p = 0; p < nparts; p++) {
        InvFull f;
        aimp_memset(&f, 0, sizeof f);
        f.part = (uint16_t)p;
        f.nparts = (uint16_t)nparts;
        f.serial = g_full_serial;
        for (int i = p * INV_FULL_MAX; i < n && f.n < INV_FULL_MAX; i++) {
            f.d[f.n].guid = cur[i].guid;
            f.d[f.n].count = cur[i].count;
            f.n++;
        }
        net_event(g_net, AIMP_EVT_INV_FULL, g_level, &f, sizeof f);
    }
    log_printf("INV: inventario completo mandado al cliente (%d ítems, %d partes, envío %u)", n, nparts, g_full_serial);
}

void inv_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level)
{
    if (!g_trace && !g_sync)
        return;
    g_level = level;
    if (player != g_player) {
        /* jugador nuevo (carga de nivel o de partida): la lectura actual es la base, sin mandar nada */
        g_player = player;
        g_have_base = 0;
        g_nbase = 0;
        /* corrida 93: las armas entran al inventario ~0,3 s después de que aparece el jugador; durante 3 s la lectura
         * solo actualiza la base (no manda nada) */
        g_settle_until = now + freq * aimp_env_int(L"AIMP_INV_SETTLE_MS", 3000) / 1000;
        g_full_due = 1; /* después de una carga, el host manda todo */
    }
    if (g_net && g_net->have_remote && !g_had_remote)
        g_full_due = 1; /* se conectó el otro */
    g_had_remote = g_net && g_net->have_remote;
    if (!player || now - g_last < freq / 10)
        return;
    g_last = now;
    if (now < g_settle_until) {
        Item cur[MAX_ITEMS];
        int n = scan(cur, NULL);
        if (n >= 0) {
            aimp_memcpy(g_base, cur, sizeof(Item) * (size_t)n);
            g_nbase = n;
            g_have_base = 1;
        }
        return;
    }
    detect();
    if (g_full_due && g_sync && g_net && g_net->mode == 1 && g_net->have_remote)
        send_full();
}

static void apply_one(uint32_t guid, int32_t delta)
{
    char nm[40];
    uint32_t def = find_def(guid, nm, sizeof nm), inv = inv_ptr();
    if (!def || !inv) {
        log_printf("INV: remoto: ítem 0x%08x %+d: no está en la base de ítems; se ignora", guid, (int)delta);
        return;
    }
    Item cur[MAX_ITEMS];
    int n = scan(cur, NULL);
    int32_t before = n >= 0 ? count_of(cur, n, guid) : 0;
    if (delta > 0) {
        if (g_add)
            g_add((void *)inv, NULL, def, delta, 0);
    } else if (delta < 0) {
        /* restar: se baja la cantidad del slot; si llega a 0, se vacía con la función del juego */
        uint8_t b[INV_NSLOTS * INV_SLOT_SIZE];
        int32_t left = -delta;
        if (mem_read(inv + INV_SLOTS, b, sizeof b))
            for (int i = 0; i < INV_NSLOTS && left > 0; i++) {
                uint32_t d;
                int32_t c;
                aimp_memcpy(&d, b + i * INV_SLOT_SIZE, 4);
                aimp_memcpy(&c, b + i * INV_SLOT_SIZE + 8, 4);
                if (d != def || c <= 0)
                    continue;
                int32_t take = c < left ? c : left;
                left -= take;
                if (c - take <= 0) {
                    if (g_clear)
                        g_clear((void *)inv, NULL, (uint32_t)i);
                } else {
                    int32_t nc = c - take;
                    mem_write(inv + INV_SLOTS + (uint32_t)(i * INV_SLOT_SIZE) + 8, &nc, 4);
                }
            }
    }
    n = scan(cur, NULL);
    int32_t after = n >= 0 ? count_of(cur, n, guid) : before;
    set_base(guid, after); /* lo aplicado no vuelve como cambio local */
    g_applied++;
    log_printf("INV: remoto: %s %+d: %d -> %d", nm, (int)delta, (int)before, (int)after);
}

void inv_remote_event(const AimpEvt *e)
{
    if (!g_sync)
        return;
    InvEvt ev;
    aimp_memset(&ev, 0, sizeof ev);
    aimp_memcpy(&ev, e->data, e->size < sizeof ev ? e->size : sizeof ev);
    if (ev.n > INV_EVT_MAX)
        return;
    if (g_player)
        detect(); /* primero los cambios locales pendientes, para no taparlos con la base nueva */
    for (uint32_t i = 0; i < ev.n; i++)
        apply_one(ev.d[i].guid, ev.d[i].delta);
}

void inv_install(AimpNet *net)
{
    g_net = net;
    g_trace = aimp_env_int(L"AIMP_TRACE_INV", 0);
    g_sync = aimp_env_int(L"AIMP_SYNC_INV", 0);
    if (!g_trace && !g_sync)
        return;
    uint8_t a[sizeof ADD_PROLOGUE], c[sizeof CLEAR_PROLOGUE];
    if (mem_read(g_aimp_exe + AIMP_RVA_INV_ADD, a, sizeof a) && aimp_memcmp(a, ADD_PROLOGUE, sizeof a) == 0)
        g_add = (AddFn)(void *)(g_aimp_exe + AIMP_RVA_INV_ADD);
    if (mem_read(g_aimp_exe + AIMP_RVA_INV_CLEAR, c, sizeof c) && aimp_memcmp(c, CLEAR_PROLOGUE, sizeof c) == 0)
        g_clear = (ClearFn)(void *)(g_aimp_exe + AIMP_RVA_INV_CLEAR);
    if (g_sync && (!g_add || !g_clear)) {
        log_printf("INV: los prólogos de FUN_0079bb20/FUN_0078ddf0 no coinciden: AIMP_SYNC_INV queda apagado");
        g_sync = 0;
    }
    log_printf("INV: inventario %s", g_sync ? "compartido (AIMP_SYNC_INV)" : "solo traza (AIMP_TRACE_INV)");
}

/* Cliente: junta las partes y, con la última, fija las cantidades a las del host. */
void inv_full_event(const AimpEvt *e)
{
    InvFull f;
    if (!g_sync || e->size < sizeof f)
        return;
    aimp_memcpy(&f, e->data, sizeof f);
    if (f.n > INV_FULL_MAX || f.nparts == 0 || f.part >= f.nparts)
        return;
    if (f.serial != g_full_rx_serial || f.part == 0) {
        g_full_rx_serial = f.serial;
        g_full_rx_parts = 0;
        g_nfull = 0;
    }
    for (uint32_t i = 0; i < f.n && g_nfull < MAX_ITEMS; i++) {
        g_full[g_nfull].guid = f.d[i].guid;
        g_full[g_nfull].count = f.d[i].count;
        g_nfull++;
    }
    g_full_rx_parts++;
    if (g_full_rx_parts < f.nparts)
        return;
    if (g_player)
        detect(); /* cambios locales pendientes primero */
    Item cur[MAX_ITEMS];
    int n = scan(cur, NULL), fixes = 0;
    if (n < 0)
        return;
    for (int i = 0; i < g_nfull; i++) {
        int32_t d = g_full[i].count - count_of(cur, n, g_full[i].guid);
        if (d) {
            apply_one(g_full[i].guid, d);
            fixes++;
        }
    }
    for (int i = 0; i < n; i++) {
        if (count_of(g_full, g_nfull, cur[i].guid) == 0 && cur[i].count > 0) {
            int found = 0;
            for (int k = 0; k < g_nfull; k++)
                found |= g_full[k].guid == cur[i].guid;
            if (!found) {
                apply_one(cur[i].guid, -cur[i].count);
                fixes++;
            }
        }
    }
    log_printf("INV: inventario completo del host recibido (%d ítems): %d correcciones", g_nfull, fixes);
}
