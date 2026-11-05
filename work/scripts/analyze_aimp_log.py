#!/usr/bin/env python3
"""Analiza un aimp.log del mod (fase 1+2) contra los chequeos de work/RUNTIME_PLAN.md.

Uso: python3 -I work/scripts/analyze_aimp_log.py <aimp.log> [--markdown]

Imprime, para cada chequeo, OK / FALLA / sin datos con la evidencia (líneas y
valores del log). No interpreta nada que el log no diga: si falta un dato, lo marca.
"""
import math
import re
import sys
from collections import Counter

LINE = re.compile(r"^\[\s*([\d.]+)(?: T\d+)?\] (.*)$")
FLOAT = r"(-?[\d.]+(?:e[+-]\d+)?|nan|-?inf)"


def parse(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINE.match(raw.rstrip("\n"))
            if m:
                out.append((float(m.group(1)), m.group(2)))
    return out


def fl(s):
    try:
        return float(s)
    except ValueError:
        return float("nan")


class Report:
    def __init__(self):
        self.rows = []

    def add(self, block, name, status, evidence):
        self.rows.append((block, name, status, evidence))

    def print(self):
        cur = None
        for block, name, status, ev in self.rows:
            if block != cur:
                print(f"\n## {block}")
                cur = block
            print(f"- [{status}] {name}: {ev}")


def median(v):
    v = sorted(v)
    return v[len(v) // 2] if v else float("nan")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    lines = parse(sys.argv[1])
    text = [t for _, t in lines]
    r = Report()

    def find(prefix):
        return [t for t in text if t.startswith(prefix)]

    # ---- Encabezado y guardas (bloque 0) ----
    hdr = find("AI_Multiplayer ")
    plat = find("plataforma:")
    print(f"# Análisis de {sys.argv[1]}")
    print(f"- {hdr[0] if hdr else 'sin encabezado'}")
    print(f"- {plat[0] if plat else 'sin plataforma'}; {len(lines)} líneas, {lines[-1][0] if lines else 0:.1f} s")
    proxy = find("proxy:")
    r.add("0. Carga y guardas", "proxy de winmm", "OK" if any("winmm real =" in p for p in proxy) else "FALLA",
          "; ".join(proxy[:2]) or "sin líneas proxy")
    for key, label in (("TimeDateStamp", "TimeDateStamp/SizeOfImage"), ("RVA ovr_Initialize", "RVAs de exports"),
                       ("prólogo de ModeManager::Run", "prólogo de ModeManager::Run")):
        ln = [t for t in find("build:") if key in t]
        st = "sin datos" if not ln else ("FALLA" if "DISTINTO" in ln[0] else "OK")
        r.add("0. Carga y guardas", label, st, ln[0] if ln else "-")
    hook = [t for t in text if t.startswith("hook ModeManager::Run")]
    r.add("0. Carga y guardas", "hook instalado", "OK" if hook and "instalado" in hook[0] else "FALLA",
          hook[0] if hook else "no hay línea de hook")
    fase1 = find("FASE 1:")
    r.add("1. Loop", "código corriendo por frame (fase 1)", "OK" if fase1 else "FALLA", fase1[0] if fase1 else "-")

    # ---- Resúmenes ST ----
    st_re = re.compile(r"ST frames \+(\d+) \(contador del juego (\d+), \+(-?\d+)\) fps=" + FLOAT +
                       r" Run prom=" + FLOAT + r" ms max=" + FLOAT + r" ms dt=" + FLOAT + r" dts=" + FLOAT)
    sts = []
    for i, t in enumerate(text):
        m = st_re.match(t)
        if not m:
            continue
        blk = {"frames": int(m.group(1)), "dgame": int(m.group(3)), "fps": fl(m.group(4)),
               "run_max": fl(m.group(6)), "dt": fl(m.group(7)), "dts": fl(m.group(8))}
        for t2 in text[i + 1:i + 10]:
            if not t2.startswith("   "):
                break
            if (m2 := re.search(r"estado (-?\d+) \((\S+)\).*\| nivel (-?\d+)/(\d+) \"(.*)\"", t2)):
                blk.update(state=int(m2.group(1)), level=int(m2.group(3)), level_name=m2.group(5))
            if (m2 := re.search(r"jugadores (\d+), NPCs (\d+).*J0 (0x[0-9a-f]+) (\S+) def (0x[0-9a-f]+) tipo (\d+) "
                                r"1c=(\d+) 1f=(\d+) ctrl (0x[0-9a-f]+) activo (\d+)", t2)):
                blk.update(np=int(m2.group(1)), nn=int(m2.group(2)), pl=m2.group(3), vt=m2.group(4),
                           f1c=int(m2.group(7)), f1f=int(m2.group(8)), ctrl=m2.group(9), act=int(m2.group(10)))
            if (m2 := re.search(r"J0 pos=\(" + FLOAT + ", " + FLOAT + ", " + FLOAT + r"\) yaw=" + FLOAT, t2)):
                blk.update(pos=(fl(m2.group(1)), fl(m2.group(2)), fl(m2.group(3))), yaw=fl(m2.group(4)))
            if (m2 := re.search(r"\|q\|\^2=" + FLOAT, t2)):
                blk["q2"] = fl(m2.group(1))
            if (m2 := re.search(r"cam - J0 = \(" + FLOAT + ", " + FLOAT + ", " + FLOAT + r"\)", t2)):
                blk["camd"] = (fl(m2.group(1)), fl(m2.group(2)), fl(m2.group(3)))
            if (m2 := re.search(r"transporte (0x[0-9a-f]+) máscara ([0-9a-f]+):([0-9a-f]+) \((\d+) cambios\).*"
                                r"proveedor (0x[0-9a-f]+)", t2)):
                blk.update(tr=m2.group(1), mask=m2.group(2) + m2.group(3), maskchg=int(m2.group(4)),
                           prov=m2.group(5))
            if (m2 := re.search(r"cola A (0x[0-9a-f]+) \(global (0x[0-9a-f]+)\).*máx (\d+), frames con datos (\d+)/(\d+), "
                                r"suma (\d+)", t2)):
                blk.update(qa=m2.group(1), qa_g=m2.group(2), qa_max=int(m2.group(3)), qa_nz=int(m2.group(4)),
                           qa_n=int(m2.group(5)), qa_sum=int(m2.group(6)))
            if (m2 := re.search(r"cola B .*máx (\d+), frames con datos (\d+)/(\d+), suma (\d+)", t2)):
                blk.update(qb_max=int(m2.group(1)), qb_nz=int(m2.group(2)), qb_n=int(m2.group(3)),
                           qb_sum=int(m2.group(4)))
        sts.append(blk)

    playing = [s for s in sts if s.get("state") == 5 and s.get("pl", "0x00000000") != "0x00000000"]
    print(f"- {len(sts)} resúmenes ST, {len(playing)} con jugador y estado 5")

    full = [s for s in sts[1:] if s["frames"] > 0]
    if full:
        # El contador 0x16410DC es de frames de SIMULACIÓN: 0 en menú y pausa (hallazgo 04).
        sim = [s for s in full if s.get("pl", "0x00000000") != "0x00000000" and s["dgame"] > 0]
        paused = [s for s in full if s.get("pl", "0x00000000") != "0x00000000" and s["dgame"] == 0]
        if sim:
            ratio = sum(s["dgame"] for s in sim) / max(1, sum(s["frames"] for s in sim))
            r.add("1. Loop", "contador de simulación +1 por frame (en juego, sin pausa)",
                  "OK" if 0.95 <= ratio <= 1.05 else "FALLA",
                  f"Δcontador/ΔRun = {ratio:.3f} en {len(sim)} resúmenes; {len(paused)} resúmenes en pausa (Δ=0)")
        else:
            r.add("1. Loop", "contador de simulación", "sin datos", "no hubo resúmenes en juego sin pausa")
        dts = [s["dt"] for s in full if s["fps"] > 5]
        fps = [s["fps"] for s in full if s["fps"] > 5]
        if dts:
            r.add("1. Loop", "dt real ≈ 1/fps", "OK" if abs(median(dts) * median(fps) - 1) < 0.25 else "FALLA",
                  f"mediana dt={median(dts):.4f}, mediana fps={median(fps):.1f}")
        r.add("1. Loop", "carga más larga dentro de Run", "info", f"{max(s['run_max'] for s in full):.1f} ms")
    states = Counter(s.get("state") for s in sts)
    r.add("1. Loop", "estados del ModeManager vistos en los ST", "info", dict(states))
    trans = [t for t in text if "ModeManager estado" in t and t.startswith("EV frame")]
    r.add("1. Loop", "transiciones de estado", "info", f"{len(trans)}: " + " | ".join(t[3:80] for t in trans[:6]))
    lv = [t for t in text if t.startswith("EV nivel actual")]
    r.add("1. Loop", "nivel actual legible", "OK" if any('"' in t and '""' not in t for t in lv) else "sin datos",
          " | ".join(lv[:4]) or "-")

    # ---- Jugador y cámara (bloque 2) ----
    if playing:
        vt_ok = all(s.get("vt") == "Character(ok)" for s in playing)
        r.add("2. Jugador", "vtable del jugador = 0x12F5D80", "OK" if vt_ok else "FALLA",
              Counter(s.get("vt") for s in playing))
        r.add("2. Jugador", "un jugador mientras se juega", "OK" if all(s.get("np") == 1 for s in playing) else "FALLA",
              Counter(s.get("np") for s in playing))
        flags = Counter((s.get("f1c"), s.get("f1f"), s.get("act")) for s in playing)
        r.add("2. Jugador", "def 1c=1, 1f=1 y controlador activo", "OK" if set(flags) == {(1, 1, 1)} else "FALLA",
              f"(1c, 1f, activo): {dict(flags)}")
        yaws = [s["yaw"] for s in playing if "yaw" in s]
        if yaws:
            ok = all(-math.pi - 1e-3 <= y <= math.pi + 1e-3 for y in yaws)
            r.add("2. Jugador", "yaw en [-π, π] y cambia al girar", "OK" if ok and max(yaws) - min(yaws) > 1.0 else
                  ("FALLA" if not ok else "sin datos (no se giró)"), f"rango [{min(yaws):.3f}, {max(yaws):.3f}]")
        pos = [s["pos"] for s in playing if "pos" in s]
        if len(pos) > 1:
            dist = sum(math.dist(a, b) for a, b in zip(pos, pos[1:]))
            ys = [p[1] for p in pos]
            r.add("2. Jugador", "la posición cambia al caminar", "OK" if dist > 1.0 else "sin datos (no se caminó)",
                  f"recorrido total {dist:.1f}; Y en [{min(ys):.2f}, {max(ys):.2f}]")
        q2 = [s["q2"] for s in playing if "q2" in s]
        if q2:
            r.add("2. Cámara", "cuaternión de la cámara con norma 1", "OK" if all(abs(q - 1) < 0.02 for q in q2) else
                  "FALLA", f"|q|^2 en [{min(q2):.4f}, {max(q2):.4f}]")
        cd = [s["camd"] for s in playing if "camd" in s]
        if cd:
            dy = median([c[1] for c in cd])
            dh = median([math.hypot(c[0], c[2]) for c in cd])
            r.add("2. Cámara", "cámara ≈ jugador + altura de ojos", "OK" if 1.0 < dy < 2.2 and dh < 1.0 else "FALLA",
                  f"mediana ΔY={dy:.2f}, distancia horizontal mediana={dh:.2f}")
    else:
        r.add("2. Jugador", "datos del jugador", "sin datos", "no hubo ST con jugador y estado 5")

    # ---- Red del engine (bloque 3) ----
    if sts:
        trs = Counter(s.get("tr") for s in sts)
        r.add("3. Red", "3.1 transporte NETZ siempre 0", "OK" if set(trs) <= {"0x00000000", None} else "FALLA",
              dict(trs))
        provs = Counter(s.get("prov") for s in sts)
        r.add("3. Red", "3.3 proveedor de sesión siempre 0", "OK" if set(provs) <= {"0x00000000", None} else "FALLA",
              dict(provs))
        r.add("3. Red", "3.2 máscara de canales NETZ", "info",
              f"{sum(s.get('maskchg', 0) for s in sts)} cambios; valores {Counter(s.get('mask') for s in sts).most_common(4)}")
        for q in ("qa", "qb"):
            nz = sum(s.get(q + "_nz", 0) for s in playing)
            n = sum(s.get(q + "_n", 0) for s in playing)
            mx = max([s.get(q + "_max", 0) for s in playing] or [0])
            tot = sum(s.get(q + "_sum", 0) for s in playing)
            secs = len(playing) or 1
            name = "3.4 cola A" if q == "qa" else "3.4 cola B (sesión)"
            r.add("3. Red", name + ": tamaño después de Run", "info",
                  f"frames con datos {nz}/{n}, máx {mx} bytes, ~{tot // secs} bytes/s "
                  "(las colas se vacían dentro del frame: el tráfico se mide con el trazado, 3.6)")
        same = all(s.get("qa") == s.get("qa_g") for s in sts if s.get("qa") not in (None, "0x00000000"))
        r.add("3. Red", "global de la cola A == g_root+0x50", "OK" if same else "FALLA", "")
    rpc = [t for t in text if t.startswith("RPC lista") and "endpoints" in t]
    r.add("3. Red", "3.5 catálogo de endpoints", "OK" if rpc else "sin datos", " | ".join(rpc) or "-")
    marks = [t for t in text if t.startswith("MARCA ")]
    if marks:
        r.add("Marcas", "marcas del usuario (F8)", "info", " | ".join(m[:60] for m in marks[:10]))
    tr_new = [t for t in text if t.startswith("TR cola") and "tipo nuevo" in t]
    r.add("3. Red", "3.6 tipos vistos por el trazado", "info" if tr_new else "sin datos (¿AIMP_TRACE_QUEUES=1?)",
          f"{len(tr_new)} tipos nuevos: " + " | ".join(t[3:70] for t in tr_new[:6]))

    # ---- Anomalías ----
    bad = [t for t in text if any(k in t for k in ("INESPERADO", "OJO", "FATAL", "DISTINTO", "ilegible", "No se instala"))]
    r.add("Anomalías", "líneas con INESPERADO / OJO / FATAL / DISTINTO / ilegible", "OK" if not bad else "REVISAR",
          f"{len(bad)}: " + " | ".join(b[:90] for b in bad[:5]))
    r.print()


if __name__ == "__main__":
    main()
