/* Fase 3: UDP mínimo. Las funciones de Winsock se resuelven en tiempo de ejecución
 * (ws2_32 ya la carga AI.exe), así el proxy no suma imports estáticos. */
#include "net.h"
#include "log.h"
#include <winsock2.h>

typedef int(WSAAPI *PWSAStartup)(WORD, LPWSADATA);
typedef SOCKET(WSAAPI *Psocket)(int, int, int);
typedef int(WSAAPI *Pbind)(SOCKET, const struct sockaddr *, int);
typedef int(WSAAPI *Psendto)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int(WSAAPI *Precvfrom)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int(WSAAPI *Pioctlsocket)(SOCKET, long, u_long *);
typedef int(WSAAPI *Pclosesocket)(SOCKET);
typedef int(WSAAPI *PWSAGetLastError)(void);

static struct {
    PWSAStartup WSAStartup;
    Psocket socket;
    Pbind bind;
    Psendto sendto;
    Precvfrom recvfrom;
    Pioctlsocket ioctlsocket;
    Pclosesocket closesocket;
    PWSAGetLastError WSAGetLastError;
} W;

static uint16_t bswap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

static int load_ws2(void)
{
    HMODULE h = GetModuleHandleW(L"ws2_32.dll");
    if (!h)
        h = LoadLibraryW(L"ws2_32.dll");
    if (!h)
        return 0;
#define R(name) (W.name = (void *)GetProcAddress(h, #name)) != NULL
    return R(WSAStartup) && R(socket) && R(bind) && R(sendto) && R(recvfrom) && R(ioctlsocket) && R(closesocket) &&
           R(WSAGetLastError);
#undef R
}

int net_parse_addr(const char *s, uint32_t *ip, uint16_t *port, uint16_t def_port)
{
    uint32_t parts[4] = { 0 }, p = def_port;
    int k = 0, digits = 0;
    for (; *s && *s != ':'; s++) {
        if (*s == '.') {
            if (!digits || ++k > 3)
                return 0;
            digits = 0;
        } else if (*s >= '0' && *s <= '9') {
            parts[k] = parts[k] * 10 + (uint32_t)(*s - '0');
            if (parts[k] > 255 || ++digits > 3)
                return 0;
        } else {
            return 0;
        }
    }
    if (k != 3 || !digits)
        return 0;
    if (*s == ':') {
        p = 0;
        for (s++; *s; s++) {
            if (*s < '0' || *s > '9')
                return 0;
            p = p * 10 + (uint32_t)(*s - '0');
            if (p > 65535)
                return 0;
        }
    }
    if (p == 0)
        return 0;
    /* orden de red: el primer octeto en el byte más bajo (x86 little-endian) */
    *ip = parts[0] | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
    *port = bswap16((uint16_t)p);
    return 1;
}

static void addr_str(char *out, size_t cap, uint32_t ip, uint16_t port)
{
    aimp_snprintf(out, cap, "%u.%u.%u.%u:%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24,
                  bswap16(port));
}

