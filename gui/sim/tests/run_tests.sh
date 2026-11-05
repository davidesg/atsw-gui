#!/bin/sh
# sim_gui conducido desde el codigo: la ventana de verdad, sus botones de
# verdad y el drvarma del arbol. Ver tests/test_gui.c. Tambien lib/sistema.
#
# Sin servidor grafico la ventana se salta con una nota ("no hay ...") y sale
# con 0. Con servidor, cada fallo es una linea "FAIL:" y sale con 1.
#
# PORTABLE: sh de POSIX, como run_gui_tests.sh de drtran_gui.

TOP=$(cd "$(dirname "$0")/.." && pwd)
L="$TOP/../../lib"
E="$TOP/../../engines/drvarma"
W="$TOP/tests/work"
rm -rf "$W"; mkdir -p "$W"
CC=${CC:-cc}

GSL_CFLAGS=$(pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS=$(pkg-config --libs gsl 2>/dev/null)
[ -n "$GSL_LIBS" ] || GSL_LIBS="-lgsl -lgslcblas"

INC="-I$TOP/include -I$E/include"
for d in sistema outvarma outdiag fuepre fuedet dates lik optim diagmv prewhiten \
         inpcheck proyecto engine coste sitio barra preview fugdraw ccfplot utils fugplot equation; do
    INC="$INC -I$L/$d"
done

# 1. lib/sistema, sin ventana.
echo "lib/sistema"
$CC -O0 -g -w $INC $GSL_CFLAGS "$L/sistema/test_sistema.c" "$L/sistema/sistema.c" \
    "$L/fuepre/fue_pre_reader.c" "$E/src/nlatools.c" "$L/dates/dates.c" "$L/fuedet/fuedet.c" \
    -o "$W/test_sistema" $GSL_LIBS -lm || exit 1
"$W/test_sistema" "$L/outvarma/datos" 2> /dev/null > "$W/sistema.txt"
rc=$?
grep -E "FALLA|fallos" "$W/sistema.txt"
[ $rc -eq 0 ] || exit 1

# 2. La ventana.
pkg-config --exists gtk+-3.0 2>/dev/null || { echo "note: sin GTK3 no se prueba la ventana"; exit 0; }
if [ -d "$E/bin" ]; then PATH="$E/bin:$PATH"; export PATH; fi
if ! command -v drvarma > /dev/null 2>&1; then
    echo "note: drvarma no esta en el PATH ni en $E/bin: no se prueba la ventana"
    exit 0
fi

# Las fuentes salen de SU Makefile: lo que el programa enlace es lo que se prueba.
SRC=$(sed -n '/^SRC *=/,/^ *$/p' "$TOP/Makefile" | tr -d '\\' | sed 's/^SRC *=//' |
      tr ' \t' '\n\n' | grep '\.c$' | grep -v '^src/main\.c$')
FUENTES=""
for f in $SRC; do
    case "$f" in
        src/*) FUENTES="$FUENTES $TOP/$f" ;;
        *)     FUENTES="$FUENTES $(echo "$f" | sed "s#^\$(LIB)#$L#; s#^\$(ENG)#$E#")" ;;
    esac
done

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0)
GTK_LIBS=$(pkg-config --libs gtk+-3.0)

case "$(uname -s)" in MINGW*|MSYS*) EXE=.exe ;; *) EXE= ;; esac
$CC -O0 -g -w "$TOP/tests/fake_drvarma.c" -o "$W/fake_drvarma$EXE" || exit 1
$CC -O0 -g -w $INC $GTK_CFLAGS -Dmain=sim_main -c "$TOP/src/main.c" -o "$W/main_gui.o" || exit 1
$CC -O0 -g -w $INC $GTK_CFLAGS $GSL_CFLAGS "$TOP/tests/test_gui.c" "$W/main_gui.o" \
    $FUENTES -o "$W/test_gui" $GTK_LIBS $GSL_LIBS -lm 2> "$W/build.txt" ||
    { cat "$W/build.txt"; exit 1; }

echo "sim_gui, conducido desde el codigo (motor: $(command -v drvarma))"
( cd "$W" && ./test_gui "$L/outvarma/datos" "$W" "$W/fake_drvarma$EXE" ) > "$W/gui.txt" 2> "$W/gui_err.txt"
rc=$?
if grep -q '^no hay' "$W/gui.txt"; then
    grep '^no hay' "$W/gui.txt"
    grep '^FAIL' "$W/gui.txt"
    exit $rc
fi
grep -E '^(FAIL|puerta|modelo|estimación|diagnosis|evaluación|simplificar|estructura|sim_gui)' "$W/gui.txt"
[ $rc -eq 0 ] || { echo "test_gui salio con $rc"; tail -20 "$W/gui_err.txt"; exit 1; }
exit 0
