/* Armas de a una (decisión del usuario, 2026-11-17; work/DESIGN_SESSION.md, "Armas de a uno").
 *
 *   AIMP_SYNC_WEAPONS=1 (con AIMP_NET): cada arma tiene dueño = el último que la equipó.
 *     - Cerca (a menos de 4 m; "lejos" desde 5 m, histéresis): cualquiera la puede sacar si el otro no la tiene en la mano.
 *     - Lejos: solo el dueño. Si el otro la saca, el mod se la guarda enseguida (FUN_007c3980).
 *     - Si el dueño está caído (death.c) o sin datos de red hace 3 s, queda libre.
 *     - Los dos al mismo tiempo: gana el host (el cliente la guarda).
 *
 * Arma en mano (hallazgo 21, lector de memoria): equipo = [Character+0x1100]; equipo+0x150C = índice actual,
 * equipo+0x1510 = hay algo en la mano, equipo[índice] = instancia, la misma que el slot+4 del inventario de esa arma.
 * FUN_007c3980 (fastcall(equipo), ret) guarda lo que hay en la mano (pone +0x1510 = 0); la llaman 27 lugares. */
#include "weapons.h"
#include "death.h"
#include "game.h"
#include "log.h"

#define CHR_EQUIP      0x1100
#define EQ_INDEX       0x150C
#define EQ_INHAND      0x1510
#define ROOT_INV       0x44
#define INV_SLOTS      0x68
#define INV_NSLOTS     0x22
#define INV_SLOT_SIZE  0x14
#define DEF_GUID       0x14
#define DEF_NAME       0x08
#define AIMP_RVA_HOLSTER 0x003C3980u
#define MAX_W 16
#define OWN_NONE 0
#define OWN_LOCAL 1
#define OWN_REMOTE 2

static const uint8_t HOLSTER_PROLOGUE[13] = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF, 0x68, 0xC8, 0xFE, 0x12, 0x01 };
typedef void(__attribute__((fastcall)) * HolsterFn)(void *equip);

static int g_on;
static AimpNet *g_net;
static HolsterFn g_holster;
static struct {
    uint32_t guid;
    int owner;
} g_own[MAX_W];
static int g_nown;
static uint32_t g_local_hold, g_remote_hold, g_player;
static int g_near;
static int64_t g_last, g_settle_until;

int weapons_enabled(void) { return g_on; }

static int *owner_of(uint32_t guid)
{
    for (int i = 0; i < g_nown; i++)
        if (g_own[i].guid == guid)
            return &g_own[i].owner;
    if (g_nown == MAX_W)
        return NULL;
    g_own[g_nown].guid = guid;
    g_own[g_nown].owner = OWN_NONE;
    return &g_own[g_nown++].owner;
}

static void wname(uint32_t guid, char *out, size_t cap)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), inv = 0;
    uint8_t b[INV_NSLOTS * INV_SLOT_SIZE];
    aimp_snprintf(out, cap, "0x%08x", guid);
    if (!root || !mem_u32(root + ROOT_INV, &inv) || !inv || !mem_read(inv + INV_SLOTS, b, sizeof b))
        return;
    for (int i = 0; i < INV_NSLOTS; i++) {
        uint32_t def, g = 0, p = 0;
        aimp_memcpy(&def, b + i * INV_SLOT_SIZE, 4);
        if (def && mem_u32(def + DEF_GUID, &g) && g == guid && mem_u32(def + DEF_NAME, &p) && p) {
            mem_cstr(p, out, cap);
            return;
        }
    }
}

/* ShortGuid del arma en la mano del jugador (0 si no hay o no es un ítem del inventario: detector, linterna). */
static uint32_t held(uint32_t player, uint32_t *equip)
{
    uint32_t eq = 0, idx = 0, inst = 0, root, inv = 0;
    uint8_t in = 0, b[INV_NSLOTS * INV_SLOT_SIZE];
    *equip = 0;
    if (!mem_u32(player + CHR_EQUIP, &eq) || !eq)
        return 0;
    *equip = eq;
    if (!mem_u8(eq + EQ_INHAND, &in) || !in || !mem_u32(eq + EQ_INDEX, &idx) || idx >= 16 || !mem_u32(eq + idx * 4, &inst) || !inst)
        return 0;
    root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT);
    if (!root || !mem_u32(root + ROOT_INV, &inv) || !inv || !mem_read(inv + INV_SLOTS, b, sizeof b))
        return 0;
    for (int i = 0; i < INV_NSLOTS; i++) {
        uint32_t def, ins, g = 0;
        aimp_memcpy(&def, b + i * INV_SLOT_SIZE, 4);
        aimp_memcpy(&ins, b + i * INV_SLOT_SIZE + 4, 4);
        if (def && ins == inst && mem_u32(def + DEF_GUID, &g))
            return g;
    }
    return 0;
}

static void send(uint32_t kind, uint32_t guid)
{
    WeaponEvt ev = { kind, guid };
    if (g_net && g_net->mode)
        net_event(g_net, AIMP_EVT_WEAPON, 0, &ev, sizeof ev);
}

