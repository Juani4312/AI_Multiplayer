#!/usr/bin/env python3
"""Activa y desactiva mods de terceros "de archivos" (texturas, PAKs, etc.) en la instalación de Steam.

  extmod.py status [NOMBRE]          lista los mods guardados y si están activos (y si algo no cuadra)
  extmod.py add NOMBRE ORIGEN        guarda un mod: ORIGEN es un .7z/.zip/.rar o una carpeta. Busca adentro la
                                     carpeta que tiene DATA/ (la raíz del juego) y la copia al almacén
  extmod.py on NOMBRE                lo activa: aparta los archivos originales del juego que pisa y pone los del mod
  extmod.py off NOMBRE               lo desactiva: devuelve los archivos del mod al almacén y restaura los originales
  extmod.py remove NOMBRE            borra el mod del almacén (tiene que estar desactivado)

Cómo funciona: el almacén (STORE) está en el mismo disco que el juego, así que activar y desactivar son
renombres (instantáneos, sin ocupar espacio extra), aunque el mod pese varios GB. Por mod:
  STORE/NOMBRE/files/     archivos del mod mientras está desactivado (rutas relativas a la carpeta del juego)
  STORE/NOMBRE/orig/      originales del juego apartados mientras está activo
  STORE/NOMBRE/manifest.json
on y off son idempotentes archivo por archivo: si algo se corta a la mitad, volver a correr el mismo comando
termina el trabajo. Se niega a correr con el juego abierto.

Seguridad respecto del proyecto: rechaza mods que traigan AI.exe o winmm.dll (el proxy del mod coop) y avisa
si traen COMMANDS.PAK (cambia los pines de CATHODE que sincroniza el coop) u otras DLL/EXE.
Si Steam "verifica los archivos" con un mod activo, restaura los originales encima: off lo detecta por tamaño
y no mete el archivo de Steam en el almacén.
"""
import json
import os
import shutil
import subprocess
import sys

GAME = os.environ.get("EXTMOD_GAME", "/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation")
STORE = os.environ.get("EXTMOD_STORE", "/mnt/1,8TB/SteamLibrary/AlienIsolation_mods")
FORBIDDEN = {"ai.exe", "winmm.dll"}
WARN_NAMES = {"commands.pak"}
WARN_EXT = {".dll", ".exe", ".asi"}


def die(msg, code=1):
    print("ERROR: " + msg, file=sys.stderr)
    sys.exit(code)


def game_running():
    return subprocess.run(["pgrep", "-x", "-i", "AI.exe"], capture_output=True).returncode == 0


def guard_idle():
    if game_running():
        die("el juego está abierto: cerralo antes (pkill -x AI.exe)", 2)


def mod_dir(name):
    if not name or "/" in name or name.startswith("."):
        die(f"nombre inválido: {name!r}")
    return os.path.join(STORE, name)


def load(name):
    p = os.path.join(mod_dir(name), "manifest.json")
    if not os.path.isfile(p):
        die(f"no hay un mod guardado con el nombre {name!r} (extmod.py status)")
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def save(name, man):
    p = os.path.join(mod_dir(name), "manifest.json")
    tmp = p + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(man, f, indent=1, ensure_ascii=False)
    os.replace(tmp, p)


def game_case(rel):
    """Traduce una ruta del mod a la grafía que ya existe en el juego (Data/ vs DATA/): ntfs3 distingue
    mayúsculas y Wine no, así que sin esto quedarían dos carpetas."""
    cur = GAME
    out = []
    for part in rel.split("/"):
        hit = part
        if os.path.isdir(cur):
            low = part.lower()
            for e in os.listdir(cur):
                if e.lower() == low:
                    hit = e
                    break
        out.append(hit)
        cur = os.path.join(cur, hit)
    return "/".join(out)


def find_root(top):
    """La carpeta del mod que equivale a la raíz del juego: la que tiene DATA/ adentro."""
    cands = []
    for d, subdirs, _ in os.walk(top):
        if any(s.lower() == "data" for s in subdirs):
            cands.append(d)
    if not cands:
        die(f"no encontré una carpeta DATA/ dentro de {top}: revisá la estructura del mod y pasá la carpeta raíz a mano")
    cands.sort(key=lambda d: d.count(os.sep))
    if len(cands) > 1 and cands[0].count(os.sep) == cands[1].count(os.sep):
        die("hay varias carpetas DATA/ al mismo nivel (¿variantes del mod?): " + ", ".join(cands[:5]) +
            "\nelegí una y pasá esa carpeta como ORIGEN")
    return cands[0]


