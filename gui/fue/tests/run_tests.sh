#!/bin/sh
# Tests of fue_gui (make check).
#
#   sh tests/run_tests.sh
#
# What the GUI does without a window: the name it derives from what the user
# types (src/utils.c) and the way it runs the engines (src/engine.c). The
# engine is a stand-in, tests/fake/fue, so nothing here depends on fue or fuf
# being installed; what is tested is that the GUI reads the exit status, that
# it passes on what the engine said, and that an argument with a space or a
# semicolon reaches the engine whole -- it used to go through /bin/sh.
#
# There is no window: a test that opens one needs an X server (xvfb-run),
# which is not assumed here.

TOP=$(cd "$(dirname "$0")/.." && pwd)
# La biblioteca compartida y los dos inpcheck, que NO son copias: se compila
# el fichero del motor renombrando la funcion, asi que es literalmente el
# mismo que corre el motor.
LIB="$TOP/../../lib"
ENG="$TOP/../../engines"
# ext.c va aparte de utils.c: lib/preview usa getExt, y lo demas de utils.c
# arrastra inpcheck y las constantes de este GUI. Faltaba, y el enlace de
# uno de los binarios auxiliares se caia por getExt.
# Y la lista sigue al Makefile del GUI: lib/datos, lib/xlsx y lib/proyecto
# entraron con la carga de datos y el proyecto, y aqui no se habian puesto.
# lib/analisis y lib/fugplot entran porque src/analisis.c los usa: las
# ventanas de diagnosis y anomalos son LAS MISMAS que las de la madre.
# plotstats con -DPLOTSTATS_TIENE_NLA, que fue_gui ya trae los asignadores.
LIB_SRCS="$LIB/preview/preview.c $LIB/engine/engine.c $LIB/outfile/outfile.c $LIB/utils/utils.c $LIB/utils/ext.c $LIB/fugdraw/fugdraw.c $LIB/datos/datos.c $LIB/xlsx/xlsx.c $LIB/proyecto/proyecto.c $LIB/fugplot/fugplot.c $LIB/fugplot/plotsupport.c $LIB/analisis/an_comun.c $LIB/analisis/an_anomalos.c $LIB/analisis/an_diagnosis.c $LIB/analisis/an_ganancia.c $LIB/equation/eqtran.c $LIB/analisis/an_sugerir.c $LIB/anomalos/anomalos.c $LIB/dictamen/dictamen.c $LIB/intervencion/intervencion.c $LIB/inpdet/inpdet.c $LIB/tabla/tabla.c"
LIB_INC="-I$LIB/preview -I$LIB/engine -I$LIB/outfile -I$LIB/utils -I$LIB/fugdraw -I$LIB/inpcheck -I$LIB/datos -I$LIB/xlsx -I$LIB/proyecto -I$LIB/fugplot -I$LIB/analisis -I$LIB/equation -I$LIB/anomalos -I$LIB/dictamen -I$LIB/intervencion -I$LIB/inpdet -I$LIB/tabla -I$LIB/rutas -I$ENG/fue/include"
WORK_EARLY="${WORK:-$TOP/tests/work}"

WORK="$TOP/tests/work"
CC=${CC:-gcc}

