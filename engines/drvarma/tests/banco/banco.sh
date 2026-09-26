#!/bin/sh
# banco.sh -- regresion de drvarma contra la referencia de la 0.4.1.
#
#   sh tests/banco/banco.sh            compara (desde engines/drvarma)
#   sh tests/banco/banco.sh --generar  reescribe ref/ (solo antes de tocar src/)
#
# Cada caso corre en un directorio temporal con una copia de su .inp; se
# comparan byte a byte la salida estandar y todos los ficheros que escribe.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
RAIZ=$(cd "$AQUI/../.." && pwd)
BIN=${DRVARMA:-$RAIZ/bin/drvarma}
REF=$AQUI/ref
MODO=${1:-comparar}
[ -x "$BIN" ] || { echo "no encuentro $BIN (make)"; exit 2; }
[ "$MODO" = --generar ] && mkdir -p "$REF"
fallos=0; total=0
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
grep -v '^#' "$AQUI/casos.txt" | grep -v '^[[:space:]]*$' |
while IFS='|' read -r nombre fich args; do
    nombre=$(echo $nombre); fich=$(echo $fich)
    d=$TMP/$nombre; mkdir -p "$d"
    cp "$RAIZ/$fich" "$d/"
    base=$(basename "$fich" .inp)
    ( cd "$d" && "$BIN" "$base" $args > "$nombre.stdout" 2>&1 )
    rm "$d/$base.inp"
    for f in "$d"/*; do
        ext=${f#$d/$base}
        [ "$f" = "$d/$nombre.stdout" ] && ext=.stdout
        if [ "$MODO" = --generar ]; then cp "$f" "$REF/$nombre$ext"; continue; fi
        if ! cmp -s "$f" "$REF/$nombre$ext"; then
            echo "FALLA $nombre$ext"; diff "$REF/$nombre$ext" "$f" | head -8
            echo x >> "$TMP/fallos"
        fi
    done
    if [ "$MODO" != --generar ]; then
        for r in "$REF/$nombre".*; do
            ext=${r#$REF/$nombre}; g=$d/$base$ext; [ "$ext" = .stdout ] && g=$d/$nombre.stdout
            [ -e "$g" ] || { echo "FALTA $nombre$ext"; echo x >> "$TMP/fallos"; }
        done
    fi
    echo "$nombre" >> "$TMP/hechos"
done
n=$(wc -l < "$TMP/hechos"); f=0; [ -e "$TMP/fallos" ] && f=$(wc -l < "$TMP/fallos")
if [ "$MODO" = --generar ]; then echo "referencia generada: $n casos"; exit 0; fi
echo "$n casos, $f diferencias"; [ "$f" -eq 0 ]
