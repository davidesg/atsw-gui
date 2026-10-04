# referencia.sh -- comparar una salida con su referencia, en cualquier
# plataforma. Se carga con ". referencia.sh" desde las baterias.
#
#   referencia REF NUEVO ID FRAGILES
#
# Devuelve 0 si son iguales, 1 si no; con 2, el caso esta en FRAGILES y la
# diferencia se APUNTA pero no cuenta como fallo.
#
# EN LINUX, EL BYTE. Las referencias se escribieron alli y se siguen
# exigiendo exactas: cualquier cambio en un numero es un cambio del programa.
#
# (ATSW_PLATAFORMA=otra lo fuerza, para probar esta rama desde Linux.)
#
# FUERA DE LINUX, LAS CIFRAS (conformidad/cifras.py). macOS y Windows dan
# los mismos coeficientes en 7-9 cifras, pero no los mismos bytes: la libm
# de cada sistema redondea distinto, y el hessiano numerico lo amplifica en
# los errores estandar. Ver la cabecera de cifras.py.
#
# LOS FRAGILES son los casos mal definidos --verosimilitud plana, hessiano
# mal condicionado-- en los que otra plataforma puede acabar en otro punto
# o dar otros errores estandar sin que el programa este mal. Van en un
# fichero de la bateria, uno por linea con su razon:
#
#   RIPC.1    verosimilitud plana: las estimaciones cambian dentro de su error
#
# Es una lista CORTA y a la vista: un caso entra con su razon, no para
# callar la bateria. Fuera de Linux se informan como FRAGIL y no fallan.

CIFRAS="$(cd "$(dirname "${REFERENCIA_SH:-.}")" 2>/dev/null && pwd)/cifras.py"

referencia() {
    cmp -s "$1" "$2" && return 0
    [ "${ATSW_PLATAFORMA:-$(uname -s)}" = Linux ] && return 1
    command -v python3 > /dev/null 2>&1 || return 1
    python3 "$CIFRAS" "$1" "$2" > /dev/null 2>&1 && return 0
    if [ -f "$4" ] && grep -q "^$3[[:space:]]" "$4"; then
        return 2
    fi
    return 1
}
