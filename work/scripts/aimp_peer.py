#!/usr/bin/env python3
"""Peer de prueba del protocolo del mod (AIMP v2, UDP).

Hace de "otro jugador": manda un estado falso (una posición que da vueltas en un
círculo) y muestra, una vez por segundo, lo que recibe del juego.

  python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811   (el juego es host)
  python3 -I work/scripts/aimp_peer.py --host 47811             (el juego hace join)

Opciones: --hz 20, --center x,y,z (centro del círculo), --level N, --seconds S (0 = sin fin).
--replay S: en vez del círculo, repite el recorrido del jugador del juego con S segundos de
retraso (posición, yaw y nivel que manda el juego). Sirve para probar el avatar que camina
(AIMP_GHOST_WALK) en una sola máquina: el "otro jugador" pisa siempre lugares transitables.
--record-events F / --play-events F [--play-offset S]: graba los eventos que manda el juego (p. ej. los
AIMP_EVT_PIN de la fase 6, con el juego de host) y los reproduce a otro juego (de join), a los mismos
segundos desde que aparece el jugador. Para probar AIMP_SYNC_PINS en una sola máquina.
--offset dx,dy,dz: corrimiento de las posiciones repetidas (para que el avatar no termine
encima del jugador).
--throw "T:plantilla:dx,dy,dz:vx,vy,vz[:time_left]" (repetible): T segundos después del primer estado
con jugador, manda un AIMP_EVT_THROW (protocolo v2) que sale de la posición del jugador del juego más
(dx, dy, dz), con esa velocidad. Ej.: --throw "5:Thrown\\flare:0,1.4,1:0,3,8:118.8". Para probar los
arrojables remotos (y AIMP_THROW_PROBE) sin nadie al teclado.
--serve-save DIR (con --host: el juego hace join con AIMP_SAVE_SYNC=1): le pasa al juego los .ais de DIR como
si fueran los saves del host (savesync.c; manda crudos). --fetch-save DIR (con --join: el juego es host): le pide
al juego sus saves y escribe en DIR los que difieran (prueba la compresión LZ del mod).
"""
import collections
import argparse
import functools
import os
import math
import signal
import socket
import struct
import time

# Salida sin buffer: el log sirve aunque el proceso se corte (p. ej. corriendo en segundo plano).
print = functools.partial(print, flush=True)

FMT = "<IBBHIIII3ff4f3f8s"  # tiene que coincidir con AimpPkt (mod/src/net.h), 76 bytes
SIZE = struct.calcsize(FMT)
MAGIC, VERSION, STATE, BYE, FLAG_PLAYER = 0x504D4941, 2, 2, 3, 1
assert SIZE == 76, SIZE


def pack(seq, level, pos, yaw, flags=FLAG_PLAYER, kind=STATE, cam=None):
    cam = cam or (pos[0], pos[1] + 1.6, pos[2])  # con --replay, la cámara del jugador (agachado = cámara baja)
    return struct.pack(FMT, MAGIC, VERSION, kind, flags, seq, int(time.monotonic() * 1000) & 0xFFFFFFFF,
                       level, 0, *pos, yaw, 0.0, 0.0, 0.0, 1.0, *cam, b"PEERTEST")


EVT_FMT = "<IBBHIIHHI"  # cabecera de AimpEvt (mod/src/net.h), 24 bytes + 112 de datos
EVENT, EVT_THROW, THROW_OP_THROW, EVT_THROW_REST = 4, 3, 1, 4


def pack_rest(eseq, level, tid, pos):
    """AIMP_EVT_THROW_REST: ThrowRest de mod/src/throw.h (id, 3 de relleno, pos[3], ms), rellenado a 112 bytes."""
    data = struct.pack("<B3x3fI", tid, *pos, 0).ljust(112, b"\0")
    return struct.pack(EVT_FMT, MAGIC, VERSION, EVENT, 0, eseq, int(time.monotonic() * 1000) & 0xFFFFFFFF,
                       EVT_THROW_REST, 20, level) + data


