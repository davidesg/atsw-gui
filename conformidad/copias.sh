#!/bin/bash
# copias.sh -- las copias de inpcheck tienen que seguir siendo copias.
#
#     bash copias.sh   (usa sustitucion de procesos: no es POSIX sh)
#
# gtk_fue.09/src/inpcheck_fue.c es fue-1.14/src/inpcheck.c copiado, y
# gtk_fue.09/src/inpcheck_fuf.c es el de fuf-1.09. Lo unico que cambia es el
# nombre de la funcion, que alli es inp_check en los dos y aqui tiene que
# distinguirlos.
#
# Esa identidad no es estetica: es lo UNICO que garantiza que el GUI acepte
# exactamente lo que el motor acepta. En cuanto divergen, el GUI carga
# ficheros que el motor rechaza, o rechaza los que acepta, y en los dos casos
# el usuario se entera tarde.
#
# Ya paso: el motor gano la cota de 10 deterministas no estandar y la copia se
# quedo sin ella, asi que el GUI seguia cargando un fichero que le destruia el
# monton. Lo destapo acuerdo.sh, de rebote. Esto lo dice de frente.
#
# Los limites PROPIOS del GUI --sus vectores estaticos son mas pequenos que
# los del motor-- no van aqui: viven en inp_fits_gui() (gtk_fue/src/utils.c),
# que es su puerta y no la compartida.

TOP=$(cd "$(dirname "$0")" && pwd)
GUI=${GUI:-$TOP/../../gtk_fue.09}
FUE=${FUE:-$TOP/../fue/fue-1.14}
FUF=${FUF:-$TOP/../fuf/fuf-1.09}

malas=0

comprueba() {
    copia=$1; original=$2; funcion=$3

    if [ ! -f "$copia" ]    ; then echo "no esta $copia";    malas=$((malas+1)); return; fi
    if [ ! -f "$original" ] ; then echo "no esta $original"; malas=$((malas+1)); return; fi

    if diff -q "$original" \
            <(sed "s/$funcion/inp_check/g" "$copia") > /dev/null 2>&1; then
        echo "ok        $(basename "$copia")  ==  $(cd "$(dirname "$original")" && pwd)/$(basename "$original")"
    else
        malas=$((malas + 1))
        echo "DIVERGEN  $(basename "$copia")  y  $(cd "$(dirname "$original")" && pwd)/$(basename "$original")"
        diff "$original" <(sed "s/$funcion/inp_check/g" "$copia") | sed 's/^/          /' | head -30
    fi
}

comprueba "$GUI/src/inpcheck_fue.c" "$FUE/src/inpcheck.c" inp_check_fue
comprueba "$GUI/src/inpcheck_fuf.c" "$FUF/src/inpcheck.c" inp_check_fuf

echo
[ $malas = 0 ] && echo "las copias estan al dia" || echo "$malas copia(s) fuera de fecha"
[ $malas = 0 ]
