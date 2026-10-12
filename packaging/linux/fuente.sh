#!/bin/bash
# LAS FUENTES PARA COMPILAR EN LINUX: solo lo que lleva la primera version
# (sin drvarma ni drvec, sin el banco de conformidad ni el estudio), con un
# Makefile que construye e instala (Makefile.fuente) y su INSTALAR.txt:
#
#   bash packaging/linux/fuente.sh [version]
#
# Deja dist/atsw-<version>-fuente.tar.gz.
set -euo pipefail

R=$(cd "$(dirname "$0")/../.." && pwd)
V=${1:-0.0}
N=atsw-$V
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT

cd "$R"
git archive --prefix="$N/" HEAD \
    lib engines/fue engines/fuf engines/fug engines/drtran engines/art gui \
    packaging docs/manual/MANUAL.md docs/manual/datos docs/manual/img |
    tar -x -C "$T"
cp packaging/linux/Makefile.fuente "$T/$N/Makefile"
cp packaging/linux/INSTALAR.txt packaging/LEEME.txt "$T/$N/"
[ -f docs/manual/MANUAL.pdf ] && cp docs/manual/MANUAL.pdf "$T/$N/docs/manual/"
mkdir -p "$R/dist"
tar -C "$T" -czf "$R/dist/$N-fuente.tar.gz" "$N"
echo "hecho: $(du -sh "$R/dist/$N-fuente.tar.gz" | cut -f1)"