def pack_throw(eseq, level, pos, vel, name, time_left, tid=0):
    m = [1.0, 0.0, 0.0, pos[0], 0.0, 1.0, 0.0, pos[1], 0.0, 0.0, 1.0, pos[2]]
    a3 = struct.unpack("<I", struct.pack("<f", 15.0))[0]
    tl = struct.unpack("<I", struct.pack("<f", time_left))[0]
    data = struct.pack("<BB2x12f3fII40s", THROW_OP_THROW, tid, *m, *vel, a3, tl, name.encode()[:39])
    assert len(data) == 112, len(data)
    return struct.pack(EVT_FMT, MAGIC, VERSION, EVENT, 0, eseq, int(time.monotonic() * 1000) & 0xFFFFFFFF,
                       EVT_THROW, 112, level) + data


# ---- copia de saves (mod/src/savesync.h) ----
XFER, X_REQ, X_MAN, X_WANT, X_DATA, X_ACK, X_DONE = 5, 1, 2, 3, 4, 5, 6
XHDR = "<IBBBBI"  # XferHdr, 12 bytes
XENT = "<16sII"   # XferEntry, 24 bytes
XDATA = "<IBBBBIIIIH"  # XferHdr + chunk, total, blob_len, len (26 bytes) + datos
CHUNK, WINDOW = 1024, 256


def fnv(b):
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h


def fnv_fast(b):
    # FNV-1a en Python puro tarda ~1 s por MB: se cachea por archivo (tamaño + mtime)
    return fnv(b)


