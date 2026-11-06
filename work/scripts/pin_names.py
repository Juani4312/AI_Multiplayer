#!/usr/bin/env python3
"""Diccionario ShortGuid -> nombre para leer las trazas de pines de CATHODE (hallazgo 15).

Busca en .text de AI.exe el patrón de los inicializadores estáticos que calculan ShortGuids:
    68 <&"nombre">      PUSH imm32
    B9 <&global>        MOV ECX, imm32
    E8 <rel32>          CALL FUN_008bf7f0  (ShortGuid de un nombre, hallazgo 06)
y calcula el ShortGuid de cada nombre con el algoritmo verificado (shortguid_check.py).
También agrega los nombres de las clases RTTI CATHODE:: (tipos de entidad).

Salida TSV: shortguid_hex(LE, como uint32 en el log) <TAB> nombre <TAB> global(VA) <TAB> origen

Uso: python3 -I work/scripts/pin_names.py game/AI.exe > work/evidence/h15/shortguids.tsv
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shortguid_check import shortguid  # noqa: E402
from vtable_slot_scan import load  # noqa: E402

SG_FUNC = 0x008BF7F0


def main():
    b, base, secs = load(sys.argv[1])

    def v2o(va):
        for _, sva, vsz, rptr, rsz in secs:
            if sva <= va < sva + min(vsz, rsz):
                return rptr + (va - sva)
        return None

    def cstr(va, n=200):
        o = v2o(va)
        if o is None:
            return None
        e = b.find(b'\0', o, o + n)
        if e <= o:
            return None
        s = b[o:e]
        if not all(0x20 <= c < 0x7F for c in s):
            return None
        return s.decode('ascii')

    text = [s for s in secs if s[0] == '.text'][0]
    _, tva, tvsz, tptr, trsz = text
    rows = {}
    n_sites = 0
    end = tptr + min(tvsz, trsz) - 15
    i = tptr
    while True:
        i = b.find(b'\x68', i, end)
        if i < 0:
            break
        if b[i + 5] == 0xB9 and b[i + 10] == 0xE8:
            va_call = tva + (i + 10 - tptr)
            rel = struct.unpack_from('<i', b, i + 11)[0]
            if va_call + 5 + rel == SG_FUNC:
                sva = struct.unpack_from('<I', b, i + 1)[0]
                gva = struct.unpack_from('<I', b, i + 6)[0]
                name = cstr(sva)
                if name is not None:
                    n_sites += 1
                    sg = struct.unpack('<I', shortguid(name))[0]
                    rows.setdefault(sg, (name, gva, 'init'))
        i += 1
    # Tipos de entidad (RTTI): mismo algoritmo, sin global.
    import re
    for m in re.finditer(rb'\.\?AV([A-Za-z0-9_]+)@CATHODE@@\x00', b):
        name = m.group(1).decode()
        sg = struct.unpack('<I', shortguid(name))[0]
        rows.setdefault(sg, (name, 0, 'rtti'))
    print(f'# sitios de init: {n_sites}; ShortGuids distintos: {len(rows)}', file=sys.stderr)
    for sg, (name, gva, src) in sorted(rows.items(), key=lambda kv: kv[1][0]):
        print(f'{sg:08x}\t{name}\t{gva:08x}\t{src}')


if __name__ == '__main__':
    main()
