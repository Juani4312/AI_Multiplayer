#!/usr/bin/env python3
"""Compara las instancias (pid) vistas en la carga de un nivel entre dos logs de AIMP_TRACE_PINS
(hallazgo 15). Si los pid son deterministas, los mismos (pid, tipo, id) aparecen en los dos.

Uso: python3 -I work/scripts/compare_pids.py log_a log_b [nivel_texto]
Toma las líneas PIN posteriores al último 'EV nivel actual ... -> <nivel_texto>' de cada log."""
import re
import sys

RE = re.compile(r'^\[\s*([\d.]+)\] PIN f\d+ ent 0x(\w+) vt (\w+) idx (\d+) fl \w+ \| id (\w+) tipo (\w+) pid (\w+) d(\d+) \| pin (\w+)')


def load(path, lvl):
    lines = open(path, errors='replace').read().split('\n')
    start = 0
    for i, l in enumerate(lines):
        if 'EV nivel actual' in l and lvl in l.split('->')[-1]:
            start = i
    # el nivel nuevo empieza a disparar ~1 s antes de la línea EV: retroceder 3 s
    t0 = float(re.match(r'\[\s*([\d.]+)', lines[start])[1]) - 3 if start else 0
    inst, pins, idx = {}, set(), {}
    for l in lines:
        m = RE.match(l)
        if not m or float(m[1]) < t0 or m[7] == '00000000':
            continue
        inst[m[7]] = (m[6], m[5], int(m[8]))
        pins.add((m[7], m[9]))
        idx[m[7]] = int(m[4])
    return inst, pins, idx


def main():
    a, b = sys.argv[1], sys.argv[2]
    lvl = sys.argv[3] if len(sys.argv) > 3 else ''
    ia, pa, xa = load(a, lvl)
    ib, pb, xb = load(b, lvl)
    common = set(ia) & set(ib)
    same = sum(1 for p in common if ia[p] == ib[p])
    print(f'instancias: A {len(ia)}, B {len(ib)}, comunes {len(common)} ({100 * len(common) / max(1, min(len(ia), len(ib))):.1f}% del menor)')
    print(f'comunes con el mismo (tipo, id, profundidad): {same}/{len(common)}')
    print(f'(pid, pin): A {len(pa)}, B {len(pb)}, comunes {len(pa & pb)}')
    sidx = sum(1 for p in common if xa[p] == xb[p] and xa[p] != 16777215)
    print(f'mismo índice de caché (idx) entre A y B: {sidx}/{len(common)}')
    only_a = [ia[p] for p in set(ia) - set(ib)][:10]
    print('ejemplos solo en A (tipo, id, prof.):', only_a)


if __name__ == '__main__':
    main()
