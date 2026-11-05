#ifndef AIMP_NET_H
#define AIMP_NET_H
#include "aimp.h"

/* Fase 3: UDP mínimo con el estado del jugador local (protocolo propio v1).
 * Sin threads: net_tick se llama una vez por frame desde el hook del loop.
 * Configuración (entorno o aimp.cfg):
 *   AIMP_NET=host|join      (vacío = red apagada)
 *   AIMP_NET_PORT=47811     (host: puerto donde escucha; join: puerto del host por defecto)
 *   AIMP_NET_PEER=ip:puerto (join: dirección del host, p. ej. la IP de Tailscale)
 *   AIMP_NET_HZ=20          (paquetes de estado por segundo)
 */

#define AIMP_NET_MAGIC 0x504D4941u /* "AIMP" */
#define AIMP_NET_VERSION 1
#define AIMP_PKT_STATE 2
#define AIMP_PKT_BYE 3
#define AIMP_FLAG_PLAYER 1u /* el emisor tiene jugador en el nivel */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t flags;
    uint32_t seq;
    uint32_t time_ms;   /* reloj del emisor (GetTickCount) */
    uint32_t level;     /* índice del nivel actual (hallazgo 09) */
    uint32_t sim_frame; /* contador de simulación (hallazgo 04) */
    float pos[3];       /* Character+0x70 */
    float yaw;          /* Character+0x174 */
    float cam_q[4];     /* cámara activa +0x2C */
    float cam_pos[3];   /* cámara activa +0x3C */
    uint8_t char_id[8]; /* Character+0x54 (id de 8 bytes de los RPCs, hallazgo 11) */
} AimpPkt;              /* 76 bytes */
#pragma pack(pop)

typedef struct {
    int mode; /* 0 apagada, 1 host, 2 join */
    uintptr_t sock;
    uint32_t peer_ip;    /* orden de red */
    uint16_t peer_port;  /* orden de red */
    int have_peer;
    uint32_t seq;
    int64_t send_ticks, last_send, freq;
    /* estadísticas del período y totales */
    uint32_t sent, recv, bad, lost, sent_total, recv_total, lost_total;
    uint32_t last_seq_in;
    int polled;           /* ya hubo un net_poll (el primero descarta lo viejo) */
    uint32_t stale_total; /* descartados en el primer net_poll */
    int have_remote;
    AimpPkt remote;
    int64_t remote_at; /* ticks de QPC cuando llegó */
    char peer_str[32];
} AimpNet;

/* Lee la config y abre el socket. Devuelve 0 si la red queda apagada. */
int net_init(AimpNet *n, int64_t qpc_freq);
/* Por frame: recibir todo lo pendiente. */
void net_poll(AimpNet *n, int64_t now);
/* ¿Toca mandar el estado? (así el que llama arma el paquete solo cuando hace falta) */
int net_due(const AimpNet *n, int64_t now);
void net_send(AimpNet *n, int64_t now, const AimpPkt *local);
/* Una línea de log con las estadísticas del período (y las resetea). */
void net_status(AimpNet *n, int64_t now);
void net_close(AimpNet *n);
/* "a.b.c.d:puerto" -> ip/puerto en orden de red. 1 si se pudo. */
int net_parse_addr(const char *s, uint32_t *ip, uint16_t *port, uint16_t def_port);

#endif
