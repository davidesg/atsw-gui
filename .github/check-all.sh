#!/bin/bash
# Pasa cada bateria de `make check` por separado y sigue aunque falle una;
# al final dice cuales fallaron. Mismo orden que el Makefile de la raiz.
#
# Cada bateria tiene un limite (LIMITE segundos, 15 min por defecto): una que
# se cuelga cuenta como fallo y no se come el trabajo entero. El vigilante
# mata el arbol de procesos entero (pkill -P recursivo), que es lo unico que
# funciona igual en Linux, macOS y MSYS2.
LIMITE=${LIMITE:-900}

# Los motores del arbol, primero en el PATH: las pruebas de los GUIs (la
# ventana de fue_gui conducida desde el codigo) los buscan ahi, y sin ellos
# se saltan en silencio.
R=$(pwd)
export PATH="$R/engines/fue/bin:$R/engines/fuf/bin:$R/engines/fug:$R/engines/drtran/bin:$R/engines/drvarma/bin:$R/engines/drvec/bin:$PATH"

# En Windows, GLib busca los esquemas de GSettings y los iconos junto al
# ejecutable (../share). En el arbol de construccion no estan, y fue_gui se
# caia al abrir el selector de ficheros ("No GSettings schemas are
# installed"). Un paquete para Windows tiene que llevar esa carpeta; aqui se
# apunta a la de MSYS2.
case "$(uname -s)" in
    MINGW*|MSYS*)
        pre=${MSYSTEM_PREFIX:-/ucrt64}
        export XDG_DATA_DIRS="$(cygpath -m "$pre/share")"
        export GSETTINGS_SCHEMA_DIR="$(cygpath -m "$pre/share/glib-2.0/schemas")"
        echo "GSETTINGS_SCHEMA_DIR=$GSETTINGS_SCHEMA_DIR"
        [ -f "$pre/share/glib-2.0/schemas/gschemas.compiled" ] ||
            echo "::warning::no hay gschemas.compiled en $pre/share/glib-2.0/schemas" ;;
esac
fallan=()
avisos=()

# LAS QUE NO BLOQUEAN FUERA DE LINUX. drvec y drvarma no van en la primera
# version de atsw-gui. Corren en las tres plataformas y lo que fallen se ve,
# pero fuera de Linux --donde sus referencias no son reproducibles: modelos
# con varios optimos locales, en los que otra libm acaba en otro-- sale como
# aviso y no tumba la CI. En Linux bloquean como todas.
NO_BLOQUEAN="engines/drvec engines/drvarma"
[ "$(uname -s)" = Linux ] && NO_BLOQUEAN=
# Windows corre mas despacio: el limite por estimacion de drvec (30 s) cortaba
# el bootstrap.
case "$(uname -s)" in MINGW*|MSYS*) export RUN_TIMEOUT=${RUN_TIMEOUT:-120} ;; esac

# Que se ve colgado: el arbol de procesos, antes de matarlo. Sin esto una
# bateria parada no dice nada -- su salida va con bufer y se pierde.
ensena_arbol() {
    local p
    ps -o pid=,etime=,args= -p "$1" 2>/dev/null | sed "s/^/    $2/"
    for p in $(pgrep -P "$1" 2>/dev/null); do ensena_arbol "$p" "$2  "; done
}

mata_arbol() {
    local p
    for p in $(pgrep -P "$1" 2>/dev/null); do mata_arbol "$p"; done
    kill -9 "$1" 2>/dev/null
}

corre() {        # corre NOMBRE ORDEN...
    local nombre=$1; shift
    echo "::group::bateria $nombre ($(date +%T))"
    local t0=$SECONDS
    "$@" &
    local pid=$!
    ( sleep "$LIMITE"; echo "::error::$nombre: mas de $LIMITE s, la paro"; ensena_arbol "$pid" ""; mata_arbol "$pid" ) &
    local vig=$!
    wait "$pid"; local rc=$?
    # EL VIGILANTE PRIMERO, su sleep despues. Al reves, al morir el sleep el
    # vigilante llegaba a escribir "mas de LIMITE s" antes de que lo mataran:
    # una falsa alarma sobre una bateria que habia pasado.
    local hijos h; hijos=$(pgrep -P "$vig" 2>/dev/null)
    kill -9 "$vig" 2>/dev/null
    for h in $hijos; do mata_arbol "$h"; done
    wait "$vig" 2>/dev/null
    echo "::endgroup::"
    echo "$nombre: rc=$rc en $((SECONDS - t0)) s"
    if [ $rc -ne 0 ]; then
        case " $NO_BLOQUEAN " in
            *" $nombre "*)
                echo "::warning::falla la bateria de $nombre (no bloquea fuera de Linux)"
                avisos+=("$nombre") ;;
            *)
                echo "::error::falla la bateria de $nombre"
                fallan+=("$nombre") ;;
        esac
    fi
}

for d in engines/fue engines/fuf engines/fug engines/drtran engines/drvarma \
         gui/fue gui/drtran; do
    corre "$d" sh -c "cd $d && sh tests/run_tests.sh"
done
corre engines/drvec make -s -C engines/drvec test

# Los GUIs conducidos desde el codigo que no van dentro de una bateria de
# arriba (los de gui/fue y gui/drtran si van).
corre "gui/fug (ventana)"     sh -c "cd gui/fug && sh tests/run_gui_tests.sh"
corre "gui/atsw (ventana)"   sh -c "cd gui/atsw && sh tests/run_gui_tests.sh"
corre "drvarma_gui (ventana)" sh -c "cd engines/drvarma && sh tests/gui/run_gui_tests.sh"

echo
[ ${#avisos[@]} -gt 0 ] && echo "fallan sin bloquear: ${avisos[*]}"
if [ ${#fallan[@]} -eq 0 ]; then echo "todas las baterias que bloquean pasan"
else echo "fallan: ${fallan[*]}"; exit 1; fi
