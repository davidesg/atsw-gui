#!/bin/sh
# bateria.sh -- la bateria de conformidad del formato univariante.
#
#   sh bateria.sh [actor] [corpus]
#
# Los cuatro actores, que son las cuatro implementaciones que ESCRIBEN el
# formato:
#
#   gtkfue   el lector+escritor en C del GUI   (gtk_fue.09/src/file_io.c)
#   pyart    el escritor de art                (art/pipeline.py::_write_inp)
#   pypre    el escritor .pre de Python        (fue/report.py::write_pre)
#   motor    el propio fue en C                (fue-1.14/src/fue.c)
#
# `todos` (por defecto) los pasa los cuatro.
#
# Por cada fichero del corpus y cada actor:
#
#   1. lo lee el lector de Python (fue.load) -- si no puede, el fichero no es
#      del dialecto univariante de fue y se aparta;
#   2. lo pasa por el lector Y el escritor del actor;
#   3. compara los dos MODELOS, no los bytes: dos implementaciones del formato
#      no van a escribir los mismos bytes nunca, y eso no importa. Lo que
#      importa es que quien lo lea despues vea lo mismo.
#
# Un fallo aqui es un fichero que ese actor estropea al guardarlo.
#
# COPIA frente a ESTIMACION. gtkfue y pyart copian: el fichero que sale tiene
# que decir lo mismo que el que entro, valores incluidos. pypre y motor
# ESTIMAN, y su .pre lleva las estimaciones -- otros valores por definicion.
# A esos se les compara en modo estructura: todo menos los valores que el
# fichero de partida declaraba LIBRES. Los FIJOS se siguen comparando, porque
# un parametro fijo que se mueve es un fallo de cualquiera.
#
# El actor sale con 4 cuando RECHAZA el fichero en la puerta -- el C por
# inpcheck, el Python porque el lector lanza. Eso no es un fallo del formato:
# es un fichero de otro dialecto (fuf, fug), y se aparta. Sale con 3 cuando la
# estimacion no sale: tampoco es un fallo del formato.

TOP=$(cd "$(dirname "$0")" && pwd)

case "$1" in
    gtkfue|pyart|pypre|motor|todos) ACTORES=$1; shift ;;
    "")                             ACTORES=todos ;;
    *)                              ACTORES=todos ;;
esac
[ "$ACTORES" = todos ] && ACTORES="gtkfue pyart pypre motor"

CORPUS=${1:-$TOP/corpus}
: > "$TOP/bateria.log"

total_malos=0

for actor in $ACTORES; do
    MODO=""
    case $actor in
        gtkfue) TOOL="$TOP/gtkfue_roundtrip"
                [ -x "$TOOL" ] || { echo "falta $TOOL"; exit 1; } ;;
        pyart)  TOOL="python3 $TOP/pyart_roundtrip.py" ;;
        pypre)  TOOL="python3 $TOP/pypre_roundtrip.py"; MODO=--estructura ;;
        motor)  TOOL="sh $TOP/motor_roundtrip.sh";      MODO=--estructura ;;
    esac

    RT="$TOP/rt.$actor"
    rm -rf "$RT"; mkdir -p "$RT" || exit 1

    ok=0; malos=0; apartados=0; sinestimar=0
    echo "=== $actor ===" >> "$TOP/bateria.log"

    for f in "$CORPUS"/*.inp "$CORPUS"/*.pre; do
        [ -f "$f" ] || continue
        b=$(basename "$f")
        n=${b%.*}

        # 1. que el lector de Python lo entienda
        if ! python3 -c "import fue,sys; fue.load(sys.argv[1])" "$f" > /dev/null 2>&1; then
            apartados=$((apartados + 1))
            echo "APARTADO  $b  (no es del dialecto univariante de fue)" >> "$TOP/bateria.log"
            continue
        fi

        # 2. el camino del actor
        dicho=$(timeout 120 $TOOL "$f" "$RT" "$n" 2>/dev/null)
        caso=$?
        if [ $caso = 4 ]; then
            apartados=$((apartados + 1))
            echo "RECHAZADO $b  ${dicho#RECHAZADO }" >> "$TOP/bateria.log"
            continue
        fi
        if [ $caso = 3 ]; then
            # la estimacion no sale: no es un fallo del formato
            sinestimar=$((sinestimar + 1))
            echo "SIN ESTIMAR $b  $dicho" >> "$TOP/bateria.log"
            continue
        fi
        if [ $caso != 0 ] || [ ! -f "$RT/$n.inp" ]; then
            malos=$((malos + 1))
            nota="no pudo reescribirlo"
            [ $caso = 124 ] && nota="SE COLGO leyendolo"
            echo "FALLO     $b  $nota" >> "$TOP/bateria.log"
            echo "[$actor] FALLO  $b  $nota"
            continue
        fi

        # 3. el mismo modelo
        if out=$("$TOP/comparar.py" $MODO "$f" "$RT/$n.inp" 2>&1); then
            ok=$((ok + 1))
        else
            malos=$((malos + 1))
            echo "FALLO     $b" >> "$TOP/bateria.log"
            echo "$out" | sed 's/^/          /' >> "$TOP/bateria.log"
            echo "[$actor] FALLO  $b  $(echo "$out" | head -1)"
        fi
    done

    extra=""
    [ $sinestimar -gt 0 ] && extra=", $sinestimar sin estimar"
    echo
    echo "$actor: $ok pasan, $malos fallan, $apartados apartados$extra"
    echo "" >> "$TOP/bateria.log"
    total_malos=$((total_malos + malos))
done

echo "(el detalle, en bateria.log)"
[ $total_malos = 0 ]