int net_init(AimpNet *n, int64_t qpc_freq)
{
    wchar_t wmode[16], wpeer[64];
    char peer[64];
    aimp_memset(n, 0, sizeof *n);
    n->sock = (uintptr_t)INVALID_SOCKET;
    n->freq = qpc_freq;
    if (!aimp_env_str(L"AIMP_NET", wmode, 16))
        return 0;
    if (wmode[0] == L'h')
        n->mode = 1;
    else if (wmode[0] == L'j')
        n->mode = 2;
    else
        return 0;
    int port = aimp_env_int(L"AIMP_NET_PORT", 47811);
    int hz = aimp_env_int(L"AIMP_NET_HZ", 20);
    if (hz < 1)
        hz = 1;
    if (hz > 120)
        hz = 120;
    n->send_ticks = qpc_freq / hz;
    if (port <= 0 || port > 65535)
        port = 47811;
    if (n->mode == 2) {
        if (!aimp_env_str(L"AIMP_NET_PEER", wpeer, 64)) {
            log_printf("NET: AIMP_NET=join sin AIMP_NET_PEER; red apagada");
            n->mode = 0;
            return 0;
        }
        aimp_utf8(peer, sizeof peer, wpeer);
        if (!net_parse_addr(peer, &n->peer_ip, &n->peer_port, (uint16_t)port)) {
            log_printf("NET: AIMP_NET_PEER inválido (\"%s\"; formato a.b.c.d:puerto); red apagada", peer);
            n->mode = 0;
            return 0;
        }
        n->have_peer = 1;
        addr_str(n->peer_str, sizeof n->peer_str, n->peer_ip, n->peer_port);
    }
    if (!load_ws2()) {
        log_printf("NET: no se pudo resolver Winsock; red apagada");
        n->mode = 0;
        return 0;
    }
    WSADATA wd;
    if (W.WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        log_printf("NET: WSAStartup falló; red apagada");
        n->mode = 0;
        return 0;
    }
    SOCKET s = W.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        log_printf("NET: socket() falló (%d); red apagada", W.WSAGetLastError());
        n->mode = 0;
        return 0;
    }
    struct sockaddr_in a;
    aimp_memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = 0; /* INADDR_ANY */
    a.sin_port = n->mode == 1 ? bswap16((uint16_t)port) : 0;
    if (W.bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
        log_printf("NET: bind al puerto %d falló (%d); red apagada", n->mode == 1 ? port : 0, W.WSAGetLastError());
        W.closesocket(s);
        n->mode = 0;
        return 0;
    }
    u_long nb = 1;
    W.ioctlsocket(s, FIONBIO, &nb);
    n->sock = (uintptr_t)s;
    if (n->mode == 1)
        log_printf("NET: host escuchando en UDP %d, %d paquetes/s", port, hz);
    else
        log_printf("NET: join hacia %s, %d paquetes/s", n->peer_str, hz);
    return 1;
}

static void handle_packet(AimpNet *n, const AimpPkt *p, int len, uint32_t ip, uint16_t port, int64_t now)
{
    if (len < (int)sizeof *p || p->magic != AIMP_NET_MAGIC || p->version != AIMP_NET_VERSION) {
        n->bad++;
        return;
    }
    if (n->mode == 1 && (!n->have_peer || ip != n->peer_ip || port != n->peer_port)) {
        /* El host aprende la dirección del cliente del primer paquete válido. */
        n->peer_ip = ip;
        n->peer_port = port;
        n->have_peer = 1;
        n->have_remote = 0;
        n->evt_last_in = 0;
        n->evt_window = 0;
        addr_str(n->peer_str, sizeof n->peer_str, ip, port);
        log_printf("NET: cliente conectado desde %s", n->peer_str);
    } else if (n->mode == 2 && (ip != n->peer_ip || port != n->peer_port)) {
        n->bad++; /* el cliente solo acepta al host configurado */
        return;
    }
    if (p->type == AIMP_PKT_BYE) {
        log_printf("NET: el otro extremo se despidió (%s)", n->peer_str);
        n->have_remote = 0;
        n->evt_last_in = 0; /* si vuelve, su secuencia de eventos empieza de nuevo */
        n->evt_window = 0;
        if (n->mode == 1)
            n->have_peer = 0;
        return;
    }
    if (p->type == AIMP_PKT_EVENT) {
        const AimpEvt *e = (const AimpEvt *)p;
        uint32_t s = e->seq;
        if (s > n->evt_last_in) {
            uint32_t sh = s - n->evt_last_in;
            n->evt_window = sh >= 64 ? 0 : n->evt_window << sh;
            n->evt_window |= 1;
            n->evt_last_in = s;
        } else if (n->evt_last_in - s >= 64) { /* el otro reinició su secuencia */
            n->evt_last_in = s;
            n->evt_window = 1;
        } else if (n->evt_window & (1ull << (n->evt_last_in - s))) {
            n->evt_dup++; /* repetido (cada evento se manda AIMP_EVT_REPEAT veces) */
            return;
        } else {
            n->evt_window |= 1ull << (n->evt_last_in - s);
        }
        uint32_t next = (n->in_tail + 1) % AIMP_EVT_QUEUE;
        if (next == n->in_head) {
            n->bad++; /* cola llena: no se aplicaron a tiempo */
            return;
        }
        n->in[n->in_tail] = *e;
        n->in_tail = next;
        n->evt_recv++;
        return;
    }
    if (p->type != AIMP_PKT_STATE) {
        n->bad++;
        return;
    }
    if (n->have_remote) {
        if (p->seq <= n->last_seq_in)
            return; /* duplicado o fuera de orden: nos quedamos con el más nuevo */
        if (p->seq > n->last_seq_in + 1) {
            n->lost += p->seq - n->last_seq_in - 1;
            n->lost_total += p->seq - n->last_seq_in - 1;
        }
    } else {
        log_printf("NET: primer estado del otro jugador: nivel %u, %s, pos=(%.2f, %.2f, %.2f) yaw=%.3f", p->level,
                   (p->flags & AIMP_FLAG_PLAYER) ? "con jugador" : "sin jugador", (double)p->pos[0],
                   (double)p->pos[1], (double)p->pos[2], (double)p->yaw);
    }
    n->last_seq_in = p->seq;
    n->remote = *p;
    n->remote_at = now;
    n->have_remote = 1;
    n->recv++;
    n->recv_total++;
}

