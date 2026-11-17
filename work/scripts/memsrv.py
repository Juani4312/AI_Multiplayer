#!/usr/bin/env python3
"""Cliente del lector de memoria del mod (AIMP_MEMSRV_FILE, mod/src/memsrv.c). Solo lee.

Uso como módulo:  from memsrv import Mem; m = Mem(); m.chars(); m.read(addr, n)
Uso por línea de comandos:
  memsrv.py chars                         lista los personajes
  memsrv.py read 0xADDR LEN               volcado hex
  memsrv.py find 0xCHR TEXTO [prof] [tam] busca TEXTO (ascii) en lo que se alcanza desde 0xCHR siguiendo punteros
                                          (profundidad 3 por defecto; tam = bytes leídos por objeto, 0x200)
El juego tiene que estar abierto con AIMP_MEMSRV_FILE=C:\\users\\steamuser\\AppData\\Local\\AI_Multiplayer\\memsrv.txt.
"""
import os
import struct
import sys
import time

DIR = "/mnt/1,8TB/SteamLibrary/steamapps/compatdata/214490/pfx/drive_c/users/steamuser/AppData/Local/AI_Multiplayer"
IN = os.path.join(DIR, "memsrv.txt")
OUT = IN + ".out"


class Mem:
    def __init__(self, timeout=5.0):
        self.timeout = timeout
        self.cache = {}

    def req(self, lines):
        last = None
        if os.path.exists(OUT):
            # 2026-11-17 (corrida 108): la respuesta de una corrida anterior puede tener el mismo número de pedido
            # que la primera de esta ("# 1") y parecer que el mod no respondió: se borra antes de pedir
            try:
                os.remove(OUT)
            except OSError:
                with open(OUT, "rb") as f:
                    last = f.readline().decode("latin1").strip()
        with open(IN, "w") as f:
            f.write("\n".join(lines) + "\n")
        t0 = time.time()
        while time.time() - t0 < self.timeout:
            time.sleep(0.05)
            if not os.path.exists(OUT):
                continue
            with open(OUT, "rb") as f:
                data = f.read().decode("latin1")
            if data and data.splitlines()[0] != last:
                return data.splitlines()[1:]
        raise TimeoutError("el mod no respondió (¿juego abierto con AIMP_MEMSRV_FILE?)")

    def chars(self):
        out = []
        for l in self.req(["C"]):
            p = l.split()
            if len(p) == 3 and p[0] in "PN":
                out.append((p[0], int(p[1]), int(p[2], 16)))
        return out

    def read_many(self, reqs, chunk=400):
        """reqs: [(addr, n)] -> {addr: bytes|None}"""
        res = {}
        todo = [(a, n) for a, n in reqs if (a, n) not in self.cache]
        for i in range(0, len(todo), chunk):
            part = todo[i:i + chunk]
            for l in self.req(["R 0x%x %d" % (a, n) for a, n in part]):
                p = l.split()
                if len(p) >= 4 and p[0] == "R":
                    a, n = int(p[1], 16), int(p[2])
                    self.cache[(a, n)] = None if p[3] == "ERR" else bytes.fromhex(p[3])
        for a, n in reqs:
            res[a] = self.cache.get((a, n))
        return res

    def read(self, a, n):
        return self.read_many([(a, n)])[a]

    def u32(self, a):
        b = self.read(a, 4)
        return struct.unpack("<I", b)[0] if b else None


def plausible_ptr(v):
    return 0x00010000 <= v < 0x7FFF0000 and v & 3 == 0


def find(m, root, text, depth=3, size=0x200, root_size=0x42D0):
    needle = text.encode("latin1")
    seen = {root}
    level = [(root, root_size, "chr")]
    hits = []
    for d in range(depth + 1):
        data = m.read_many([(a, n) for a, n, _ in level])
        nxt = []
        for a, n, path in level:
            b = data.get(a)
            if not b:
                continue
            i = b.find(needle)
            while i >= 0:
                s = b[max(0, i - 40):i + 60].split(b"\0")
                hits.append((path, i, b[max(0, i - 40):i + 80]))
                i = b.find(needle, i + 1)
            if d == depth:
                continue
            for off in range(0, len(b) - 3, 4):
                v = struct.unpack_from("<I", b, off)[0]
                if plausible_ptr(v) and v not in seen:
                    seen.add(v)
                    nxt.append((v, size, "%s+0x%x" % (path, off)))
        print("profundidad %d: %d objetos, %d siguientes, %d hallazgos" % (d, len(level), len(nxt), len(hits)), file=sys.stderr)
        level = nxt[:20000]
    return hits


if __name__ == "__main__":
    m = Mem()
    cmd = sys.argv[1] if len(sys.argv) > 1 else "chars"
    if cmd == "chars":
        for k, i, a in m.chars():
            print(k, i, hex(a))
    elif cmd == "read":
        b = m.read(int(sys.argv[2], 16), int(sys.argv[3], 0))
        print(b.hex() if b else "ERR")
    elif cmd == "find":
        root = int(sys.argv[2], 16)
        depth = int(sys.argv[4]) if len(sys.argv) > 4 else 3
        size = int(sys.argv[5], 0) if len(sys.argv) > 5 else 0x200
        for path, off, ctx in find(m, root, sys.argv[3], depth, size):
            print(path, hex(off), ctx)
