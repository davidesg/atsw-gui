#!/bin/sh
# run_tests.sh -- drvarma's batteries, as `make check` at the root runs them:
#   tests/banco/banco.sh        the .inp path against 0.4.1, byte for byte
#   tests/escalera/pruebas.sh   the ladder (.pre input): the gate, fue's
#                               numbers, BUG-2, the line search, its regression
#   ../../lib/outvarma/run_test.sh  the reader of the ladder's output (SIM, S1),
#                               against files this drvarma has just written
cd "$(dirname "$0")/.." || exit 2
sh tests/banco/banco.sh      || exit 1
sh tests/escalera/pruebas.sh || exit 1
sh ../../lib/outvarma/run_test.sh "$(pwd)/bin/drvarma" || exit 1
