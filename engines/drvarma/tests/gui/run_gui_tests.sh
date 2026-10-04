#!/bin/sh
# Pruebas conducidas de drvarma_gui (make check-gui).
#
#   cd engines/drvarma && sh tests/gui/run_gui_tests.sh
#
# test_drvarma_gui.c incluye gui/drvarma_gui.c entero, levanta la ventana
# principal sin ensenarla y la maneja como el usuario: carga datos con el
# selector, contesta el dialogo de propiedades, toca las pestanas, corre el
# drvarma de verdad (el del PATH, como lo busca el GUI), y mira la barra de
# estado, el visor y los ficheros que quedan. El motor que falla es
# fake_drvarma.c, un programa: en Windows un guion de shell no se lanza.
#
# Sin servidor grafico no falla: lo dice ("no hay ...") y sale con 0. En
# Linux sin DISPLAY se usa xvfb-run si esta. Sale con 1 si algo falla; cada
# fallo es una linea "FAIL: ...".

cd "$(dirname "$0")/../.." || exit 1
TOP=$(pwd)
WORK="$TOP/tests/gui/work"

# Linux sin pantalla: xvfb-run, si lo hay (en macOS y Windows GTK no
# necesita servidor X).
if [ "$(uname -s)" = Linux ] && [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ] &&
   [ -z "$DRVARMA_GUI_XVFB" ] && command -v xvfb-run > /dev/null 2>&1; then
    DRVARMA_GUI_XVFB=1 exec xvfb-run -a sh "$0" "$@"
fi

# Los flags del GUI, del Makefile: los -I de lib/, GTK, GSL y -DDRVARMA_GUI.
FLAGS=$(make -s --no-print-directory gui-flags) || { echo "FAIL: make gui-flags"; exit 1; }
CC=${CC:-$(echo "$FLAGS" | sed -n 's/^CC=//p')}
GUI_CFLAGS=$(echo "$FLAGS" | sed -n 's/^CFLAGS=//p')
GUI_LIBS=$(echo "$FLAGS" | sed -n 's/^LIBS=//p')

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=.exe ;;
    *) EXE= ;;
esac

# El motor: el de ESTE arbol, delante en el PATH (construido si falta). Se
# prueba el GUI con su motor; un drvarma viejo instalado (~/.local/bin) que
# no entiende -volexp haria fallar la prueba por algo que no es del GUI. El
# GUI lo sigue buscando en el PATH, como en una instalacion.
[ -x "$TOP/bin/drvarma$EXE" ] || make -s drvarma > /dev/null 2>&1 ||
    { echo "FAIL: no se pudo construir drvarma"; exit 1; }
PATH="$TOP/bin:$PATH"; export PATH

rm -rf "$WORK"; mkdir -p "$WORK" || exit 1

# -O0 para que un fallo se pueda seguir con el depurador.
# shellcheck disable=SC2086
$CC $GUI_CFLAGS -O0 -Wno-unused-function \
    tests/gui/test_drvarma_gui.c gui/seasonal_detection.c gui/johansen_test.c gui/vecm.c \
    -o "$WORK/test_drvarma_gui$EXE" $GUI_LIBS > "$WORK/build.txt" 2>&1 ||
    { cat "$WORK/build.txt"; echo "FAIL: no compila la prueba del GUI"; exit 1; }
$CC -O0 tests/gui/fake_drvarma.c -o "$WORK/fake_drvarma$EXE" ||
    { echo "FAIL: no compila el motor falso"; exit 1; }

# Sin el puente de accesibilidad (avisa si no hay bus de sesion, como en la
# CI) y con la configuracion en memoria: la prueba no toca la del usuario.
NO_AT_BRIDGE=1; GSETTINGS_BACKEND=memory; export NO_AT_BRIDGE GSETTINGS_BACKEND

# El GUI escribe mucho DEBUG por stdout: se guarda y se ensena lo que importa.
"$WORK/test_drvarma_gui$EXE" "$TOP/data" "$WORK/fake_drvarma$EXE" > "$WORK/gui.txt" 2> "$WORK/gui_err.txt"
st=$?

grep -E '^(FAIL|no hay)' "$WORK/gui.txt"
if grep -q '^no hay' "$WORK/gui.txt"; then
    exit 0
fi
grep '^drvarma_gui:' "$WORK/gui.txt"
if [ $st -ne 0 ] || grep -q '^FAIL' "$WORK/gui.txt"; then
    grep -q '^drvarma_gui:' "$WORK/gui.txt" || {
        echo "FAIL: la prueba se interrumpio (estado $st); lo ultimo que dijo:"
        tail -5 "$WORK/gui.txt"; tail -5 "$WORK/gui_err.txt"
    }
    exit 1
fi
echo "drvarma_gui: todo bien"
exit 0
