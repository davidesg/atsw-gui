#!/bin/bash
# Pasa cada bateria de `make check` por separado y sigue aunque falle una;
# al final dice cuales fallaron. Mismo orden que el Makefile de la raiz.
BANCOS="engines/fue engines/fuf engines/fug engines/drtran engines/drvarma gui/fue gui/drtran"
fallan=()
for d in $BANCOS; do
    echo "::group::bateria $d"
    if ( cd "$d" && sh tests/run_tests.sh ); then
        echo "::endgroup::"
    else
        echo "::endgroup::"
        echo "::error::falla la bateria de $d"
        fallan+=("$d")
    fi
done
echo "::group::bateria engines/drvec"
if make -s -C engines/drvec test; then echo "::endgroup::"
else echo "::endgroup::"; echo "::error::falla la bateria de engines/drvec"; fallan+=("engines/drvec"); fi
echo
if [ ${#fallan[@]} -eq 0 ]; then echo "todas las baterias pasan"
else echo "fallan: ${fallan[*]}"; exit 1; fi
