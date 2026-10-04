#!/bin/sh
# drtran_gui conducido desde el codigo: la ventana de verdad, sus botones de
# verdad y el drtran del PATH. Ver tests/test_gui.c.
#
# Comprueba lo que el banco de run_tests.sh no podia: que las siete pestanas
# FUNCIONAN -- cargar .pre, la CCF dibujada, poblar la red, el modelo, estimar
# con el motor, la diagnosis, la prevision, el proyecto -- y que los caminos
# malos (un .pre roto, un .dag que nombra a quien no esta, un motor que se
# niega o que no esta) dan un mensaje y no un cuelgue.
#
# Sin servidor grafico se salta con una nota: el programa dice "no hay ..." y
# sale con 0. Con servidor, cada fallo es una linea "FAIL:" y sale con 1.
#
# PORTABLE A PROPOSITO: sh de POSIX, nada de readlink -f, timeout ni sed \|,
# porque esto corre igual en Linux, en macOS (BSD) y en MSYS2.

TOP=$(cd "$(dirname "$0")/.." && pwd)
L="$TOP/../../lib"
E="$TOP/../../engines/drtran"
# Su propio directorio, debajo del work/ que ya ignora git. run_tests.sh borra
# work/ entero al empezar; esto solo borra lo suyo.
W="$TOP/tests/work/gui"
rm -rf "$W"; mkdir -p "$W"
CC=${CC:-cc}

pkg-config --exists gtk+-3.0 2>/dev/null || { echo "note: sin GTK3 no se prueba la ventana"; exit 0; }

# drtran se busca SOLO por el PATH (estima.c). El del arbol, primero: el
# instalado puede ser de hace meses y la prueba seria de otro programa.
if [ -d "$E/bin" ]; then PATH="$E/bin:$PATH"; export PATH; fi
if ! command -v drtran > /dev/null 2>&1; then
    echo "note: drtran no esta en el PATH ni en $E/bin: no se prueba la ventana con el motor"
    exit 0
fi

INC="-I$TOP/include -I$E/include"
for d in fuepre dates prewhiten netfile nsop slots intervencion verdict outdiag \
         gof tabla rutas proyecto outfcst engine ccfplot fugdraw fugplot \
         equation preview utils inpcheck lik; do
    INC="$INC -I$L/$d"
done

# Las fuentes del GUI salen de SU Makefile, para no llevar una lista copiada
# que se quede atras: lo que el programa enlace es lo que se prueba.
SRC=$(sed -n '/^SRC *=/,/^ *$/p' "$TOP/Makefile" | tr -d '\\' | sed 's/^SRC *=//' |
      tr ' \t' '\n\n' | grep '\.c$' | grep -v '^src/main\.c$')
FUENTES=""
for f in $SRC; do
    case "$f" in
        src/*)   FUENTES="$FUENTES $TOP/$f" ;;
        *)       FUENTES="$FUENTES $(echo "$f" | sed "s#^\$(LIB)#$L#; s#^\$(ENG)#$E#")" ;;
    esac
done

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0)
GTK_LIBS=$(pkg-config --libs gtk+-3.0)
GSL_CFLAGS=$(pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS=$(pkg-config --libs gsl 2>/dev/null)
[ -n "$GSL_LIBS" ] || GSL_LIBS="-lgsl -lgslcblas"

# main.c entra con otro nombre: asi se prueban sus opciones (--help,
# --proyecto con un manifiesto roto) sin arrancar la aplicacion.
$CC -O0 -g -w $INC $GTK_CFLAGS -Dmain=drtran_main -c "$TOP/src/main.c" \
    -o "$W/main_gui.o" || exit 1
$CC -O0 -g -w $INC $GTK_CFLAGS $GSL_CFLAGS "$TOP/tests/test_gui.c" "$W/main_gui.o" \
    $FUENTES -o "$W/test_gui" $GTK_LIBS $GSL_LIBS -lm 2> "$W/build.txt" ||
    { cat "$W/build.txt"; exit 1; }

echo "drtran_gui, conducido desde el codigo (motor: $(command -v drtran))"
( cd "$W" && ./test_gui "$E/tests/data" "$W" ) > "$W/gui.txt" 2> "$W/gui_err.txt"
rc=$?
if grep -q '^no hay' "$W/gui.txt"; then
    grep '^no hay' "$W/gui.txt"
    exit 0
fi
# Lo que se vio, resumido (una linea por cosa), y los fallos enteros.
grep -E '^(pre malo|ventana|identificacion|grafico|motor que falla|modelo|estimacion|diagnosis|ajuste|proyecto|dialogos) ' "$W/gui.txt" |
    sed 's/^/  /'
grep '^FAIL' "$W/gui.txt"
tail -1 "$W/gui.txt"
if [ $rc != 0 ]; then
    # Si murio sin decir nada, que al menos se vea por que.
    grep -q '^FAIL' "$W/gui.txt" || { echo "FAIL: test_gui salio con $rc"; tail -20 "$W/gui_err.txt"; }
    exit 1
fi
exit 0