def cmd_add(name, src):
    md = mod_dir(name)
    if os.path.exists(md):
        die(f"ya existe {md}")
    src = os.path.abspath(src)
    os.makedirs(md)
    extract = None
    try:
        if os.path.isfile(src):
            extract = os.path.join(md, "extract")
            os.makedirs(extract)
            print(f"extrayendo {src} …")
            r = subprocess.run(["7z", "x", "-y", "-bso0", "-bsp1", "-o" + extract, src])
            if r.returncode != 0:
                die("7z no pudo extraer el archivo (¿descarga incompleta?)")
            top = extract
        elif os.path.isdir(src):
            top = src
        else:
            die(f"no existe {src}")
        root = find_root(top)
        files = []
        for d, _, fs in os.walk(root):
            for fn in fs:
                rel = os.path.relpath(os.path.join(d, fn), root).replace(os.sep, "/")
                files.append(rel)
        bad = [f for f in files if os.path.basename(f).lower() in FORBIDDEN]
        if bad:
            die("el mod trae archivos que rompen el mod coop o la build esperada: " + ", ".join(bad))
        dest = os.path.join(md, "files")
        entries = []
        warn = []
        for rel in sorted(files):
            g = game_case(rel)
            s = os.path.join(root, rel)
            t = os.path.join(dest, g)
            os.makedirs(os.path.dirname(t), exist_ok=True)
            if extract:
                os.replace(s, t)          # mismo disco: renombre
            else:
                shutil.copy2(s, t)
            entries.append({"path": g, "size": os.path.getsize(t)})
            base = os.path.basename(g).lower()
            if base in WARN_NAMES or os.path.splitext(base)[1] in WARN_EXT:
                warn.append(g)
        save(name, {"name": name, "source": src, "state": "off", "files": entries, "created_dirs": []})
        total = sum(e["size"] for e in entries)
        pisa = sum(1 for e in entries if os.path.exists(os.path.join(GAME, e["path"])))
        print(f"guardado {name}: {len(entries)} archivos, {total / 2**30:.2f} GiB; pisa {pisa} archivos del juego, "
              f"agrega {len(entries) - pisa}")
        for w in warn:
            print(f"  AVISO: trae {w}" + (" (cambia los pines de CATHODE: el coop necesita el mismo en las dos máquinas)"
                                          if w.lower().endswith("commands.pak") else " (código: revisar antes de activar)"))
    except BaseException:
        shutil.rmtree(md, ignore_errors=True)
        raise
    finally:
        if extract and os.path.isdir(extract):
            shutil.rmtree(extract, ignore_errors=True)


def cmd_on(name):
    guard_idle()
    man = load(name)
    if man["state"] == "on":
        print(f"{name} ya estaba activo")
        return
    for other in list_mods():
        if other != name and load(other)["state"] != "off":
            paths = {e["path"].lower() for e in load(other)["files"]}
            clash = [e["path"] for e in man["files"] if e["path"].lower() in paths]
            if clash:
                die(f"{other} está activo y pisa los mismos archivos ({clash[0]} …): desactivalo primero")
    md = mod_dir(name)
    man["state"] = "partial"
    save(name, man)
    created = set(man.get("created_dirs", []))
    n = 0
    for e in man["files"]:
        rel = e["path"]
        s = os.path.join(md, "files", rel)
        g = os.path.join(GAME, rel)
        o = os.path.join(md, "orig", rel)
        if not os.path.exists(s):
            continue                      # ya movido (corrida anterior cortada)
        d = os.path.dirname(rel)
        while d and not os.path.isdir(os.path.join(GAME, d)):
            created.add(d)
            d = os.path.dirname(d)
        os.makedirs(os.path.dirname(g), exist_ok=True)
        if os.path.exists(g):
            if os.path.exists(o):
                die(f"ya hay un original apartado en {o}: estado inconsistente, revisar a mano")
            os.makedirs(os.path.dirname(o), exist_ok=True)
            os.replace(g, o)
        os.replace(s, g)
        n += 1
    man["created_dirs"] = sorted(created)
    man["state"] = "on"
    save(name, man)
    print(f"{name} activo ({n} archivos movidos al juego)")


