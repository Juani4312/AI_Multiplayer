#!/usr/bin/env python3
"""Peer de prueba del protocolo de la fase 3 del mod (AIMP v1, UDP).

Hace de "otro jugador": manda un estado falso (una posición que da vueltas en un
círculo) y muestra, una vez por segundo, lo que recibe del juego.

  python3 -I work/scripts/aimp_peer.py --join 127.0.0.1:47811   (el juego es host)
  python3 -I work/scripts/aimp_peer.py --host 47811             (el juego hace join)

Opciones: --hz 20, --center x,y,z (centro del círculo), --level N, --seconds S (0 = sin fin).
"""
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
MAGIC, VERSION, STATE, BYE, FLAG_PLAYER = 0x504D4941, 1, 2, 3, 1
assert SIZE == 76, SIZE


def pack(seq, level, pos, yaw, flags=FLAG_PLAYER, kind=STATE):
    return struct.pack(FMT, MAGIC, VERSION, kind, flags, seq, int(time.monotonic() * 1000) & 0xFFFFFFFF,
                       level, 0, *pos, yaw, 0.0, 0.0, 0.0, 1.0, pos[0], pos[1] + 1.6, pos[2], b"PEERTEST")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--join", help="ip:puerto del juego en modo host")
    g.add_argument("--host", type=int, help="puerto donde escuchar (el juego hace join)")
    ap.add_argument("--hz", type=float, default=20)
    ap.add_argument("--center", default="40,14.5,-25")
    ap.add_argument("--level", type=int, default=10)
    ap.add_argument("--seconds", type=float, default=0)
    a = ap.parse_args()
    # SIGTERM (y SIGINT, que en segundo plano viene ignorado) terminan ordenado: BYE + resumen.
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    cx, cy, cz = (float(v) for v in a.center.split(","))
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
                if f[0] != MAGIC or f[1] != VERSION:
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
            if peer and now - last_send >= 1.0 / a.hz:
                ang = (now - t0) * 0.5
                pos = (cx + 3 * math.cos(ang), cy, cz + 3 * math.sin(ang))
                seq += 1
                s.sendto(pack(seq, a.level, pos, math.atan2(math.cos(ang), -math.sin(ang))), peer)
                last_send = now
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
