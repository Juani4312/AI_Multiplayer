#!/usr/bin/env python3
"""Analiza las líneas PIN de aimp.log (AIMP_TRACE_PINS=1, hallazgo 15).

Traduce los ShortGuids de los pines (work/evidence/h15/shortguids.tsv) y las vtables a clases
(work/evidence/h15/vtables.tsv; para vtables concretas con varias clases elige la más específica:
la que tiene menos vtables concretas asociadas). Cruza con las marcas F8 (MARCA n) y los cambios
de nivel.

Uso: python3 -I work/scripts/analyze_pins.py aimp.log [--timeline] [--links-only] [--around SEG]
  --timeline    imprime cada disparo detallado, en orden (si no, solo el resumen)
  --links-only  solo disparos de pines con links (los que hacen algo en el grafo)
  --around S    con marcas: disparos a menos de S segundos de cada marca
"""
import argparse
import collections
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, '..', 'evidence', 'h15')

RE_PIN = re.compile(
    r'^\[\s*([\d.]+)\] PIN f(\d+) ent 0x([0-9a-f]+) vt ([0-9a-f]+) idx (\d+) fl ([0-9a-f]+) \| pin ([0-9a-f]+) n(\d+) \| '
    r'links (\d+) tgt 0x([0-9a-f]+) \| demora ([0-9a-f]{16}) flag ([0-9a-f]+) msg 0x([0-9a-f]+) \| ctx 0x([0-9a-f]+) '
    r'h 0x([0-9a-f]+) o 0x([0-9a-f]+) \(ent([+-]\d+)\) ovt ([0-9a-f]+) \| desde ([0-9a-f]+)(.*)$')
RE_PIN2 = re.compile(
    r'^\[\s*([\d.]+)\] PIN f(\d+) ent 0x([0-9a-f]+) vt ([0-9a-f]+) idx (\d+) fl ([0-9a-f]+) \| id ([0-9a-f]+) tipo ([0-9a-f]+) \| '
    r'pin ([0-9a-f]+) n(\d+) \| links (\d+) tgt 0x([0-9a-f]+) \| demora ([0-9a-f]{16}) flag ([0-9a-f]+) \| msg 0x([0-9a-f]+) '
    r'm 0x([0-9a-f]+) mfl ([0-9a-f]+) mref (\d+) \| ctx 0x([0-9a-f]+) h 0x([0-9a-f]+) o ([+-]\d+) \| desde ([0-9a-f]+)(.*)$')
RE_MARK = re.compile(r'^\[\s*([\d.]+)\] MARCA (\d+) .*?pos=\(([^)]*)\)')
RE_LEVEL = re.compile(r'^\[\s*([\d.]+)\] .*(nivel|level)[^\n]*', re.I)
RE_TOP = re.compile(r'^\[\s*([\d.]+)\] PIN\s+(\d+) x  ent 0x([0-9a-f]+) vt ([0-9a-f]+) idx (\d+) pin ([0-9a-f]+) links (\d+)')
RE_SUM = re.compile(r'^\[\s*([\d.]+)\] PIN (período|totales[^:]*): (.*)$')


def load_names():
    names = {}
    p = os.path.join(EVID, 'shortguids.tsv')
    if os.path.exists(p):
        for line in open(p):
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            names[int(f[0], 16)] = f[1]
    return names