void net_poll(AimpNet *n, int64_t now)
{
    if (!n->mode)
        return;
    SOCKET s = (SOCKET)n->sock;
    /* El socket se abre al cargar la DLL y se lee desde el primer frame (~35 s después).
     * Lo que llegó en ese tiempo es viejo: el primer poll lo descarta sin procesarlo. Si
     * no, se toma un estado de hace medio minuto y el hueco que dejó el kernel con el
     * buffer lleno se cuenta como perdidos (corrida por Tailscale del 2026-11-05: 361). */
    int first = !n->polled;
    n->polled = 1;
    for (int i = 0; i < (first ? 8192 : 1024); i++) {
        AimpPkt buf[2]; /* lugar de sobra: paquetes más largos se truncan y cuentan como malos */
        struct sockaddr_in from;
        int fl = sizeof from;
        int r = W.recvfrom(s, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (r == SOCKET_ERROR) {
            int e = W.WSAGetLastError();
            if (e != WSAEWOULDBLOCK && e != WSAECONNRESET)
                n->bad++;
            if (e == WSAEWOULDBLOCK)
                break;
            continue;
        }
        if (first) {
            n->stale_total++;
            continue;
        }
        handle_packet(n, buf, r, from.sin_addr.s_addr, from.sin_port, now);
    }
    if (first && n->stale_total)
        log_printf("NET: descartados %u paquetes que llegaron antes del primer frame", n->stale_total);
}

int net_due(const AimpNet *n, int64_t now) { return n->mode && n->have_peer && now - n->last_send >= n->send_ticks; }

void net_send(AimpNet *n, int64_t now, const AimpPkt *local)
{
    if (!net_due(n, now))
        return;
    SOCKET s = (SOCKET)n->sock;
    AimpPkt p = *local;
    p.magic = AIMP_NET_MAGIC;
    p.version = AIMP_NET_VERSION;
    p.type = AIMP_PKT_STATE;
    p.seq = ++n->seq;
    p.time_ms = GetTickCount();
    struct sockaddr_in to;
    aimp_memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = n->peer_ip;
    to.sin_port = n->peer_port;
    if (W.sendto(s, (const char *)&p, sizeof p, 0, (struct sockaddr *)&to, sizeof to) == (int)sizeof p) {
        n->sent++;
        n->sent_total++;
    }
    /* Cadencia fija: el próximo envío se cuenta desde el horario previsto, no desde el
     * frame en que salió. Si no, a ~60 fps muchas veces hay que esperar 4 frames
     * (66,7 ms) y salen ~16/s en vez de 20 (medido en la laptop y en papito). */
    n->last_send += n->send_ticks;
    if (now - n->last_send >= n->send_ticks) /* muy atrasados (carga, pausa): sin ráfagas */
        n->last_send = now;
}

static void peer_addr(const AimpNet *n, struct sockaddr_in *to)
{
    aimp_memset(to, 0, sizeof *to);
    to->sin_family = AF_INET;
    to->sin_addr.s_addr = n->peer_ip;
    to->sin_port = n->peer_port;
}

int net_event(AimpNet *n, uint16_t kind, uint32_t level, const void *data, uint16_t size)
{
    if (!n->mode || !n->have_peer || size > AIMP_EVT_DATA)
        return 0;
    for (int i = 0; i < AIMP_EVT_QUEUE; i++) {
        if (n->out_left[i])
            continue;
        AimpEvt *e = &n->out[i];
        aimp_memset(e, 0, sizeof *e);
        e->magic = AIMP_NET_MAGIC;
        e->version = AIMP_NET_VERSION;
        e->type = AIMP_PKT_EVENT;
        e->seq = ++n->evt_seq;
        e->time_ms = GetTickCount();
        e->kind = kind;
        e->size = size;
        e->level = level;
        aimp_memcpy(e->data, data, size);
        n->out_left[i] = AIMP_EVT_REPEAT;
        return 1;
    }
    return 0;
}

void net_send_events(AimpNet *n)
{
    if (!n->mode || !n->have_peer)
        return;
    struct sockaddr_in to;
    peer_addr(n, &to);
    for (int i = 0; i < AIMP_EVT_QUEUE; i++) {
        if (!n->out_left[i])
            continue;
        W.sendto((SOCKET)n->sock, (const char *)&n->out[i], sizeof n->out[i], 0, (struct sockaddr *)&to, sizeof to);
        if (n->out_left[i] == AIMP_EVT_REPEAT)
            n->evt_sent++;
        n->out_left[i]--;
    }
}

int net_take_event(AimpNet *n, AimpEvt *out)
{
    if (n->in_head == n->in_tail)
        return 0;
    *out = n->in[n->in_head];
    n->in_head = (n->in_head + 1) % AIMP_EVT_QUEUE;
    return 1;
}

void net_status(AimpNet *n, int64_t now)
{
    if (!n->mode)
        return;
    if (!n->have_remote) {
        log_printf("NET %s: enviados %u, recibidos %u, inválidos %u | sin datos del otro jugador%s%s", n->mode == 1 ? "host" : "join",
                   n->sent, n->recv, n->bad, n->have_peer ? " (par: " : "", n->have_peer ? n->peer_str : "");
    } else {
        const AimpPkt *r = &n->remote;
        double age = (double)(now - n->remote_at) * 1000.0 / (double)n->freq;
        log_printf("NET %s %s: enviados %u, recibidos %u, perdidos %u, inválidos %u | remoto: nivel %u %s pos=(%.3f, %.3f, %.3f) yaw=%.3f seq %u, hace %.0f ms",
                   n->mode == 1 ? "host" : "join", n->peer_str, n->sent, n->recv, n->lost, n->bad, r->level,
                   (r->flags & AIMP_FLAG_PLAYER) ? "con jugador" : "sin jugador", (double)r->pos[0], (double)r->pos[1],
                   (double)r->pos[2], (double)r->yaw, r->seq, age);
        if (n->evt_sent || n->evt_recv || n->evt_dup)
            log_printf("NET eventos: enviados %u, recibidos %u, repetidos descartados %u (totales)", n->evt_sent, n->evt_recv,
                       n->evt_dup);
    }
    n->sent = n->recv = n->lost = n->bad = 0;
}

void net_close(AimpNet *n)
{
    if (!n->mode)
        return;
    if (n->have_peer) {
        AimpPkt p;
        aimp_memset(&p, 0, sizeof p);
        p.magic = AIMP_NET_MAGIC;
        p.version = AIMP_NET_VERSION;
        p.type = AIMP_PKT_BYE;
        p.seq = ++n->seq;
        struct sockaddr_in to;
        aimp_memset(&to, 0, sizeof to);
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = n->peer_ip;
        to.sin_port = n->peer_port;
        W.sendto((SOCKET)n->sock, (const char *)&p, sizeof p, 0, (struct sockaddr *)&to, sizeof to);
    }
    W.closesocket((SOCKET)n->sock);
    log_printf("NET: cerrado (enviados %u, recibidos %u, perdidos %u en total)", n->sent_total, n->recv_total,
               n->lost_total);
    n->mode = 0;
}
