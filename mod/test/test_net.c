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
    log_close();
    printf("%s (%d fallas)\n", g_fail ? "TEST_NET CON FALLAS" : "TEST_NET OK", g_fail);
    return g_fail ? 1 : 0;
}
