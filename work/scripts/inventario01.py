#!/usr/bin/env python3
"""Hallazgo 01: datos del PE que REA no reporta directamente.

Lee (sin ejecutar) el AI.exe indicado e imprime:
  1. El debug directory y el registro CodeView RSDS (ruta del PDB, GUID, age).
  2. El manifest embebido (RT_MANIFEST id 1).
  3. Un escaneo byte a byte de nombres de DLL (ASCII y UTF-16LE, sin
     distinguir mayúsculas), para ver cuáles aparecen fuera de la tabla de imports.

Uso: python3 -I work/scripts/inventario01.py game/AI.exe
Los offsets salen de las cabeceras del propio archivo; no hay valores fijos
salvo los nombres a buscar.
"""
import struct
import sys

DLL_NAMES = [
    # candidatos a proxy (importados estáticamente) y los ocupados
    "winmm", "psapi", "imagehlp", "wsock32", "setupapi", "wldap32",
    "dxgi", "d3d11", "xinput1_3", "steam_api",
    # proxies habituales que AI.exe NO importa
    "version.dll", "dinput8", "dinput.dll", "xinput1_4", "xinput9_1_0",
    # posibles cargas dinámicas
    "dbghelp", "d3dcompiler", "xaudio2", "x3daudio", "dsound", "opengl32",
    "dwrite", "d2d1", "hid.dll", "nvapi", "nvpowerapi", "amd_ags", "atiags",
    "crashreporter", "mscoree", "ovbaseifx", "ws2_32", "kernel32", "user32",
]


def sections(data, pe):
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    base = pe + 24 + opt_size
    out = []
    for i in range(nsec):
        o = base + 40 * i
        name = data[o:o + 8].rstrip(b"\0").decode()
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
        out.append((name, va, vsize, raw, rsize))
    return out


def rva_to_off(secs, rva):
    for name, va, vsize, raw, rsize in secs:
        if va <= rva < va + max(vsize, rsize):
            return rva - va + raw, name
    raise ValueError(f"RVA {rva:#x} fuera de las secciones")


def hits(blob, pat, limit=8):
    res, i = [], blob.find(pat)
    while i != -1 and len(res) < limit:
        res.append(f"{i:#x}")
        i = blob.find(pat, i + 1)
    return res


def main(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0"
    opt = pe + 24
    assert struct.unpack_from("<H", data, opt)[0] == 0x10B, "se espera PE32"
    secs = sections(data, pe)
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    entry_rva = struct.unpack_from("<I", data, opt + 16)[0]
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    checksum = struct.unpack_from("<I", data, opt + 64)[0]
    print(f"ImageBase={image_base:#x} EntryRVA={entry_rva:#x} "
          f"EntryVA={image_base + entry_rva:#x} entry_en={rva_to_off(secs, entry_rva)[1]} "
          f"TimeDateStamp={timestamp:#x} CheckSum={checksum:#x}")
    for name, va, vsize, raw, rsize in secs:
        print(f"  {name:8s} RVA={va:#010x} VSize={vsize:#010x} Raw={raw:#010x} RawSize={rsize:#010x}")

    # Data directory 6 = debug, 2 = resources
    dd = opt + 96
    dbg_rva, dbg_size = struct.unpack_from("<II", data, dd + 6 * 8)
    off, sec = rva_to_off(secs, dbg_rva)
    print(f"\n[debug directory] RVA={dbg_rva:#x} size={dbg_size:#x} file_off={off:#x} ({sec})")
    for k in range(dbg_size // 28):
        ch, ts, maj, mnr, typ, size, addr, ptr = struct.unpack_from("<IIHHIIII", data, off + 28 * k)
        print(f"  entry {k}: type={typ} ts={ts:#x} size={size:#x} rva={addr:#x} file_off={ptr:#x}")
        rec = data[ptr:ptr + size]
        if typ == 2 and rec[:4] == b"RSDS":
            d1, d2, d3 = struct.unpack_from("<IHH", rec, 4)
            g = rec[4:20]
            guid = f"{d1:08X}-{d2:04X}-{d3:04X}-{g[8:10].hex().upper()}-{g[10:16].hex().upper()}"
            age = struct.unpack_from("<I", rec, 20)[0]
            pdb = rec[24:].split(b"\0")[0].decode("latin-1")
            print(f"  CodeView RSDS: GUID={guid} Age={age} PDB={pdb}")

    # Manifest: recorrer el árbol de recursos tipo 24
    rs_rva, rs_size = struct.unpack_from("<II", data, dd + 2 * 8)
    rs_off, _ = rva_to_off(secs, rs_rva)

    def entries(dir_off):
        named, ids = struct.unpack_from("<HH", data, dir_off + 12)
        for j in range(named + ids):
            nid, ptr = struct.unpack_from("<II", data, dir_off + 16 + 8 * j)
            yield nid, ptr

    for tid, tptr in entries(rs_off):
        if tid != 24:
            continue
        for _nid, nptr in entries(rs_off + (tptr & 0x7FFFFFFF)):
            for _lid, lptr in entries(rs_off + (nptr & 0x7FFFFFFF)):
                drva, dsize = struct.unpack_from("<II", data, rs_off + lptr)
                doff, _ = rva_to_off(secs, drva)
                print(f"\n[RT_MANIFEST] rva={drva:#x} file_off={doff:#x} size={dsize}")
                print(data[doff:doff + dsize].decode("utf-8", "replace"))

    low = data.lower()
    print("\n[escaneo de nombres de DLL] offsets de archivo (máx. 8 por codificación)")
    for n in DLL_NAMES:
        print(f"  {n:14s} ascii={hits(low, n.encode())} utf16le={hits(low, n.encode('utf-16-le'))}")


if __name__ == "__main__":
    main(sys.argv[1])
