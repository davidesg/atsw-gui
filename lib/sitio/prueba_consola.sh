#!/bin/bash
# ¿ASOMA UNA CONSOLA CUANDO UN GUI LANZA UN MOTOR? (gui/atsw/BUGS.md BUG-0001)
#
# Solo en Windows (MSYS2), con el arbol construido. Compila test_consola con
# -mwindows --sin consola propia, como los GUIs-- y hijo_consola como
# programa de consola, y mira cada camino por el que un GUI lanza un motor
# (ver test_consola.c). Mide ademas lo que tarda el vistazo con fug.
#
# Y mira que los GUIs estan enlazados como GUIs (subsistema 2, «Windows
# GUI»): uno de consola abriria la suya al arrancar.
#
# Sale con 0 si ningun camino abre consola y todos los GUIs lo son.
case "$(uname -s)" in
    MINGW*|MSYS*) ;;
    *) echo "prueba_consola: solo en Windows"; exit 0 ;;
esac
R=$(cd "$(dirname "$0")/../.." && pwd)
L="$R/lib"
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
CC=${CC:-gcc}
mal=0

$CC -O2 -o "$W/hijo_consola.exe" "$L/sitio/hijo_consola.c" || exit 1
$CC -O2 -mwindows -I"$L/sitio" -I"$L/engine" -I"$L/coste" $(pkg-config --cflags glib-2.0) \
    "$L/sitio/test_consola.c" "$L/sitio/sitio.c" "$L/engine/engine.c" "$L/coste/coste.c" \
    -o "$W/test_consola.exe" $(pkg-config --libs glib-2.0) -lm || exit 1

mkdir -p "$W/cache"
cp "$R/engines/fug/tests/data/ART.inp" "$W/cache/vistazo.inp"
"$W/test_consola.exe" "$(cygpath -m "$W/informe.txt")" "$(cygpath -m "$W/hijo_consola.exe")" \
    "$(cygpath -m "$R/engines/fug/fug.exe")" "$(cygpath -m "$W/cache/vistazo")"
rc=$?
cat "$W/informe.txt" 2>/dev/null || echo "test_consola no dejo informe (rc=$rc)"
[ $rc -eq 0 ] || { echo "::error::un GUI que lanza un motor abre una consola (BUG-0001 de la madre)"; mal=1; }

echo "los GUIs, enlazados como GUIs:"
for g in gui/atsw/atsw_gui gui/fue/bin/fue_gui gui/drtran/drtran_gui gui/fug/gtk_fmg; do
    s=$(objdump -p "$R/$g.exe" 2>/dev/null | awk '/^Subsystem/ { print $2, $3, $4 }')
    echo "  $g.exe: ${s:-?}"
    case "$s" in *"Windows GUI"*) ;; *) echo "::error::$g.exe no es un GUI de Windows"; mal=1 ;; esac
done
exit $mal