/* ¿Hay datos frescos del otro jugador? */
static int remote_alive(int64_t now, int64_t freq)
{
    return g_net && g_net->mode && g_net->have_remote && now - g_net->remote_at < freq * 3;
}

void weapons_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level)
{
    if (!g_on || !player || now - g_last < freq / 10)
        return;
    g_last = now;
    if (player != g_player) {
        /* corrida 96: al cargar, el juego tiene la escopeta en la mano ~0,1 s; 3 s sin mandar nada */
        g_player = player;
        g_local_hold = 0;
        g_settle_until = now + freq * 3;
    }
    /* distancia al otro, con histéresis */
    int alive = remote_alive(now, freq);
    if (alive && g_net->remote.level == level) {
        float p[3];
        if (mem_read(player + CHR_POS, p, sizeof p)) {
            float dx = p[0] - g_net->remote.pos[0], dy = p[1] - g_net->remote.pos[1], dz = p[2] - g_net->remote.pos[2];
            float d2 = dx * dx + dy * dy + dz * dz;
            if (g_near && d2 > 25.0f)
                g_near = 0;
            else if (!g_near && d2 < 16.0f)
                g_near = 1;
        }
    } else
        g_near = 0;
    uint32_t eq = 0, h = held(player, &eq);
    if (now < g_settle_until) {
        g_local_hold = h;
        return;
    }
    if (h == g_local_hold)
        return;
    char nm[40];
    if (h) {
        wname(h, nm, sizeof nm);
        int *own = owner_of(h);
        int free_ = !alive || death_remote_is_downed();
        int ok = !own || *own != OWN_REMOTE || free_ || (g_near && g_remote_hold != h);
        if (own && *own == OWN_REMOTE && g_remote_hold == h && !free_)
            ok = 0; /* el otro la tiene en la mano */
        if (!ok) {
            log_printf("WEAPONS: %s no está disponible (%s); se guarda", nm, g_remote_hold == h ? "la tiene el otro en la mano" : "la tiene el otro y está lejos");
            if (g_holster && eq)
                g_holster((void *)eq);
            g_local_hold = 0;
            return;
        }
        if (own)
            *own = OWN_LOCAL;
        log_printf("WEAPONS: saca %s (ahora es el dueño)%s", nm, g_near ? ", cerca del otro" : "");
        send(WEAPON_EQUIP, h);
    } else if (g_local_hold) {
        wname(g_local_hold, nm, sizeof nm);
        log_printf("WEAPONS: guarda %s", nm);
        send(WEAPON_UNEQUIP, g_local_hold);
    }
    g_local_hold = h;
}

void weapons_remote_event(const AimpEvt *e)
{
    WeaponEvt ev;
    if (!g_on || e->size < sizeof ev)
        return;
    aimp_memcpy(&ev, e->data, sizeof ev);
    char nm[40];
    wname(ev.guid, nm, sizeof nm);
    if (ev.kind == WEAPON_EQUIP) {
        g_remote_hold = ev.guid;
        int *own = owner_of(ev.guid);
        if (g_local_hold == ev.guid && g_net && g_net->mode == 1) {
            /* los dos al mismo tiempo: gana el host; el cliente la guarda al recibir el EQUIP del host */
            log_printf("WEAPONS: el otro sacó %s a la vez; gana el host (acá): queda acá", nm);
            return;
        }
        if (own)
            *own = OWN_REMOTE;
        log_printf("WEAPONS: el otro sacó %s (ahora es el dueño)", nm);
        if (g_local_hold == ev.guid && g_player) {
            uint32_t eq = 0;
            held(g_player, &eq);
            log_printf("WEAPONS: %s la tenía también acá: gana el host; se guarda", nm);
            if (g_holster && eq)
                g_holster((void *)eq);
            g_local_hold = 0;
        }
    } else if (ev.kind == WEAPON_UNEQUIP) {
        if (g_remote_hold == ev.guid)
            g_remote_hold = 0;
        log_printf("WEAPONS: el otro guardó %s (sigue siendo el dueño)", nm);
    }
}

void weapons_install(AimpNet *net)
{
    g_net = net;
    g_on = aimp_env_int(L"AIMP_SYNC_WEAPONS", 0);
    if (!g_on)
        return;
    uint8_t p[sizeof HOLSTER_PROLOGUE];
    if (mem_read(g_aimp_exe + AIMP_RVA_HOLSTER, p, sizeof p) && aimp_code_cmp(p, HOLSTER_PROLOGUE, sizeof p) == 0)
        g_holster = (HolsterFn)(void *)(g_aimp_exe + AIMP_RVA_HOLSTER);
    else
        log_printf("WEAPONS: el prólogo de FUN_007c3980 (guardar el arma) no coincide: no se puede bloquear");
    log_printf("WEAPONS: armas de a una (AIMP_SYNC_WEAPONS)");
}