# plotstats.o va aparte porque necesita -DPLOTSTATS_TIENE_NLA, y se añade a
# LIB_SRCS AQUI y no arriba: $WORK todavia no estaba definido alli.
LIB_SRCS="$LIB_SRCS $WORK/plotstats.o"

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS=$(pkg-config --libs   gtk+-3.0 2>/dev/null)
[ -n "$GTK_CFLAGS" ] || { echo "GTK+3 not found (pkg-config gtk+-3.0)"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK" || exit 1

mkdir -p "$WORK_EARLY"
INPCHECK_O="$WORK_EARLY/inpcheck_fue.o $WORK_EARLY/inpcheck_fuf.o"
$CC -O0 -g -w -I"$TOP/include" $LIB_INC $GTK_CFLAGS -Dinp_check=inp_check_fue \
    -c "$ENG/fue/src/inpcheck.c" -o "$WORK_EARLY/inpcheck_fue.o" || exit 1
$CC -O0 -g -w -I"$TOP/include" -I"$ENG/fuf/include" $LIB_INC $GTK_CFLAGS \
    -Dinp_check=inp_check_fuf -c "$ENG/fuf/src/inpcheck.c" \
    -o "$WORK_EARLY/inpcheck_fuf.o" || exit 1

$CC -O0 -g -w -I"$TOP/include" $LIB_INC $GTK_CFLAGS -DPLOTSTATS_TIENE_NLA \
    -c "$LIB/analisis/plotstats.c" -o "$WORK/plotstats.o" || exit 1

$CC -O0 -g -Wall -I"$TOP/include" $LIB_INC $GTK_CFLAGS \
    "$TOP/tests/test_units.c" $LIB/engine/engine.c $LIB/utils/utils.c $LIB/outfile/outfile.c $INPCHECK_O \
    -o "$WORK/test_units" $GTK_LIBS -lm || exit 1

PATH="$TOP/tests/fake:$PATH" "$WORK/test_units" "$TOP/data"
rc=$?

# --------------------------------------------------------------------------
# La ventana de graficos (src/preview.c) sin ventana: se dibuja una pagina
# con fugdraw, se escribe como PDF y como EPS, y se vuelven a leer con el
# mismo codigo que usa la ventana. La tinta tiene que caer donde se dibujo,
# y los dos caminos tienen que dar la misma imagen.
# --------------------------------------------------------------------------
$CC -O0 -g -Wall -I"$TOP/include" $LIB_INC $GTK_CFLAGS \
    "$TOP/tests/test_preview.c" $LIB/fugdraw/fugdraw.c $LIB/utils/utils.c $LIB/utils/ext.c $INPCHECK_O \
    -o "$WORK/test_preview" $GTK_LIBS -lm || exit 1

pv_fail=0
( cd "$WORK" && ./test_preview make f.pdf f.eps ) || { echo "FAIL: no se pudo dibujar la pagina"; pv_fail=1; }
for f in pdf eps; do
    out=$( cd "$WORK" && ./test_preview show f.$f 150 f_$f.png )
    echo "$out" | grep -q '^pages 1$'              || { echo "FAIL: f.$f: paginas: $out"; pv_fail=1; }
    echo "$out" | grep -q '^points 200.00 100.00$' || { echo "FAIL: f.$f: tamano: $out";  pv_fail=1; }
    # el marco va de (20,20) a (180,80) pt; a 150 ppp, x 41..375 e y 41..167
    ink=$(echo "$out" | sed -n 's/^ink //p')
    ok=$(echo "$ink" | awk '{ print ($1>=39 && $1<=43 && $2>=373 && $2<=377 &&
                                     $3>=39 && $3<=43 && $4>=165 && $4<=169) ? 1 : 0 }')
    [ "$ok" = 1 ] || { echo "FAIL: f.$f: la tinta esta en [$ink], se dibujo en [41 375 41 167]"; pv_fail=1; }
done
cmp -s "$WORK/f_pdf.png" "$WORK/f_eps.png" ||
    { echo "FAIL: el PDF y el EPS de la misma pagina no dan la misma imagen"; pv_fail=1; }

# La lupa: g.pdf lleva un disco de radio 6 en (100,50). Puesta sobre el, el
# disco tiene que salir centrado en el cristal (280x280 -> 140,140) y con
# radio 6*aumento; puesta en (85,40), desplazado 4*15 y 4*10 pixeles.
for z in 2 4 8; do
    out=$( cd "$WORK" && ./test_preview glass g.pdf 100 50 $z )
    half=$(( $(echo "$out" | sed -n 's/^glass //p') / 2 ))
    ink=$(echo "$out" | sed -n 's/^ink //p')
    ok=$(echo "$ink" | awk -v z=$z -v c=$half '{ cx=($1+$2)/2; cy=($3+$4)/2; r=($2-$1)/2;
         print (cx>c-2 && cx<c+2 && cy>c-2 && cy<c+2 && r>z*6-2 && r<z*6+2) ? 1 : 0 }')
    [ "$ok" = 1 ] || { echo "FAIL: la lupa x$z sobre el disco da [$ink], centro esperado $half"; pv_fail=1; }
