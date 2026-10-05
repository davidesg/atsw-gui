#!/bin/sh
# Regression tests of art (the identifier).
#   Usage: sh tests/run_tests.sh [path/to/art] [--update]
# Each case runs art on a file of tests/corpus and compares DATA_art.out
# (without its date line) and DATA_art.cand with tests/golden, byte for byte.
# --update rewrites the golden files from this run.

ART=${1:-bin/art}
case "$ART" in /*) ;; *) ART="$(pwd)/$ART" ;; esac
UPDATE=0
[ "$2" = "--update" ] && UPDATE=1
TOP=$(cd "$(dirname "$0")/.." && pwd)
WORK="$TOP/tests/work"
GOLD="$TOP/tests/golden"
# Byte a byte en Linux; fuera, las cifras con tolerancia y sin \r (ver
# conformidad/referencia.sh): en macOS las PACF teoricas que son cero salen
# como ruido de maquina distinto (4e-17 / -6e-18), y en Windows el .cand se
# escribe con \r\n.
REFERENCIA_SH="$TOP/../../conformidad/referencia.sh"
. "$REFERENCIA_SH"
PASS=0
FAIL=0

rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 1
cp "$TOP"/tests/corpus/* .

ok()  { PASS=$((PASS+1)); }
bad() { FAIL=$((FAIL+1)); echo "FAIL: $*"; }

# run <name> <expected rc> art-args...
run() {
    name=$1; want=$2; shift 2
    "$ART" "$@" > "$name.log" 2> "$name.err"
    rc=$?
    if [ "$rc" != "$want" ]; then
        bad "$name: exit status $rc (expected $want)"; sed 's/^/    /' "$name.err"; return 1
    fi
    ok
}

# same <name> <data base>: the .out (date line dropped) and the .cand
same() {
    name=$1; base=$2
    sed 2d "${base}_art.out" > "$name.out"
    cp "${base}_art.cand" "$name.cand"
    for ext in out cand; do
        if [ $UPDATE = 1 ]; then cp "$name.$ext" "$GOLD/$name.$ext"; ok
        elif referencia "$GOLD/$name.$ext" "$name.$ext" "$name" "$TOP/tests/fragiles.txt"; then ok
        else bad "$name.$ext differs from the golden"; diff "$GOLD/$name.$ext" "$name.$ext" | head -8 | sed 's/^/    /'
        fi
    done
}

# E2: the identification graphs' series, with lambda, d, D given
run wti      0 wti.txt -s 12 -l -d 1          && same wti wti
run PS       0 PS.txt  -s 12 -l -d 1          && same PS PS
run GY       0 GY.txt  -s 4  -l -d 1          && same GY GY
# the frequency declared by the file wins over -s (and gives GY's answer)
run GY_freq  0 GY_freq.txt -l -d 1            && same GY_freq GY_freq
sed 2d GY.cand > a.tmp; sed 2d GY_freq.cand > b.tmp    # line 2 is the name
if cmp -s a.tmp b.tmp; then ok; else bad "GY_freq: not GY's candidates"; fi
# E3: a base model's residuals -- d = D = 0, the harmonics already modelled
run resid    0 residuals.txt -s 12 --harmonics off   && same resid residuals
# no tests: no seasonal F, no ADF/KPSS
run notests  0 wti.txt -s 12 -l -d 1 --no-tests
if grep -q '^seasonal \|^adf ' wti_art.cand; then bad "notests: tests in the .cand"; else ok; fi

# errors: nothing is written
run nofile   1 does_not_exist.txt
run badopt   1 wti.txt --frobnicate
run logzero  2 zero.txt -l -d 1
if [ -f zero_art.out ] || [ -f zero_art.cand ]; then bad "logzero: wrote a result"; else ok; fi

echo "art: $PASS passed, $FAIL failed"
[ $FAIL = 0 ]
