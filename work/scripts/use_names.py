#!/usr/bin/env python3
"""Nombres de los objetos usados en un log del mod (líneas "SYNC ... objetivo XXXXXXXXYYYYYYYY").

El id de 8 bytes del RPC 0x26 es el EntityHandle de CathodeLib (corrida 119, verificado con 6 de 6 objetivos):
los primeros 4 bytes son el ShortGuid de la entidad (UI_Icon/UI_Interact) y los otros 4, el id de instancia del
composite que la contiene; el log los muestra en orden de memoria (hay que darlos vuelta para compararlos con el
volcado, que los guarda como uint32). Necesita el volcado de work/tools/cathode-dump.sh (columna inst).

Uso: python3 -I work/scripts/use_names.py <aimp.log> work/evidence/cathode/commands/<NIVEL>.tsv.gz
"""
import gzip
import re
import sys


def le(h):
    return bytes.fromhex(h)[::-1].hex()


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    log, dump = sys.argv[1], sys.argv[2]
    rx = re.compile(r"SYNC (uso local \d+|: aplicando uso remoto \(evento \d+\)).*?objetivo ([0-9a-f]{16})")
    want = {}
    order = []
    with open(log, errors="replace") as f:
        for line in f:
            m = rx.search(line)
            if not m:
                continue
            t = m.group(2)
            key = (le(t[:8]), le(t[8:]))
            if key not in want:
                want[key] = None
            order.append((line[:13].strip(), m.group(1), t, key))
    op = gzip.open if dump.endswith(".gz") else open
    with op(dump, "rt", encoding="utf-8") as f:
        for line in f:
            if line[0] in "#p":
                continue
            c = line.rstrip("\n").split("\t")
            if len(c) < 12:
                sys.exit("el volcado no tiene la columna inst: regenerarlo con work/tools/cathode-dump.sh")
            key = (c[2], c[11])
            if key in want and want[key] is None:
                want[key] = (c[0], c[7])
    for t, what, tid, key in order:
        n = want.get(key)
        print(f"{t} {what}: {tid} -> " + (f"pid {n[0]} {n[1]}" if n else "(no está en el volcado)"))


if __name__ == "__main__":
    main()
