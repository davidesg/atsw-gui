#!/bin/sh
# run_test.sh -- lib/partida (la ruta U de SIM) con el art y el fue del arbol.
#
#   sh lib/partida/run_test.sh
#
# La regla contra sima sobre los datos crudos de docs/ejemplos, y un modelo
# de partida hecho de verdad en una copia de johansen_juselius.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$AQUI/.." && pwd)
R=$(cd "$LIB/.." && pwd)
E="$R/engines"
CC=${CC:-cc}
ART="$E/art/bin/art"
FUE="$E/fue/bin"
[ -x "$ART.exe" ] && ART="$ART.exe"
[ -x "$ART" ] || { echo "no encuentro $ART (make -C engines/art)"; exit 2; }
[ -x "$FUE/fue" ] || [ -x "$FUE/fue.exe" ] || { echo "no encuentro $FUE/fue (make -C engines/fue)"; exit 2; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

GSL_CFLAGS=$(pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS=$(pkg-config --libs gsl 2>/dev/null)
[ -n "$GSL_LIBS" ] || GSL_LIBS="-lgsl -lgslcblas"

$CC -O2 -w -I"$AQUI" -I"$LIB/artcand" -I"$LIB/raices" -I"$LIB/proyecto" -I"$LIB/inpdet" \
    -I"$LIB/inpcheck" -I"$LIB/engine" -I"$LIB/sitio" -I"$LIB/coste" -I"$LIB/datos" -I"$LIB/xlsx" \
    -I"$E/fug/src" -I"$E/art/include" -I"$E/fue/include" \
    $(pkg-config --cflags glib-2.0) $GSL_CFLAGS \
    "$AQUI/test_partida.c" "$AQUI/partida.c" "$AQUI/construye.c" \
    "$LIB/raices/raiz.c" "$E/art/src/unit_root_tests.c" \
    "$LIB/artcand/artcand.c" "$LIB/artcand/artrun.c" "$LIB/inpdet/inpdet.c" \
    "$LIB/proyecto/proyecto.c" "$LIB/engine/engine.c" "$LIB/sitio/sitio.c" "$LIB/coste/coste.c" \
    "$E/fug/src/inpfile.c" \
    -Dinp_check=inp_check_fue "$E/fue/src/inpcheck.c" \
    -o "$W/test_partida" $(pkg-config --libs glib-2.0) $GSL_LIBS -lm || exit 1

cp -R "$R/docs/ejemplos/johansen_juselius" "$W/jj"
PATH="$FUE:$PATH"; export PATH
"$W/test_partida" "$R/docs/ejemplos" "$ART" "$W"
