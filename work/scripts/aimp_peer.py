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
--offset dx,dy,dz: corrimiento de las posiciones repetidas (para que el avatar no termine
encima del jugador).
--throw "T:plantilla:dx,dy,dz:vx,vy,vz[:time_left]" (repetible): T segundos después del primer estado
con jugador, manda un AIMP_EVT_THROW (protocolo v2) que sale de la posición del jugador del juego más
(dx, dy, dz), con esa velocidad. Ej.: --throw "5:Thrown\\flare:0,1.4,1:0,3,8:118.8". Para probar los
arrojables remotos (y AIMP_THROW_PROBE) sin nadie al teclado.
"""
import collections
import argparse
import functools
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
    ap.add_argument("--rest", action="append", default=[],
                    help="T:id:dx,dy,dz -> AIMP_EVT_THROW_REST del lanzamiento id (1 = el primer --throw) en la "
                         "posición del jugador del juego + (dx, dy, dz)")
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
    try:
        while not a.seconds or time.monotonic() - t0 < a.seconds:
            now = time.monotonic()
            while True:
                try:
                    data, addr = s.recvfrom(256)
                except BlockingIOError:
                    break
                except ConnectionResetError:
                    continue
                if len(data) < SIZE:
                    continue
                f = struct.unpack(FMT, data[:SIZE])
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