def lz_dec(b, raw):
    out, i = bytearray(), 0
    while i < len(b):
        tok = b[i]; i += 1
        nl = tok >> 4
        if nl == 15:
            while True:
                x = b[i]; i += 1; nl += x
                if x != 255:
                    break
        out += b[i:i + nl]; i += nl
        if i == len(b):
            break
        off = b[i] | b[i + 1] << 8; i += 2
        ml = tok & 15
        if ml == 15:
            while True:
                x = b[i]; i += 1; ml += x
                if x != 255:
                    break
        ml += 4
        st = len(out) - off
        if off >= ml:
            out += out[st:st + ml]
        else:
            pat = bytes(out[st:])
            out += (pat * (ml // off + 1))[:ml]
    if len(out) != raw:
        raise ValueError(f"LZ: {len(out)} bytes en vez de {raw}")
    return bytes(out)


class SaveXfer:
    """Un extremo de savesync: 'serve' (host) o 'fetch' (cliente)."""

    def __init__(self, role, d, send):
        self.role, self.dir, self.send = role, d, send
        self.xid, self.files, self.blob, self.sent_at, self.acked = 0, [], b"", [], []
        self.state, self.last_req, self.got, self.mask, self.t0 = "off", 0.0, None, 0, 0.0
        self.sends = self.resends = 0
        os.makedirs(d, exist_ok=True)

    def hdr(self, op, n=0):
        return struct.pack(XHDR, MAGIC, VERSION, XFER, op, n, self.xid)

    def local(self):
        out = []
        for nm in sorted(os.listdir(self.dir)):
            if nm.lower().endswith(".ais") and len(nm) < 16:
                b = open(os.path.join(self.dir, nm), "rb").read()
                out.append((nm, b))
        return out

    # host
    def on_host(self, op, xid, data, now):
        if op == X_REQ:
            if not self.blob and not self.xid:
                self.files = [(nm, len(b), fnv_fast(b), b) for nm, b in self.local()]
                self.xid = (int(now * 1000) << 4 | 1) & 0xFFFFFFFF
                print(f"SAVE serve: manifiesto {self.xid:08x} con {len(self.files)} saves de {self.dir}")
            pkt = self.hdr(X_MAN, len(self.files)) + b"".join(
                struct.pack(XENT, nm.encode(), sz, h) for nm, sz, h, _ in self.files)
            self.send(pkt)
        elif xid != self.xid:
            return
        elif op == X_WANT:
            mask = struct.unpack_from("<I", data, 12)[0]
            if self.blob or not mask:
                return
            parts = []
            for i, (nm, sz, h, b) in enumerate(self.files):
                if mask >> i & 1:
                    parts.append(struct.pack("<BI", 0, len(b)) + b)
            self.blob = b"".join(parts)
            n = (len(self.blob) + CHUNK - 1) // CHUNK
            self.sent_at, self.acked, self.t0 = [0.0] * n, [False] * n, now
            print(f"SAVE serve: el juego pide {[f[0] for i, f in enumerate(self.files) if mask >> i & 1]}: "
                  f"{len(self.blob)} bytes, {n} fragmentos")
        elif op == X_ACK and self.blob:
            nxt = struct.unpack_from("<I", data, 12)[0]
            bits = data[16:16 + WINDOW // 8]
            for c in range(min(nxt, len(self.acked))):
                self.acked[c] = True
            for i in range(WINDOW):
                if i // 8 < len(bits) and bits[i // 8] >> (i % 8) & 1 and nxt + 1 + i < len(self.acked):
                    self.acked[nxt + 1 + i] = True
        elif op == X_DONE and self.blob:
            st, wr = struct.unpack_from("<II", data, 12)
            print(f"SAVE serve: el juego {'copió' if st == 0 else 'NO copió (código %d)' % st} los saves "
                  f"(escritos {wr:08x}); {self.sends} envíos, {self.resends} reenvíos, {now - self.t0:.1f} s")
            self.blob, self.xid = b"", 0

    def tick_host(self, now):
        if not self.blob:
            return
        n, sent, total = len(self.acked), 0, len(self.blob)
        base = next((c for c in range(n) if not self.acked[c]), n)
        for c in range(base, min(n, base + WINDOW)):
            if sent >= 64:
                break
            if self.acked[c] or (self.sent_at[c] and now - self.sent_at[c] < 0.25):
                continue
            chunk = self.blob[c * CHUNK:(c + 1) * CHUNK]
            self.send(struct.pack(XDATA, MAGIC, VERSION, XFER, X_DATA, 0, self.xid, c, n, total, len(chunk)) + chunk)
            if self.sent_at[c]:
                self.resends += 1
            self.sent_at[c] = now
            self.sends += 1
            sent += 1

    # cliente
    def on_client(self, op, xid, data, now):
        if op == X_MAN and self.state == "wait":
            n = data[7]
            self.xid = xid
            self.files = [struct.unpack_from(XENT, data, 12 + 24 * i) for i in range(n)]
            mine = dict(self.local())
            self.mask = 0
            for i, (nm, sz, h) in enumerate(self.files):
                nm = nm.rstrip(b"\0").decode()
                b = mine.get(nm)
                if b is None or len(b) != sz or fnv_fast(b) != h:
                    self.mask |= 1 << i
            names = [self.files[i][0].rstrip(b"\0").decode() for i in range(n) if self.mask >> i & 1]
            print(f"SAVE fetch: manifiesto del juego: {n} saves; distintos: {names or 'ninguno'}")
            self.state = "xfer" if self.mask else "done"
            self.last_req = 0.0
        elif xid != self.xid or self.state != "xfer":
            return
        elif op == X_DATA:
            _, _, _, _, _, _, c, total, blen, ln = struct.unpack_from(XDATA, data)
            if self.got is None:
                self.got, self.buf, self.t0 = [False] * total, bytearray(blen), now
                print(f"SAVE fetch: recibiendo {blen} bytes en {total} fragmentos")
            self.buf[c * CHUNK:c * CHUNK + ln] = data[26:26 + ln]
            self.got[c] = True
            self.ack_due = True
            if all(self.got):
                self.ack()
                self.apply(now)

    def ack(self):
        nxt = next((c for c, g in enumerate(self.got) if not g), len(self.got))
        bits = bytearray(WINDOW // 8)
        for i in range(WINDOW):
            if nxt + 1 + i < len(self.got) and self.got[nxt + 1 + i]:
                bits[i // 8] |= 1 << (i % 8)
        self.send(self.hdr(X_ACK) + struct.pack("<I", nxt) + bytes(bits))

    def apply(self, now):
        off, status, written = 0, 0, 0
        for i, (nm, sz, h) in enumerate(self.files):
            if not self.mask >> i & 1:
                continue
            nm = nm.rstrip(b"\0").decode()
            method, cl = struct.unpack_from("<BI", self.buf, off)
            raw = bytes(self.buf[off + 5:off + 5 + cl])
            off += 5 + cl
            try:
                b = raw if method == 0 else lz_dec(raw, sz)
            except Exception as e:
                print(f"SAVE fetch: {nm}: {e}")
                status = 3
                continue
            if fnv_fast(b) != h:
                print(f"SAVE fetch: {nm}: hash distinto")
                status = 4
                continue
            open(os.path.join(self.dir, nm), "wb").write(b)
            written |= 1 << i
            print(f"SAVE fetch: {nm}: {cl} -> {sz} bytes ({'LZ' if method else 'crudo'}), hash OK")
        print(f"SAVE fetch: {'listo' if not status else 'con errores'} en {now - self.t0:.1f} s; escritos {written:08x}")
        for _ in range(3):
            self.send(self.hdr(X_DONE) + struct.pack("<II", status, written))
        self.state = "done"

    def tick_client(self, now):
        if self.state == "off":
            self.state = "wait"
            print("SAVE fetch: pidiendo los saves del juego")
        if self.state == "wait" and now - self.last_req > 1.0:
            self.send(self.hdr(X_REQ))
            self.last_req = now
        if self.state == "xfer":
            if self.got is None and now - self.last_req > 0.5:
                self.send(self.hdr(X_WANT) + struct.pack("<I", self.mask))
                self.last_req = now
            if self.got is not None and getattr(self, "ack_due", False) and now - getattr(self, "last_ack", 0) > 0.03:
                self.ack()
                self.ack_due, self.last_ack = False, now

    def on_packet(self, data, now):
        if len(data) < 12:
            return
        _, _, _, op, _, xid = struct.unpack_from(XHDR, data)
        (self.on_host if self.role == "serve" else self.on_client)(op, xid, data, now)

    def tick(self, now):
        (self.tick_host if self.role == "serve" else self.tick_client)(now)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--join", help="ip:puerto del juego en modo host")
    g.add_argument("--host", type=int, help="puerto donde escuchar (el juego hace join)")
    ap.add_argument("--hz", type=float, default=20)
    ap.add_argument("--center", default="40,14.5,-25")
    ap.add_argument("--level", type=int, default=10)
    ap.add_argument("--seconds", type=float, default=0)
    ap.add_argument("--replay", type=float, default=0, help="segundos de retraso para repetir el recorrido del juego")
    ap.add_argument("--offset", default="0,0,0", help="corrimiento dx,dy,dz de las posiciones repetidas")
    ap.add_argument("--throw", action="append", default=[], help="T:plantilla:dx,dy,dz:vx,vy,vz[:time_left]")
    ap.add_argument("--event", action="append", default=[],
                    help="T:tipo:hex -> evento crudo (AIMP_EVT_<tipo>, datos en hex) T segundos después del jugador")
    ap.add_argument("--record-events", help="archivo donde grabar los eventos del juego (t desde el jugador, paquete en hex)")
    ap.add_argument("--play-events", help="archivo grabado con --record-events: reproduce esos eventos al juego")
    ap.add_argument("--play-offset", type=float, default=0.0, help="segundos que se suman a los tiempos de --play-events")
    ap.add_argument("--rest", action="append", default=[],
                    help="T:id:dx,dy,dz -> AIMP_EVT_THROW_REST del lanzamiento id (1 = el primer --throw) en la "
                         "posición del jugador del juego + (dx, dy, dz)")
    ap.add_argument("--serve-save", help="con --host: carpeta con .ais que el peer le pasa al juego (AIMP_SAVE_SYNC=1)")
    ap.add_argument("--fetch-save", help="con --join: carpeta donde el peer copia los saves del juego host")
    a = ap.parse_args()
    # SIGTERM (y SIGINT, que en segundo plano viene ignorado) terminan ordenado: BYE + resumen.
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    cx, cy, cz = (float(v) for v in a.center.split(","))
    ox, oy, oz = (float(v) for v in a.offset.split(","))
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setblocking(False)
    peer = None
    if a.host:
        s.bind(("0.0.0.0", a.host))
        print(f"escuchando en UDP {a.host}")
    else:
        ip, port = a.join.rsplit(":", 1)
        peer = (ip, int(port))
        print(f"mandando a {peer}")
    xfer = None
    if a.serve_save or a.fetch_save:
        xfer = SaveXfer("serve" if a.serve_save else "fetch", a.serve_save or a.fetch_save,
                        lambda pkt: peer and s.sendto(pkt, peer))
    seq, t0, last_print, last_send = 0, time.monotonic(), 0.0, 0.0
    got, lost, last_seq, last = 0, 0, None, None
    hist = collections.deque()  # (t, pos, yaw, nivel) del jugador del juego, para --replay
    throws = []
    for spec in a.throw:
        p = spec.split(":")
        throws.append([float(p[0]), p[1], [float(v) for v in p[2].split(",")], [float(v) for v in p[3].split(",")],
                       float(p[4]) if len(p) > 4 else 0.0, False])
    rests = []
    for spec in a.rest:
        p = spec.split(":")
        rests.append([float(p[0]), int(p[1]), [float(v) for v in p[2].split(",")], False])
    t_player, eseq = None, 0
    seen_evt = set()
    raws = []
    for spec in a.event:
        t, k, h = spec.split(":")
        raws.append([float(t), int(k), bytes.fromhex(h), False])
    rec_f = open(a.record_events, "w") if a.record_events else None
    plays = []
    if a.play_events:
        for line in open(a.play_events):
            if line.strip():
                t, h = line.split()
                plays.append([float(t) + a.play_offset, bytes.fromhex(h), False])
        print(f"{len(plays)} eventos para reproducir")
    try:
        while not a.seconds or time.monotonic() - t0 < a.seconds:
            now = time.monotonic()
            while True:
                try:
                    data, addr = s.recvfrom(2048)
                except BlockingIOError:
                    break
                except ConnectionResetError:
                    continue
                if len(data) >= 12 and data[5] == XFER and xfer:
                    if struct.unpack_from("<I", data)[0] == MAGIC and addr == peer:
                        xfer.on_packet(data, now)
                    continue
                if len(data) < SIZE:
                    continue
                f = struct.unpack(FMT, data[:SIZE])
                if f[0] == MAGIC and f[1] == VERSION and f[2] == EVENT and len(data) >= struct.calcsize(EVT_FMT):
                    e = struct.unpack(EVT_FMT, data[:struct.calcsize(EVT_FMT)])
                    if e[4] not in seen_evt:  # el juego manda cada evento 3 veces
                        seen_evt.add(e[4])
                        rel = now - t_player if t_player is not None else 0.0
                        print(f"[{now - t0:6.1f}s] EVENTO {e[4]} tipo {e[6]} ({e[7]} bytes, nivel {e[8]}) a {rel:.2f} s del "
                              f"jugador: {data[24:24 + e[7]].hex()}")
                        if rec_f:
                            rec_f.write(f"{rel:.3f} {data.hex()}\n")
                            rec_f.flush()
                    continue
                if f[0] != MAGIC or f[1] != VERSION or f[2] not in (STATE, BYE):
                    continue
                if a.host and peer != addr:
                    peer = addr
                    print(f"juego conectado desde {addr}")
                if f[2] == BYE:
                    print("el juego se despidió (BYE)")
                    continue
                if last_seq is not None and f[4] > last_seq + 1:
                    lost += f[4] - last_seq - 1
                last_seq, last, got = f[4], f, got + 1
                if t_player is None and f[3] & FLAG_PLAYER:
                    t_player = now
                if a.replay and f[3] & FLAG_PLAYER:
                    hist.append((now, tuple(f[8:11]), f[11], f[6], tuple(f[16:19])))
            if peer and now - last_send >= 1.0 / a.hz:
                seq += 1
                if a.replay:
                    # el estado de hace a.replay segundos; mientras no haya historia, sin jugador (el host
                    # igual aprende la dirección del peer con este paquete)
                    while len(hist) > 1 and hist[1][0] <= now - a.replay:
                        hist.popleft()
                    if hist and hist[0][0] <= now - a.replay:
                        _, pos, yaw, lvl, cam = hist[0]
                        s.sendto(pack(seq, lvl, (pos[0] + ox, pos[1] + oy, pos[2] + oz), yaw,
                                      cam=(cam[0] + ox, cam[1] + oy, cam[2] + oz)), peer)
                    else:
                        s.sendto(pack(seq, a.level, (0.0, 0.0, 0.0), 0.0, 0), peer)
                else:
                    ang = (now - t0) * 0.5
                    pos = (cx + 3 * math.cos(ang), cy, cz + 3 * math.sin(ang))
                    s.sendto(pack(seq, a.level, pos, math.atan2(math.cos(ang), -math.sin(ang))), peer)
                last_send = now
            if xfer and peer and last is not None:
                xfer.tick(now)
            for th in throws:
                if th[5] or t_player is None or not peer or now - t_player < th[0]:
                    continue
                th[5] = True
                eseq += 1
                gp = last[8:11]
                pos = (gp[0] + th[2][0], gp[1] + th[2][1], gp[2] + th[2][2])
                pkt = pack_throw(eseq, last[6], pos, th[3], th[1], th[4], throws.index(th) + 1)
                for _ in range(3):
                    s.sendto(pkt, peer)
                print(f"[{now - t0:6.1f}s] THROW {eseq}: {th[1]} desde ({pos[0]:.2f}, {pos[1]:.2f}, {pos[2]:.2f}) "
                      f"vel {th[3]} nivel {last[6]}")
            for rw in raws:
                if rw[3] or t_player is None or not peer or now - t_player < rw[0]:
                    continue
                rw[3] = True
                eseq += 1
                pkt = struct.pack(EVT_FMT, MAGIC, VERSION, EVENT, 0, eseq, int(time.monotonic() * 1000) & 0xFFFFFFFF,
                                  rw[1], len(rw[2]), last[6]) + rw[2].ljust(112, b"\0")
                for _ in range(3):
                    s.sendto(pkt, peer)
                print(f"[{now - t0:6.1f}s] EVENTO crudo {eseq} tipo {rw[1]} ({len(rw[2])} bytes, nivel {last[6]})")
            for pe in plays:
                if pe[2] or t_player is None or not peer or now - t_player < pe[0]:
                    continue
                pe[2] = True
                for _ in range(3):
                    s.sendto(pe[1], peer)
                e = struct.unpack(EVT_FMT, pe[1][:struct.calcsize(EVT_FMT)])
                print(f"[{now - t0:6.1f}s] REPRODUCIDO evento {e[4]} tipo {e[6]}: {pe[1][24:24 + e[7]].hex()}")
            for r in rests:
                if r[3] or t_player is None or not peer or now - t_player < r[0]:
                    continue
                r[3] = True
                eseq += 1
                gp = last[8:11]
                pos = (gp[0] + r[2][0], gp[1] + r[2][1], gp[2] + r[2][2])
                pkt = pack_rest(eseq, last[6], r[1], pos)
                for _ in range(3):
                    s.sendto(pkt, peer)
                print(f"[{now - t0:6.1f}s] REST {eseq}: lanzamiento {r[1]} asentado en ({pos[0]:.2f}, {pos[1]:.2f}, {pos[2]:.2f})")
            if now - last_print >= 1.0:
                if last:
                    flags, lvl, sim = last[3], last[6], last[7]
                    x, y, z, yaw = last[8:12]
                    cid = last[19].hex()
                    print(f"[{now - t0:6.1f}s] recibidos {got} perdidos {lost} | juego: nivel {lvl} "
                          f"{'con' if flags & FLAG_PLAYER else 'sin'} jugador pos=({x:.2f}, {y:.2f}, {z:.2f}) "
                          f"yaw={yaw:.3f} sim={sim} id={cid}")
                else:
                    print(f"[{now - t0:6.1f}s] todavía nada del juego (enviados {seq})")
                last_print = now
            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    if peer:
        s.sendto(pack(seq + 1, a.level, (0, 0, 0), 0, 0, BYE), peer)
    print(f"fin: enviados {seq}, recibidos {got}, perdidos {lost}")


if __name__ == "__main__":
    main()