def load_vtables():
    p = os.path.join(EVID, 'vtables.tsv')
    by_vt = collections.defaultdict(set)
    per_cls = collections.Counter()
    rtti = {}
    if os.path.exists(p):
        for line in open(p):
            f = line.rstrip('\n').split('\t')
            vt, cls, kind = int(f[0], 16), f[1], f[2]
            if kind == 'rtti':
                rtti[vt] = cls
            else:
                by_vt[vt].add(cls)
                per_cls[cls] += 1
    out = dict(rtti)
    for vt, cs in by_vt.items():
        if vt not in out:
            out[vt] = min(cs, key=lambda c: (per_cls[c], c))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--timeline', action='store_true')
    ap.add_argument('--links-only', action='store_true')
    ap.add_argument('--around', type=float, default=0)
    a = ap.parse_args()
    names, vts = load_names(), load_vtables()
    base = 0x400000

    def pn(g):
        return names.get(g, f'?{g:08x}')

    def cn(vt_rva):
        return vts.get(vt_rva + base, f'vt?{vt_rva:x}')

    fires, marks, levels, sums = [], [], [], []
    for line in open(a.log, encoding='utf-8', errors='replace'):
        m = RE_PIN2.match(line)
        if m:
            g = m.groups()
            fires.append(dict(t=float(g[0]), frame=int(g[1]), ent=int(g[2], 16), vt=int(g[3], 16), idx=int(g[4]),
                              fl=int(g[5], 16), eid=int(g[6], 16), type=int(g[7], 16), pin=int(g[8], 16), n=int(g[9]),
                              links=int(g[10]), tgt=int(g[11], 16), delay=g[12], flag=int(g[13], 16), msg=int(g[14], 16),
                              mobj=int(g[15], 16), mfl=int(g[16], 16), ctx=int(g[18], 16), orel=int(g[20]),
                              frm=int(g[21], 16), extra=g[22].strip()))
            continue
        m = RE_PIN.match(line)
        if m:
            g = m.groups()
            fires.append(dict(t=float(g[0]), frame=int(g[1]), ent=int(g[2], 16), vt=int(g[3], 16), idx=int(g[4]),
                              fl=int(g[5], 16), pin=int(g[6], 16), n=int(g[7]), links=int(g[8]), tgt=int(g[9], 16),
                              delay=g[10], flag=int(g[11], 16), msg=int(g[12], 16), ctx=int(g[13], 16),
                              o=int(g[15], 16), orel=int(g[16]), ovt=int(g[17], 16), frm=int(g[18], 16),
                              extra=g[19].strip()))
            continue
        m = RE_MARK.match(line)
        if m:
            marks.append((float(m[1]), int(m[2]), m[3]))
            continue
        m = RE_SUM.match(line)
        if m:
            sums.append(line.strip())
            continue
        if 'cambio de nivel' in line or 'nivel actual' in line:
            levels.append(line.strip())
    sel = [f for f in fires if f['links'] or not a.links_only]
    print(f'disparos detallados: {len(fires)} (con links: {sum(1 for f in fires if f["links"])}); marcas: {len(marks)}')
    by_pin = collections.Counter((pn(f['pin'])) for f in fires)
    def tn(f):
        if f.get('type'):
            return names.get(f['type'], f'?{f["type"]:08x}')
        return cn(f['vt'])

    by_cls = collections.Counter(tn(f) for f in fires)
    by_cp = collections.Counter((tn(f), pn(f['pin'])) for f in fires)
    by_from = collections.Counter((f['frm'], pn(f['pin'])) for f in fires)
    print('\nPines más vistos (líneas detalladas):')
    for k, v in by_pin.most_common(30):
        print(f'  {v:6}  {k}')
    print('\nClases más vistas:')
    for k, v in by_cls.most_common(30):
        print(f'  {v:6}  {k}')
    print('\nClase / pin:')
    for (c, p), v in by_cp.most_common(40):
        print(f'  {v:6}  {c} . {p}')
    print('\nQuién dispara (dirección de retorno, RVA) / pin:')
    for (r, p), v in by_from.most_common(30):
        print(f'  {v:6}  0x{r:06x} (VA 0x{r + base:08x})  {p}')
    orel = collections.Counter(f['orel'] for f in fires)
    print('\nctx->h->o respecto de la entidad (o - ent):', dict(orel.most_common(8)))
    if sums:
        print('\nResúmenes del mod:')
        for s in sums[-6:]:
            print('  ' + s)

    def fmt(f):
        return (f'[{f["t"]:9.3f}] f{f["frame"]:<7} {tn(f):24} id {f.get("eid", 0):08x} idx {f["idx"]:<6} {pn(f["pin"]):28} '
                f'n{f["n"]} links {f["links"]} desde 0x{f["frm"]:06x} {f["extra"]}')

    if a.around and marks:
        for t, n, pos in marks:
            print(f'\n== MARCA {n} t={t:.3f} pos=({pos}) ±{a.around}s')
            for f in sel:
                if abs(f['t'] - t) <= a.around:
                    print('  ' + fmt(f))
    if a.timeline:
        print('\n== Línea de tiempo')
        mi = 0
        for f in sel:
            while mi < len(marks) and marks[mi][0] <= f['t']:
                print(f'---- MARCA {marks[mi][1]} t={marks[mi][0]:.3f} pos=({marks[mi][2]})')
                mi += 1
            print(fmt(f))


if __name__ == '__main__':
    main()
