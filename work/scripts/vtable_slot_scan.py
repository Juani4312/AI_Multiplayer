#!/usr/bin/env python3
"""Recorre las vtables con RTTI de clases CATHODE:: en AI.exe (PE32) y reporta el valor
de un slot dado (por defecto +0x394, "disparar pin de salida").

Uso: vtable_slot_scan.py game/AI.exe [0x394 ...]

Método (MSVC RTTI, x86): vtable[-1] = Complete Object Locator (COL);
COL = {sig=0, offset, cdOffset, pTypeDescriptor, pClassHierarchy}; TypeDescriptor+8 = nombre
decorado ".?AVNombre@CATHODE@@". Solo se cuentan COLs con offset 0 (vtable principal).
"""
import struct
import sys
from collections import Counter, defaultdict


def load(path):
    b = open(path, 'rb').read()
    pe = struct.unpack_from('<I', b, 0x3C)[0]
    nsec = struct.unpack_from('<H', b, pe + 6)[0]
    optsz = struct.unpack_from('<H', b, pe + 20)[0]
    base = struct.unpack_from('<I', b, pe + 24 + 28)[0]
    secs = []
    off = pe + 24 + optsz
    for i in range(nsec):
        name = b[off:off + 8].rstrip(b'\0').decode()
        vsz, va, rsz, rptr = struct.unpack_from('<IIII', b, off + 8)
        secs.append((name, base + va, vsz, rptr, rsz))
        off += 40
    return b, base, secs


def main():
    path = sys.argv[1]
    slots = [int(x, 16) for x in sys.argv[2:]] or [0x394]
    b, base, secs = load(path)

    def v2o(va):
        for _, sva, vsz, rptr, rsz in secs:
            if sva <= va < sva + min(vsz, rsz):
                return rptr + (va - sva)
        return None

    def u32(va):
        o = v2o(va)
        return None if o is None else struct.unpack_from('<I', b, o)[0]

    def cstr(va, n=120):
        o = v2o(va)
        if o is None:
            return None
        e = b.find(b'\0', o, o + n)
        return b[o:e].decode('latin1') if e > 0 else None

    rdata = [s for s in secs if s[0] == '.rdata'][0]
    _, rva0, rvsz, rptr, rsz = rdata
    found = {}
    for o in range(rptr, rptr + min(rvsz, rsz) - 8, 4):
        col = struct.unpack_from('<I', b, o)[0]
        if not (rva0 <= col < rva0 + rvsz):
            continue
        sig, offs = u32(col), u32(col + 4)
        if sig != 0 or offs != 0:
            continue
        td = u32(col + 12)
        if td is None:
            continue
        name = cstr(td + 8)
        if not name or not name.startswith('.?AV') or not name.endswith('@CATHODE@@'):
            continue
        vt = rva0 + (o - rptr) + 4
        found[vt] = name[4:-10]
    print(f'vtables CATHODE con RTTI: {len(found)}')
    for s in slots:
        c = Counter()
        by = defaultdict(list)
        for vt, n in found.items():
            v = u32(vt + s)
            c[v] += 1
            by[v].append((n, vt))
        print(f'\nslot +0x{s:x}: {len(c)} valores distintos')
        for v, k in c.most_common():
            ej = ', '.join(f'{n}@0x{vt:08X}' for n, vt in sorted(by[v])[:6])
            print(f'  0x{v:08X} x{k}: {ej}{" ..." if k > 6 else ""}')


if __name__ == '__main__':
    main()
