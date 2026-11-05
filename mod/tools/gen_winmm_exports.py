#!/usr/bin/env python3
"""Genera la lista de exports del proxy winmm: src/winmm_exports.inc y src/winmm.def.

Fuentes (las dos son locales y verificables):
  1. Exports de la winmm.dll de Proton 9 (Wine), con sus ordinales: el JSON de REA
     (inspect_native_exports) de work/evidence/mod/wine9-winmm-exports.json.
  2. Nombres de la import library de mingw-w64 (libwinmm.a), generada a partir de la
     winmm.dll de Windows. Aporta los exports que Wine no tiene (WOW*, *32Message, ...).

El proxy exporta la unión. Un nombre que la winmm real no tenga queda apuntando a un
thunk que loguea el nombre y termina el proceso (ver proxy.c). Faltar un nombre, en
cambio, haría fallar la carga de cualquier módulo que lo importe.

Uso: gen_winmm_exports.py <exports.json> <libwinmm.a> <dir_salida>
"""
import json
import re
import subprocess
import sys
from pathlib import Path


def wine_exports(path):
    data = json.loads(Path(path).read_text())
    res = data["normalized_result"]
    named, ordinal_only = {}, []
    for e in res["exports"]:
        if e.get("forwarder"):
            raise SystemExit(f"export con forwarder inesperado: {e}")
        if e["name"]:
            named[e["name"]] = e["ordinal"]
        else:
            ordinal_only.append(e["ordinal"])
    return named, sorted(ordinal_only), res["artifact"]["sha256"]


def mingw_names(lib):
    out = subprocess.run(["i686-w64-mingw32-nm", lib], check=True,
                         capture_output=True, text=True).stdout
    names = set()
    for m in re.finditer(r"__imp__([A-Za-z0-9_]+?)(?:@\d+)?$", out, re.M):
        names.add(m.group(1))
    return names


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    named, ordinal_only, wine_sha = wine_exports(sys.argv[1])
    extra = sorted(mingw_names(sys.argv[2]) - set(named))
    out_dir = Path(sys.argv[3])

    # Los nombres de Wine conservan su ordinal; los que solo tiene Windows van después.
    next_ord = max(list(named.values()) + ordinal_only) + 1
    entries = sorted(named.items(), key=lambda kv: kv[1])
    for name in extra:
        entries.append((name, next_ord))
        next_ord += 1

    header = (f"Generado por mod/tools/gen_winmm_exports.py. No editar a mano.\n"
              f"Fuentes: winmm.dll de Proton 9 (sha256 {wine_sha}) y libwinmm.a de mingw-w64.\n"
              f"{len(named)} nombres de Wine + {len(extra)} solo de Windows + "
              f"{len(ordinal_only)} solo por ordinal.")

    inc = ["/* " + header.replace("\n", "\n * ") + "\n *",
           " * X(nombre, ordinal): export por nombre.  O(ordinal): export solo por ordinal.",
           " * W(nombre, ordinal): export que solo tiene la winmm de Windows. */"]
    inc += [f"O({o})" for o in ordinal_only]
    extra_set = set(extra)
    for name, o in entries:
        inc.append(f"{'W' if name in extra_set else 'X'}({name}, {o})")
    (out_dir / "winmm_exports.inc").write_text("\n".join(inc) + "\n")

    deff = ["; " + header.replace("\n", "\n; "), 'LIBRARY "winmm.dll"', "EXPORTS"]
    deff += [f"    aimp_ord{o} = aimp_s_ord{o} @{o} NONAME" for o in ordinal_only]
    deff += [f"    {name} = aimp_s_{name} @{o}" for name, o in entries]
    (out_dir / "winmm.def").write_text("\n".join(deff) + "\n")
    print(f"ok: {len(entries)} por nombre, {len(ordinal_only)} por ordinal -> {out_dir}")


if __name__ == "__main__":
    main()
