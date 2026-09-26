#!/bin/sh
# run_tests.sh -- drvarma's batteries, as `make check` at the root runs them:
#   tests/banco/banco.sh        the .inp path against 0.4.1, byte for byte
#   tests/escalera/pruebas.sh   the ladder (.pre input): the gate, fue's
#                               numbers, BUG-2, the line search, its regression
cd "$(dirname "$0")/.." || exit 2
sh tests/banco/banco.sh      || exit 1
sh tests/escalera/pruebas.sh || exit 1
