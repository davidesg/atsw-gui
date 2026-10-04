#!/bin/sh
# Prueba de gtk_fmg conducido desde el codigo (make check).
#
#   cd gui/fug && sh tests/run_gui_tests.sh
#
# Compila el GUI de verdad -- src/*.c y lo de lib/ que usa, la misma lista
# del Makefile -- con su main.c renombrado (tests/main_wrap.c), y lo conduce
# tests/test_gui.c: carga datos (.inp, texto, CSV, .xlsx), toca las opciones,
# pulsa los botones, corre fug DE VERDAD y mira que la ventana del grafico
# quede dibujada. Ver la cabecera de test_gui.c.
#
# Sin servidor grafico la prueba lo dice ("no hay ...") y no falla; si hay
# xvfb-run, se usa. Sale con 0 si todo fue bien y con 1 si algo fallo.
#
# Portable a proposito: sh de POSIX, nada de readlink -f ni timeout ni sed
# con \|, porque corre tambien en macOS y en MSYS2.

TOP=$(cd "$(dirname "$0")/.." && pwd)
ROOT="$TOP/../.."
LIB="$ROOT/lib"
ENG="$ROOT/engines/fug"
WORK="$TOP/tests/work"
CC=${CC:-cc}

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=.exe ;;
    *)                    EXE= ;;
esac

# Linux sin pantalla: con xvfb-run, si lo hay. macOS y Windows no la piden.
if [ "$EXE" = "" ] && [ "$(uname -s)" != Darwin ] && [ -z "$DISPLAY" ] &&
   [ -z "$WAYLAND_DISPLAY" ] && [ -z "$FMG_TEST_XVFB" ] && command -v xvfb-run > /dev/null 2>&1; then
    FMG_TEST_XVFB=1 exec xvfb-run -a sh "$0" "$@"
fi

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS=$(pkg-config --libs gtk+-3.0 2>/dev/null)
[ -n "$GTK_CFLAGS" ] || { echo "FAIL: GTK+3 no esta (pkg-config gtk+-3.0)"; exit 1; }

rm -rf "$WORK"
mkdir -p "$WORK/obj" "$WORK/data" || exit 1

INC="-I$TOP/include -I$LIB/sitio -I$LIB/preview -I$LIB/fugdraw -I$LIB/datos -I$LIB/xlsx -I$LIB/proyecto -I$ENG/src"

# LO QUE NO TIENE QUE PASAR EN UNA PRUEBA: que un dialogo espere una mano o
# que se abra un visor. Estas funciones de GTK se llaman, DENTRO DEL GUI,
# como las de test_gui.c; el codigo del GUI no se toca. Van en ganchos.h,
# forzada con -include, y no con -D: ver por que en la propia cabecera.
HOOKS="-include $TOP/tests/ganchos.h"

# La lista del Makefile, con main.c por main_wrap.c.
SRCS="$TOP/tests/main_wrap.c $TOP/src/callbacks.c $TOP/src/data_load.c $TOP/src/nlutils.c \
 $TOP/src/fug_run.c $LIB/preview/preview.c $LIB/fugdraw/fugdraw.c $LIB/datos/datos.c \
 $LIB/proyecto/proyecto.c $LIB/xlsx/xlsx.c $LIB/sitio/sitio.c $ENG/src/inpfile.c"

OBJS=
for f in $SRCS; do
    o="$WORK/obj/$(basename "$f" .c).o"
    $CC -O0 -g -w $GTK_CFLAGS $INC $HOOKS -c "$f" -o "$o" 2> "$WORK/build.txt" ||
        { cat "$WORK/build.txt"; echo "FAIL: no compila $f"; exit 1; }
    OBJS="$OBJS $o"
done
$CC -O0 -g -Wall -Wno-deprecated-declarations $GTK_CFLAGS $INC -c "$TOP/tests/test_gui.c" \
    -o "$WORK/obj/test_gui.o" 2> "$WORK/build.txt" ||
    { cat "$WORK/build.txt"; echo "FAIL: no compila test_gui.c"; exit 1; }
$CC -o "$WORK/test_gui$EXE" "$WORK/obj/test_gui.o" $OBJS $GTK_LIBS -lz -lm 2> "$WORK/build.txt" ||
    { cat "$WORK/build.txt"; echo "FAIL: no enlaza test_gui"; exit 1; }

# EL MOTOR DEL ARBOL, POR DELANTE. En el PATH puede haber un fug viejo que no
# entiende lo de ahora y sale con 0 (ver fug_program en src/fug_run.c).
if [ -x "$ENG/fug$EXE" ]; then
    PATH="$(cd "$ENG" && pwd):$PATH"; export PATH
fi
unset FUG FUG_TEST_FAKE
command -v fug > /dev/null 2>&1 || { echo "FAIL: fug no esta (make -C engines/fug)"; exit 1; }

cp "$ENG/tests/data/ART.inp" "$ENG/tests/data/FULL.inp" "$WORK/data/" || exit 1
XLSX=-
if [ -f "$ROOT/engines/drvarma/data/IPC.xlsx" ]; then
    cp "$ROOT/engines/drvarma/data/IPC.xlsx" "$WORK/data/IPC.xlsx" && XLSX="$WORK/data/IPC.xlsx"
fi

( cd "$WORK/data" && "$WORK/test_gui$EXE" "$WORK/data" "$WORK/data/ART.inp" \
      "$WORK/data/FULL.inp" "$XLSX" ) > "$WORK/gui.txt" 2> "$WORK/gui.err"
rc=$?

if grep -q '^no hay' "$WORK/gui.txt"; then
    grep '^no hay' "$WORK/gui.txt"
    exit 0
fi
grep '^FAIL' "$WORK/gui.txt"
tail -n 1 "$WORK/gui.txt"
if [ $rc != 0 ]; then
    # Un cuelgue o un fallo sin FAIL: que se vea por que.
    grep -q '^FAIL' "$WORK/gui.txt" || tail -n 20 "$WORK/gui.err"
    echo "FAIL: gtk_fmg conducido: estado $rc"
    exit 1
fi
echo "gtk_fmg conducido: bien"
exit 0
