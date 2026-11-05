#!/bin/sh
# run_test.sh -- lib/outvarma contra salidas DE VERDAD de drvarma.
#
#   sh lib/outvarma/run_test.sh [DRVARMA]
#
# Pide a drvarma, con los .pre de Jenkins y Alavi (datos/, los del ejemplo
# jenkins_alavi de sima), las cinco salidas que lee test_outvarma.c y le
# pasa la carpeta. Lo corre la bateria de drvarma (engines/drvarma/tests/
# run_tests.sh): si el formato del .out cambia, se sabe el mismo dia.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$AQUI/.." && pwd)
BIN=${1:-$LIB/../engines/drvarma/bin/drvarma}
CC=${CC:-cc}
[ -x "$BIN" ] || [ -x "$BIN.exe" ] || { echo "no encuentro $BIN (make -C engines/drvarma)"; exit 2; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

$CC -O2 -Wall -Wextra -I"$AQUI" -I"$LIB/outdiag" \
    "$AQUI/test_outvarma.c" "$AQUI/outvarma.c" "$LIB/outdiag/outdiag.c" \
    -o "$W/test_outvarma" -lm || exit 1

cp "$AQUI/datos/MUSKRAT_m03.pre" "$AQUI/datos/MINK_m02.pre" "$W/"
# Un .pre retocado (el primer AR de MUSKRAT) ya no es un optimo: un .inp.
sed 's/^0\.6522 1$/0.3000 1/' "$W/MUSKRAT_m03.pre" > "$W/MUSKX.inp"
cmp -s "$W/MUSKRAT_m03.pre" "$W/MUSKX.inp" && { echo "FALLA: no se pudo retocar el .pre"; exit 1; }

cd "$W" || exit 1
M=MUSKRAT_m03.pre; K=MINK_m02.pre
corre() {   # corre NOMBRE ARGS...
    n=$1; shift
    "$BIN" "$@" -o "$n" > "$n.log" 2>&1
    rc=$?
    [ $rc -eq 0 ] || { echo "FALLA: drvarma $* -o $n salio con $rc"; tail -5 "$n.log"; exit 1; }
}
corre puerta $M $K 0 0 -diagcov
corre ar2    $M $K 2 0 -forecast 10
corre rec    $M $K 2 0 -forecast 4 -estwin 48
corre pared  $M $K 1 1 -diagcov -links "MUSKRAT<-MINK"
corre noopt  MUSKX.inp $K 0 0 -diagcov

"$W/test_outvarma" "$W"
