#!/bin/bash
# tests/run_tests.sh — regression and invariant suite for drvec.
#
# Seven kinds of check, in increasing order of value:
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
#   5. THE BRIDGE   the .inp drvec writes for fue must be readable BY fue, and
#                   the .pre it reads back must land in the right place.  The
#                   format has no validation, so what is checked is what bit
#                   during F2: pure ASCII, and the annual-difference section
#                   present.  Plus the identity that a Theta = 0 seed must
#                   reproduce the cold start exactly, which is what says the
#                   seeding plumbing is right regardless of whether seeding
#                   helps -- it does not; see docs/PLAN_BETA.md F2.7.  And a
#                   round-trip of the reader against the file itself, through
#                   tests/pre_probe.c, which is the only check that reads the
#                   series and the refactor at all.
#   6. INTERPRETATION  alpha = A*psi and its LR; Pi; Sigma = P D P'; and the
#                   normalisation alarm, checked in BOTH directions -- an alarm
#                   with no case to fire on is not an alarm.
#   7. KNOWN TRUTH  the rank test on data generated to have a known rank.  Every
#                   other check of -lrtest compares against another program's
#                   answer; these compare against the truth.
#   7b. BOOTSTRAP   -bootstrap must give ordered, usable critical values and must
#                   fill the case-3 gap where the asymptotic tables have none.
#   8. MEMORY       valgrind over the main paths.  OPT-IN (VALGRIND=1) so the
#                   suite is deterministic anywhere.  It has already caught one
#                   real leak in the multi-start block.
#
# Usage:  tests/run_tests.sh [-v]        (or: make test)
#         DRVEC=path/to/mutant tests/run_tests.sh    (to check the suite bites)
# Exit:   0 all passed, 1 otherwise.
#
# WHAT IT ACTUALLY PROTECTS, measured by mutation on 2026-08-17 against mutants
# built from the CURRENT source (mutating an older source is not a valid
# measurement: it fails the baselines for the wrong reason):
#
#   mutation                                            failures raised
#   ---------------------------------------------------------------------
#   sign of Lambda in PhiBar_1 (transformation core)          14
#   the output ignores -diagma (the §4.1 bug)                  4
#   the .inp writer drops the annual-difference section        2
#   the .pre reader's annual bug (F2.1 / drtran BUG-11)        2
#   B2 read transposed in vec_shootx (the ESTIMATOR)           1
#   the LDL' cross term negated, on M=3                        1   (0 on M=2)
#   the normalisation alarm disabled                           1
#   the Sigma positive-definiteness check removed              0   <-- not caught
#   B2 fill transposed in the printer                          0   <-- not caught
#
# The reader's annual bug raises nothing through the ESTIMATION path -- the
# seeding only reads the MA block, which sits earlier in the file -- so it is
# caught by the round-trip check at 5c-bis, which is the only thing that looks
# at the series and the refactor.  Without that check it scored zero.
#
# The two zeros are real gaps, stated so a green suite is not over-read:
#   - the PD check is INSURANCE: no case here drives Sigma non-PD, so nothing
#     exercises it.  It guards a region the optimiser does not currently reach.
#   - a printer-only transposition is invisible because, since the §4.2 fix, both
#     printers read one shared copy (see test 2a).


set -u
cd "$(dirname "$0")/.." || exit 1

DRVEC=${DRVEC:-bin/drvec}    # override to test a mutant build
TMP=tests/tmp
VERBOSE=${1:-}
PASS=0; FAIL=0
TOL=1e-6          # absolute tolerance on logelf
RUN_TIMEOUT=${RUN_TIMEOUT:-30}   # seconds per estimation; a hang must FAIL, not hang

[ -x "$DRVEC" ] || { echo "ERROR: $DRVEC not built. Run make first."; exit 1; }
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

