/* Huella de la partida cargada (ver session.h). */
#include "session.h"
#include "game.h"
#include "log.h"

static AimpNet *g_net;

static float sq(float x)
{
    float r = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 20 && x > 0.0f; i++)
        r = 0.5f * (r + x / r);
    return x > 0.0f ? r : 0.0f;
}
static SessionEvt g_mine, g_theirs;
static int g_have_mine, g_have_theirs, g_match = -1, g_due, g_had_remote;
static uint32_t g_player;
static int64_t g_appeared;

int session_match(void) { return g_match; }

static void compare(void)
{
    if (!g_have_mine || !g_have_theirs)
        return;
    float dx = g_mine.pos[0] - g_theirs.pos[0], dy = g_mine.pos[1] - g_theirs.pos[1], dz = g_mine.pos[2] - g_theirs.pos[2];
    float d2 = dx * dx + dy * dy + dz * dz;
    int m = g_mine.level == g_theirs.level && d2 < 0.25f;
    if (m != g_match) {
        if (m)
            log_printf("SESSION: misma partida (nivel %u, aparición a %.2f m)", g_mine.level, (double)sq(d2));
        else
            log_printf("SESSION: PARTIDAS DISTINTAS: acá nivel %u aparición (%.2f, %.2f, %.2f); el otro nivel %u (%.2f, %.2f, %.2f). "
                       "Carguen el mismo save (saves/README.md)",
                       g_mine.level, (double)g_mine.pos[0], (double)g_mine.pos[1], (double)g_mine.pos[2], g_theirs.level,
                       (double)g_theirs.pos[0], (double)g_theirs.pos[1], (double)g_theirs.pos[2]);
    }
    g_match = m;
}

void session_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level)
{
    if (!g_net || !g_net->mode)
        return;
    if (player != g_player) {
        g_player = player;
        g_have_mine = 0;
        g_appeared = now;
    }
    /* la posición de aparición: el primer frame con jugador (el save la fija) */
    if (player && !g_have_mine && mem_read(player + CHR_POS, g_mine.pos, sizeof g_mine.pos) && mem_f32(player + CHR_YAW, &g_mine.yaw)) {
        g_mine.level = level;
        g_have_mine = 1;
        g_due = 1;
        compare();
    }
    if (g_net->have_remote && !g_had_remote)
        g_due = 1;
    g_had_remote = g_net->have_remote;
    if (g_due && g_have_mine && g_net->have_remote && now - g_appeared > freq) {
        g_due = 0;
        net_event(g_net, AIMP_EVT_SESSION, level, &g_mine, sizeof g_mine);
    }
}

void session_remote_event(const AimpEvt *e)
{
    if (e->size < sizeof g_theirs)
        return;
    aimp_memcpy(&g_theirs, e->data, sizeof g_theirs);
    g_have_theirs = 1;
    g_match = -1; /* que se loguee de nuevo */
    compare();
}

void session_install(AimpNet *net) { g_net = net; }
