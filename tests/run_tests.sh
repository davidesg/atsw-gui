#!/bin/bash
# tests/run_tests.sh — regression and invariant suite for drvec.
#
# Four kinds of check, in increasing order of value:
#
#   1. STRUCTURAL   the parameter walk consumes exactly npar; no out-of-bounds
#                   read in any configuration.  Catches §4.1-type bugs.
#   2. INVARIANTS   properties that must hold whatever the numbers are, so they
#                   need no external reference and cannot go stale:
#                     - B2 printed by both printers must agree (§4.2)
#                     - logL must be monotone in r (rank r is nested in r+1)
#                     - the -fixb2 restricted fit cannot beat the free one
#   3. THE GATE     with r=0 and a diagonal structure the exact likelihood
#                   factorises, so the joint logL must equal the sum of the
#                   univariate ones.  This is the cast's oracle: if it breaks,
#                   the fault is in vec_shootx or the seeding, never in elf().
#                   See docs/PLAN_BETA.md §2.
#   4. GOLDEN       current logL values, to detect unintended drift.
#                   *** These are REGRESSION BASELINES, not correct answers. ***
#                   Most stop on termcode 3; see docs/ANALISIS_PRELIMINAR.md.
#                   A baseline is only valid for the exact input it was measured
#                   on: the UK fixture is written with %.10f here, and a value
#                   measured on a %.8f copy differs in the 6th decimal of logL.
#
# Usage:  tests/run_tests.sh [-v]        (or: make test)
#         DRVEC=path/to/mutant tests/run_tests.sh    (to check the suite bites)
# Exit:   0 all passed, 1 otherwise.
#
# WHAT IT ACTUALLY PROTECTS, measured by mutation on 2026-08-17.  Four bugs were
# reintroduced deliberately and the suite re-run:
#
#   mutation                                            failures raised
#   ---------------------------------------------------------------------
#   sign of Lambda in PhiBar_1 (transformation core)          13
#   the output ignores -diagma (the §4.1 bug)                  4
#   B2 read transposed in vec_shootx (the ESTIMATOR)           1
#   B2 fill transposed in the printer                          0   <-- not caught
#
# The last one is a real gap and is explained at test 2a: since the §4.2 fix both
# printers share one copy, so a printer-only transposition is invisible from the
# output.  Do not read a green suite as covering that.

set -u
cd "$(dirname "$0")/.." || exit 1

DRVEC=${DRVEC:-bin/drvec}    # override to test a mutant build
TMP=tests/tmp
VERBOSE=${1:-}
PASS=0; FAIL=0
TOL=1e-6          # absolute tolerance on logelf

[ -x "$DRVEC" ] || { echo "ERROR: $DRVEC not built. Run make first."; exit 1; }
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

