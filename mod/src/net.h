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
#define AIMP_PKT_EVENT 4    /* evento discreto (fase 5): se manda 3 veces, el receptor descarta repetidos */
#define AIMP_FLAG_PLAYER 1u /* el emisor tiene jugador en el nivel */
#define AIMP_EVT_USE 1      /* "usar" del jugador (RPC A 0x26, hallazgo 11): objetivo[8] | personaje[8] | flag[4] */
#define AIMP_EVT_DATA 52
#define AIMP_EVT_QUEUE 16
#define AIMP_EVT_REPEAT 3

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

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t type; /* AIMP_PKT_EVENT */
    uint16_t flags;
    uint32_t seq;     /* secuencia de eventos (aparte de la de estados) */
    uint32_t time_ms;
    uint16_t kind;    /* AIMP_EVT_* */
    uint16_t size;    /* bytes válidos en data */
    uint32_t level;   /* nivel del emisor: el receptor lo aplica solo en el mismo nivel */
    uint8_t data[AIMP_EVT_DATA];
} AimpEvt;            /* 76 bytes */
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
    /* eventos (fase 5) */
    uint32_t evt_seq;                       /* último enviado */
    AimpEvt out[AIMP_EVT_QUEUE];             /* pendientes de (re)envío */
    uint8_t out_left[AIMP_EVT_QUEUE];        /* envíos que faltan de cada uno */
    uint32_t evt_last_in;                   /* mayor secuencia recibida */
    uint64_t evt_window;                    /* bit i: llegó evt_last_in - i (descarta repetidos fuera de orden) */
    AimpEvt in[AIMP_EVT_QUEUE];              /* recibidos, sin aplicar */
    uint32_t in_head, in_tail;              /* anillo: head = próximo a leer */
    uint32_t evt_sent, evt_recv, evt_dup;   /* totales */
    uint32_t evt_logged;                    /* firma de los totales ya logueados */
} AimpNet;

/* Lee la config y abre el socket. Devuelve 0 si la red queda apagada. */
int net_init(AimpNet *n, int64_t qpc_freq);
/* Por frame: recibir todo lo pendiente. */
void net_poll(AimpNet *n, int64_t now);
/* ¿Toca mandar el estado? (así el que llama arma el paquete solo cuando hace falta) */
int net_due(const AimpNet *n, int64_t now);
void net_send(AimpNet *n, int64_t now, const AimpPkt *local);
/* Encola un evento para mandar (sale en los próximos AIMP_EVT_REPEAT net_send_events). 0 si no hay par o lugar. */
int net_event(AimpNet *n, uint16_t kind, uint32_t level, const void *data, uint16_t size);
/* Por frame: manda una copia de cada evento pendiente. */
void net_send_events(AimpNet *n);
/* Saca el próximo evento recibido. 1 si había. */
int net_take_event(AimpNet *n, AimpEvt *out);
/* Una línea de log con las estadísticas del período (y las resetea). */
void net_status(AimpNet *n, int64_t now);
void net_close(AimpNet *n);
/* "a.b.c.d:puerto" -> ip/puerto en orden de red. 1 si se pudo. */
int net_parse_addr(const char *s, uint32_t *ip, uint16_t *port, uint16_t def_port);

#endif