def cmd_off(name):
    guard_idle()
    man = load(name)
    if man["state"] == "off":
        print(f"{name} ya estaba desactivado")
        return
    md = mod_dir(name)
    man["state"] = "partial"
    save(name, man)
    n = 0
    lost = []
    for e in man["files"]:
        rel = e["path"]
        s = os.path.join(md, "files", rel)
        g = os.path.join(GAME, rel)
        o = os.path.join(md, "orig", rel)
        if os.path.exists(g) and not os.path.exists(s):
            if os.path.getsize(g) == e["size"]:
                os.makedirs(os.path.dirname(s), exist_ok=True)
                os.replace(g, s)
                n += 1
            else:
                lost.append(rel)          # Steam lo pisó con el original: no meterlo en el almacén
                if os.path.exists(o):
                    os.remove(o)          # el original apartado sobra: el juego ya tiene el de Steam
                continue
        if os.path.exists(o):
            if os.path.exists(g):
                os.remove(o)              # el juego ya tiene un original (restaurado por Steam)
            else:
                os.replace(o, g)
    for d in sorted(man.get("created_dirs", []), key=lambda x: -x.count("/")):
        try:
            os.rmdir(os.path.join(GAME, d))
        except OSError:
            pass
    orig = os.path.join(md, "orig")
    if os.path.isdir(orig) and not any(fs for _, _, fs in os.walk(orig)):
        shutil.rmtree(orig)
    man["created_dirs"] = []
    man["state"] = "off"
    save(name, man)
    print(f"{name} desactivado ({n} archivos devueltos al almacén)")
    if lost:
        print(f"  AVISO: {len(lost)} archivos del mod ya no estaban en el juego (¿Steam verificó los archivos?); "
              f"el juego quedó con los originales y esos archivos del mod se perdieron: hay que volver a hacer add. "
              f"Ej.: {lost[0]}")


def cmd_remove(name):
    man = load(name)
    if man["state"] != "off":
        die(f"{name} está {man['state']}: desactivalo antes (extmod.py off {name})")
    shutil.rmtree(mod_dir(name))
    print(f"borrado {name} del almacén")


def list_mods():
    if not os.path.isdir(STORE):
        return []
    return sorted(d for d in os.listdir(STORE) if os.path.isfile(os.path.join(STORE, d, "manifest.json")))


def cmd_status(only=None):
    print(f"juego: {GAME}{'  (ABIERTO)' if game_running() else ''}")
    print(f"almacén: {STORE}")
    mods = [only] if only else list_mods()
    if not mods:
        print("no hay mods guardados")
    for name in mods:
        man = load(name)
        md = mod_dir(name)
        bad = []
        for e in man["files"]:
            g = os.path.join(GAME, e["path"])
            s = os.path.join(md, "files", e["path"])
            if man["state"] == "on":
                if not (os.path.exists(g) and os.path.getsize(g) == e["size"]):
                    bad.append(e["path"])
            elif man["state"] == "off":
                if not os.path.exists(s):
                    bad.append(e["path"])
        total = sum(e["size"] for e in man["files"]) / 2**30
        estado = {"on": "ACTIVO", "off": "desactivado"}.get(man["state"], "A MEDIAS (repetí on u off)")
        print(f"- {name}: {estado}; {len(man['files'])} archivos, {total:.2f} GiB")
        if bad:
            print(f"  ¡{len(bad)} archivos no cuadran! (ej. {bad[0]})")


def main(a):
    if not a or a[0] in ("-h", "--help", "help"):
        print(__doc__)
        return
    cmd, rest = a[0], a[1:]
    if cmd == "status" and len(rest) <= 1:
        cmd_status(*rest)
    elif cmd == "add" and len(rest) == 2:
        cmd_add(*rest)
    elif cmd in ("on", "off", "remove") and len(rest) == 1:
        {"on": cmd_on, "off": cmd_off, "remove": cmd_remove}[cmd](rest[0])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
