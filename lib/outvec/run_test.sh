#!/bin/sh
# run_test.sh -- lib/outvec contra salidas DE VERDAD de drvec.
#
#   sh lib/outvec/run_test.sh [DRVEC]
#
# Pide a drvec, con los .pre de Johansen y Juselius (datos/, los del ejemplo
# johansen_juselius de sima), las tres salidas que lee test_outvec.c: la
# prueba del rango SIN bootstrap (2 s; la del bootstrap va guardada en
# datos/), el VEC con r = 1 (con -fdhess, como sima) y el mismo con una
# restriccion. Lo corre la bateria de drvec (engines/drvec/tests/
# run_tests.sh): si el formato del .out cambia, se sabe el mismo dia.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
BIN=${1:-$AQUI/../../engines/drvec/bin/drvec}
CC=${CC:-cc}
[ -x "$BIN" ] || [ -x "$BIN.exe" ] || { echo "no encuentro $BIN (make -C engines/drvec)"; exit 2; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

$CC -O2 -Wall -Wextra -I"$AQUI" "$AQUI/test_outvec.c" "$AQUI/outvec.c" -o "$W/test_outvec" -lm || exit 1

cp "$AQUI"/datos/*.pre "$W/"
cd "$W" || exit 1
corre() {   # corre NOMBRE ARGS...
    n=$1; shift
    "$BIN" "$@" -name "$n" > "$n.log" 2>&1
    rc=$?
    [ $rc -eq 0 ] || { echo "FALLA: drvec $* -name $n salio con $rc"; tail -5 "$n.log"; exit 1; }
}
corre rango LRM_u.pre LRY_u.pre IBO_u.pre IDE_u.pre 2 0 0 -case 2 -lrtest
corre vec1  LRY_u.pre IBO_u.pre IDE_u.pre LRM_u.pre 2 0 1 -case 2 -fdhess
corre wx    LRY_u.pre IBO_u.pre IDE_u.pre LRM_u.pre 2 0 1 -case 2 -weakex 2

"$W/test_outvec" "$W" "$AQUI/datos"
