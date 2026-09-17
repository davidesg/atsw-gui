#!/bin/sh
# motor_roundtrip.sh -- el actor `motor`: el propio fue en C.
#
#     motor_roundtrip.sh <entrada.inp> <directorio> <nombre> [ruta-a-fue]
#
# fue siempre concatena .inp a su argumento, asi que no se le puede dar el
# fichero donde esta: hay que copiarlo a un sitio suyo con el nombre desnudo.
# Eso es parte del contrato -- no se puede ejecutar fue sobre un .pre -- y por
# eso este actor necesita un guion y no una llamada.
#
# Deja <directorio>/<nombre>.inp, que es el .pre que escribe el motor.
#
# Sale 4 si inpcheck lo rechaza (otro dialecto), 3 si la estimacion falla
# (fue escribe los ficheros con las semillas y sale con 3: un .pre que NO es
# un optimo, y que no queremos comparar como si lo fuera).

ENTRADA=$1
DESTINO=$2
NOMBRE=$3
FUE=${4:-fue}

[ -f "$ENTRADA" ] || { echo "no esta $ENTRADA"; exit 2; }

TRABAJO=$(mktemp -d) || exit 2
trap 'rm -rf "$TRABAJO"' EXIT

cp "$ENTRADA" "$TRABAJO/x.inp" || exit 2

salida=$( cd "$TRABAJO" && "$FUE" x 2>&1 )
caso=$?

case $caso in
    0) ;;
    2) echo "RECHAZADO $(echo "$salida" | grep -i '^error' | head -1)"; exit 4 ;;
    3) echo "no estima: $(echo "$salida" | tail -1)"; exit 3 ;;
    *) echo "fue salio con $caso: $(echo "$salida" | tail -1)"; exit 1 ;;
esac

[ -f "$TRABAJO/x.pre" ] || { echo "fue no escribio el .pre"; exit 1; }
cp "$TRABAJO/x.pre" "$DESTINO/$NOMBRE.inp" || exit 1
exit 0
