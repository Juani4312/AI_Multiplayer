#!/usr/bin/env python3
r"""Disparadores "limpios" para hacer aparecer a las NPC mujeres de un nivel (avatar mujer, 2026-11-21).

Para cada personaje FEMALE de CHARACTERACCESSORYSETS.BIN (volcado <NIVEL>.chars.tsv), busca su Character en el árbol de
scripts (<NIVEL>.tsv.gz: id + inst), sube por sus composites y, en el composite padre de cada uno, las entidades que le
mandan un pin a una entrada "spawn*" (p. ej. CHR_DeathMonitor_3.killed -> [M16]Worker_NPC_7.spawn_npc). Un disparador
es "limpio" si ese pin no va a ningún otro lado: reinyectarlo (FIRE pid pin) la hace aparecer sin tocar nada más.
Verificado a mano en HAB_ShoppingCentre (corrida 158: FIRE 232725ed 9653cab3 -> aparece una obrera, el avatar la elige).

Uso: python3 -I work/scripts/female_spawns.py <NIVEL> [<NIVEL> ...]   (lee work/evidence/cathode/commands/)
Salida: nivel, pid y pin (hex, como los usa el mod), limpio sí/no, tipo del composite que aparece (el avatar usa civiles:
Archetypes\NPCs\Human\Civilian\...), quién dispara y a quién hace aparecer.
"""
import gzip
import hashlib
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "evidence", "cathode", "commands")


def shortguid(name):
    """Igual que shortguid_check.py (hallazgo 06), como uint32 (así lo muestran los logs del mod)."""
    d = hashlib.sha1(name.encode()).digest()
    guid = d[12:16] + d[8:12] + d[4:8] + d[0:4]
    hexs = "".join("%02X" % b for b in reversed(guid))
    return int.from_bytes(hashlib.sha1(hexs.encode()).digest()[:4], "little")


def rows(level):
    with gzip.open(os.path.join(ROOT, level + ".tsv.gz"), "rt", encoding="utf-8") as f:
        for line in f:
            if line[0] in "#p":
                continue
            yield line.rstrip("\n").split("\t")


def level_spawns(level):
    chars = os.path.join(ROOT, level + ".chars.tsv")
    if not os.path.exists(chars):
        return []
    fem = {}
    with open(chars, encoding="utf-8") as f:
        next(f)
        for line in f:
            c = line.rstrip("\n").split("\t")
            if c[3] == "FEMALE":
                fem[(c[1], c[2])] = c[0]
    if not fem:
        return []
    # 1) ruta de cada Character mujer y sus ancestros (ruta -> id del composite instanciado)
    paths = {}
    by_path = {}
    for c in rows(level):
        if (c[2], c[11]) in fem:
            paths[c[7]] = fem[(c[2], c[11])]
    if not paths:
        return []
    want = {}  # ruta del composite padre -> {id del hijo: ruta del hijo (ancestro del Character)}
    anc_paths = set()
    for p in paths:
        parts = p.split("/")
        for k in range(len(parts) - 1, 1, -1):
            anc_paths.add("/".join(parts[:k]))
    types = {}
    for c in rows(level):
        if c[7] in anc_paths:
            by_path[c[7]] = c[2]
            types[c[7]] = c[5]
    for a in anc_paths:
        if a in by_path:
            parent = a.rsplit("/", 1)[0]
            want.setdefault(parent, {})[by_path[a]] = a
    # 2) entidades de esos composites padre que mandan un pin a una entrada spawn* del ancestro
    out = []
    for c in rows(level):
        parent = c[7].rsplit("/", 1)[0]
        if parent not in want or not c[8]:
            continue
        links = [l for l in c[8].split(",") if ">" in l]
        for l in links:
            pin, tgt = l.split(">", 1)
            tid, _, tparam = tgt.partition(".")
            if tid in want[parent] and tparam.lower().startswith("spawn"):
                same_pin = [x for x in links if x.split(">", 1)[0] == pin]
                clean = len(same_pin) == 1
                victim = want[parent][tid]
                out.append((level, c[0], "%08x" % shortguid(pin), pin, clean, c[7], victim, tparam, types.get(victim, "?")))
    return out


def main():
    levels = sys.argv[1:]
    if not levels:
        sys.exit(__doc__)
    for lv in levels:
        for level, pid, pinh, pin, clean, who, victim, tparam, vtype in level_spawns(lv):
            print(f"{level}\t{pid}\t{pinh}\t{'limpio' if clean else 'con otros efectos'}\t{vtype}\t{who}.{pin}\t-> {victim}.{tparam}")


if __name__ == "__main__":
    main()
