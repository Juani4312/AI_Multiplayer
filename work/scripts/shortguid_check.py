#!/usr/bin/env python3
"""Verificación offline del algoritmo nombre -> ShortGuid de Cathode (hallazgo 06).

Implementa lo leído en FUN_008bf460 / FUN_008bf730 / FUN_008bf690 de AI.exe:
  d    = SHA1(nombre)
  guid = d[12:16] + d[8:12] + d[4:8] + d[0:4]       (16 bytes en memoria)
  hexs = HEX(guid[15]) ... HEX(guid[0])  (mayúsculas, del último byte al primero)
  sg   = SHA1(hexs)[0:4]                            (ShortGuid, little-endian)
Cuenta las apariciones de cada ShortGuid en un archivo (p. ej. un COMMANDS.PAK)
y las compara con variantes "de control" (otros órdenes de bytes).

Uso: python3 -I work/scripts/shortguid_check.py <archivo> [nombre ...]
"""
import hashlib
import sys

DEFAULT_NAMES = [
    "Door", "TriggerSimple", "on_triggered", "trigger", "Zone", "Checkpoint",
    "position", "name", "reference", "on_started", "on_finished", "PlayerTorch",
    "Character", "AccessTerminal", "UI_Keypad", "PickupSpawner", "LevelLoaded",
    "IsMultiplayerMode", "HostOnlyTrigger", "NetPlayerCounter", "SyncOnAllPlayers",
]


def guid_from_name(name: str) -> bytes:
    if name.startswith("$"):
        name = name[1:]
    d = hashlib.sha1(name.encode("latin-1")).digest()
    return d[12:16] + d[8:12] + d[4:8] + d[0:4]


def shortguid(name: str, variant: str = "binario") -> bytes:
    if variant == "binario":
        g = guid_from_name(name)
        hexs = "".join(f"{b:02X}" for b in reversed(g))
        return hashlib.sha1(hexs.encode()).digest()[:4]
    if variant == "sin_reordenar":          # control: guid = d[0:16], hex en orden directo
        g = hashlib.sha1(name.encode("latin-1")).digest()[:16]
        hexs = "".join(f"{b:02X}" for b in g)
        return hashlib.sha1(hexs.encode()).digest()[:4]
    if variant == "sha1_directo":           # control: primeros 4 bytes de SHA1(nombre)
        return hashlib.sha1(name.encode("latin-1")).digest()[:4]
    raise ValueError(variant)


def main(path, names):
    data = open(path, "rb").read()
    print(f"archivo={path} bytes={len(data)}")
    totals = {}
    for variant in ("binario", "sin_reordenar", "sha1_directo"):
        hits = 0
        rows = []
        for n in names:
            sg = shortguid(n, variant)
            c = data.count(sg)
            hits += c > 0
            rows.append(f"{n}={sg.hex()}:{c}")
        totals[variant] = hits
        print(f"[{variant}] nombres con ≥1 aparición: {hits}/{len(names)}")
        print("   " + " ".join(rows))
    print("resumen:", totals)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:] or DEFAULT_NAMES)
