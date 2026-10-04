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
fallan=()

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
    ( sleep "$LIMITE"; echo "::error::$nombre: mas de $LIMITE s, la paro"; mata_arbol "$pid" ) &
    local vig=$!
    wait "$pid"; local rc=$?
    mata_arbol "$vig" 2>/dev/null; wait "$vig" 2>/dev/null
    echo "::endgroup::"
    echo "$nombre: rc=$rc en $((SECONDS - t0)) s"
    if [ $rc -ne 0 ]; then
        echo "::error::falla la bateria de $nombre"
        fallan+=("$nombre")
    fi
}

for d in engines/fue engines/fuf engines/fug engines/drtran engines/drvarma \
         gui/fue gui/drtran; do
    corre "$d" sh -c "cd $d && sh tests/run_tests.sh"
done
corre engines/drvec make -s -C engines/drvec test

echo
if [ ${#fallan[@]} -eq 0 ]; then echo "todas las baterias pasan"
else echo "fallan: ${fallan[*]}"; exit 1; fi