ok()   { PASS=$((PASS+1)); [ -n "$VERBOSE" ] && printf '  ok   %s\n' "$1"; return 0; }
bad()  { FAIL=$((FAIL+1)); printf '  FAIL %s\n' "$1"; [ $# -gt 1 ] && printf '        %s\n' "$2"; return 0; }

# logelf_of <inp-basename-without-ext> -> prints the value, empty if none
logelf_of() { grep -a 'logelf' "$1.out" 2>/dev/null | awk '{print $3}'; }

# run <label> <src.inp> <args...>   ; leaves $TMP/case.out, echoes nothing
# Sets STDERR and TIMEDOUT.  A configuration that does not finish is a failure:
# an ill-conditioned surface can send the optimiser into a region where each
# likelihood evaluation is very slow, and that has happened for real (see the
# note on -differenced -case 3 in docs/PLAN_BETA.md F1).
run() {
    local src=$1; shift
    cp "$src" "$TMP/case.inp"
    rm -f "$TMP/case.out"
    TIMEDOUT=""
    STDERR=$(timeout "$RUN_TIMEOUT" "$DRVEC" "$TMP/case" "$@" 2>&1 >/dev/null)
    [ $? -eq 124 ] && TIMEDOUT="yes"
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

# The same mink-muskrat data in the LEGACY pre-differenced layout, so that
# -differenced is exercised on more than one model.  Col 1 = nabla log muskrat,
# col 2 = log mink in levels.
awk -F, 'NR>1 && NF>=3 {n++; a[n]=log($2); b[n]=log($3)}
  END{ printf "* fixture: mink-muskrat, LEGACY pre-differenced layout\n1\n2 %d 1 1851\ndmuskrat mink\n1.0 0 0\n", n-1;
       for(i=2;i<=n;i++) printf "%.10f %.10f\n", b[i]-b[i-1], a[i] }' \
  datasets/mauricio/mink_muskrat.csv > "$TMP/mmold.inp"

MM=datasets/mauricio/mink_muskrat.inp
UK=$TMP/uk.inp
DK=$TMP/dk.inp
MMOLD=$TMP/mmold.inp

echo "drvec test suite"
echo

# ============================================================== 1 STRUCTURAL ==
echo "[1] structural: the parameter walk consumes exactly npar"
struct_case() {
    local label=$1 src=$2; shift 2
    run "$src" "$@"
    if [ -n "$TIMEDOUT" ]; then
        bad "$label" "did not finish in ${RUN_TIMEOUT}s"
    elif printf '%s' "$STDERR" | grep -qiE 'ERROR (output|init_guess)'; then
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
for o in "-case 1" "-case 2" "-case 3"; do
    struct_case "legacy layout q=1 $o" "$MMOLD" 2 1 1 $o -differenced
done
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
# El bloque lleva ahora "(sd ...)" detras de cada valor; se quita para comparar.
b2_block=$(sed -n '/^B2 (s x r)/,/^$/p' "$TMP/case.out" | grep -aE '^ +-?[0-9]' \
           | sed 's/(sd[^)]*)//g' | tr -s ' ' | sed 's/^ //; s/ $//')
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
# 2d. |Sigma| must agree between the two LAYOUTS of the same model.  The levels
#     and the pre-differenced layouts differ only by an offset in Y2 that a free
#     E[W] absorbs, so they are the same model reparameterised: |Sigma| is
#     invariant (Cbar has |det| = 1).  They stop at slightly different points on
#     a flat-ish surface, hence a 5% tolerance rather than equality.  This is the
#     invariant F1 was really about: the dispersion across equivalent set-ups.
sigdet2() {   # prints |Sigma| for an M=2 run, from the Sigma = sigma2*Q block
    sed -n '/^Sigma = sigma2 \* Q/,/^B2/p' "$1" | grep -aE '^ +-?[0-9]' \
      | awk 'NR==1{a=$1} NR==2{b=$1; c=$2} END{if(a=="")print ""; else printf "%.9f", a*c-b*b}'
}
run "$MM"    2 1 1 -case 2;               d_lev=$(sigdet2 "$TMP/case.out")
run "$MMOLD" 2 1 1 -case 2 -differenced;  d_old=$(sigdet2 "$TMP/case.out")
if [ -z "$d_lev" ] || [ -z "$d_old" ]; then
    bad "|Sigma| across layouts" "could not read Sigma (lev=[$d_lev] old=[$d_old])"
elif awk -v a="$d_lev" -v b="$d_old" 'BEGIN{r=(a>b)?a/b:b/a; exit !(r<=1.05)}'; then
    ok "|Sigma| agrees across layouts within 5% ($d_lev vs $d_old)"
else
    bad "|Sigma| across layouts" "$d_lev vs $d_old differ by more than 5%"
fi
echo

# ================================================================== 3 THE GATE ==
echo "[3] the diagonal gate: r=0 + diagonal => logL factorises"
# References: exact-ML ARMA(1,1) fits, NO MEAN, on the two differenced log
# series, measured 2026-08-17 with drvarma's Python port (estimate_w_py, the
# faithful mirror of the C engine).  With r=0 drvec's effective AR order on
# nabla Y is p-1, so p=2 here corresponds to ARMA(1,1) univariately.
#
# SECOND, INDEPENDENT SOURCE (2026-08-18): fue 1.13 (python) reproduces both,
# fitting the same two series with mu FIXED AT ZERO -- -20.057954 and -14.569865,
# i.e. within 6e-5 of the constants below.  Two programs from different lineages
# now agree on them, which is what F0's contingency asked for.
#   The "no mean" is not a detail: with mu free fue reaches -19.908084 on the
# muskrat series, 0.15 BETTER, and reading that as a discrepancy is a mistake
# that was actually made during F2.  These constants are the no-mean fits
# because the drvec run they are compared against is -case 1, which HAS no mean.
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
golden   3.6856397544 "$MM" 2 1 1 -case 1
golden   6.4786201604 "$MM" 2 1 1 -case 2
golden   6.5140062493 "$MM" 2 1 1 -case 3
golden  -2.5419963582 "$MM" 2 1 1 -case 2 -diagar
golden   0.8816637342 "$MM" 2 1 1 -case 2 -diagma
golden   0.5696296891 "$MM" 2 1 1 -case 2 -diagcov
golden   5.4717136367 "$MM" 2 1 1 -case 2 -fixb2
golden  -8.4835302747 "$MM" 2 1 1 -case 2 -fixb2 0
golden 570.2297062756 "$UK" 2 0 2 -case 2
golden 828.8447477597 "$DK" 2 0 2 -case 2   # s=3, r=2: guards the B2 read order
golden -318.8131393592 data/AL.inp 2 0 1 -case 2 -differenced
echo

# ============================================== 5 THE SUITE BRIDGE (F2) ==
echo "[5] the suite bridge: the .inp drvec writes, and the .pre it reads"
# What these guard are the two traps that actually bit during F2, both of them
# silent: a file that is not pure ASCII (fue's Python parser cannot read Latin-1,
# BUG-0010, and this engine's sources ARE Latin-1), and a missing annual-
# difference section (both fue writers always emit it, and leaving it out shifts
# every section after it with no error).  See docs/PLAN_BETA.md F2.

wrote_ok() {   # <label> <mode-flag> <prefix> <src> <args...>
    local label=$1 flag=$2 pre=$3 src=$4; shift 4
    rm -f "$pre".*.inp
    run "$src" "$@" "$flag" "$pre"
    local n bad=""
    n=$(ls "$pre".*.inp 2>/dev/null | wc -l)
    [ "$n" -eq 2 ] || bad="wrote $n files, expected 2"
    for f in "$pre".*.inp; do
        [ -f "$f" ] || continue
        if [ "$(LC_ALL=C grep -c '[^ -~]' "$f")" -ne 0 ]; then
            bad="$f is not pure ASCII"; break
        fi
        grep -q 'Individual factors of the annual difference' "$f" \
            || { bad="$f lacks the annual-difference section"; break; }
        local nsec
        nsec=$(grep -c '^\*\*' "$f")
        [ "$nsec" -ge 12 ] || { bad="$f has only $nsec ** sections"; break; }
    done
    if [ -n "$bad" ]; then bad "$label" "$bad"; else ok "$label"; fi
}
wrote_ok "-writeres writes 2 usable .inp" -writeres "$TMP/wr" "$MM" 2 1 1 -case 2
wrote_ok "-writeinp writes 2 usable .inp" -writeinp "$TMP/wy" "$MM" 2 1 1 -case 2

# 5c. THE IDENTITY: a seed with Theta = 0 must reproduce the cold start EXACTLY.
#     This is what says the seeding plumbing is right -- reader, factor
#     expansion, coordinate route and parameter layout -- independently of
#     whether seeding helps.  The zeroed file is named .inp on purpose: it is a
#     specification, not an optimum, so calling it .pre would be a false claim.
run "$MM" 2 1 1 -case 2
cold=$(logelf_of "$TMP/case")
for i in 1 2; do
    awk -v OFS='' '
        /^\*\* Number and orders of regular MA/{print; mark=NR+3; next}
        mark && NR==mark{print "0.000000  1"; next}
        {print}' "tests/fixtures/mmres.$i.pre" > "$TMP/zero.$i.inp"
done
run "$MM" 2 1 1 -case 2 -seed "$TMP/zero"
zseed=$(logelf_of "$TMP/case")
if [ -n "$cold" ] && [ "$cold" = "$zseed" ]; then
    ok "a Theta = 0 seed reproduces the cold start exactly ($cold)"
else
    bad "zero-seed identity" "cold=$cold  seeded=$zseed  (must be identical)"
fi

# 5c-bis. THE READER, past the MA block.  The seeding only ever reads the ARMA
#     factors and mu, which sit BEFORE the annual-difference section, so no
#     estimation run touches the series or the refactor -- and the reader bug
#     that F2 found (BUG-11 of drtran, fixed in our copy) corrupts exactly
#     those.  Measured by mutation: restoring it raises zero failures anywhere
#     else.  This closes that gap.
#     Nothing here is a golden number: the expected values are read out of the
#     .pre with awk, so the check cannot go stale.
PROBE=${PROBE:-bin/pre_probe}
for i in 1 2; do
    f="tests/fixtures/mmres.$i.pre"
    if [ ! -x "$PROBE" ]; then
        bad "reader round-trip $i" "$PROBE not built -- run make first"
        continue
    fi
    got=$("$PROBE" "$f" 2>&1)
    want=$(awk '
        /^\*\* ACF\/PACF bands/ {getline; refac=$2; next}
        /^\*\* Time series/     {ind=1; next}
        ind && NF                {n++; d[n]=$1}
        /^\*\* Frequency of time series/ {getline; freq=$1; next}
        END{ printf "%d %d %.10g %.10f %.10f %.10f %.10f",
                    n, freq, refac, d[1], d[2], d[n-1], d[n] }' "$f")
    if [ "$got" = "$want" ]; then
        ok "the .pre reader round-trips fixture $i (n, freq, refactor, series)"
    else
        bad "reader round-trip $i" "reader: $got
        file:   $want"
    fi
done

# 5c-ter. THE LADDER'S TWO CONTRACTS, at the diagonal rung.
#     From drtran-python/docs/LADDER_AS_OPTIMISATION.md sections 2.1 and 3:
#
#       SUM_i logL(series i)  =  logL(joint DIAGONAL fit)  <=  logL(joint model)
#       logL(diagonal fit)   >=  logL(AT the stored values), equality iff the
#                                stored values are the univariate optima
#
#     The first proves the CROSSING -- transformation, scaling, seeds and orders
#     all arrived intact, or the identity fails.  The second is a free
#     optimality CERTIFICATE: one likelihood evaluation, no optimisation, and
#     its sign says whether the .pre it was handed are optima.
#     Both are checked here on r=0 with diagonal structure, which is the rung
#     where the factorisation holds.  The tolerance is 1e-4 because a .pre
#     stores its coefficients with %.6f, and that rounding is what bounds how
#     sharp the certificate can be.
LAD="2 1 0 -case 1 -diagar -diagma -diagcov"
run "$MM" $LAD -seedybar tests/fixtures/mmdiag -eval
ev=$(grep -a 'eval logelf'    "$TMP/case.out" | awk '{print $4}')
su=$(grep -a 'sum univariate' "$TMP/case.out" | awk '{print $4}')
run "$MM" $LAD -seedybar tests/fixtures/mmdiag
fit=$(logelf_of "$TMP/case")
if [ -z "$ev" ] || [ -z "$su" ] || [ -z "$fit" ]; then
    bad "ladder contracts" "missing values (eval=$ev sum=$su fit=$fit)"
else
    awk -v e="$ev" -v s="$su" 'BEGIN{d=e-s; if(d<0)d=-d; exit !(d<=1e-4)}' \
      && ok "crossing identity: joint at the .pre = sum of univariates ($ev vs $su)" \
      || bad "crossing identity" "joint=$ev  sum of univariates=$su"
    awk -v f="$fit" -v e="$ev" 'BEGIN{ exit !(f-e >= -1e-9 && f-e <= 1e-4) }' \
      && ok "optimality certificate: gap = fit - eval is >= 0 and ~0 ($fit vs $ev)" \
      || bad "optimality certificate" "fit=$fit  eval=$ev  gap must be >=0 and ~0"
fi

# 5d. Regression baseline for the seeded fit, on the committed .pre fixtures.
#     Those were written by fue 1.13 (python) from drvec's own -writeres output
#     for mink-muskrat p=2 q=1 r=1 -case 2, so the optimality claim is fue's.
#     NOTE this value is WORSE than the cold start: that is the measured F2
#     result, not a defect.  See docs/PLAN_BETA.md F2.7.
run "$MM" 2 1 1 -case 2 -seed tests/fixtures/mmres
got=$(logelf_of "$TMP/case")
if [ -z "$got" ]; then bad "seeded fit from fixtures" "no logelf"
elif near "$got" 6.4460747665; then ok "seeded from .pre fixtures = $got"
else bad "seeded fit from fixtures" "expected 6.4460747665, got $got"; fi
echo

# ==================================== 6 RESTRICTIONS ON ALPHA (H1(r)) ==
echo "[6] alpha = A*psi: the restriction, its LR, and its guards"
# Johansen and Swensen (2024): H1(r) is alpha = A*psi with A known.  Weak
# exogeneity is the special case where A selects rows, so -weakex is a shorthand
# for -alpha and must give exactly the same numbers -- that is an invariant, so
# it needs no golden value and cannot go stale.

# A declaring equation 2 not to adjust: the same thing -weakex 2 builds.
printf '* A: alpha_2 = 0\n2 1\n1\n0\n' > "$TMP/A_eq2.txt"
lr_of() { grep -a 'LR = 2(libre' "$1.out" 2>/dev/null | awk '{print $5}'; }

run "$MM" 2 1 1 -case 2 -weakex 2
lr_short=$(lr_of "$TMP/case"); free_ll=$(grep -a 'logL H(r)' "$TMP/case.out" | awk '{print $5}')
restr_ll=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -alpha "$TMP/A_eq2.txt"
lr_gen=$(lr_of "$TMP/case")

if [ -z "$lr_short" ] || [ -z "$lr_gen" ]; then
    bad "-weakex == -alpha" "no LR (short=$lr_short general=$lr_gen)"
elif [ "$lr_short" = "$lr_gen" ]; then
    ok "-weakex 2 and the equivalent -alpha file agree exactly (LR = $lr_gen)"
else
    bad "-weakex == -alpha" "short=$lr_short  general=$lr_gen"
fi

# The restricted fit cannot beat the free one: H1(r) is nested in H(r), so the
# LR is non-negative.  A negative value does not mean the theory is wrong, it
# means one of the two fits did not converge -- which is worth knowing.
if [ -n "$free_ll" ] && [ -n "$restr_ll" ] && \
   awk -v f="$free_ll" -v r="$restr_ll" 'BEGIN{exit !(f - r >= -1e-6)}'; then
    ok "H1(r) does not beat H(r) (free $free_ll >= restricted $restr_ll)"
else
    bad "nesting of H1(r) in H(r)" "free=$free_ll restricted=$restr_ll"
fi

# A rank-deficient A must be refused BEFORE estimating: with A'A singular psi is
# not identified, and estimating anyway would return numbers for a model that
# does not have them.
printf '2 2\n1 2\n2 4\n' > "$TMP/A_bad.txt"
run "$MM" 2 1 1 -case 2 -alpha "$TMP/A_bad.txt"
if printf '%s' "$STDERR" | grep -q 'A no tiene rango'; then
    ok "a rank-deficient A is refused before estimating"
else
    bad "rank guard on A" "no complaint about a singular A'A"
fi

# 6a-bis. -lrtest UNDER a restriction on alpha must NOT print the tabulated
#     critical values.  Under alpha = A*psi the statistic has a different
#     distribution -- the tables are for alpha free -- so printing them would be
#     wrong numbers wearing the right clothes, which is worse than none.
run "$MM" 2 1 0 -case 2 -lrtest -weakex 1
if grep -aq "critical values do not apply" "$TMP/case.out" && \
   ! grep -aqE '^  [0-9]+ +[0-9]+ +-?[0-9.]+ +[0-9]+\.[0-9]{2} ' "$TMP/case.out"; then
    ok "-lrtest with a restriction suppresses the critical values and says why"
else
    bad "-lrtest under a restriction" "still prints tabulated critical values"
fi
# ...and without the restriction it still prints them.
run "$MM" 2 1 0 -case 2 -lrtest
if grep -aqE '^  [0-9]+ +[0-9]+ +-?[0-9.]+ +[0-9]+\.[0-9]{2} ' "$TMP/case.out"; then
    ok "-lrtest unrestricted still reports the critical values"
else
    bad "-lrtest unrestricted" "critical values disappeared"
fi

# And the parameter walk must still consume exactly npar with the restriction on.
struct_case "M=2 with -weakex 1"  "$MM" 2 1 1 -case 2 -weakex 1
struct_case "M=3 with -weakex 2"  "$UK" 2 0 2 -case 2 -weakex 2

# 6b. Pi = Lambda B' must be reported, and it is what to compare fits on: unlike
#     Lambda and B it does not move under a reparameterisation of the
#     cointegrating space.  Its rank is r BY CONSTRUCTION, so the M-r zero
#     eigenvalues prove nothing about the rank -- which is why the output says so.
run "$MM" 2 1 1 -case 2
if grep -aq "^Pi = Lambda B'" "$TMP/case.out" && \
   grep -aq "eigenvalues of Pi:" "$TMP/case.out"; then
    ok "Pi and its eigenvalues are reported"
else
    bad "Pi block" "missing from the output"
fi

# 6b-bis. Sigma = P D P' must RECONSTRUCT Sigma.  An identity, so no golden
#     value: the check reads P, D and Sigma out of the same .out and multiplies
#     them back.
#     It runs on M = 3 and not on M = 2 ON PURPOSE.  With M = 2 the LDL' inner
#     loop never executes -- there is no third variable for the cross term to
#     accumulate over -- so a mutation of that term is invisible.  Measured:
#     flipping its sign raises 0 failures on M = 2 and 1 on M = 3.
ldl_reconstruction_error() {   # <out file> -> worst absolute error
    awk '
        /^Sigma = sigma2 \* Q/  {mode="S"; n=0; next}
        /^Sigma = P D P/        {mode="";  next}
        /^P =/                  {mode="P"; n=0; next}
        /^D \(diagonal\)/       {mode="D"; next}
        mode=="S" && /^ +[-0-9]/ {n++; for(j=1;j<=NF;j++) S[n","j]=$j; M=n; next}
        mode=="S"               {mode=""}
        mode=="P" && /^ +[-0-9]/ {n++; for(j=1;j<=NF;j++) P[n","j]=$j; next}
        mode=="P"               {mode=""}
        mode=="D" && /^ +[-0-9]/ {for(j=1;j<=NF;j++) D[j]=$j; mode=""; got=1; next}
        END{
            if (!got || M<2) {print "missing"; exit}
            worst=0
            for (a=1;a<=M;a++) for (b=1;b<=a;b++) {
                acc=0
                for (k=1;k<=b;k++) acc += P[a","k]*D[k]*P[b","k]
                e = S[a","b]-acc; if (e<0) e=-e
                if (e>worst) worst=e
            }
            printf "%.9f", worst
        }' "$1"
}
run "$UK" 2 0 2 -case 2
recon=$(ldl_reconstruction_error "$TMP/case.out")
if [ "$recon" = "missing" ] || [ -z "$recon" ]; then
    bad "Sigma = P D P'" "could not read P, D or Sigma from the output"
elif awk -v w="$recon" 'BEGIN{exit !(w <= 1e-5)}'; then
    ok "Sigma = P D P' reconstructs Sigma on M=3 (worst entry off by $recon)"
else
    bad "Sigma = P D P'" "reconstruction is off by $recon"
fi

# 6c. THE NORMALISATION ALARM, on a case built to fire it.  B = [I_r; B2]
#     normalises on the Y1 block, and if that block does not appear in the
#     cointegrating relation the fit describes something else with an inflated
#     B2 -- silently.  datasets/synthetic/badnorm.inp is exactly that case.
#     An alarm with no case to fire on is not an alarm, so both directions are
#     checked: it must fire there and stay quiet on real data.
run datasets/synthetic/badnorm.inp 2 0 1 -case 2
if printf '%s' "$STDERR" | grep -q 'barely involves the Y1 block'; then
    ok "the normalisation alarm fires on the case built for it"
else
    bad "normalisation alarm" "did not fire on datasets/synthetic/badnorm.inp"
fi
run "$MM" 2 1 1 -case 2
if printf '%s' "$STDERR" | grep -q 'barely involves the Y1 block'; then
    bad "normalisation alarm" "fired on mink-muskrat, where Y1 carries 75% of the weight"
else
    ok "the normalisation alarm stays quiet on a well-normalised fit"
fi
echo

# 6c-bis. MULTI-START can never do worse than a single start.  An invariant, not
#     a golden value: start 1 is the unperturbed seed, so the best of n starts is
#     by construction at least as good as the one start.  If this ever fails, the
#     book-keeping that carries the best point back is wrong.
#     It also has to be MONOTONE in n -- the first n starts of a long run are the
#     starts of a short one -- which is why the jitter ladder depends only on the
#     start index.  Getting that wrong was measured: with the amplitude scaled by
#     n, asking for 40 starts gave a WORSE answer than asking for 24.
run "$MM" 2 1 1 -case 2
one=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -multistart 5
five=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -multistart 10
ten=$(logelf_of "$TMP/case")
if [ -z "$one" ] || [ -z "$five" ] || [ -z "$ten" ]; then
    bad "multi-start" "missing logelf (1=$one 5=$five 10=$ten)"
elif awk -v a="$one" -v b="$five" -v c="$ten" \
        'BEGIN{exit !(b >= a - 1e-9 && c >= b - 1e-9)}'; then
    ok "multi-start is monotone: 1 -> $one, 5 -> $five, 10 -> $ten"
else
    bad "multi-start" "not monotone: 1 -> $one, 5 -> $five, 10 -> $ten"
fi

# 6c-ter. MULTI-START must report REAL standard errors.  cov comes from the
#     factor raxopt accumulates WHILE iterating, so re-running est from the
#     already-optimal point leaves it at its initialisation and every standard
#     error comes out identical.  That is what this checks, and it is not
#     hypothetical: it was the behaviour when multi-start was first written --
#     0.134231 for three parameters whose real values are 0.062, 0.125 and 0.106.
run "$MM" 2 1 1 -case 2 -multistart 5
nsd=$(grep -aoE '\(sd +[0-9.]+\)' "$TMP/case.out" | sort -u | wc -l)
if [ "$nsd" -ge 2 ]; then
    ok "multi-start reports standard errors that differ across parameters ($nsd distinct)"
else
    bad "multi-start standard errors" "all identical -- cov is the initialisation, not the Hessian"
fi

# 6e. RESIDUAL DIAGNOSTICS must be present, must be REAL, and must not count
#     the contemporaneous correlation as a defect.
#     "Real" is the point: with -multistart there is no final est() call, and
#     the residuals were arriving from that call by accident -- so the diagnosis
#     printed Q = nan next to "residuals appear white noise", which is the worst
#     possible way to be wrong.  They are now computed explicitly.
run "$MM" 2 1 1 -case 2 -multistart 5
q=$(grep -a 'Q(' "$TMP/case.out" | head -1)
if [ -z "$q" ]; then
    bad "residual diagnostics" "the Hosking block is missing"
elif printf '%s' "$q" | grep -qi 'nan'; then
    bad "residual diagnostics" "Q is nan: the residuals were never computed ($q)"
else
    ok "residual diagnostics report a real Hosking statistic ($q)"
fi
# The k=0 off-diagonal is Sigma's, not a missing cross effect: it must be
# reported apart and NOT drive the verdict.
if grep -aq 'Contemporaneous innovation correlation' "$TMP/case.out" && \
   grep -aq 'Cross DYNAMICS left in the residuals (k >= 1)' "$TMP/case.out"; then
    ok "the contemporaneous correlation is reported apart from the cross dynamics"
else
    bad "residual diagnostics" "lag 0 is not separated from the cross dynamics"
fi

# 6e-bis. THE PORTMANTEAU P-VALUE MUST BE ON THE RIGHT TAIL.  No golden value
#     needed: for a chi-square, a statistic BELOW its df (below the mean) must
#     have an upper-tail p above 0.5.  That is arithmetic, and it is what the
#     suite's chisq() got wrong for df >= 30 -- it inverted the tail exactly in
#     the case where the residuals are FINE, so the diagnosis printed
#     "REJECT H0: residuals are not white noise" for a p of 0.98.
#     The CASE matters, and finding one took measuring.  The suite's chisq() is
#     correct for df < 30 (it uses gammap there) and inverts the tail only for
#     df >= 30 AND a statistic BELOW its mean.  So the defect cannot show on
#     mink-muskrat (28 df) nor on UK (72 df but Q = 148, above the mean): it
#     needs a model that FITS.  datasets/synthetic/rank0.inp with r=1 gives
#     Q(126) = 110, below the mean, which is exactly the region.
#     Measured: restoring the original expression raises 0 failures on the other
#     two cases and 1 here.
run datasets/synthetic/rank0.inp 2 0 1 -case 2
line=$(grep -a 'Q(' "$TMP/case.out" | head -1)
qv=$(printf '%s' "$line" | sed -n 's/.*= *\([0-9.]*\), p-value.*/\1/p')
dfv=$(printf '%s' "$line" | sed -n 's/.*Q(\([0-9]*\)).*/\1/p')
pv=$(printf '%s' "$line" | sed -n 's/.*p-value = *\([0-9.-]*\).*/\1/p')
if [ -z "$qv" ] || [ -z "$pv" ]; then
    bad "portmanteau p-value" "could not parse: $line"
elif awk -v q="$qv" -v d="$dfv" -v p="$pv" \
        'BEGIN{ exit !( (q < d && p > 0.5) || (q > d && p < 0.5) || (q == d) ) }'; then
    ok "the portmanteau p-value is on the upper tail (Q=$qv, df=$dfv, p=$pv)"
else
    bad "portmanteau p-value" "Q=$qv df=$dfv p=$pv -- the tail is inverted"
fi

# 6d. THE CONVERGENCE NOTE must be present and must AGREE with the banner.
#     Not a golden value: it is a consistency check between two things the same
#     run says.  "OPTIMIZER STOPPED" and a note claiming a clean convergence
#     would be worse than saying nothing.
run "$MM" 2 1 1 -case 2
if ! grep -aq "^Convergence note:" "$TMP/case.out"; then
    bad "convergence note" "not present in the output"
else
    banner_stopped=$(grep -ac "OPTIMIZER STOPPED" "$TMP/case.out")
    note_notconv=$(grep -ac "^Convergence note: NOT a convergence" "$TMP/case.out")
    if [ "$banner_stopped" -gt 0 ] && [ "$note_notconv" -eq 0 ]; then
        bad "convergence note" "banner says STOPPED but the note does not"
    elif [ "$banner_stopped" -eq 0 ] && [ "$note_notconv" -gt 0 ]; then
        bad "convergence note" "note says NOT a convergence but the banner does not"
    else
        ok "the convergence note agrees with the optimiser's banner"
    fi
fi
# And on a run that really does stop on termcode 3, the note must say so.
run "$MM" 2 1 1 -case 3
if grep -aq "OPTIMIZER STOPPED" "$TMP/case.out" && \
   grep -aq "^Convergence note: NOT a convergence" "$TMP/case.out"; then
    ok "a termcode-3 run is reported as NOT a convergence"
elif grep -aq "OPTIMIZER STOPPED" "$TMP/case.out"; then
    bad "convergence note" "termcode 3 not reported as a non-convergence"
else
    ok "case 3 converged this time; nothing to check"
fi
echo

# ================================= 7 THE RANK TEST AGAINST KNOWN TRUTH ==
echo "[7] the rank test on data whose rank is known by construction"
# Every other check of -lrtest compares against another program's answer.  These
# two compare against the TRUTH, because the data was generated to have it:
# rank0.inp is three independent random walks and rank2.inp is three series
# sharing one common trend.  They bracket the test from both ends -- one asks
# whether it invents relations, the other whether it finds them.
#
# What they do NOT establish is the test's size in finite samples.  Measured
# separately over 20 replications of a true r = 1 process at n = 120, the
# sequential test at the 5% asymptotic level picked r = 1 in 16, r = 2 in 3 and
# r = 0 in 1.  Over-rejection of ~15% against a nominal 5% is the finite-sample
# distortion the parametric bootstrap of F4 exists to fix.  See
# docs/HOMOLOGATION.md 2.3.

selected_rank() {   # <out file> -> the rank the sequential test selects
    awk '/^  r    M-r/{t=1; next}
         t && /^ +[0-9]/ {
             if (index($0,"not rejected")) {print sel+0; done=1; exit}
             if (index($0,"reject"))       sel=$1+1
             next
         }
         t && /NOT INTERPRETABLE/ {print sel+0; done=1; exit}
         END{if (!done) print sel+0}' "$1"
}

run datasets/synthetic/rank0.inp 2 0 0 -case 2 -lrtest
got=$(selected_rank "$TMP/case.out")
[ "$got" = "0" ] && ok "r = 0 recovered on three independent random walks" \
                 || bad "rank on rank0.inp" "selected r = $got, truth is 0"

run datasets/synthetic/rank2.inp 2 0 0 -case 2 -lrtest
got=$(selected_rank "$TMP/case.out")
[ "$got" = "2" ] && ok "r = 2 recovered on three series with one common trend" \
                 || bad "rank on rank2.inp" "selected r = $got, truth is 2"
echo

# ================================================== 8 MEMORY (opt-in) ==
# Off by default so `make test` is deterministic on any machine; run it with
#     VALGRIND=1 make test
# Not decoration: the multi-start block leaked 1080 bytes when it was written --
# it re-allocated the VARMA structure while the first allocation was still live
# -- and nothing else would have noticed.
#
# The reader had no deallocator anywhere in the suite -- drtran does not free
# what read_fue_pre allocates either, which is its BUG-12 -- so one was written
# (free_fue_pre, in src/fue_bridge.c, as NEW code beside the vendored reader
# rather than a patch to it).  These checks are how a hand-written deallocator
# for someone else's allocator gets verified: valgrind catches over-freeing as
# well as leaking.
if [ "${VALGRIND:-0}" = "1" ]; then
    echo "[8] memory (valgrind)"
    if ! command -v valgrind >/dev/null 2>&1; then
        bad "valgrind" "VALGRIND=1 was requested but valgrind is not installed"
    else
        vg_clean() {   # <label> <src> <args...>
            local label=$1 src=$2; shift 2
            cp "$src" "$TMP/case.inp"
            valgrind --leak-check=full --show-leak-kinds=definite \
                     --errors-for-leak-kinds=definite --error-exitcode=9 \
                     "$DRVEC" "$TMP/case" "$@" >/dev/null 2>"$TMP/vg.txt"
            if [ $? -eq 9 ]; then
                bad "$label" "$(grep -E 'definitely lost|Invalid' "$TMP/vg.txt" | head -2)"
            else ok "$label"; fi
        }
        vg_clean "no leaks: plain fit"        "$MM" 2 1 1 -case 2
        vg_clean "no leaks: multi-start"      "$MM" 2 1 1 -case 2 -multistart 3
        vg_clean "no leaks: alpha = A*psi"    "$MM" 2 1 1 -case 2 -weakex 1
        vg_clean "no leaks: r=0 diagonal"     "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov
        vg_clean "no leaks: lrtest"           "$MM" 2 1 0 -case 2 -lrtest
        vg_clean "no leaks: seeding from .pre" "$MM" 2 1 0 -case 1 \
                 -diagar -diagma -diagcov -seedybar tests/fixtures/mmdiag
    fi
    echo
fi

# 7b. THE BOOTSTRAP must produce usable critical values, ordered, and must fill
#     the case-3 gap where the asymptotic tables have nothing.  Small B on
#     purpose: this checks the machinery, not the calibration -- the calibration
#     is a Monte Carlo study, not a unit test.
run "$MM" 2 1 0 -case 2 -lrtest -bootstrap 40
if ! grep -aq 'Parametric bootstrap under H0' "$TMP/case.out"; then
    bad "bootstrap" "the bootstrap block is missing"
else
    ok10=$(grep -aA3 'p-value  reps' "$TMP/case.out" | sed -n '3p' | awk '{print $4}')
    ok05=$(grep -aA3 'p-value  reps' "$TMP/case.out" | sed -n '3p' | awk '{print $5}')
    ok01=$(grep -aA3 'p-value  reps' "$TMP/case.out" | sed -n '3p' | awk '{print $6}')
    if [ -z "$ok01" ]; then
        bad "bootstrap" "no critical values in the bootstrap row"
    elif awk -v a="$ok10" -v b="$ok05" -v c="$ok01" \
            'BEGIN{exit !(a>0 && a<=b && b<=c)}'; then
        ok "bootstrap critical values are positive and ordered ($ok10 <= $ok05 <= $ok01)"
    else
        bad "bootstrap" "critical values not ordered: $ok10 $ok05 $ok01"
    fi
fi
# case 3 has no tabulated values; the bootstrap must supply them
run "$MM" 2 1 0 -case 3 -lrtest -bootstrap 40
if grep -aq 'NOT TABULATED HERE' "$TMP/case.out" && \
   grep -aA3 'p-value  reps' "$TMP/case.out" | sed -n '3p' | grep -qE '[0-9]+\.[0-9]+'; then
    ok "the bootstrap fills the case-3 gap, where the tables have nothing"
else
    bad "bootstrap on case 3" "no critical values where the asymptotic table has none"
fi
echo

# ===================================================================== summary =
echo "-----------------------------------------------"
printf 'passed %d, failed %d\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
