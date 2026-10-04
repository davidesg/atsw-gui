#!/bin/sh
# La madre conducida desde el codigo (make check en gui/atsw).
#
#   sh tests/run_gui_tests.sh
#
# Las reglas sin ventana de la madre --test_atsw, test_editor, test_genera--
# siguen en la bateria de gui/drtran. Esto es LA VENTANA: tests/test_gui.c
# levanta atsw_gui con su activate() de verdad y la recorre entera (proyecto,
# datos, series, vistazo con fug, editor con fue, linaje, muestras, y los
# lanzamientos de los hermanos). Ver la cabecera de test_gui.c.
#
# Hace falta un servidor grafico (xvfb-run en Linux sin pantalla); sin el,
# la prueba dice «no hay ...» y sale con 0. Hacen falta los motores fue y fug
# compilados en el arbol, y los tres GUIs hermanos compilados: la prueba
# comprueba justamente que la madre los encuentra ahi.
#
# PORTABLE A PROPOSITO: corre igual en Linux, macOS (clang, userland BSD) y
# Windows/MSYS2. Nada de readlink -f, timeout, sed con \| ni /proc.

TOP=$(cd "$(dirname "$0")/.." && pwd)
R=$(cd "$TOP/../.." && pwd)
CC=${CC:-cc}

# En Windows los ejecutables llevan .exe y el compilador lo pone solo; los
# nombres que se borran al final tienen que llevarlo tambien.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=.exe ;;
    *)                    EXE= ;;
esac

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS=$(pkg-config --libs gtk+-3.0 2>/dev/null)
[ -n "$GTK_LIBS" ] || { echo "GTK+3 no esta (pkg-config gtk+-3.0)"; exit 1; }

WORK=$(mktemp -d 2>/dev/null || mktemp -d -t atswgui) || exit 1

# LA PRUEBA SE COMPILA EN gui/atsw/, AL LADO DE atsw_gui: atsw_programa()
# busca a los hermanos relativo al ejecutable que corre, y si la prueba
# viviera en un temporal lo que se probaria es el PATH. El hijo falso va
# alli tambien, porque la madre lo busca en «./».
PRUEBA="$TOP/test_gui_atsw$EXE"
HIJO="$TOP/atsw_hijo_falso$EXE"
trap 'rm -rf "$WORK" "$PRUEBA" "$HIJO"' EXIT INT TERM

# Los fuentes y las -I salen del Makefile, que es el unico sitio donde se
# dice con que se compila la madre. main.c, editor.c y vistazo.c NO van en
# la lista: test_gui.c los incluye, para llegar a sus gestos static.
cd "$TOP" || exit 1
make -s inpcheck_fue.o > "$WORK/make.txt" 2>&1 || { cat "$WORK/make.txt"; exit 1; }
SRCS=$(make -s --no-print-directory fuentes | tr ' ' '\n' |
       grep -v -E '^src/(main|editor|vistazo)\.c$' | tr '\n' ' ')
INC=$(make -s --no-print-directory cabeceras)

# Los de lib/ con -w: sus avisos son de ellos y tienen su propia bateria.
# test_gui.c con -Wall, que es el codigo de aqui.
mkdir -p "$WORK/o"
OBJS=
for s in $SRCS; do
    o="$WORK/o/$(echo "$s" | tr '/.' '__').o"
    $CC -O0 -g -w $GTK_CFLAGS $INC -c "$s" -o "$o" 2>> "$WORK/build.txt" ||
        { cat "$WORK/build.txt"; exit 1; }
    OBJS="$OBJS $o"
done
$CC -O0 -g -Wall $GTK_CFLAGS $INC -c tests/test_gui.c -o "$WORK/test_gui.o" \
    2>> "$WORK/build.txt" || { cat "$WORK/build.txt"; exit 1; }
grep -i 'test_gui.c.*warning' "$WORK/build.txt"
$CC -o "$PRUEBA" "$WORK/test_gui.o" $OBJS inpcheck_fue.o $GTK_LIBS -lz -lm \
    2>> "$WORK/build.txt" || { cat "$WORK/build.txt"; exit 1; }
$CC -O0 -o "$HIJO" tests/hijo_falso.c 2>> "$WORK/build.txt" ||
    { cat "$WORK/build.txt"; exit 1; }

# Lo que la prueba necesita del arbol. Sin los motores o sin los hermanos la
# prueba fallaria por lo que no es suyo; se dice y no se corre.
falta=
for p in engines/fue/bin/fue engines/fug/fug gui/fue/bin/fue_gui \
         gui/fug/gtk_fmg gui/drtran/drtran_gui; do
    [ -x "$R/$p$EXE" ] || falta="$falta $p$EXE"
done
if [ -n "$falta" ]; then
    echo "FAIL: faltan en el arbol:$falta (make en la raiz)"
    exit 1
fi

CSV="$R/engines/drtran/examples/passthrough/data/ipc_wti.csv"

t0=$(date +%s)
"$PRUEBA" "$CSV" > "$WORK/gui.txt" 2> "$WORK/gui_err.txt"
rc=$?
t1=$(date +%s)

if grep -q '^no hay' "$WORK/gui.txt"; then
    cat "$WORK/gui.txt"
    exit 0
fi
cat "$WORK/gui.txt"
if [ $rc != 0 ]; then
    # Si revento, lo ultimo que dijo GTK suele ser la causa.
    grep -q '^FAIL' "$WORK/gui.txt" || echo "FAIL: la prueba acabo con $rc sin decir por que"
    tail -n 20 "$WORK/gui_err.txt"
    exit 1
fi
echo "la madre, conducida: todo en orden ($((t1 - t0)) s)"
exit 0
