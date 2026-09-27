#!/bin/sh
# bug55_ma_residuals.sh -- BUG-55: the forecast's MA part uses STANDARDISED
# residuals for every series but the first.
#
#   sh tests/repro/bug55_ma_residuals.sh        (from engines/drtran)
#
# The diagonal system (-0, no transfer) of two univariate models must forecast
# each series exactly as its univariate model does. ES_CPI_airline has an MA
# (MA(1) x SMA(1)_12). fue forecasts 1/2020 at 81.89 and 12/2020 at 83.82
# (drvarma's ladder too). drtran gets it right with the airline FIRST, and
# wrong with it SECOND, behind WTI:
#
#   forecast_levels / transfer_forecast call elf with atf = FALSE. That does
#   not run cres, and what is left in `a` is L^-1 a, the conditional
#   residuals premultiplied by the inverse Cholesky factor of Q (elfvarma.c,
#   block [5.2]), not the residuals. forecast_model uses them in the MA part
#   as if they were a. Row 1 has Q11 = 1 and comes out right; row 2 is scaled
#   by 1/sqrt(Q22) (and mixed by L21). With WTI first, Q22 = s2_ES/s2_WTI ~ 1e-3
#   and the MA part is ~30 times too large.
#
# Exit 1 while the defect is there, 0 once fixed.
cd "$(dirname "$0")/../.." || exit 2
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
C=tests/cases
lvl() {   # the level forecast of ES_CPI for a date, from a .out
    awk -v d="$2" '/VARIABLE NAME  : ES_CPI/{f=1} f && $1==d {print $2; exit}' "$1"
}
./bin/drtran $C/ES_CPI_airline.pre $C/WTI_ar1.pre -0 -f 12 -o $T/first.out  >/dev/null 2>&1
./bin/drtran $C/WTI_ar1.pre $C/ES_CPI_airline.pre -0 -f 12 -o $T/second.out >/dev/null 2>&1
bad=0
for d in 1/2020 2/2020 12/2020; do
    ref=$(case $d in 1/2020) echo 81.89;; 2/2020) echo 81.91;; 12/2020) echo 83.82;; esac)
    a=$(lvl $T/first.out $d); b=$(lvl $T/second.out $d)
    ok=$(python3 -c "print(int(abs($b-$ref)<=0.02 and abs($a-$ref)<=0.02))")
    echo "  $d  fue $ref   drtran airline first $a   airline second $b"
    [ "$ok" = 1 ] || bad=1
done
[ $bad -eq 0 ] && echo "BUG-55: not reproduced (fixed)" || echo "BUG-55: REPRODUCED"
exit $bad
