/* Test de la fase 3 (correr con wine): host y cliente del protocolo en el mismo
 * proceso, por UDP en 127.0.0.1. Usa el log real (AIMP_LOG). */
#include <stdio.h>
#include <string.h>
#include "aimp.h"
#include "log.h"
#include "net.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

static int g_fail;
/* La condición se evalúa UNA vez (puede tener efectos, p. ej. net_init). */
#define CHECK(cond, ...)                          \
    do {                                          \
        int ok_ = (cond);                         \
        printf(ok_ ? "  ok    " : "  FALLA ");    \
        printf(__VA_ARGS__);                      \
        printf("\n");                             \
        if (!ok_)                                 \
            g_fail++;                             \
    } while (0)

static AimpPkt state(float x, float yaw, uint32_t level)
{
    AimpPkt p;
    memset(&p, 0, sizeof p);
    p.flags = AIMP_FLAG_PLAYER;
    p.level = level;
    p.pos[0] = x;
    p.pos[1] = 14.5f;
    p.pos[2] = -20.0f;
    p.yaw = yaw;
    memcpy(p.char_id, "\x2f\xf7\x22\xb2\xb8\x9a\x51\x72", 8);
    return p;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    LARGE_INTEGER f, now;
    QueryPerformanceFrequency(&f);

    printf("[parseo]\n");
    uint32_t ip;
    uint16_t port;
    CHECK(net_parse_addr("100.64.1.2:47811", &ip, &port, 1) && ip == 0x02014064u && port == 0xC3BA,
          "100.64.1.2:47811 -> ip %08x puerto %04x", ip, port);
    CHECK(net_parse_addr("127.0.0.1", &ip, &port, 47811) && port == 0xC3BA, "puerto por defecto");
    CHECK(!net_parse_addr("1.2.3", &ip, &port, 1) && !net_parse_addr("1.2.3.256:1", &ip, &port, 1) &&
              !net_parse_addr("a.b.c.d:1", &ip, &port, 1) && !net_parse_addr("1.2.3.4:70000", &ip, &port, 1),
          "direcciones inválidas rechazadas");
    CHECK(sizeof(AimpPkt) == 76, "tamaño del paquete %u", (unsigned)sizeof(AimpPkt));

    printf("[host + join por 127.0.0.1]\n");
    AimpNet host, cli;
    SetEnvironmentVariableW(L"AIMP_NET", L"host");
    SetEnvironmentVariableW(L"AIMP_NET_PORT", L"47899");
    SetEnvironmentVariableW(L"AIMP_NET_HZ", L"100");
    CHECK(net_init(&host, f.QuadPart), "host abierto");
    SetEnvironmentVariableW(L"AIMP_NET", L"join");
    SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47899");
    CHECK(net_init(&cli, f.QuadPart), "cliente abierto");

    for (int i = 0; i < 60; i++) {
        QueryPerformanceCounter(&now);
        AimpPkt a = state(1.0f + (float)i, 0.5f, 10), b = state(-5.0f - (float)i, -1.0f, 10);
        net_poll(&cli, now.QuadPart);
        net_send(&cli, now.QuadPart, &a);
        net_poll(&host, now.QuadPart);
        net_send(&host, now.QuadPart, &b);
        Sleep(12);
    }
    QueryPerformanceCounter(&now);
    net_poll(&host, now.QuadPart);
    net_poll(&cli, now.QuadPart);
    CHECK(host.have_peer && host.have_remote && host.remote.pos[0] > 30.0f && host.remote.level == 10 &&
              memcmp(host.remote.char_id, "\x2f\xf7\x22\xb2", 4) == 0,
          "el host recibe al cliente: x=%.1f nivel %u (recibidos %u)", (double)host.remote.pos[0], host.remote.level,
          host.recv_total);
    CHECK(cli.have_remote && cli.remote.pos[0] < -30.0f && (cli.remote.flags & AIMP_FLAG_PLAYER),
          "el cliente recibe al host: x=%.1f (recibidos %u)", (double)cli.remote.pos[0], cli.recv_total);
    CHECK(host.lost_total == 0 && cli.lost_total == 0 && host.bad == 0 && cli.bad == 0, "sin pérdidas ni inválidos");
    net_status(&host, now.QuadPart);
    net_status(&cli, now.QuadPart);
    net_close(&cli); /* manda BYE */
    Sleep(30);
    QueryPerformanceCounter(&now);
    net_poll(&host, now.QuadPart);
    CHECK(!host.have_peer && !host.have_remote, "el host procesa el BYE del cliente");
    net_close(&host);

    printf("[cadencia: 20 Hz con frames de 16,5 ms (tiempo simulado)]\n");
    {
        AimpNet r;
        SetEnvironmentVariableW(L"AIMP_NET", L"join");
        SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47897"); /* nadie escucha: no importa */
        SetEnvironmentVariableW(L"AIMP_NET_HZ", L"20");
        CHECK(net_init(&r, f.QuadPart), "cliente abierto");
        AimpPkt a = state(0.0f, 0.0f, 10);
        QueryPerformanceCounter(&now);
        int64_t base = now.QuadPart, frame = f.QuadPart * 165 / 10000;
        int sends = 0;
        for (int i = 0; i < 606; i++) { /* ~10 s */
            int64_t t = base + (int64_t)i * frame;
            if (net_due(&r, t)) {
                net_send(&r, t, &a);
                sends++;
            }
        }
        CHECK(sends >= 199 && sends <= 201, "%d envíos en 10 s (esperado 200; con la cadencia vieja salían ~150)", sends);
        /* Después de una pausa de 2 s (carga): un envío y sigue la cadencia normal, sin ráfaga. */
        int64_t t0 = base + 606 * frame + 2 * f.QuadPart;
        int burst = 0;
        for (int i = 0; i < 6; i++) {
            int64_t t = t0 + (int64_t)i * frame;
            if (net_due(&r, t)) {
                net_send(&r, t, &a);
                burst++;
            }
        }
        CHECK(burst == 2, "después de una pausa: %d envíos en 6 frames (esperado 2)", burst);
        net_close(&r);
    }

    printf("[paquetes que llegan antes del primer frame]\n");
    {
        AimpNet h2, c2;
        SetEnvironmentVariableW(L"AIMP_NET", L"host");
        SetEnvironmentVariableW(L"AIMP_NET_PORT", L"47898");
        CHECK(net_init(&h2, f.QuadPart), "host abierto");
        SetEnvironmentVariableW(L"AIMP_NET", L"join");
        SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47898");
        CHECK(net_init(&c2, f.QuadPart), "cliente abierto");
        QueryPerformanceCounter(&now);
        int64_t t = now.QuadPart;
        AimpPkt a = state(7.0f, 0.0f, 10);
        for (int i = 0; i < 30; i++, t += c2.send_ticks) /* el host todavía no lee */
            net_send(&c2, t, &a);
        Sleep(30);
        net_poll(&h2, t);
        CHECK(h2.stale_total == 30 && !h2.have_peer && !h2.have_remote, "primer poll: %u descartados, sin par ni estado",
              h2.stale_total);
        a.pos[0] = 9.0f;
        for (int i = 0; i < 5; i++, t += c2.send_ticks)
            net_send(&c2, t, &a);
        Sleep(30);
        net_poll(&h2, t);
        CHECK(h2.have_peer && h2.have_remote && h2.recv_total == 5 && h2.lost_total == 0 && h2.remote.pos[0] == 9.0f,
              "después: recibidos %u, perdidos %u (el hueco con los viejos no cuenta), x=%.1f", h2.recv_total,
              h2.lost_total, (double)h2.remote.pos[0]);
        net_close(&c2);
        net_close(&h2);
    }
    log_close();
    printf("%s (%d fallas)\n", g_fail ? "TEST_NET CON FALLAS" : "TEST_NET OK", g_fail);
    return g_fail ? 1 : 0;
}
