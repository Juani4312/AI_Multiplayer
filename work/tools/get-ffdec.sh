#!/usr/bin/env bash
# Baja JPEXS FFDec (descompilador de Flash/GFX, GPL-3) a ~/tools/ffdec-<versión> y verifica el SHA-256.
# Se usa para leer el ActionScript de la UI del juego (DATA/UI.PAK -> *.GFX; ver work/tools/README.md).
set -euo pipefail
VER=26.5.0
SHA=8d00c2713a724bb3f7e6c9a64994836ce3a953b5ac264f405d98f92e7c266d33
DEST="$HOME/tools/ffdec-$VER"
if [ -x "$DEST/ffdec.sh" ]; then echo "ya está: $DEST"; exit 0; fi
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
curl -fsSL -o "$tmp/ffdec.zip" "https://github.com/jindrapetrik/jpexs-decompiler/releases/download/version$VER/ffdec_$VER.zip"
echo "$SHA  $tmp/ffdec.zip" | sha256sum -c -
mkdir -p "$DEST" && (cd "$DEST" && unzip -q "$tmp/ffdec.zip") && chmod +x "$DEST/ffdec.sh"
echo "instalado en $DEST (correr: $DEST/ffdec.sh -help)"