done
out=$( cd "$WORK" && ./test_preview glass g.pdf 85 40 4 )
half=$(( $(echo "$out" | sed -n 's/^glass //p') / 2 ))
ink=$(echo "$out" | sed -n 's/^ink //p')
ok=$(echo "$ink" | awk -v c=$half '{ cx=($1+$2)/2; cy=($3+$4)/2;
     print (cx>c+58 && cx<c+62 && cy>c-42 && cy<c-38) ? 1 : 0 }')
[ "$ok" = 1 ] || { echo "FAIL: la lupa fuera del centro da [$ink], se esperaba ($((half+60)),$((half-40)))"; pv_fail=1; }
[ $pv_fail = 0 ] && echo "la ventana de graficos: la pagina y la lupa, donde se dibujaron" || rc=1

# --------------------------------------------------------------------------
# El programa de verdad, conducido desde el codigo: se levanta la ventana
# principal (sin ensenarla), se pone el modelo y se llama al boton de Run.
# Comprueba lo que el usuario acaba viendo. Hace falta un servidor grafico;
# si no lo hay, se salta.
# --------------------------------------------------------------------------
GUI_SRCS="$(ls "$TOP"/src/*.c | grep -v '/main\.c$') $LIB_SRCS $INPCHECK_O"
$CC -O0 -g -Wall -I"$TOP/include" $LIB_INC $GTK_CFLAGS \
    "$TOP/tests/test_gui.c" $GUI_SRCS \
    -o "$WORK/test_gui" $GTK_LIBS -lm 2> "$WORK/gui_build.txt" ||
    { cat "$WORK/gui_build.txt"; exit 1; }

# El EDITOR DEL .inp ya no esta aqui: se mudo a la madre, sobre un nodo del
# proyecto (gui/atsw/src/editor.c, docs/DISENO-editor.md §7). Con el se fue
# tests/test_editor.c, que probaba los botones de esta consola.
#
# LA REGLA QUE PROBABA NO SE HA PERDIDO -- "un guardado fallido no puede
# costar trabajo, y se dice la linea" -- : se prueba en el banco de la madre,
# sobre atsw_guarda_inp(), que valida con ESTE MISMO inpcheck.

if command -v fue > /dev/null 2>&1; then
    mkdir -p "$WORK/gui"
    cp "$TOP/data/D1.inp" "$WORK/gui/"
    ( cd "$WORK/gui" && "$WORK/test_gui" "$PWD" D1 ) 2>/dev/null > "$WORK/gui.txt"
    if [ $? = 0 ]; then
        sed -n 's/^\(barra\|globo\|pestana\)/  &/p' "$WORK/gui.txt"
    else
        grep -E '^FAIL|^no hay' "$WORK/gui.txt"
        grep -q '^no hay' "$WORK/gui.txt" || rc=1
    fi
else
    echo "note: sin fue instalado no se puede probar la ventana con el motor"
fi

# The engines the GUI will really call: it must find them and they must
# answer with a status it understands. Only a note when they are not there.
for e in fue fuf; do
    if command -v $e > /dev/null 2>&1; then
        ( cd "$WORK" && $e NO_SUCH_MODEL > /dev/null 2>&1 )
        s=$?
        if [ $s = 1 ]; then
            echo "note: $e is installed and reports a missing file with status 1"
        else
            echo "note: $e is installed but reports a missing file with status $s"
            echo "      (fue >= 1.14 and fuf >= 1.09 are the ones with the exit codes)"
        fi
    else
        echo "note: $e is not in the PATH"
    fi
done

exit $rc