ok()   { PASS=$((PASS+1)); [ -n "$VERBOSE" ] && printf '  ok   %s\n' "$1"; return 0; }
bad()  { FAIL=$((FAIL+1)); printf '  FAIL %s\n' "$1"; [ $# -gt 1 ] && printf '        %s\n' "$2"; return 0; }

# logelf_of <inp-basename-without-ext> -> prints the value, empty if none
logelf_of() { grep -a 'logelf' "$1.out" 2>/dev/null | awk '{print $3}'; }

# run <label> <src.inp> <args...>   ; leaves $TMP/case.out, echoes nothing
run() {
    local src=$1; shift
    cp "$src" "$TMP/case.inp"
    STDERR=$("$DRVEC" "$TMP/case" "$@" 2>&1 >/dev/null)
}

near() { awk -v a="$1" -v b="$2" -v t="$TOL" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=t)}'; }

# ---------------------------------------------------------------- test data --
# UK consumption/income/price in logs, levels layout (M=3).  Derived here rather
# than committed: it is a fixture, not a dataset.
awk -F, 'NR>1 && NF>=3 && $1+0>0 {n++; a[n]=log($1); b[n]=log($2); c[n]=log($3)}
  END{ printf "* fixture: UK consumption (logs), levels\n4\n3 %d 1 1955\nlcons linc lprice\n1.0 0 0\n", n;
       for(i=1;i<=n;i++) printf "%.10f %.10f %.10f\n", a[i], b[i], c[i] }' \
  datasets/urca_UKconsumption.csv > "$TMP/uk.inp"

# Danish money demand, levels layout (M=5).  Needed because a transposed read of
# B2 is only detectable when BOTH s>1 and r>1: with r=2 here s=3, so B2 is 3x2.
# Column order is [Y2 block ; Y1 block] = [LPY IBO IDE ; LRM LRY].  It also mixes
# logs with interest rates, which stresses the variance-ratio seeding.
awk -F, 'NR>1 && NF>=6 && $2+0!=0 {n++; p[n]=$4; o[n]=$5; e[n]=$6; m[n]=$2; y[n]=$3}
  END{ printf "* fixture: Danish money demand, levels\n4\n5 %d 2 1974\nLPY IBO IDE LRM LRY\n1.0 0 0\n", n;
       for(i=1;i<=n;i++) printf "%.10f %.10f %.10f %.10f %.10f\n", p[i],o[i],e[i],m[i],y[i] }' \
  datasets/urca_denmark.csv > "$TMP/dk.inp"

MM=datasets/mauricio/mink_muskrat.inp
UK=$TMP/uk.inp
DK=$TMP/dk.inp

echo "drvec test suite"
echo

# ============================================================== 1 STRUCTURAL ==
echo "[1] structural: the parameter walk consumes exactly npar"
struct_case() {
    local label=$1 src=$2; shift 2
    run "$src" "$@"
    if printf '%s' "$STDERR" | grep -qiE 'ERROR (output|init_guess)'; then
        bad "$label" "$(printf '%s' "$STDERR" | grep -iE 'ERROR (output|init_guess)' | head -1)"
    else ok "$label"; fi
}
for o in "-case 1" "-case 2" "-case 3" \
         "-case 2 -diagar" "-case 2 -diagma" "-case 2 -diagcov" \
         "-case 2 -diagar -diagma" "-case 3 -diagar -diagma -diagcov" \
         "-case 2 -fixb2" "-case 2 -fixb2 0" "-case 2 -fixb2 -0.5" "-case 2 -fixb2 -diagma"; do
    struct_case "M=2 p=2 q=1 r=1 $o" "$MM" 2 1 1 $o
done
struct_case "M=2 lrtest case 2"                "$MM" 2 1 0 -case 2 -lrtest
struct_case "M=2 lrtest case 1 all-diag"       "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -lrtest
struct_case "M=2 lrtest p=1 (nreg=0 path)"     "$MM" 1 0 0 -case 2 -lrtest
struct_case "M=3 r=2"                          "$UK" 2 0 2 -case 2
struct_case "M=3 lrtest"                       "$UK" 2 0 0 -case 2 -lrtest
struct_case "M=3 lrtest + fixb2"               "$UK" 2 0 0 -case 2 -lrtest -fixb2
struct_case "M=5 r=2 (s=3, r=2: B2 is 3x2)"    "$DK" 2 0 2 -case 2
struct_case "M=5 lrtest"                       "$DK" 2 0 0 -case 2 -lrtest
struct_case "legacy layout (-differenced)"     data/AL.inp 2 0 1 -case 2 -differenced
echo

# =============================================================== 2 INVARIANTS ==
echo "[2] invariants (no external reference needed)"

# 2a. B2 printed by both printers must agree, for every row.
#     NOTE, honestly: since the §4.2 fix both printers read ONE shared copy, so
#     they agree by construction and this is a STRUCTURAL check, not a behavioural
#     one -- verified by mutation: transposing the printer's fill order is NOT
#     caught here.  What guards the order that matters (the ESTIMATOR's read in
#     vec_shootx) is the M=5 r=2 golden value below, where s=3 and r=2 so a
#     transposed read changes the likelihood.  Making the printer provably right
#     would need it to read the same array vec_shootx built; noted as follow-up.
run "$UK" 2 0 2 -case 2
b2_block=$(sed -n '/^B2 (s x r)/,/^$/p' "$TMP/case.out" | grep -aE '^ +-?[0-9]' | tr -s ' ' | sed 's/^ //')
b_rows=$(sed -n '/^Cointegration matrix B/,/^$/p' "$TMP/case.out" | grep -a '^  row' \
         | awk 'NR>2{$1="";$2="";print}' | tr -s ' ' | sed 's/^ //')
if [ -n "$b2_block" ] && [ "$b2_block" = "$b_rows" ]; then
    ok "B2 block agrees with the B matrix rows (M=3, r=2)"
else
    bad "B2 block vs B matrix rows" "block=[$b2_block] rows=[$b_rows]"
fi

# 2b. logL monotone in r: rank r is nested in r+1, so L(r+1) >= L(r).
#     A violation proves at least one fit did not converge.
mono_check() {
    local label=$1 src=$2; shift 2
    run "$src" "$@"
    local lls; lls=$(sed -n '/^  r    npar/,/^$/p' "$TMP/case.out" \
                     | awk '$1 ~ /^[0-9]+$/ {print $3}')
    local n; n=$(printf '%s\n' "$lls" | grep -c .)
    if [ "$n" -lt 2 ]; then bad "$label" "only $n ranks estimated"; return; fi
    if printf '%s\n' "$lls" | awk 'NR==1{p=$1;next} {if($1<p-1e-9){exit 1} p=$1}'; then
        ok "$label (logL monotone in r: $(printf '%s' "$lls" | tr '\n' ' '))"
    else
        bad "$label" "logL not monotone: $(printf '%s' "$lls" | tr '\n' ' ')"
    fi
}
mono_check "M=2 nesting" "$MM" 2 1 0 -case 2 -lrtest
mono_check "M=3 nesting" "$UK" 2 0 0 -case 2 -lrtest

# 2c. The restricted fit (-fixb2 at an a-priori value) cannot beat the free one.
run "$MM" 2 1 1 -case 2;          free=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -fixb2 0; restr=$(logelf_of "$TMP/case")
if [ -n "$free" ] && [ -n "$restr" ] && \
   awk -v f="$free" -v r="$restr" 'BEGIN{exit !(r <= f + 1e-9)}'; then
    ok "restricted (-fixb2 0) <= free  ($restr <= $free)"
else
    bad "restricted vs free" "free=$free restricted=$restr"
fi
echo

# ================================================================== 3 THE GATE ==
echo "[3] the diagonal gate: r=0 + diagonal => logL factorises"
# References: exact-ML ARMA(1,1) fits, no mean, on the two differenced log
# series, measured 2026-08-17 with drvarma's Python port (estimate_w_py, the
# faithful mirror of the C engine).  With r=0 drvec's effective AR order on
# nabla Y is p-1, so p=2 here corresponds to ARMA(1,1) univariately.
GATE_U1=-20.0580        # nabla log muskrat
GATE_U2=-14.5698        # nabla log mink
GATE_SUM=$(awk -v a=$GATE_U1 -v b=$GATE_U2 'BEGIN{printf "%.4f", a+b}')
GATE_TOL=5e-4           # the references are quoted to 4 decimals

run "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -lrtest
joint=$(sed -n '/^  r    npar/,/^$/p' "$TMP/case.out" | awk '$1=="0"{print $3}')
if [ -z "$joint" ]; then
    bad "diagonal gate" "no r=0 row in the lrtest table"
elif awk -v j="$joint" -v s="$GATE_SUM" -v t="$GATE_TOL" \
        'BEGIN{d=j-s; if(d<0)d=-d; exit !(d<=t)}'; then
    ok "joint r=0 all-diagonal = sum of univariates ($joint vs $GATE_SUM)"
else
    bad "diagonal gate" "joint=$joint  sum=$GATE_SUM  (tol $GATE_TOL) -- the fault is in vec_shootx or the seeding, not in elf()"
fi
echo

# ================================================================== 4 GOLDEN ==
echo "[4] golden logL values (regression baselines, NOT correct answers)"
golden() {
    local want=$1 src=$2; shift 2
    run "$src" "$@"
    local got; got=$(logelf_of "$TMP/case")
    if [ -z "$got" ]; then bad "$* -> expected $want" "no logelf in output"
    elif near "$got" "$want"; then ok "$* = $got"
    else bad "$*" "expected $want, got $got"; fi
}
golden  -8.0795410874 "$MM" 2 1 1 -case 1
golden   6.4679281924 "$MM" 2 1 1 -case 2
golden   5.6105401561 "$MM" 2 1 1 -case 3
golden   0.8239382998 "$MM" 2 1 1 -case 2 -diagar
golden   0.8927053355 "$MM" 2 1 1 -case 2 -diagma
golden   0.5817489849 "$MM" 2 1 1 -case 2 -diagcov
golden   5.2136846166 "$MM" 2 1 1 -case 2 -fixb2
golden  -7.4652170472 "$MM" 2 1 1 -case 2 -fixb2 0
golden 570.2297062757 "$UK" 2 0 2 -case 2   # measured on the fixture below, not on an ad-hoc .inp
golden 749.5833234383 "$DK" 2 0 2 -case 2   # s=3, r=2: guards the B2 read order
golden -318.8131395273 data/AL.inp 2 0 1 -case 2 -differenced
echo

# ===================================================================== summary =
echo "-----------------------------------------------"
printf 'passed %d, failed %d\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
