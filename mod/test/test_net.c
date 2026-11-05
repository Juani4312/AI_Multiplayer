/* Test de la fase 3 (correr con wine): host y cliente del protocolo en el mismo
 * proceso, por UDP en 127.0.0.1. Usa el log real (AIMP_LOG). */
#include <winsock2.h> /* sendto directo en el test de eventos (net.c resuelve Winsock en runtime) */
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

    printf("[eventos: cada uno llega una vez, aunque se mande 3 veces y fuera de orden]\n");
    {
        AimpNet h3, c3;
        SetEnvironmentVariableW(L"AIMP_NET", L"host");
        SetEnvironmentVariableW(L"AIMP_NET_PORT", L"47896");
        CHECK(net_init(&h3, f.QuadPart), "host abierto");
        SetEnvironmentVariableW(L"AIMP_NET", L"join");
        SetEnvironmentVariableW(L"AIMP_NET_PEER", L"127.0.0.1:47896");
        CHECK(net_init(&c3, f.QuadPart), "cliente abierto");
        QueryPerformanceCounter(&now);
        int64_t t = now.QuadPart;
        AimpPkt a = state(1.0f, 0.0f, 16);
        net_poll(&h3, t); /* primer poll: no hay nada viejo */
        net_send(&c3, t, &a); /* el host aprende la dirección del cliente */
        Sleep(20);
        net_poll(&h3, t);
        CHECK(h3.have_peer, "el host conoce al cliente");
        static const uint8_t use1[20] = { 0x04, 0x41, 0x06, 0xb1, 0x6f, 0x52, 0x99, 0x59, 0x2f, 0xf7, 0x22, 0xb2, 0x09, 0x9d, 0x5b, 0x9b, 0, 0x7c, 0x58, 0x01 };
        uint8_t use2[20];
        memcpy(use2, use1, 20);
        use2[0] = 0xee;
        CHECK(net_event(&c3, AIMP_EVT_USE, 16, use1, 20) && net_event(&c3, AIMP_EVT_USE, 16, use2, 20), "dos eventos encolados");
        for (int i = 0; i < AIMP_EVT_REPEAT + 1; i++) /* uno más: ya no queda nada para mandar */
            net_send_events(&c3);
        Sleep(30);
        net_poll(&h3, t);
        AimpEvt e1, e2, e3;
        int got1 = net_take_event(&h3, &e1), got2 = net_take_event(&h3, &e2), got3 = net_take_event(&h3, &e3);
        CHECK(got1 && got2 && !got3, "llegaron exactamente 2 eventos (%d %d %d)", got1, got2, got3);
        CHECK(got1 && got2 && e1.kind == AIMP_EVT_USE && e1.size == 20 && e1.level == 16 && memcmp(e1.data, use1, 20) == 0 &&
                  memcmp(e2.data, use2, 20) == 0,
              "contenido, tipo y nivel intactos");
        CHECK(h3.evt_recv == 2 && h3.evt_dup == 4 && c3.evt_sent == 2, "recibidos %u, repetidos descartados %u, enviados %u",
              h3.evt_recv, h3.evt_dup, c3.evt_sent);
        /* fuera de orden: llega el 4 y después el 3 */
        AimpEvt o;
        memset(&o, 0, sizeof o);
        o.magic = AIMP_NET_MAGIC;
        o.version = AIMP_NET_VERSION;
        o.type = AIMP_PKT_EVENT;
        o.kind = AIMP_EVT_USE;
        o.size = 20;
        o.level = 16;
        AimpNet *hp = &h3;
        uint32_t before = hp->evt_recv;
        /* se los entrega por el socket del cliente, armados a mano */
        SOCKET cs = (SOCKET)c3.sock;
        struct sockaddr_in to;
        memset(&to, 0, sizeof to);
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = 0x0100007Fu;
        to.sin_port = (uint16_t)((47896 >> 8) | ((47896 & 0xFF) << 8));
        o.seq = 4;
        sendto(cs, (const char *)&o, sizeof o, 0, (struct sockaddr *)&to, sizeof to);
        o.seq = 3;
        sendto(cs, (const char *)&o, sizeof o, 0, (struct sockaddr *)&to, sizeof to);
        o.seq = 3; /* repetido */
        sendto(cs, (const char *)&o, sizeof o, 0, (struct sockaddr *)&to, sizeof to);
        Sleep(30);
        net_poll(hp, t);
        CHECK(hp->evt_recv == before + 2, "el 4 y el 3 se aceptan aunque lleguen al revés, y el 3 repetido no (%u)",
              hp->evt_recv - before);
        /* el otro reinicia su secuencia (vuelve a 1) mucho más atrás */
        hp->evt_last_in = 500;
        hp->evt_window = 1;
        o.seq = 1;
        sendto(cs, (const char *)&o, sizeof o, 0, (struct sockaddr *)&to, sizeof to);
        Sleep(30);
        net_poll(hp, t);
        CHECK(hp->evt_recv == before + 3 && hp->evt_last_in == 1, "reinicio de la secuencia aceptado (last %u)", hp->evt_last_in);
        AimpNet solo;
        memset(&solo, 0, sizeof solo);
        CHECK(!net_event(&solo, AIMP_EVT_USE, 16, use1, 20), "sin red o sin par no se encola nada");
        net_close(&c3);
        net_close(&h3);
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
