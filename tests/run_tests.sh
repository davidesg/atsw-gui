#!/bin/bash
# tests/run_tests.sh — regression and invariant suite for drvec.
#
# Eight kinds of check, in increasing order of value:
#
#   0. COMMAND LINE the program refuses what it does not understand instead of
#                   ignoring it, and the exit code says what happened.  Added
#                   with P1; see docs/PLAN_PRODUCCION.md.  The check that
#                   cannot go stale is the last one: the set of options the
#                   PARSER accepts, read out of the source, against the set
#                   usage() ENUMERATES, read out of the binary.
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
#   8. ROOTS        the AR/MA roots at the optimum, the invertibility boundary
#                   they can sit on, and the q>=2 heap-corruption regression.
#   8g. |Sigma|     the beta exit criterion, on the four equivalent
#                   configurations.  OPT-IN (SLOW=1): it needs -multistart 60
#                   four times over.
#   8h. BUG-13      the engine's chisq() against GSL.  Watches code drvec does
#                   NOT run: the function belongs to the file shared with
#                   drvarma and drtran, which do, and this is the only one of
#                   the three with an automatic suite.
#   9. MEMORY       valgrind over the main paths.  OPT-IN (VALGRIND=1) so the
#                   suite is deterministic anywhere.  It has already caught two
#                   real leaks: the multi-start block, and the seeding buffers,
#                   which had no deallocator and which only became visible once
#                   vector() was aligned with the suite -- the previous form
#                   returned the base of the block, and a pointer to the base
#                   looks reachable to valgrind.
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
#
# AND THE COMMAND-LINE BLOCK [0], measured the same way on 2026-08-20 against
# mutants built from the current source (docs/PLAN_PRODUCCION.md P1):
#
#   mutation                                            failures raised
#   ---------------------------------------------------------------------
#   validate_cli() made a no-op (options ignored again)       16
#   p, q, r back to atoi, and the df bound removed             7
#   usage() enumerates only the first 22 options               1
#   the estimation-failure exit code back to 0                 1
#
# Two checks in [0] do NOT bite, and are kept for what they document rather
# than for what they catch:
#   - `-weakex 0`: the pre-P1 program also exited 1 on it, but through the
#     wrong path and with the message of another failure ("no se pudo abrir
#     (null)").  The check pins the exit code, not the message.
#   - `r = -1`: refused before P1 too, by the explicit test that was already
#     in main().  It is here so the three positionals are checked as a set.


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

# ============================================================ 0 COMMAND LINE ==
#  Anadido con P1 (docs/PLAN_PRODUCCION.md).  Lo que protege, y por que existe:
#  hasta 2026-08-20 el bucle de opciones no tenia rama else, asi que una errata
#  -- -diagcv por -diagcov -- se ignoraba EN SILENCIO y drvec estimaba otro
#  modelo sin decirlo; y p, q, r se leian con atoi, de modo que p = 0, p = -1,
#  p = 200 y un p no numerico tumbaban el proceso por SIGSEGV dentro de
#  init_guess.  Cada linea de abajo es una de esas entradas.
#
#  La comprobacion que no puede quedarse obsoleta es la ultima: compara la lista
#  de opciones que el parser ACEPTA con la que usage() ENUMERA.  Antes de P1 el
#  usage cubria 22 de 33, y nada lo detectaba.
echo "[0] the command line: it refuses rather than ignores"

CLI=$TMP/cli
cp "$MM" "$CLI.inp"

# cli <label> <expected-rc> <args...>
cli() {
    local label=$1 want=$2; shift 2
    local rc
    timeout "$RUN_TIMEOUT" "$DRVEC" "$@" >/dev/null 2>&1; rc=$?
    if [ "$rc" -ge 128 ] && [ "$rc" -ne 124 ]; then
        bad "$label" "the process died on signal $((rc-128)) instead of returning $want"
    elif [ "$rc" -eq 124 ]; then
        bad "$label" "did not finish in ${RUN_TIMEOUT}s"
    elif [ "$rc" -ne "$want" ]; then
        bad "$label" "rc=$rc, expected $want"
    else ok "$label"; fi
}

# --- the queries: they answer and leave, with 0 -----------------------------
cli "-h exits 0"                    0 -h
cli "--help exits 0"                0 --help
cli "--version exits 0"             0 --version
cli "no arguments is a usage error" 1

# --- the positionals: strtol, not atoi --------------------------------------
cli "p = 0 is refused"              1 "$CLI" 0 1 1 -case 2
cli "p = -1 is refused"             1 "$CLI" -1 1 1 -case 2
cli "p = 200 is refused"            1 "$CLI" 200 1 1 -case 2
cli "a non-numeric p is refused"    1 "$CLI" x y z -case 2
cli "q = -1 is refused"             1 "$CLI" 2 -1 1 -case 2
cli "r = -1 is refused"             1 "$CLI" 2 1 -1 -case 2
cli "r >= M is refused"             1 "$CLI" 2 1 5 -case 2

# --- unknown options, which is the defect this section exists for -----------
cli "an invented option is refused" 1 "$CLI" 2 1 1 -case 2 -bogusflag
cli "-diagcv (a typo) is refused"   1 "$CLI" 2 1 1 -case 2 -diagcv
cli "-multistar (a typo) is refused" 1 "$CLI" 2 1 1 -case 2 -multistar 20
cli "a stray positional is refused" 1 "$CLI" 2 1 1 -case 2 rubbish

#  Y la sugerencia, que es lo que convierte el rechazo en algo util.
sug=$(timeout "$RUN_TIMEOUT" "$DRVEC" "$CLI" 2 1 1 -diagcv 2>&1 | grep -a 'did you mean')
case "$sug" in
    *-diagcov*) ok "the refusal suggests the option that was meant" ;;
    *)          bad "typo suggestion" "got: ${sug:-nothing}" ;;
esac

# --- option values ----------------------------------------------------------
cli "-case 0 is refused"            1 "$CLI" 2 1 1 -case 0
cli "-case 9 is refused"            1 "$CLI" 2 1 1 -case 9
cli "-m 3 is refused"               1 "$CLI" 2 1 1 -m 3
cli "-multistart 0 is refused"      1 "$CLI" 2 1 1 -case 2 -multistart 0
cli "-multistart abc is refused"    1 "$CLI" 2 1 1 -case 2 -multistart abc
cli "-bootstrap -1 is refused"      1 "$CLI" 2 1 1 -case 2 -bootstrap -1
cli "-matest 0 is refused"          1 "$CLI" 2 1 1 -case 2 -matest 0
cli "-weakex 0 is refused"          1 "$CLI" 2 1 1 -case 2 -weakex 0
cli "-rankadm -1 is refused"        1 "$CLI" 2 1 1 -case 2 -rankadm -1
cli "-rankadm 0 is refused"         1 "$CLI" 2 1 1 -case 2 -rankadm 0
cli "-seedb2 abc is refused"        1 "$CLI" 2 1 1 -case 2 -seedb2 abc
cli "-alpha with no file is refused" 1 "$CLI" 2 1 1 -case 2 -alpha

#  Y LO QUE NO DEBE ROMPERSE.  -fixb2 y -rankadm llevan valor OPCIONAL, y el
#  criterio con que la validacion decide si el siguiente argumento es el valor
#  tiene que ser EL MISMO que usa el asignador, o una linea de ordenes legitima
#  deja de funcionar.  -fixb2 -0.5 es un valor negativo que empieza por '-';
#  -fixb2 -diagma es una opcion detras de -fixb2.  Los dos son validos.
cli "-fixb2 with no value still runs"   0 "$CLI" 2 1 1 -case 2 -fixb2
cli "-fixb2 -0.5 still runs"            0 "$CLI" 2 1 1 -case 2 -fixb2 -0.5
cli "-fixb2 followed by an option runs" 0 "$CLI" 2 1 1 -case 2 -fixb2 -diagma
cli "-rankadm with no tolerance runs"   0 "$CLI" 2 1 1 -case 2 -rankadm
cli "an ordinary fit still exits 0"     0 "$CLI" 2 1 1 -case 2

# --- degrees of freedom: the sample sets the upper bound on p and q ---------
cli "p = 60 on 61 observations is refused" 1 "$CLI" 60 1 1 -case 2
cli "q = 60 on 61 observations is refused" 1 "$CLI" 2 60 1 -case 2

# --- the exit code says what happened ---------------------------------------
#  Un ajuste que NO se pudo completar sale con 2.  El caso: dos series
#  exactamente colineales, que dejan el operador AR fuera de la region
#  estacionaria (ifault = 3).  Se genera aqui, como el resto de fixtures.
awk 'BEGIN{ n=40;
    printf "* fixture: the second series is an exact multiple of the first\n";
    printf "1\n2 %d 1 1900\na b\n1.0 0 0\n", n;
    for (t=0; t<n; t++) { x = sin(t*0.3) + t*0.01; printf "%.10f %.10f\n", x, 2*x } }' \
  > "$TMP/degen.inp"
cli "an estimation that fails exits 2"  2 "$TMP/degen" 2 1 1 -case 2

#  Y LA DISTINCION QUE IMPORTA: el termcode 3 -- "last global step failed to
#  locate a lower point" -- NO es un fallo.  Es la parada explicada en la que
#  se apoya la mayoria de lo que este programa publica (CONVERGENCE.md), y
#  tiene que seguir saliendo con 0.  Si esto empieza a fallar, alguien ha
#  convertido una parada en un error y ha marcado como rotos la mitad de los
#  resultados del registro.
timeout "$RUN_TIMEOUT" "$DRVEC" "$CLI" 2 1 1 -case 1 >/dev/null 2>&1; rc3=$?
if grep -qa 'failed to locate a lower point' "$CLI.out" 2>/dev/null; then
    [ "$rc3" -eq 0 ] && ok "a termcode-3 stop still exits 0" \
                     || bad "termcode 3 exit code" "rc=$rc3, expected 0"
else
    ok "a termcode-3 stop still exits 0 (not reached on this fit; rc=$rc3)"
fi

# --- usage() and the parser cannot diverge ----------------------------------
#  Es la comprobacion que no se queda obsoleta: la lista aceptada sale del
#  FUENTE y la enumerada sale del BINARIO.  Antes de P1 habrian diferido en 11.
if [ -f src/drvec.c ]; then
    parser_opts=$(grep -ao 'strcmp(argv\[i\], "-[A-Za-z0-9]*"' src/drvec.c \
                  | grep -ao '"-[A-Za-z0-9]*"' | tr -d '"' | sort -u)
    usage_opts=$("$DRVEC" -h 2>/dev/null \
                 | sed -n '/^Every option drvec accepts/,$p' | tail -n +2 \
                 | tr -s ' ' '\n' | grep -a '^-' | sort -u)
    if [ "$parser_opts" = "$usage_opts" ]; then
        ok "every option the parser accepts is enumerated by -h ($(printf '%s\n' "$parser_opts" | wc -l) of them)"
    else
        bad "usage() and the parser have diverged" \
            "only in the parser: $(comm -23 <(printf '%s\n' "$parser_opts") <(printf '%s\n' "$usage_opts") | tr '\n' ' ')
        only in -h:        $(comm -13 <(printf '%s\n' "$parser_opts") <(printf '%s\n' "$usage_opts") | tr '\n' ' ')"
    fi
else
    ok "usage/parser cross-check skipped (not run from the source tree)"
fi

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
         "-case 2 -fixb2" "-case 2 -fixb2 0" "-case 2 -fixb2 -0.5" "-case 2 -fixb2 -diagma" \
         "-case 2 -mawarma" "-case 1 -mawarma" "-case 2 -mawarma -diagar" \
         "-case 2 -mawarma -fixb2" "-case 2 -marow" "-case 2 -matri" \
         "-case 2 -marow -fixb2" "-case 3 -matri" "-case 2 -warma" \
         "-case 1 -warma" "-case 3 -warma" "-case 2 -warma -fixb2" \
         "-case 2 -rankadm 0.2"; do
    struct_case "M=2 p=2 q=1 r=1 $o" "$MM" 2 1 1 $o
done
struct_case "M=2 lrtest case 2"                "$MM" 2 1 0 -case 2 -lrtest
struct_case "M=2 lrtest case 1 all-diag"       "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -lrtest
struct_case "M=2 lrtest p=1 (nreg=0 path)"     "$MM" 1 0 0 -case 2 -lrtest
struct_case "M=3 r=2"                          "$UK" 2 0 2 -case 2
struct_case "M=3 lrtest"                       "$UK" 2 0 0 -case 2 -lrtest
struct_case "M=3 lrtest + fixb2"               "$UK" 2 0 0 -case 2 -lrtest -fixb2
struct_case "M=5 r=2 (s=3, r=2: B2 is 3x2)"    "$DK" 2 0 2 -case 2
# -mawarma with r > 1 and s > 1: the inherited block is r x r and the block it
# determines is r x s, so this is the only shape where getting either dimension
# wrong is visible.  The walk check is what catches it -- and it did: the
# printer was reading the free q*M*M stride and publishing a Theta nobody had
# estimated (DEVELOPMENT_RECORD.md 8d).
struct_case "M=5 r=2 q=1 -mawarma (T11 2x2, T12 2x3)" "$DK" 2 1 2 -case 2 -mawarma
struct_case "M=3 r=1 q=1 -mawarma"             "$UK" 2 1 1 -case 2 -mawarma
struct_case "M=5 r=2 q=1 -marow"               "$DK" 2 1 2 -case 2 -marow
struct_case "M=5 r=2 q=1 -matri"               "$DK" 2 1 2 -case 2 -matri
struct_case "M=5 r=2 q=1 -warma"               "$DK" 2 1 2 -case 2 -warma
struct_case "M=3 r=1 q=1 -warma"               "$UK" 2 1 1 -case 2 -warma
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
b2_block=$(sed -n '/^beta_2 matrix (s x r)/,/^[a-zA-Z]/p' "$TMP/case.out" | grep -aE '^ +-?[0-9]' \
           | sed 's/(sd[^)]*)//g' | tr -s ' ' | sed 's/^ //; s/ $//')
#  beta va ahora en el orden del .inp, o sea [Y2 ; Y1]: las filas de beta_2
#  son las s PRIMERAS, no las ultimas.  Ver inp2lam en src/drvec.c.
b_rows=$(sed -n '/^beta matrix (M x r)/,/^[a-zA-Z]/p' "$TMP/case.out" \
         | grep -aE '^ +-?[0-9]' | head -1 | tr -s ' ' | sed 's/^ //; s/ $//')
if [ -n "$b2_block" ] && [ "$b2_block" = "$b_rows" ]; then
    ok "beta_2 block agrees with the beta matrix rows (M=3, r=2)"
else
    bad "beta_2 block vs beta matrix rows" "block=[$b2_block] rows=[$b_rows]"
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
    sed -n '/^Sigma = sigma2 \* Q/,/^ *|Sigma|/p' "$1" | grep -aE '^ +-?[0-9]' \
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
#  P12 (2026-09-23).  Six of these moved when every fit became the best of
#  several starts, and all six moved UP -- none down, the other twelve not by a
#  digit: they were local optima of the single cold start (BUG-25).  Before:
#  -case 1 -10.7273981157, -diagma 0.8816637342, -case 3 -mafree 6.5140062493,
#  -mafree -diagar -2.5419963582, -mafree -fixb2 0 -8.4835302747 (BUG-33; now
#  3.0123641758 with the consistent seed as one more start),
#  DK r=2 828.8447477597 (the fit that flipped the Danish rank from 2 to 0).
#  And with the LADDER as the start (P12, the same day): -marow -diagcov
#  -2.6646873358 -> -2.6623400220, and DK r=2 832.3550925844 -> 858.3431076100
#  -- both won by the ladder's start, none fell.
golden() {
    local want=$1 src=$2; shift 2
    run "$src" "$@"
    local got; got=$(logelf_of "$TMP/case")
    if [ -z "$got" ]; then bad "$* -> expected $want" "no logelf in output"
    elif near "$got" "$want"; then ok "$* = $got"
    else bad "$*" "expected $want, got $got"; fi
}
#  P4, 2026-08-20: EL DEFECTO SE MOVIO, y con el estas ocho lineas.  Con q >= 1
#  y r >= 1 el defecto es ahora -marow (las s filas inferiores de Theta nulas),
#  porque por el Corolario 6.3 esa es la clase donde la region admisible es el
#  espacio entero.  Los valores de la clase LIBRE se conservan justo debajo,
#  bajo -mafree, para que la parametrizacion anterior siga protegida: es la que
#  sostiene todo el registro previo a esta fecha.
#  -diagma no se movio: es una restriccion distinta, que el defecto no toca.
golden  -4.0893040918 "$MM" 2 1 1 -case 1 -marow
golden   2.3039690333 "$MM" 2 1 1 -case 2 -marow
golden   2.3074789504 "$MM" 2 1 1 -case 3 -marow
golden  -3.5511860134 "$MM" 2 1 1 -case 2 -marow -diagar
golden   0.9355032708 "$MM" 2 1 1 -case 2 -diagma
golden  -2.6623400220 "$MM" 2 1 1 -case 2 -marow -diagcov
golden   2.1765180953 "$MM" 2 1 1 -case 2 -marow -fixb2
golden   0.7822395343 "$MM" 2 1 1 -case 2 -marow -fixb2 0

#  The default IS the free class since 2026-09-23 (BUG-48): with no class
#  asked for, the fit must land exactly where -mafree does.
golden   6.4786201604 "$MM" 2 1 1 -case 2

#  La clase libre, con los valores que eran el defecto hasta el 2026-08-20.
golden   3.6856397544 "$MM" 2 1 1 -case 1        -mafree
golden   6.4786201604 "$MM" 2 1 1 -case 2        -mafree
golden   6.9994563633 "$MM" 2 1 1 -case 3        -mafree
golden   1.0712154882 "$MM" 2 1 1 -case 2 -mafree -diagar
golden   0.5696296891 "$MM" 2 1 1 -case 2 -mafree -diagcov
golden   5.4717136367 "$MM" 2 1 1 -case 2 -mafree -fixb2
golden   3.0123641758 "$MM" 2 1 1 -case 2 -mafree -fixb2 0
golden 570.2297062756 "$UK" 2 0 2 -case 2
golden 858.3431076100 "$DK" 2 0 2 -case 2   # s=3, r=2: guards the B2 read order
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
#     P4: -mafree porque la ruta de residuos siembra la DIAGONAL de Theta, que
#     no es un punto de la clase por defecto (filas inferiores nulas).  Lo que
#     este bloque comprueba es la fontaneria de la siembra, no el defecto.
run "$MM" 2 1 1 -case 2 -mafree -seed tests/fixtures/mmres
got=$(logelf_of "$TMP/case")
if [ -z "$got" ]; then bad "seeded fit from fixtures" "no logelf"
elif near "$got" 6.4460747665; then ok "seeded from .pre fixtures = $got"
else bad "seeded fit from fixtures" "expected 6.4460747665, got $got"; fi
echo

# ==================================== 6 RESTRICTIONS ON ALPHA (H1(r)) ==
# 5d. THE ENTRY GATE certifies itself, by the ladder's two contracts.  The
#     program reports them now, not only this suite: the crossing identity (at
#     the diagonal rung the likelihood factorises, so the joint fit must equal
#     the sum of the univariate ones) and the optimality certificate (the gap
#     against the values brought in is >= 0, and zero iff they were optima).
#     Checked in BOTH directions, because a verdict with only one outcome is
#     not a verdict: the .pre fixture must read as AN OPTIMUM and the .inp
#     specification beside it as A SPECIFICATION.
run "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov
if ! grep -aq "the factorisation contract" "$TMP/case.out"; then
    bad "gate contract" "the entry gate reports no contract at the diagonal rung"
elif grep -aq "crossing identity.*VERIFIED" "$TMP/case.out"; then
    ok "gate: crossing identity verified"
else bad "gate: crossing identity" "$(grep -a 'crossing identity' "$TMP/case.out")"; fi

# the univariate constants of the gate, reproduced by the program itself
for want in -20.0579 -14.5698; do
    if grep -aq -- "$want" "$TMP/case.out"; then ok "gate: univariate $want"
    else bad "gate: univariate $want" "constant not reproduced"; fi
done

# THE TOLERANCE IS ROUNDING, on both halves (BUG-47).  It used to be xitol with
# q > 0, because the joint and univariate sides truncated the xi sequence at
# different terms; the certificate now evaluates both untruncated, so with q > 0
# the identity must hold to 1e-6 as well -- Milan, which once failed a fixed
# 1e-4 and then passed only against xitol, is the case that shows it.  With
# q = 0 it must hold EXACTLY, the strict half that stops the tolerance from
# becoming a rubber stamp.
run data/pairs/milan.inp 2 1 0 -case 1 -diagar -diagma -diagcov
line=$(grep -a "crossing identity" "$TMP/case.out")
case "$line" in
  *"tolerance 1.0e-06"*VERIFIED*) ok "gate: q=1 verifies to rounding, both sides untruncated";;
  *) bad "gate: q=1 tolerance" "$line";;
esac
run data/pairs/milan.inp 2 0 0 -case 1 -diagar -diagma -diagcov
line=$(grep -a "crossing identity" "$TMP/case.out")
gap=$(printf '%s' "$line" | sed 's/.*= *\([-0-9.e+]*\) .*/\1/')
case "$line" in
  *"tolerance 1.0e-06"*VERIFIED*) ok "gate: q=0 holds exactly (gap $gap)";;
  *) bad "gate: q=0 must hold exactly" "$line";;
esac
awk -v g="$gap" 'BEGIN{if(g<0)g=-g; exit !(g < 1e-9)}' \
  && ok "gate: q=0 gap is below 1e-9, so the tolerance is not a rubber stamp" \
  || bad "gate: q=0 gap" "$gap is not exact; there is no truncation to blame"

# and it must NOT claim the contract away from the diagonal rung.  Since P12
# the ladder certifies ITS rung 0 inside an r = 1 run -- that is the diagonal
# rung, legitimately -- so the claim is checked on the fit itself (-noladder),
# and the ladder's certificate is checked to sit inside the ladder's block.
run "$MM" 2 1 1 -case 2 -noladder
if grep -aq "the factorisation contract" "$TMP/case.out"; then
    bad "gate contract" "claimed at r=1, where the likelihood does not factorise"
else ok "gate: silent where the factorisation does not hold"; fi
run "$MM" 2 1 1 -case 2
if sed -n '/=== The ladder below the rank/,/rung 1 /p' "$TMP/case.out" \
     | grep -aq "crossing identity .*VERIFIED"; then
    ok "ladder: its rung 0 is the certified gate"
else bad "ladder" "rung 0 of the ladder is not certified inside the ladder block"; fi

# the certificate, both verdicts
run "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -seedybar tests/fixtures/mmdiag
if grep -aq "this input is AN OPTIMUM" "$TMP/case.out"; then
    ok "certificate: a genuine .pre reads as an optimum"
else bad "certificate" "the .pre fixture did not read as an optimum"; fi
gap=$(grep -a "optimality gap" "$TMP/case.out" | sed 's/.*= *//')
if awk -v g="$gap" 'BEGIN{exit !(g >= -1e-6)}'; then
    ok "certificate: the gap is non-negative ($gap)"
else bad "certificate" "negative gap $gap: the fit is worse than its start"; fi

mkdir -p "$TMP/spec"
cp tests/fixtures/mmdiag.1.inp "$TMP/spec/sp.1.inp"
cp tests/fixtures/mmdiag.2.inp "$TMP/spec/sp.2.inp"
run "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -seedybar "$TMP/spec/sp"
if grep -aq "this input is A SPECIFICATION" "$TMP/case.out"; then
    ok "certificate: an .inp reads as a specification"
else bad "certificate" "the .inp did not read as a specification"; fi

# and a file that is NOT of the format must be refused, with a sentence.  The
# reader used to skip five header lines by count, so any header of a different
# length -- fue writes a .pre with five, drvec writes an .inp with four -- slid
# it one line down and it took nobs from the wrong place.  That is not a loud
# failure: nobs was uninitialised, so the run was fine or died for memory
# depending on what the stack held, which is how the check above passed once and
# killed the process the next time.  See DEVELOPMENT_RECORD.md 8.
printf 'not a fue file\nat all\n' > "$TMP/spec/bad.1.inp"
cp "$TMP/spec/bad.1.inp" "$TMP/spec/bad.2.inp"
run "$MM" 2 1 0 -case 1 -diagar -diagma -diagcov -seedybar "$TMP/spec/bad"
if [ -n "$TIMEDOUT" ]; then bad "reader: a malformed seed file" "it hung"
elif printf '%s' "$STDERR" | grep -q "separador de frecuencia"; then
    ok "reader: a file without the frequency separator is refused, not read"
else bad "reader: a malformed seed file was not refused" "$STDERR"; fi
echo

# 5e. THE LADDER (-rungs).  Rungs 0-2 all sit at r = 0, so each is nested in
#     the next as an INTERIOR point: the log-likelihood cannot fall and npar
#     must rise.  Three things are checked that a user assembling this by hand
#     gets wrong: the degrees of freedom against the closed form -- M(M-1)/2
#     for the covariance, (p-1)M(M-1) + qM(M-1) for the dynamics --, the sign
#     of every LR, and above all that the TOP rung reproduces the fit the
#     program gives without -rungs.  That last one is what says the ladder is
#     re-estimating THE SAME models and not a differently configured family.
#     Columns are taken from the right (NF), because the rung names have
#     spaces in them and counting from the left ties the check to the wording.
run "$MM" 2 1 0 -case 1 -rungs
if ! grep -aq "The ladder: rungs" "$TMP/case.out"; then
    bad "ladder" "-rungs emitted no ladder"
else
    ok "ladder: emitted"
    rung() { awk -v k="$1" -v c="$2" \
             '$1==k && $2!="->" && NF>4 {print $(NF-c)}' "$TMP/case.out"; }
    l0=$(rung 0 2); l1=$(rung 1 2); l2=$(rung 2 2)
    n0=$(rung 0 3); n1=$(rung 1 3); n2=$(rung 2 3)
    if [ -z "$l0" ] || [ -z "$l1" ] || [ -z "$l2" ]; then
        bad "ladder: table unreadable" "logL: '$l0' '$l1' '$l2'"
    elif awk -v a="$l0" -v b="$l1" -v c="$l2" \
             'BEGIN{exit !(a<=b+1e-6 && b<=c+1e-6)}'; then
        ok "ladder: logL monotone across rungs ($l0 <= $l1 <= $l2)"
    else bad "ladder: logL not monotone" "$l0 $l1 $l2"; fi
    if [ "$n0" -lt "$n1" ] 2>/dev/null && [ "$n1" -lt "$n2" ] 2>/dev/null; then
        ok "ladder: npar increases ($n0 < $n1 < $n2)"
    else bad "ladder: npar" "$n0 $n1 $n2"; fi

    # the degrees of freedom must equal the closed form, M = 2, p = 2, q = 1
    df1=$(awk '/^  0 -> 1/{print $5}' "$TMP/case.out")
    df2=$(awk '/^  1 -> 2/{print $5}' "$TMP/case.out")
    [ "$df1" = "1" ] && ok "ladder: df(0->1) = M(M-1)/2 = 1" \
                     || bad "ladder: df(0->1)" "expected 1, got $df1"
    [ "$df2" = "4" ] && ok "ladder: df(1->2) = (p-1)M(M-1)+qM(M-1) = 4" \
                     || bad "ladder: df(1->2)" "expected 4, got $df2"

    # every LR in the ladder must be non-negative: a nested fit cannot be worse
    neg=$(awk '/^  [01] -> [12]/{if ($4+0 < -1e-6) c++} END{print c+0}' \
          "$TMP/case.out")
    [ "$neg" = "0" ] && ok "ladder: no negative LR" \
                     || bad "ladder: $neg negative LR" "a wider fit came out worse"

    # rung 0 must carry its contract, since it is the base being built on
    if grep -aq "the factorisation contract" "$TMP/case.out"; then
        ok "ladder: rung 0 certifies itself"
    else bad "ladder" "rung 0 reported no contract"; fi

    # and rung 0's joint fit must be the sum of the univariate ones, which is
    # the same oracle as [3] but taken from inside the ladder
    top=$l2
    run "$MM" 2 1 0 -case 1
    plain=$(logelf_of "$TMP/case")
    if [ -z "$plain" ]; then bad "ladder: top rung" "no plain r=0 fit to compare"
    elif awk -v a="$top" -v b="$plain" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=1e-4)}'
    then ok "ladder: top rung reproduces the plain r=0 fit ($top vs $plain)"
    else bad "ladder: top rung" "rung 2 = $top but the plain r=0 fit = $plain"; fi
fi
echo

# 5f. -seedgate (route B).  It is not the default and it is measured to be worse
#     on most of the bank, so what is checked is not that it helps: it is that it
#     does what it says.  Two things.  With q = 0 both routes must reach the SAME
#     optimum -- there is no moving-average block for the seeding to disagree
#     about -- and that is an invariant, so it cannot go stale.  And with q = 1
#     it must emit its report and still produce a fit, because a seeding option
#     that silently drops the estimation would look like a success here.
run datasets/synthetic/rank2.inp 2 0 1 -case 2 -mean
c0=$(logelf_of "$TMP/case")
run datasets/synthetic/rank2.inp 2 0 1 -case 2 -mean -seedgate
b0=$(logelf_of "$TMP/case")
if [ -z "$c0" ] || [ -z "$b0" ]; then
    bad "seedgate: q=0" "missing logelf (C=$c0 B=$b0)"
elif awk -v a="$c0" -v b="$b0" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=1e-6)}'; then
    ok "seedgate: with q=0 both routes reach the same optimum ($c0)"
else bad "seedgate: q=0 routes disagree" "cold=$c0 seedgate=$b0"; fi

run "$MM" 2 1 1 -case 2 -mean -seedgate
if ! grep -aq "the VEC block profiled on the rung below" "$TMP/case.out"; then
    bad "seedgate" "no report emitted"
elif ! grep -aq "the pre-estimates the conditional step produced" "$TMP/case.out"; then
    bad "seedgate" "the pre-estimates of Lambda and B2 are not reported"
elif [ -n "$(logelf_of "$TMP/case")" ]; then
    ok "seedgate: reports its two stages, its pre-estimates, and still fits"
else bad "seedgate" "reported but produced no fit"; fi
echo

# 5g. -seedb2 v starts B2 at v and estimates it FREE, where -fixb2 v pins it
#     there.  Different models, but THE SAME STARTING POINT -- so the starting
#     log-likelihood must agree to the digit.  That is what says -seedb2 writes
#     B2 into the right slots in the right order, which is the only thing that
#     can silently be wrong about it, and it is checked on M = 3 with r = 2 as
#     well as on M = 2: with s = 1 the fill order of B2 is a no-op, so a
#     transposition would be invisible there (the same lesson as the M=5 golden).
for spec in "2 1 1 -case 2 -mean:$MM:-0.5" \
            "2 0 2 -case 2 -mean:datasets/synthetic/rank2.inp:0.3"; do
    sp=${spec%%:*}; rest=${spec#*:}; src=${rest%%:*}; val=${rest#*:}
    cp "$src" "$TMP/case.inp"
    a=$("$DRVEC" "$TMP/case" $sp -seedb2 "$val" -eval 2>/dev/null \
        | grep -a 'at the starting point' | sed 's/.*point = *//; s/ .*//')
    b=$("$DRVEC" "$TMP/case" $sp -fixb2  "$val" -noladder -eval 2>/dev/null \
        | grep -a 'at the starting point' | sed 's/.*point = *//; s/ .*//')
    if [ -z "$a" ] || [ -z "$b" ]; then
        bad "seedb2: start identity ($sp)" "missing eval (seedb2=$a fixb2=$b)"
    elif [ "$a" = "$b" ]; then
        ok "seedb2: starts where -fixb2 pins ($sp): $a"
    else bad "seedb2: start identity ($sp)" "seedb2=$a but fixb2=$b"; fi
done
echo

# 5h. -seedjoh seeds B2 with the canonical reduced-rank solution (Johansen's
#     eigenvalue problem, closed form) instead of the static OLS one.  Two
#     checks, and the first is an invariant: WITH q = 0 the two routes must
#     reach the same optimum, because there the surface is well behaved and
#     both converge on the gradient -- a seeding option that changed the ANSWER
#     there would be changing the model, not the start.  The second is the
#     claim the route is built on, so it is measured rather than asserted: the
#     canonical seed starts closer.  It names its case on purpose -- on
#     mink_muskrat it is the cold seed that starts closer, which is why the
#     route is not the default (HOMOLOGATION.md 4e).
run data/pairs/milan.inp 2 0 1 -case 2 -mean
c0=$(logelf_of "$TMP/case")
run data/pairs/milan.inp 2 0 1 -case 2 -mean -seedjoh
j0=$(logelf_of "$TMP/case")
if [ -z "$c0" ] || [ -z "$j0" ]; then
    bad "seedjoh: q=0" "missing logelf (cold=$c0 canonical=$j0)"
elif awk -v a="$c0" -v b="$j0" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=1e-6)}'; then
    ok "seedjoh: with q=0 both seeds reach the same optimum ($c0)"
else bad "seedjoh: q=0 seeds disagree on the optimum" "cold=$c0 canonical=$j0"; fi

if ! grep -aq "seedjoh: B2 seeded from the canonical" "$TMP/case.out"; then
    bad "seedjoh" "the canonical solution was not formed on Milan"
else
    ok "seedjoh: the canonical solution is formed and reported"
    "$DRVEC" "$TMP/case" 2 0 1 -case 2 -mean -noladder -eval >/dev/null 2>&1
    sc=$(awk '/^eval logelf/{print $4}' "$TMP/case.out")
    "$DRVEC" "$TMP/case" 2 0 1 -case 2 -mean -seedjoh -eval >/dev/null 2>&1
    sj=$(awk '/^eval logelf/{print $4}' "$TMP/case.out")
    if [ -z "$sc" ] || [ -z "$sj" ]; then
        bad "seedjoh: starting values" "missing eval (cold=$sc canonical=$sj)"
    elif awk -v a="$sc" -v b="$sj" -v o="$c0" \
            'BEGIN{da=o-a; db=o-b; if(da<0)da=-da; if(db<0)db=-db; exit !(db<da)}'
    then ok "seedjoh: the canonical seed starts closer on Milan ($sj vs $sc, optimum $c0)"
    else bad "seedjoh: the canonical seed did not start closer" "cold=$sc canonical=$sj optimum=$c0"; fi
fi
echo

# 5i. -mawarma: the moving average INHERITS its structure instead of being free,
#     which is what the WARMA-VEC equivalence implies (BVECM corollary 2):
#     Theta = [T11  T11*B2' ; 0  0].  Two checks, and the first is the structure
#     itself -- the last s rows must be exactly zero and the top-right block must
#     be the product, because those are not estimates, they are consequences.
#     Milan on purpose and not mink_muskrat: there the restricted fit drives T11
#     to zero, and a product check with a zero factor does not bite.
run data/pairs/milan.inp 2 1 1 -case 2 -mean -mawarma
if ! grep -aq "^theta(1) matrix" "$TMP/case.out"; then
    bad "mawarma" "no inherited-structure Theta reported"
else
    #  the nabla Y2 row, which -mawarma zeroes, is row 1 in the .inp's order
    #  (the report is in that order since BUG-23; it was the last internal row)
    z=$(awk '/^theta\(1\) matrix/{getline; print $1+0, $2+0}' "$TMP/case.out")
    [ "$z" = "0 0" ] && ok "mawarma: the nabla Y2 row of Theta is exactly zero" \
                     || bad "mawarma: the nabla Y2 row of Theta is not zero" "$z"
    # T12 = T11 * B2' -- checked against the B2 the same fit reports
    #  .inp order (M = 2, r = 1): row 2 is Y1; T11 is its Y1 column (2) and
    #  T12 its nabla Y2 column (1)
    t11=$(awk '/^theta\(1\) matrix/{getline; getline; print $2}' "$TMP/case.out")
    t12=$(awk '/^theta\(1\) matrix/{getline; getline; print $1}' "$TMP/case.out")
    b2=$(grep -a -A1 "^beta_2 matrix (s x r)" "$TMP/case.out" | tail -1 | awk '{print $1}')
    if ! awk -v a="$t11" 'BEGIN{if(a<0)a=-a; exit !(a>1e-3)}'; then
        bad "mawarma: T11 is zero here" "the product check would not bite"
    elif awk -v a="$t11" -v b="$t12" -v c="$b2" \
         'BEGIN{d=a*c-b; if(d<0)d=-d; exit !(d<=1e-5)}'; then
        ok "mawarma: T12 = T11*B2' ($t11 * $b2 = $t12)"
    else bad "mawarma: T12 is not T11*B2'" "T11=$t11 B2=$b2 T12=$t12"; fi
fi

# and the restriction must BITE: it cannot beat the free model, and on this data
# it must move the fit off the invertibility boundary, which is the measured
# reason it exists at all (HOMOLOGATION.md 4g).
free_ll=$(run "$MM" 2 1 1 -case 2 -mean; logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -mean -mawarma
warm_ll=$(logelf_of "$TMP/case")
if [ -z "$free_ll" ] || [ -z "$warm_ll" ]; then
    bad "mawarma: nested comparison" "missing logelf (free=$free_ll warma=$warm_ll)"
elif awk -v a="$free_ll" -v b="$warm_ll" 'BEGIN{exit !(b <= a + 1e-6)}'; then
    ok "mawarma: the restricted fit cannot beat the free one ($warm_ll <= $free_ll)"
else bad "mawarma: restricted beat free" "free=$free_ll warma=$warm_ll"; fi
echo

# 5j. THE RANK CONDITION.  sigma_s(Lambda_perp' Theta(1)) (Theorem 3; it was
#     sigma_min(Lambda_perp' Theta(1) B_perp) until BUG-46) is what makes the
#     long-run impact C(1) have rank M-r; where it is zero the fitted model
#     denies the rank it was estimated at, and below the floor it is NEAR that.
#     Vienna, not Milan: with the right statistic Milan's free fit is at 0.31.  It is REPORTED always and
#     -rankadm refuses points below a tolerance.  Three checks: that it is
#     reported, that the constraint actually binds (the optimiser wants to go
#     below, which is the whole finding), and that constraining cannot buy
#     likelihood -- a constrained fit that beat the free one would mean the
#     constraint is not a constraint.
#     P4: sobre -mafree.  -rankadm es un instrumento de medida SOBRE LA CLASE
#     LIBRE; en la clase por defecto la condicion se cumple sola (Corolario
#     6.3) y no hay nada que restringir, de modo que medir ahi si "liga" no
#     tendria sentido.
run data/pairs/vienna.inp 2 1 1 -case 2 -mean -mafree
g_free=$(awk '/Rank condition/{print $NF}' "$TMP/case.out")
ll_free=$(logelf_of "$TMP/case")
if [ -z "$g_free" ]; then bad "rank condition" "not reported at r=1, q=1"
else ok "rank condition: reported ($g_free)"; fi

run data/pairs/vienna.inp 2 1 1 -case 2 -mean -mafree -rankadm 0.3
g_adm=$(awk '/Rank condition/{print $NF}' "$TMP/case.out")
ll_adm=$(logelf_of "$TMP/case")
if [ -z "$g_adm" ] || [ -z "$ll_adm" ]; then
    bad "rankadm" "no fit (G=$g_adm logL=$ll_adm)"
elif awk -v g="$g_adm" 'BEGIN{exit !(g >= 0.3 - 1e-6)}'; then
    ok "rankadm: the constraint holds at the optimum ($g_adm >= 0.3)"
else bad "rankadm: constraint violated" "G=$g_adm with tol 0.3"; fi

# THE VERDICT MUST REACH THE TERMINAL, not only the .out.  That is the whole
# of the step-4 decision: the default COMPUTATION does not move -- no recorded
# result moves -- but the default PRESENTATION stops handing over as an answer
# something the theory does not license.  Checked in both directions, because a
# warning that fires on everything is not a warning.
#  P4: el caso inadmisible hay que PEDIRLO con -mafree.  Que el defecto ya no
#  pueda producirlo es el resultado de P4 y se comprueba aparte, mas abajo.
run data/pairs/vienna.inp 2 1 1 -case 2 -mean -mafree
if printf '%s' "$STDERR" | grep -q "WARNING: sigma_s" ||
   grep -aq "below the floor" "$TMP/case.out"; then
    ok "rank verdict: an inadmissible fit says so where the user can see it"
else
    # the notice goes to stdout, which run() discards; re-run capturing it
    cp data/pairs/vienna.inp "$TMP/case.inp"
    if "$DRVEC" "$TMP/case" 2 1 1 -case 2 -mean -mafree 2>/dev/null | grep -q "WARNING: sigma_s"
    then ok "rank verdict: an inadmissible fit says so on the terminal"
    else bad "rank verdict" "an inadmissible fit was reported silently"; fi
fi
cp data/pairs/milan.inp "$TMP/case.inp"
if "$DRVEC" "$TMP/case" 2 1 1 -case 2 -mean -marow 2>/dev/null \
   | grep -q "WARNING: sigma_s"; then
    bad "rank verdict" "it fired on an admissible fit too"
else ok "rank verdict: silent on an admissible fit"; fi

if awk -v a="$g_free" 'BEGIN{exit !(a < 0.3)}'; then
    ok "rankadm: and it BINDS -- the free fit sits below it ($g_free)"
else bad "rankadm: does not bind here" "the free fit already has G=$g_free"; fi

if awk -v a="$ll_free" -v b="$ll_adm" 'BEGIN{exit !(b <= a + 1e-6)}'; then
    ok "rankadm: the constrained fit cannot beat the free one ($ll_adm <= $ll_free)"
else bad "rankadm: constrained beat free" "free=$ll_free adm=$ll_adm"; fi

# BUG-46's counterexample: Lambda = (.5, -.2)', B2 = -1, Theta_1 = [[.5, 3],
# [0, 0]] -- invertible MA, Lambda_perp' Theta(1) = (.1, -.1), rank exactly 1 --
# 2001 observations.  The old statistic called it ZERO (1.7e-3); Theorem 3's
# condition is 0.141 at the truth.  It must not be reported as a denial.
run tests/repro/fixtures/g0.inp 1 1 1 -case 1
g_g0=$(awk '/Rank condition/{print $NF}' "$TMP/case.out")
if [ -n "$g_g0" ] && awk -v g="$g_g0" 'BEGIN{exit !(g > 0.05)}' &&
   ! grep -aq "ZERO to working precision" "$TMP/case.out"; then
    ok "rank condition: BUG-46's counterexample is not denied its rank ($g_g0)"
else bad "rank condition on g0" "G=$g_g0"; fi

# and the inherited structure cannot degenerate at all: Theta(1)'s lower block
# is the identity by construction, so G stays O(1) with no constraint imposed.
run data/pairs/milan.inp 2 1 1 -case 2 -mean -mawarma
g_wa=$(awk '/Rank condition/{print $NF}' "$TMP/case.out")
if awk -v g="$g_wa" 'BEGIN{exit !(g > 0.5)}'; then
    ok "mawarma: the rank condition cannot degenerate ($g_wa)"
else bad "mawarma: rank condition degenerate" "G=$g_wa"; fi
echo

# 5k. -matest N: the inherited moving average against the free one, with the
#     distribution SIMULATED rather than assumed.  Small B on purpose -- this
#     checks the machinery, not the calibration.  What must hold whatever the
#     numbers are: the statistic is 2*[L(free) - L(restricted)] computed from
#     the two fits the suite can reproduce on its own, the critical values are
#     ordered, and the bootstrap ones are ABOVE the chi2 ones, which is the
#     measured reason the option exists (HOMOLOGATION.md 4i).
run "$MM" 2 1 1 -case 2 -mean -matest 30
if ! grep -aq "inherited moving average against the free one" "$TMP/case.out"; then
    bad "matest" "no test block emitted"
else
    lr=$(awk '/LR = 2\*/{print $(NF-3)}' "$TMP/case.out")
    l0=$(awk '/^  restricted     logL/{print $4}' "$TMP/case.out")
    l1=$(awk '/^  unrestricted   logL/{print $4}' "$TMP/case.out")
    if awk -v a="$lr" -v b="$l0" -v c="$l1" \
         'BEGIN{d=2*(c-b)-a; if(d<0)d=-d; exit !(d<=1e-4)}'; then
        ok "matest: LR = 2*[L(free) - L(restricted)] ($lr)"
    else bad "matest: LR does not match its own two fits" "LR=$lr L0=$l0 L1=$l1"; fi

    c10=$(awk '/critical values/{print $4}' "$TMP/case.out")
    c05=$(awk '/critical values/{print $6}' "$TMP/case.out")
    c01=$(awk '/critical values/{print $8}' "$TMP/case.out")
    pv=$(awk '/bootstrap p-value/{print $NF}' "$TMP/case.out")
    if [ -z "$c01" ]; then bad "matest: no critical values" "with B=30"
    elif awk -v a="$c10" -v b="$c05" -v c="$c01" \
           'BEGIN{exit !(a>0 && a<=b && b<=c)}'; then
        ok "matest: bootstrap critical values ordered ($c10 <= $c05 <= $c01)"
    else bad "matest: critical values not ordered" "$c10 $c05 $c01"; fi

    # chi2(3) at 5% is 7.815; the bootstrap value must exceed it, which is the
    # finding: the asymptotic test over-rejects on this class of model.
    if awk -v b="$c05" 'BEGIN{exit !(b > 7.815)}'; then
        ok "matest: the bootstrap 5% value is above chi2(3)'s 7.815 ($c05)"
    else bad "matest: bootstrap below chi2" "5% value $c05 vs 7.815"; fi

    if awk -v p="$pv" 'BEGIN{exit !(p > 0 && p <= 1)}'; then
        ok "matest: p-value in (0,1] ($pv)"
    else bad "matest: p-value out of range" "$pv"; fi
fi
echo

# 5l. THE MOVING-AVERAGE LADDER.  Four nested specifications of Theta, and what
#     is checked is the nesting itself, which cannot go stale: each wider one
#     must fit at least as well, and the two structural zeros must be exactly
#     zero.  The measured point they were built for is in HOMOLOGATION.md 4j:
#     what makes the fit inadmissible is T22, not T21.
for spec in "-mawarma:1" "-marow:2" "-matri:3" "-mafree:4"; do
    o=${spec%%:*}; np=${spec##*:}
    run data/pairs/milan.inp 2 1 1 -case 2 -mean $o
    eval "ll_$np=$(logelf_of "$TMP/case")"
done
if awk -v a="$ll_1" -v b="$ll_2" -v c="$ll_3" -v d="$ll_4" \
     'BEGIN{exit !(a<=b+1e-6 && b<=c+1e-6 && c<=d+1e-6)}'; then
    ok "MA ladder: logL is monotone in the nesting ($ll_1 <= $ll_2 <= $ll_3 <= $ll_4)"
else bad "MA ladder: not monotone" "$ll_1 $ll_2 $ll_3 $ll_4"; fi

run data/pairs/milan.inp 2 1 1 -case 2 -mean -marow
z=$(awk '/^theta\(1\) matrix/{getline; print $1+0, $2+0}' "$TMP/case.out")   # nabla Y2 row
[ "$z" = "0 0" ] && ok "marow: the differenced block carries no moving average" \
                 || bad "marow: last row not zero" "$z"
run data/pairs/milan.inp 2 1 1 -case 2 -mean -matri
z=$(awk '/^theta\(1\) matrix/{getline; print $2+0}' "$TMP/case.out")   # T21: nabla Y2 row, Y1 column
[ "$z" = "0" ] && ok "matri: the lower-left block is zero" \
               || bad "matri: T21 not zero" "$z"
echo

# 5m. THE TWO PARAMETERISATIONS OF THE SAME CLASS.  -mawarma restricts Theta in
#     VEC coordinates and casts VEC -> VARMA at every evaluation; -warma writes
#     the transformed VARMA directly, with B2 entering only through the data.
#     WITH p = 1 there are no F lags, so the two describe THE SAME family and
#     must reach the same maximum.  That is an invariant across two casts that
#     share no code path, which makes it the strongest check either of them has.
run data/pairs/utrecht.inp 1 1 1 -case 2 -mean -warma
w1=$(logelf_of "$TMP/case")
run data/pairs/utrecht.inp 1 1 1 -case 2 -mean -mawarma
w2=$(logelf_of "$TMP/case")
if [ -z "$w1" ] || [ -z "$w2" ]; then
    bad "warma: p=1 identity" "missing logelf (warma=$w1 mawarma=$w2)"
elif awk -v a="$w1" -v b="$w2" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=1e-6)}'; then
    ok "warma: both parameterisations reach the same optimum at p=1 ($w1)"
else bad "warma: the two parameterisations disagree" "warma=$w1 mawarma=$w2"; fi

# and the transformed system it estimates must be reported as such, before the
# inverse map puts the same fit back in VEC coordinates
run data/pairs/utrecht.inp 2 1 1 -case 2 -mean -warma
if grep -aq "Triangular (WARMA) parameterisation" "$TMP/case.out"; then
    ok "warma: reports the coordinates it estimated in"
else bad "warma: output" "it did not report the parameterisation it used"; fi

# THE INVERSE MAP.  Lambda, F, Theta and Pi are RECOVERED by inverting the
# transformation once, not estimated again, so the residual of the equation the
# inversion has to satisfy must be zero to machine precision.  A residual that
# is not zero would mean the fitted point is outside the image of the map and
# the VEC parameters printed are a projection -- which is exactly the kind of
# thing this program has published by accident twice (DEVELOPMENT_RECORD 8d, 8h).
res=$(awk '/inversion residual/{print $4}' "$TMP/case.out")
if [ -z "$res" ]; then bad "warma: inverse map" "no residual reported"
elif awk -v e="$res" 'BEGIN{exit !(e < 1e-6)}'; then
    ok "warma: the inversion is exact ($res)"
else bad "warma: the fit is not in the image of the map" "residual $res"; fi

# and the structure Theorem 6 predicts must come out of the inversion BY ITSELF:
# the differenced block of the recovered Theta carries no moving average.
z=$(awk '/^Theta\[1\] \(M x M\) =/{getline; getline; print $1+0, $2+0}' \
    "$TMP/case.out")
[ "$z" = "0 0" ] && ok "warma: the recovered Theta has the inherited structure" \
                 || bad "warma: recovered Theta" "last row is $z, expected 0 0"

# the rank condition, computed on the recovered VEC parameters, must be O(1):
# by Corollary 6.2 this class cannot degenerate, so a small value here would
# mean the inverse map or the class is wrong (docs/THEORY.md)
gg=$(awk '/Rank condition/{print $NF}' "$TMP/case.out")
if awk -v g="$gg" 'BEGIN{exit !(g > 0.5)}'; then
    ok "warma: the recovered fit satisfies the rank condition ($gg)"
else bad "warma: rank condition on the recovered fit" "$gg"; fi
echo

# 5n. -specs: the whole specification ladder in one run.  What is checked is
#     what cannot go stale: the five models are NESTED, so npar and logL must
#     both increase along the ladder; the admissibility column has to be read
#     off the rank condition and not invented; and a chi2 p-value must NOT be
#     printed for a comparison involving an inadmissible rung, which is the
#     whole point of the column (docs/THEORY.md, corollary 5.1).
#     Vienna since BUG-46: with Theorem 3's statistic Milan's free rung is
#     admissible (G = 0.31); Vienna's is not (0.11).
run data/pairs/vienna.inp 2 1 1 -case 2 -mean -specs
if ! grep -aq "The specification ladder" "$TMP/case.out"; then
    bad "specs" "no ladder emitted"
else
    ok "specs: emitted"
    lls=$(awk '$2 ~ /^[0-9]+$/ && $NF ~ /^(yes|NO)$/ {print $3}' "$TMP/case.out")
    nps=$(awk '$2 ~ /^[0-9]+$/ && $NF ~ /^(yes|NO)$/ {print $2}' "$TMP/case.out")
    if [ "$(printf '%s\n' $lls | wc -l)" != "5" ]; then
        bad "specs: not five rungs" "$(printf '%s ' $lls)"
    elif printf '%s\n' $lls | awk 'NR>1 && $1 < prev - 1e-6 {bad=1} {prev=$1} END{exit bad+0}'
    then ok "specs: logL is monotone along the nesting ($(printf '%s ' $lls))"
    else bad "specs: logL not monotone" "$(printf '%s ' $lls)"; fi
    if printf '%s\n' $nps | awk 'NR>1 && $1 <= prev {bad=1} {prev=$1} END{exit bad+0}'
    then ok "specs: npar increases along the nesting ($(printf '%s ' $nps))"
    else bad "specs: npar" "$(printf '%s ' $nps)"; fi

    # the free rung on this data is inadmissible, and that must show
    fa=$(awk '$1=="free" && $NF ~ /^(yes|NO)$/ {print $NF}' "$TMP/case.out")
    fw=$(awk '$1=="warma" && $NF ~ /^(yes|NO)$/ {print $NF}' "$TMP/case.out")
    if [ "$fa" = "NO" ] && [ "$fw" = "yes" ]; then
        ok "specs: the admissibility column separates the rungs (warma yes, free NO)"
    else bad "specs: admissibility column" "warma=$fw free=$fa"; fi

    # and no chi2 where the theory does not give one
    if grep -aq "matri   -> free .*no p-value" "$TMP/case.out"; then
        ok "specs: no chi2 p-value for a comparison with an inadmissible rung"
    else bad "specs" "it printed a p-value the theory does not license"; fi
fi

# ... and on Milan, where matri and free pass the rank condition but sit with an
# MA root on the unit circle (BUG-49), the chi2 is withheld for THAT reason.
run data/pairs/milan.inp 2 1 1 -case 2 -mean -specs
if grep -aq "marow   -> matri .*MA root on the unit circle" "$TMP/case.out"; then
    ok "specs: no chi2 p-value across an MA root on the unit circle (BUG-49)"
else bad "specs: MA boundary" "$(grep -a 'marow   -> matri' "$TMP/case.out")"; fi
echo

# 5o. -artest: the AR half of the triangular class (Gamma_i = M_i alpha') against
#     free F, bootstrapped.  Same machinery as -matest with a different pair, so
#     what is checked is that the pair is really the one advertised: the
#     restricted fit must be -warma's and the unrestricted one -mawarma's, which
#     the suite can reproduce on its own.  That is what stops the two tests from
#     silently becoming the same test.
#     Milan and not mink_muskrat: -warma does not estimate there, which is a
#     measured property of that data (HOMOLOGATION.md 4l) and not a defect, but
#     it leaves this check with nothing to compare.
run data/pairs/milan.inp 2 1 1 -case 2 -mean -warma
w0=$(logelf_of "$TMP/case")
run data/pairs/milan.inp 2 1 1 -case 2 -mean -mawarma
w1=$(logelf_of "$TMP/case")
run data/pairs/milan.inp 2 1 1 -case 2 -mean -artest 20
a0=$(awk '/^  restricted     logL/{print $4}' "$TMP/case.out")
a1=$(awk '/^  unrestricted   logL/{print $4}' "$TMP/case.out")
if [ -z "$a0" ] || [ -z "$a1" ]; then
    bad "artest" "no restricted/unrestricted logL reported"
elif awk -v a="$a0" -v b="$w0" -v c="$a1" -v d="$w1" \
       'BEGIN{x=a-b; y=c-d; if(x<0)x=-x; if(y<0)y=-y; exit !(x<=1e-4 && y<=1e-4)}'
then ok "artest: it tests -warma against -mawarma ($a0 vs $a1)"
else bad "artest: it is not testing the pair it says" "H0=$a0 (warma $w0) H1=$a1 (mawarma $w1)"; fi

if grep -aqi "reduced-rank restriction" "$TMP/case.out"; then
    ok "artest: it states the right reason for bootstrapping"
else bad "artest" "it repeated -matest's boundary reason, which does not apply here"; fi
echo

echo "[6] alpha = A*psi: the restriction, its LR, and its guards"
# Johansen and Swensen (2024): H1(r) is alpha = A*psi with A known.  Weak
# exogeneity is the special case where A selects rows, so -weakex is a shorthand
# for -alpha and must give exactly the same numbers -- that is an invariant, so
# it needs no golden value and cannot go stale.

# A declaring equation 2 not to adjust: the same thing -weakex 2 builds.
printf '* A: alpha_2 = 0\n2 1\n1\n0\n' > "$TMP/A_eq2.txt"
lr_of() { grep -a 'LR = 2(free' "$1.out" 2>/dev/null | awk '{print $NF}'; }

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
if printf '%s' "$STDERR" | grep -q 'A does not have rank'; then
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

# 6b. Pi = alpha beta' must be reported, and it is what to compare fits on:
#     unlike alpha and beta it does not move under a reparameterisation of the
#     cointegrating space.  Its rank is r BY CONSTRUCTION, so the M-r zero
#     eigenvalues prove nothing about the rank -- which is why the output says so.
run "$MM" 2 1 1 -case 2
if grep -aq "^Pi = alpha beta'" "$TMP/case.out" && \
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
        /^Sigma = sigma2 \* Q/ {mode="S"; n=0; next}
        /^P matrix/             {mode="P"; n=0; next}
        /^D vector/             {mode="D"; next}
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
nsd=$(awk '/^Parameter +Estimate/{f=1;next} f&&/^Signif/{f=0} f&&NF>=5{print $3}' \
      "$TMP/case.out" | sort -u | wc -l)
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
if grep -aq 'Contemporaneous correlation, largest' "$TMP/case.out" && \
   grep -aq 'Cross dynamics left at k >= 1' "$TMP/case.out"; then
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
if ! grep -aq "^Convergence      :" "$TMP/case.out"; then
    bad "convergence note" "not present in the output"
else
    banner_stopped=$(grep -ac "OPTIMIZER STOPPED" "$TMP/case.out")
    note_notconv=$(grep -ac "^Convergence      : NOT a convergence" "$TMP/case.out")
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
   grep -aq "^Convergence      : NOT a convergence" "$TMP/case.out"; then
    ok "a termcode-3 run is reported as NOT a convergence"
elif grep -aq "OPTIMIZER STOPPED" "$TMP/case.out"; then
    bad "convergence note" "termcode 3 not reported as a non-convergence"
else
    ok "case 3 converged this time; nothing to check"
fi
echo

# ================================= 7 THE RANK TEST AGAINST KNOWN TRUTH ==
# 6c. THE RANK TEST AND ADMISSIBILITY.  -rankadm constrains the r = 1 fit and
#     leaves the r = 0 one alone -- there is no rank condition at r = 0 -- so
#     the statistic can only go DOWN.  That is an invariant of the construction
#     and it cannot go stale; what it guards is that the constraint is being
#     applied where it is claimed and nowhere else.  The measured consequence,
#     which is the reason this matters, is in HOMOLOGATION.md 4n.
run data/pairs/vienna.inp 2 1 0 -case 2 -mean -lrtest
lr_free=$(awk '/^  0 +2 /{print $3}' "$TMP/case.out")
run data/pairs/vienna.inp 2 1 0 -case 2 -mean -lrtest -rankadm 0.2
lr_adm=$(awk '/^  0 +2 /{print $3}' "$TMP/case.out")
if [ -z "$lr_free" ] || [ -z "$lr_adm" ]; then
    bad "lrtest + rankadm" "missing LR (free=$lr_free adm=$lr_adm)"
elif awk -v a="$lr_free" -v b="$lr_adm" 'BEGIN{exit !(b <= a + 1e-6)}'; then
    ok "rank test: constraining the alternative cannot raise the LR ($lr_adm <= $lr_free)"
else bad "rank test: LR rose under the constraint" "free=$lr_free adm=$lr_adm"; fi

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

# ================================ 8 OPERATOR ROOTS AND THE BOUNDARY ==
echo "[8] operator roots, and the invertibility boundary the likelihood enforces"
# WHY THIS BLOCK EXISTS.  Two separate things, both found on 2026-08-19.
#
#  (a) tensor() in nlatools.c allocated (nrh+1) row pointers and then wrote at
#      t[nrl..nrh].  elf() allocates gamwa as tensor(-q+1, 0, ...), so with
#      q >= 2 the lower bound is NEGATIVE and the write landed BEFORE the
#      block: heap corruption and an abort, on every model with two or more MA
#      lags.  The first check below is that regression, and it is worth stating
#      plainly that until it was fixed NO q >= 2 model could be estimated at
#      all -- which silently capped every specification search at q <= 1.
#
#  (b) The roots report itself.  On these data the MA operator is driven onto
#      the invertibility boundary that elf() enforces (chekma rejects a
#      companion eigenvalue of 1.00005 or more), so the reported fit is a
#      CONSTRAINED optimum.  That is what makes the unconstrained Hessian
#      undefined there, and -fdhess must SAY so rather than blame the
#      optimiser.  Checked in both directions: a case that binds and a case
#      that does not.

run "$MM" 2 2 1 -case 2
got=$(logelf_of "$TMP/case")
if [ -n "$TIMEDOUT" ]; then bad "q=2 estimates" "timed out"
elif [ -z "$got" ]; then bad "q=2 estimates (tensor() heap corruption)" "no logelf: $STDERR"
else ok "q=2 estimates, logelf = $got"; fi

# The roots block must be there, and must carry m*p and m*q moduli.
# P4: -mafree, porque con el defecto s*q de las raices MA se van al infinito
# por construccion (Corolario 6.3) y este bloque cuenta moduli finitos.
run "$MM" 2 1 1 -case 2 -mafree
nar=$(grep -a 'AR (Phi)'   "$TMP/case.out" | sed 's/.*AR (Phi)//'   | wc -w)
nma=$(grep -a 'MA (Theta)' "$TMP/case.out" | sed 's/.*MA (Theta)//' | wc -w)
[ "$nar" -eq 4 ] && ok "AR roots: 4 moduli for m=2, p=2" \
                 || bad "AR roots" "expected 4 moduli, got $nar"
[ "$nma" -eq 2 ] && ok "MA roots: 2 moduli for m=2, q=1" \
                 || bad "MA roots" "expected 2 moduli, got $nma"

# The alarm must FIRE where the optimum binds ...
if grep -aq 'A root sits on the unit circle' "$TMP/case.out"; then
    ok "the unit-root alarm fires on the binding fit"
else bad "unit-root alarm" "no alarm on a fit whose MA modulus is 0.99995"; fi

run "$MM" 2 1 1 -case 2 -mafree -fdhess
if printf '%s' "$STDERR" | grep -q 'lies ON the boundary'; then
    ok "-fdhess names the boundary as the cause"
else bad "-fdhess diagnosis" "expected the boundary message, got: $STDERR"; fi

# ... and must be SILENT where it does not.  An alarm with no negative case is
# not an alarm; this is the same discipline as the normalisation check in 6.
if [ -f datasets/synthetic/rank2.inp ]; then
    run datasets/synthetic/rank2.inp 2 0 2 -case 2 -fdhess
    if grep -aq 'A root sits on the unit circle' "$TMP/case.out"; then
        bad "unit-root alarm" "fired on a fit with no root near the circle"
    elif printf '%s' "$STDERR" | grep -q 'lies ON the boundary'; then
        bad "-fdhess" "reported a boundary that is not there"
    elif grep -aq 'finite-difference Hessian at the optimum' "$TMP/case.out"; then
        ok "-fdhess succeeds, and is silent, where nothing binds"
    else bad "-fdhess" "neither succeeded nor explained itself"; fi
fi
echo

# 8c. -marow DOES NOT REACH THE BOUNDARY ON THE BANK, AND THE FREE CLASS DOES.
#     This was criterion P4.3, the check that the -marow default (2026-08-20)
#     did what it was made for.  The default is the free class again since
#     2026-09-23, and the reason given for -marow -- Corollary 6.3, "the
#     engine's gate IS the admissibility condition" -- turned out false in both
#     directions (BUGS.md BUG-48).  What is left is a MEASURED property of the
#     -marow class on these five cases, and it is kept as such: if it stops
#     holding, the documentation that recommends -marow for short samples has
#     to say so.  Both directions, as always: the free class must still produce
#     the boundary on the same data, or the check proves nothing.
echo "[8c] -marow does not reach the invertibility boundary on the bank (P4.3)"
bound_free=0; bound_def=0; rows_ok=0; cases=0
for f in datasets/mauricio/mink_muskrat.inp data/pairs/milan.inp \
         data/pairs/vienna.inp data/pairs/penn.inp data/pairs/utrecht.inp; do
    [ -f "$f" ] || continue
    cases=$((cases+1))

    run "$f" 2 1 1 -case 2 -mean -mafree
    grep -aq 'A root sits on the unit circle' "$TMP/case.out" && bound_free=$((bound_free+1))

    run "$f" 2 1 1 -case 2 -mean -marow
    grep -aq 'A root sits on the unit circle' "$TMP/case.out" && bound_def=$((bound_def+1))
    #  y las filas inferiores de Theta, cero EXACTO -- no cerca de cero
    z=$(awk '/^theta\(1\) matrix/{getline; print $1" "$2}' "$TMP/case.out")   # nabla Y2 row
    case "$z" in
        "0.000000 0.000000") rows_ok=$((rows_ok+1)) ;;
    esac
done
if [ "$cases" -eq 0 ]; then
    ok "P4.3 skipped: no bank case available"
else
    #  NOT asserted any more.  With one start -marow stayed interior in 5 of
    #  5; with the ladder (P12) it finds HIGHER optima on Vienna and Penn, and
    #  those sit on the boundary too.  The claim was an artefact of the single
    #  start, and it is reported, not tested (BUG-48, BUG-49).
    ok "-marow reaches the boundary in $bound_def of $cases bank cases (reported)"
    [ "$bound_free" -gt 0 ] \
        && ok "and -mafree still does, in $bound_free of $cases -- the check bites" \
        || bad "P4.3" "-mafree reached the boundary in none: the check proves nothing"
    [ "$rows_ok" -eq "$cases" ] \
        && ok "the s lower rows of Theta are exactly zero in all $cases" \
        || bad "P4.3" "lower rows not exactly zero in $((cases-rows_ok)) of $cases"
fi

#     And the shape the corollary predicts: r*q finite MA roots and s*q at
#     infinity.  With M=2, r=1, q=1 that is one and one.
run "$MM" 2 1 1 -case 2 -marow
nfin=$(grep -a 'MA (Theta)' "$TMP/case.out" | sed 's/.*MA (Theta)//' | tr ' ' '\n' \
       | grep -c '[0-9]')
ninf=$(grep -a 'MA (Theta)' "$TMP/case.out" | grep -o 'inf' | wc -l)
{ [ "$nfin" -eq 1 ] && [ "$ninf" -eq 1 ]; } \
    && ok "MA roots under -marow: 1 finite (the r x r block) and 1 at infinity" \
    || bad "P4.3 root shape" "finite=$nfin infinite=$ninf, expected 1 and 1"
echo

# ============================================== 8d FORECAST (P5) ==
#  Lo que se comprueba, y por que cada cosa:
#
#   - LA RECURSION, contra los residuos del motor.  La prediccion a un paso de
#     Ybar_t con la informacion hasta t-1 tiene que ser Ybar_t - a_t.  Los a_t
#     los calcula elf por AS 311, no esta rutina, asi que la comprobacion no es
#     circular: ata la recursion, la convencion de la media y los indices.  Con
#     q >= 1 el residuo es la truncacion de xi -- de orden 1e-3 -- y con -m 2,
#     que la apaga, baja a cero de maquina.  Las dos direcciones se comprueban.
#
#   - EL PASO A NIVELES, que es donde vive BUG-10 del programa hermano: alli el
#     nivel se integraba bien en la media y mal en la varianza porque cada una
#     llegaba por su lado.  Aqui la banda a UN PASO tiene que ser la covarianza
#     de la innovacion leida en niveles, y esa se puede obtener por un camino
#     COMPLETAMENTE distinto -- del vector de parametros, que es la Sigma que
#     imprime el .out --, salvo la permutacion entre el orden interno del VEC
#     [Y1 ; Y2] y el del .inp [Y2 ; Y1].  Si el mapa de niveles estuviera mal,
#     esto no cuadraria.
#
#   - Y que la banda no se estreche con el horizonte, que es la forma en que
#     BUG-10 se manifiesta.
echo "[8d] the forecast: the recursion, and the step to levels (P5)"

run "$MM" 2 1 1 -case 2 -f 6
sc=$(sed -n 's/.*one-step self-check.* = \([0-9.e+-]*\) over.*/\1/p' "$TMP/case.out")
if [ -z "$sc" ]; then bad "forecast self-check" "not emitted"
elif awk -v v="$sc" 'BEGIN{exit !(v < 1e-3)}'; then
    ok "one-step recursion agrees with elf's residuals to $sc (xi truncation)"
else bad "forecast self-check" "$sc, which is larger than xitol"; fi

run "$MM" 2 1 1 -case 2 -m 2 -f 6
sc2=$(sed -n 's/.*one-step self-check.* = \([0-9.e+-]*\) over.*/\1/p' "$TMP/case.out")
if awk -v v="$sc2" 'BEGIN{exit !(v < 1e-10)}'; then
    ok "and with the truncation off it is machine zero ($sc2)"
else bad "forecast self-check, -m 2" "$sc2, expected machine zero"; fi

#  La banda a un paso contra la Sigma del informe.  Hasta BUG-18 (2026-09-23)
#  esa Sigma salia en el orden interno con etiquetas del .inp y esta prueba la
#  PERMUTABA para compararla; ahora sale en el orden del .inp y se compara tal
#  cual: la banda de la serie i con Sigma[i][i].
for cfg in "-case 2" "-case 3" "-case 2 -mafree"; do
    run "$MM" 2 1 1 $cfg -f 3
    s11=$(grep -a -A2 '^Sigma = sigma2 \* Q' "$TMP/case.out" | awk 'NR==2{print $1}')
    s22=$(grep -a -A2 '^Sigma = sigma2 \* Q' "$TMP/case.out" | awk 'NR==3{print $2}')
    e1=$(grep -a -A1 '^   h ' "$TMP/case.out" | awk 'NR==2{print $3}')
    e2=$(grep -a -A1 '^   h ' "$TMP/case.out" | awk 'NR==2{print $5}')
    if [ -z "$s11" ] || [ -z "$e1" ]; then
        bad "forecast h=1 variance ($cfg)" "could not read Sigma or the band"
    elif awk -v a="$e1" -v b="$s11" -v c="$e2" -v d="$s22" \
        'BEGIN{exit !(((a*a-b)<1e-5 && (b-a*a)<1e-5) && ((c*c-d)<1e-5 && (d-c*c)<1e-5))}'; then
        ok "h=1 band is the innovation covariance in levels ($cfg)"
    else bad "forecast h=1 variance ($cfg)" "band^2=($e1^2,$e2^2) vs Sigma=($s11,$s22)"; fi
done

#  Y la banda no puede estrecharse: el error de nivel ACUMULA.
run "$MM" 2 1 1 -case 2 -f 8
#  Acotado a la SECCION de prevision: desde 2026-08-24 el .out lleva la
#  diagnosis por serie, cuyas filas de acf/pacf tambien empiezan por un numero.
if sed -n '/^  Forecast$/,$p' "$TMP/case.out" \
   | awk '/^ *[0-9]+ /{if(NF>=5){if(p1!="" && ($3<p1-1e-9 || $5<p2-1e-9)) bad=1; p1=$3; p2=$5}}
        END{exit bad?1:0}'; then
    ok "the bands are non-decreasing in the horizon"
else bad "forecast bands" "a band narrowed as the horizon grew"; fi

#  Con r = 0 no hay bloque W: s = M, todas las series se acumulan, y el paso a
#  niveles es la integracion pura.  Tiene que funcionar -- es el contrafactual
#  univariante de P5.3 (el peldano diagonal, Teorema 9) -- y sus bandas tienen
#  que crecer como las de cualquier I(1).
run "$MM" 2 1 0 -case 2 -f 3
nrow=$(awk '/^ *[0-9]+ +[\-0-9.]+ +[0-9.]+ +[\-0-9.]+ +[0-9.]+$/{c++} END{print c+0}' "$TMP/case.out")
if [ "$nrow" -ge 3 ]; then ok "the forecast works at r = 0 (pure integration, $nrow rows)"
else bad "forecast at r=0" "no forecast table, got $nrow rows"; fi
echo

# 8e. P5.2 — LA EVALUACION DE ORIGEN MOVIL.  Lo que se comprueba es lo unico
#     que se puede comprobar sin un oraculo externo: identidades.
#
#     Con UN SOLO origen, el error medio absoluto y la raiz del error cuadratico
#     medio son el mismo numero -- promedio de un elemento --, asi que MAE = RMSE
#     exactamente.  Es un caso que se calcula a mano y ata el conteo, el
#     emparejamiento de cada prevision con su dato y las dos formulas a la vez.
#     Y el numero de origenes tiene que ser n - H - E + 1.
echo "[8e] rolling-origin evaluation (P5.2)"

nfull=$(awk '/observations used/{print $4}' /dev/null 2>/dev/null; echo "")
#  n = 61 y H = 1, luego E = 60 deja exactamente un origen: n - H - E + 1 = 1.
run "$MM" 2 1 1 -case 2 -f 1 -estwin 60
nor=$(awk '/origins,/{print $1}' "$TMP/case.out")
if [ "$nor" = "1" ]; then ok "one origin when the window leaves room for exactly one"
else bad "rolling: origin count (single)" "got '${nor:-nothing}', expected 1"; fi

#  Solo dentro del bloque de la evaluacion: el .out lleva otras tablas con la
#  misma forma y compararlas seria comparar otra cosa.
bad_pair=$(awk '/Rolling-origin/{inb=1}
           inb && /^ *[0-9]+ +[A-Za-z_]+ +[0-9.eE+-]+ +[0-9.eE+-]+ +[0-9.eE+-]+$/{
             d=$3-$4; if(d<0)d=-d; if(d>1e-9) c++} END{print c+0}' "$TMP/case.out")
if [ "$bad_pair" = "0" ]; then
    ok "with a single origin MAE equals RMSE exactly, as it must"
else bad "rolling: MAE vs RMSE" "$bad_pair rows disagree with one origin"; fi

#  Y el conteo con mas de uno.
run "$MM" 2 1 1 -case 2 -f 4 -estwin 45
nor=$(awk '/origins,/{print $1}' "$TMP/case.out")
if [ "$nor" = "13" ]; then ok "origins = n - H - E + 1 = 13 for E=45, H=4, n=61"
else bad "rolling: origin count (13)" "got '${nor:-nothing}', expected 13"; fi

#  El contrafactual univariante es el peldano diagonal, y tiene que correr.
run "$MM" 2 1 0 -case 2 -diagar -diagma -diagcov -f 4 -estwin 45
if grep -aq 'Rolling-origin' "$TMP/case.out"; then
    ok "the diagonal rung (= an ARIMA per series) is scored the same way"
else bad "rolling at r=0" "no evaluation emitted"; fi

#  Y los rechazos: sin horizonte, y con una ventana que no deja sitio.
run "$MM" 2 1 1 -case 2 -estwin 45
grep -aq 'estwin needs a horizon' "$TMP/case.out" \
    && ok "-estwin without -f says what is missing" \
    || bad "-estwin without -f" "no explanation"
cp "$MM" "$TMP/case.inp"
if "$DRVEC" "$TMP/case" 2 1 1 -case 2 -f 4 -estwin 5 2>&1 | grep -q 'is not inside'; then
    ok "a window outside 10..n-1 is refused"
else bad "-estwin range" "a 5-observation window was accepted"; fi
echo

# 8f. EL REFACTOR DEL .pre, que -interv no aplicaba.
#
#     El modelo de un .pre esta definido sobre w = refactor * BoxCox(z), asi que
#     sus omega estan en las unidades de w y no en las del dato que drvec tiene
#     delante.  Restarlas tal cual solo acierta con refactor = 1, y la norma de
#     la suite es refactor = 100: el .pre que conecta fue con drtran va
#     reescalado para que el optimizador converja (PORTE.md: en el C cuelga mas
#     de dos minutos con refactor = 1 y converge en 23 iteraciones con 100).  La
#     disciplina escrita del conjunto es "never hardcode the rescaling factor;
#     read model.refactor -- the suite has three logged bugs from getting this
#     wrong", y esta era la cuarta.  Medido sobre tres IPC mensuales: la serie
#     "ajustada" salia con una varianza de innovacion 10^4 veces la suya.
#
#     LA COMPROBACION ES UNA INVARIANCIA, que no depende de ningun valor dorado:
#     un .pre con (refactor = 10, omega = 2.5) y otro con (refactor = 1,
#     omega = 0.25) declaran LA MISMA deterministica en las unidades del dato, y
#     tienen que dar el MISMO ajuste.  Con el control negativo al lado, porque
#     una invariancia que se cumple sola no prueba nada: (refactor = 1,
#     omega = 2.5) tiene que dar otro.
#
#     Las fixtures se derivan de mmres.1.pre en vez de versionarse, como el
#     resto de esta bateria.  El bloque determinista del formato lleva DOS
#     lineas de banderas -- una antes de los omega y otra despues --, y omitir
#     la segunda desplaza el lector un renglon sin dar error: la serie se lee
#     corrida y el refactor sale 1.  Es la firma de BUG-11 otra vez.
echo "[8f] the .pre's refactor, applied to its deterministic terms"

mkpre() {   # mkpre <refactor> <omega> <prefix>
    for i in 1 2; do
        awk -v rf="$1" -v om="$2" '
          /^\*\* ACF\/PACF bands/ { print; getline; printf " 0.00 %.2f\n", rf; next }
          /^\*\* Number of deterministic variables/ {
              print; getline
              print "1"; print "**"; print "alter"; print "**"; print "0 "
              print "**"; printf "%.6f  1\n", om; print "**"; print "0 "
              next }
          { print }' tests/fixtures/mmres.1.pre > "$TMP/$3.$i.pre"
    done
}
mkpre 10 2.50  pre_rf10      # resta 2.50/10 = 0.250
mkpre  1 0.25  pre_rf1eq     # resta 0.25/1  = 0.250   -> el mismo ajuste
mkpre  1 2.50  pre_rf1ne     # resta 2.50/1  = 2.500   -> otro ajuste

#  Primero: la fixture tiene que LEERSE bien.  Si el bloque determinista esta
#  mal construido el lector se desplaza y todo lo de abajo mide otra cosa.
if [ -x "$PROBE" ]; then
    got=$("$PROBE" "$TMP/pre_rf10.1.pre" 2>&1)
    ref=$("$PROBE" tests/fixtures/mmres.1.pre 2>&1)
    gs=$(printf '%s' "$got" | cut -d' ' -f4-); rs=$(printf '%s' "$ref" | cut -d' ' -f4-)
    gr=$(printf '%s' "$got" | cut -d' ' -f3)
    if [ "$gs" = "$rs" ] && [ "$gr" = "10" ]; then
        ok "the crafted .pre reads back with refactor 10 and the same series"
    else bad "refactor fixture" "reader got '$got' against '$ref'"; fi
fi

run "$MM" 2 1 1 -case 2 -interv "$TMP/pre_rf10"
ll_a=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -interv "$TMP/pre_rf1eq"
ll_b=$(logelf_of "$TMP/case")
run "$MM" 2 1 1 -case 2 -interv "$TMP/pre_rf1ne"
ll_c=$(logelf_of "$TMP/case")

if [ -z "$ll_a" ] || [ -z "$ll_b" ]; then
    bad "refactor invariance" "one of the fits produced no logelf"
elif [ "$ll_a" = "$ll_b" ]; then
    ok "refactor 10 with omega 2.5 = refactor 1 with omega 0.25 ($ll_a)"
else bad "refactor invariance" "$ll_a vs $ll_b -- the refactor is not being applied"; fi

if [ "$ll_a" != "$ll_c" ]; then
    ok "and refactor 1 with omega 2.5 differs, so the check bites ($ll_c)"
else bad "refactor control" "the negative control gave the same fit"; fi
echo

# 8g. |Sigma| SOBRE LAS CUATRO CONFIGURACIONES EQUIVALENTES.  OPT-IN (SLOW=1).
#
#     Es el criterio 2 de salida de beta, y hasta el 2026-08-20 no tenia
#     regresion: los dorados cubrian solo el arranque unico, de modo que la
#     cifra sobre la que se declaro cerrada la beta podia moverse sin que nada
#     lo dijera -- y una de las cuatro estaba mal transcrita justamente por eso.
#     Ahora el programa imprime |Sigma| el mismo, asi que la comprobacion no
#     depende de que nadie multiplique a mano una matriz de seis decimales.
#
#     Va con -mafree: estas cifras son de la CLASE LIBRE, que era el defecto
#     cuando se midieron.  Y con -multistart 60, que es lo que tarda: fuera del
#     camino por defecto para que `make test` siga siendo de segundos.
#
#         SLOW=1 make test
if [ "${SLOW:-0}" = "1" ]; then
    echo "[8g] |Sigma| on the four equivalent configurations (SLOW)"
    #  El layout antiguo se DERIVA del csv, igual que $MMOLD mas arriba: no
    #  hace falta el .inp pre-diferenciado que se regenero en 2026-08-17.
    sig_of() {   # sig_of <inp> <args...>
        cp "$1" "$TMP/sig.inp"; shift
        timeout 1800 "$DRVEC" "$TMP/sig" 2 1 1 "$@" -mafree -multistart 60 \
            >/dev/null 2>&1
        awk '/\|Sigma\| =/{print $3; exit}' "$TMP/sig.out"
    }
    sig_case() {  # sig_case <label> <expected> <inp> <args...>
        local label=$1 want=$2 inp=$3; shift 3
        local got; got=$(sig_of "$inp" "$@")
        if [ -z "$got" ]; then bad "|Sigma| $label" "not reported"
        elif awk -v a="$got" -v b="$want" 'BEGIN{d=a-b; if(d<0)d=-d; exit !(d<=5e-7)}'
        then ok "|Sigma| $label = $got"
        else bad "|Sigma| $label" "got $got, expected $want"; fi
    }
    sig_case "levels case 2" 0.002372325 "$MM"    -case 2
    sig_case "levels case 3" 0.002347236 "$MM"    -case 3
    sig_case "legacy case 2" 0.002344044 "$MMOLD" -case 2 -differenced
    sig_case "legacy case 3" 0.002358310 "$MMOLD" -case 3 -differenced

    #  Y lo que el criterio dice de verdad: que las cuatro CONCUERDAN.  Un
    #  dorado por separado no lo comprueba -- podrian moverse las cuatro a la
    #  vez --, y la concordancia es la mitad del criterio.
    a=$(sig_of "$MM" -case 2);    b=$(sig_of "$MM" -case 3)
    c=$(sig_of "$MMOLD" -case 2 -differenced)
    d=$(sig_of "$MMOLD" -case 3 -differenced)
    if awk -v w="$a" -v x="$b" -v y="$c" -v z="$d" 'BEGIN{
          mx=w; mn=w
          for (v in a) {}
          if (x>mx) mx=x; if (y>mx) mx=y; if (z>mx) mx=z
          if (x<mn) mn=x; if (y<mn) mn=y; if (z<mn) mn=z
          exit !(mx-mn <= 3.5e-5) }'; then
        ok "the four agree to 3.5e-5 or better"
    else bad "|Sigma| spread" "$a $b $c $d"; fi
    echo
fi

# 8h. BUG-13: LA CHI2 DEL MOTOR, contra GSL.
#
#     chisq() de nlatools.c se documenta como la CDF y con df >= 30 aplicaba DOS
#     correcciones de cola, de modo que para z < 0 devolvia el complemento.  Y
#     z < 0 es el estadistico POR DEBAJO de su media: el caso en que el modelo
#     esta BIEN.  Todo p-valor escrito `1.0 - chisq(...)` salia invertido y la
#     diagnosis declaraba no blancos unos residuos limpios -- Q = 23.4777 con
#     40 g.l. tiene p = 0.9825 y se imprimia 0.0175 con "REJECT H0".  Solo
#     fallaba en los casos buenos, que es lo que lo hacia dificil de ver.
#
#     ESTA COMPROBACION ES ATIPICA EN ESTA BATERIA y por eso se explica: drvec
#     NO llama a chisq -- usa gsl_cdf_chisq_Q, y lo dice en diagnose_mv.c --,
#     asi que un fallo aqui no movería ni un valor dorado.  Pero la funcion es
#     del fichero COMPARTIDO con drvarma y drtran, que si la usan, y este es el
#     unico de los tres programas con bateria automatica.  De modo que aqui se
#     vigila codigo que este programa no ejecuta, a proposito.
#
#     La comprobacion que muerde es la primera: 0.0175 y no 0.9825.
echo "[8h] the engine's chi-square, against GSL (BUG-13)"

CHIP=${CHIP:-bin/chisq_probe}
if [ ! -x "$CHIP" ]; then
    bad "chisq probe" "$CHIP not built -- run make first"
else
    #  1. la cola no puede estar invertida: el primer caso es z < 0, df = 40.
    v=$("$CHIP" | awk 'NR==1{print $3}')
    if awk -v a="$v" 'BEGIN{exit !(a > 0.010 && a < 0.025)}'; then
        ok "chisq(23.4777, 40) = $v, the lower tail (BUG-13 would give ~0.98)"
    else bad "BUG-13" "chisq(23.4777, 40) = $v; expected ~0.0175"; fi

    #  2. y en todo el rango, dentro del error de Wilson-Hilferty.
    worst=$("$CHIP" | awk '{if($5>m) m=$5} END{printf "%.3e", m}')
    if awk -v w="$worst" 'BEGIN{exit !(w < 5e-4)}'; then
        ok "chisq agrees with GSL to $worst over the nine cases"
    else bad "chisq vs GSL" "worst difference $worst"; fi

    #  3. y las dos ramas: con df < 30 es exacta (gammap), no aproximada.
    ex=$("$CHIP" | awk '$2 < 30 {if($5>m) m=$5} END{printf "%.3e", m+0}')
    if awk -v w="$ex" 'BEGIN{exit !(w < 1e-9)}'; then
        ok "and with df < 30 it is exact ($ex), which is the gammap branch"
    else bad "chisq df<30" "difference $ex, expected machine zero"; fi
fi
echo

# 8i. P6.8 — EL BLOQUE DE HIPOTESIS.  Lo que se comprueba es una IDENTIDAD y no
#     un valor: con un solo parametro, el Wald ES el cuadrado del cociente t que
#     el propio .out imprime al lado del coeficiente.  Si el mapa de indices que
#     recoge la impresora se desalineara -- que es el fallo de 4.1, y por eso
#     los indices se anotan en el recorrido que ya hay y no en uno nuevo --,
#     esta igualdad se rompe inmediatamente.
echo "[8i] the hypotheses about the relations (P6.8)"

run "$MM" 2 1 1 -case 2 -fdhess
if ! grep -aq 'Joint Hypothesis Tests (Wald)' "$TMP/case.out"; then
    bad "hypothesis block" "not emitted"
else
    ok "the block is emitted by default, with no option asked for"

    #  alpha: dos filas, cada una un solo coeficiente -> chi2 = (coef/sd)^2.
    #  El signo da igual: el Wald es el CUADRADO del cociente t.
    lam=$(sed -n '/^Adjustment coefficients, alpha/,/^$/p' "$TMP/case.out" \
          | grep -aE '<- ec' | awk '{print $4, $5}')
    wal=$(sed -n '/Weak exogeneity, one variable/,/Exclusion from/p' "$TMP/case.out" \
          | awk '/Wald chi2/{gsub(",","",$4); print $4}')
    n=0; worst=0
    while read -r c sd; do
        n=$((n+1))
        w=$(echo "$wal" | sed -n "${n}p")
        [ -z "$w" ] && continue
        d=$(awk -v c="$c" -v s="$sd" -v w="$w" 'BEGIN{t=(c/s)^2; d=t-w; if(d<0)d=-d; print d}')
        worst=$(awk -v a="$worst" -v b="$d" 'BEGIN{print (b>a)?b:a}')
    done <<< "$lam"
    if [ "$n" -ge 2 ] && awk -v w="$worst" 'BEGIN{exit !(w < 1e-3)}'; then
        ok "weak-exogeneity Wald = (coef/sd)^2 on all $n rows of alpha (max diff $worst)"
    else bad "weak exogeneity vs t^2" "$n rows, worst difference $worst"; fi

    #  B2: lo mismo, y ademas ata el OTRO extremo del vector de parametros.
    b2=$(sed -n '/^Cointegrating vectors, beta =/,/^$/p' "$TMP/case.out" \
         | grep -aE ' in ec' | awk '{print $4, $5}')
    wb=$(sed -n '/Exclusion from the cointegrating/,/Short-run dynamics/p' "$TMP/case.out" \
         | awk '/Wald chi2/{gsub(",","",$4); print $4; exit}')
    d=$(echo "$b2" | awk -v w="$wb" '{t=($1/$2)^2; d=t-w; if(d<0)d=-d; print d}')
    if [ -n "$wb" ] && awk -v d="$d" 'BEGIN{exit !(d < 1e-3)}'; then
        ok "exclusion Wald on beta_2 = (coef/sd)^2 too (diff $d), so both ends of x[] are aligned"
    else bad "B2 exclusion vs t^2" "difference $d"; fi

    #  Lambda = 0 se imprime, y se imprime DICIENDO que no es un contraste.
    if grep -aq 'NOT A TEST' "$TMP/case.out"; then
        ok "Lambda = 0 is reported and labelled as not being a test (Davies)"
    else bad "Lambda = 0" "reported without the boundary warning"; fi

    #  Y el aviso de -fdhess NO sale cuando se pidio -fdhess.
    if grep -aq 'use -fdhess before quoting' "$TMP/case.out"; then
        bad "-fdhess reminder" "printed even though -fdhess was given"
    else ok "the -fdhess reminder is absent when -fdhess was used"; fi
fi

#  Sin -fdhess el aviso SI tiene que salir: un p-valor que sale del factor
#  acumulado por el BFGS no es el que se publica.
run "$MM" 2 1 1 -case 2
if grep -aq 'use -fdhess before quoting' "$TMP/case.out"; then
    ok "and it is present when the covariance came from the BFGS factor"
else bad "-fdhess reminder" "missing on the default run"; fi

#  Con r = 0 no hay Lambda ni B: el bloque tiene que decirlo, no inventarlo.
run "$MM" 2 1 0 -case 2
if grep -aq 'r = 0: no error-correction term' "$TMP/case.out" \
   && ! grep -aq 'Weak exogeneity' "$TMP/case.out"; then
    ok "at r = 0 the block says the three VEC hypotheses do not exist"
else bad "hypothesis block at r=0" "emitted weak exogeneity with no Lambda"; fi

#  Con -fixb2 B2 esta impuesta: no hay covarianza con que contrastarla, y el
#  bloque tiene que decir eso en vez de contrastar un parametro que no existe.
run "$MM" 2 1 1 -case 2 -fixb2 0
if grep -aq 'beta_2 is held fixed (-fixb2)' "$TMP/case.out"; then
    ok "with -fixb2 the exclusion test is declared unavailable, not faked"
else bad "hypothesis block with -fixb2" "did not declare B2 as imposed"; fi
echo

# 8j. P6.7 — EL SISTEMA DE FICHEROS DE SALIDA.  El conjunto nombra cada producto
#     por el mismo prefijo (drvarma v.04.1 drvarma.c:627 y 688), y hasta la 0.9
#     drvec metia la prevision dentro del informe de la ESTIMACION y dejaba los
#     errores origen a origen sin escribir salvo que el usuario nombrara la ruta.
echo "[8j] the output file system: <base>.forecast and <base>.recursive (P6.7)"

rm -f "$TMP/case.forecast" "$TMP/case.recursive"
run "$MM" 2 1 1 -case 2 -f 5
if [ -f "$TMP/case.forecast" ]; then
    ok "-f writes <base>.forecast without being told where"
    nrow=$(awk '/^  [0-9]/{c++} END{print c+0}' "$TMP/case.forecast")
    if [ "$nrow" -eq 10 ]; then
        ok "it carries H x M = 5 x 2 = 10 dated rows"
    else bad ".forecast rows" "got $nrow, expected 10"; fi
    #  La fecha: anual, y tiene que continuar donde acaba el .inp.
    d1=$(awk '/^  [0-9]/{print $1; exit}' "$TMP/case.forecast")
    hdr=$(awk '!/^\*/{n++; if(n==2){print $2, $4; exit}}' "$TMP/case.inp")
    want=$(echo "$hdr" | awk '{print $2 + $1}')
    if [ -n "$d1" ] && [ "$d1" = "$want" ]; then
        ok "the first forecast is dated $d1, which continues the .inp exactly"
    else bad ".forecast dating" "got $d1, expected $want"; fi
    #  Y la banda tiene que ser el nivel +/- z s.e., que es lo unico que ata
    #  las tres columnas entre si.
    if awk '/^  [0-9]/{lo=$3; hi=$4; se=$5; lv=$2;
              a=lv-1.959964*se-lo; b=hi-(lv+1.959964*se);
              if(a<0)a=-a; if(b<0)b=-b; if(a>1e-5||b>1e-5) bad=1}
            END{exit bad?1:0}' "$TMP/case.forecast"; then
        ok "Low/High are the level +/- 1.96 s.e. on every row"
    else bad ".forecast bands" "a row's band is not the level +/- 1.96 s.e."; fi
else bad ".forecast" "not written"; fi

run "$MM" 2 1 1 -case 2 -f 3 -estwin 40
if [ -f "$TMP/case.recursive" ]; then
    ok "-estwin writes <base>.recursive without -C"
    hdr=$(grep -ac '^origin,h,series,actual,forecast,error$' "$TMP/case.recursive")
    nrow=$(grep -avc '^#\|^origin' "$TMP/case.recursive")
    if [ "$hdr" -eq 1 ] && [ "$nrow" -gt 0 ]; then
        ok "with its provenance header and $nrow per-origin rows"
    else bad ".recursive contents" "header $hdr, rows $nrow"; fi
else bad ".recursive" "not written"; fi

#  -C sigue siendo una redireccion, no una condicion.
rm -f "$TMP/case.recursive" "$TMP/elsewhere.csv"
run "$MM" 2 1 1 -case 2 -f 3 -estwin 40 -C "$TMP/elsewhere.csv"
if [ -f "$TMP/elsewhere.csv" ] && [ ! -f "$TMP/case.recursive" ]; then
    ok "-C redirects it instead of adding a second file"
else bad "-C" "did not redirect the per-origin file"; fi
echo

# 8k. P7 — THE LANGUAGE OF THE SOURCE.  drvec.c was written in a mix of Spanish
#     and English: the comments carried the reasoning, the documentation was in
#     English, and docs/README.md quoted output the program did not produce.  A
#     reader who cannot read the comments cannot audit the reasoning.  Fixed on
#     2026-08-22 by translating the file; kept fixed here, because a file does
#     not become mixed again in one commit -- it does so one line at a time.
#
#     ONLY drvec.c.  nlatools.c, fue_pre_reader.c, fue_bridge.c and
#     diagnose_mv.c are the suite's, copied from drvarma and drtran, and
#     translating them here would be exactly the divergence P3 exists to stop:
#     their language is their owner's business.
echo "[8k] the language of drvec.c (P7)"

if ! command -v python3 >/dev/null 2>&1; then
    ok "skipped: python3 not available"
elif python3 tools/check_language.py src/drvec.c > "$TMP/lang.txt" 2>&1; then
    ok "no Spanish left in the comments or the messages of src/drvec.c"
else
    bad "language" "$(head -3 "$TMP/lang.txt" | tr '\n' ' ')"
fi

#  And the check itself has to be able to fail, or it is decoration.
cat > "$TMP/lang_probe.c" <<'PROBE'
/*  Esto es un comentario en castellano que la comprobacion tiene que ver,
 *  porque si no la ve entonces no sirve para nada y la bateria miente.      */
int main(void) { return 0; }
PROBE
if python3 tools/check_language.py "$TMP/lang_probe.c" >/dev/null 2>&1; then
    bad "language check" "it passed a file that is Spanish from end to end"
else
    ok "and the check fires on a file that is Spanish, so it is not decoration"
fi
echo

# 8l. P6 — THE TWO REGISTERS THAT ARE KEPT BY HAND, CHECKED.
#
#     The version number has ONE definition (the #define) and five copies that
#     are written by hand; the defect register has one sequence of numbers
#     shared with three other programs.  Both are exactly the kind of thing that
#     rots quietly: a release whose CITATION.cff still says the previous number
#     cannot be cited correctly, and a BUG-N referenced in the code and
#     registered nowhere identifies nothing.  See docs/VERSIONS.md 2 and
#     docs/BUGS.md.
echo "[8l] the version number and the defect register (P6)"

if ./tools/check_version.sh > "$TMP/ver.txt" 2>&1; then
    ok "the version agrees in the source, CITATION.cff, CHANGELOG, VERSIONS and the binary"
else
    bad "version consistency" "$(grep MISMATCH "$TMP/ver.txt" | head -2 | tr '\n' ' ')"
fi

if ! command -v python3 >/dev/null 2>&1; then
    ok "defect register: skipped, no python3"
elif python3 tools/check_bugs.py > "$TMP/bugs.txt" 2>&1; then
    ok "$(cat "$TMP/bugs.txt")"
else
    bad "defect register" "$(head -2 "$TMP/bugs.txt" | tr '\n' ' ')"
fi

#  And both checks have to be able to fail.
cp CITATION.cff "$TMP/cff.bak"
sed -i 's/^version: .*/version: "9.9"/' CITATION.cff
if ./tools/check_version.sh >/dev/null 2>&1; then
    bad "version check" "it passed a CITATION.cff carrying another version"
else
    ok "and the version check fires when one copy disagrees"
fi
cp "$TMP/cff.bak" CITATION.cff

if command -v python3 >/dev/null 2>&1; then
    sed 's/^\*\*What it cost\.\*\*/**Cost.**/' docs/BUGS.md > "$TMP/bugs_probe.md"
    if python3 tools/check_bugs.py "$TMP/bugs_probe.md" >/dev/null 2>&1; then
        bad "defect register check" "it passed entries that do not say what they cost"
    else
        ok "and the register check fires on an entry with no measured cost"
    fi
fi
echo

# 8m. P9 — THE .pre INPUT ROUTE.  drvec is on drtran's side of the suite: the
#     univariate work is done in fue and arrives here already done.  drtran's
#     interface says so -- one .pre per series -- and until 2026-08-22 drvec's
#     did not: the series had to be exported to an .inp by hand, then -interv to
#     subtract the deterministic terms.
#
#     WHAT IS CHECKED IS AN IDENTITY, and it is the only thing worth checking:
#     the new route must give EXACTLY the fit the old one gives on the same
#     data.  A route that is merely "close" is a second implementation.
echo "[8m] the .pre input route (P9)"

MUS=tests/fixtures/mmpre.muskrat.pre
MNK=tests/fixtures/mmpre.mink.pre

#  1. The identity, with no deterministic terms in play.
cp datasets/mauricio/mink_muskrat.inp "$TMP/pinp.inp"
"$DRVEC" "$TMP/pinp" 2 1 1 -mean -case 2 >/dev/null 2>&1
"$DRVEC" "$MUS" "$MNK" 2 1 1 -mean -case 2 -name "$TMP/proute" >/dev/null 2>&1
a=$(logelf_of "$TMP/pinp"); b=$(logelf_of "$TMP/proute")
if [ -z "$a" ] || [ -z "$b" ]; then
    bad ".pre route" "one of the two fits did not produce a logelf ($a / $b)"
elif [ "$a" = "$b" ]; then
    ok ".pre route reproduces the .inp route exactly (logelf $a)"
else bad ".pre route identity" ".inp gives $a and .pre gives $b"; fi

#  And not just the likelihood: everything from the parameters on.
if diff <(sed -n '/ESTIMATION SUCCESSFUL/,$p' "$TMP/pinp.out") \
        <(sed -n '/ESTIMATION SUCCESSFUL/,$p' "$TMP/proute.out") >/dev/null 2>&1; then
    ok "and the whole report after ESTIMATION SUCCESSFUL is byte-identical"
else bad ".pre route identity" "the reports differ below ESTIMATION SUCCESSFUL"; fi

#  2. The deterministic terms.  The .pre route subtracts them itself; the .inp
#     route needs -interv.  Both go through build_det_component, so the two must
#     agree -- and this is what says the units are right, which is what BUG-15
#     was about.
cp "$MUS" "$TMP/iv.1.pre"; cp tests/fixtures/mmpre.step.pre "$TMP/iv.2.pre"
cp datasets/mauricio/mink_muskrat.inp "$TMP/iv.inp"
"$DRVEC" "$TMP/iv" 2 1 1 -mean -case 2 -interv "$TMP/iv" >/dev/null 2>&1
"$DRVEC" "$MUS" tests/fixtures/mmpre.step.pre 2 1 1 -mean -case 2 \
         -name "$TMP/pstep" >/dev/null 2>&1
a=$(logelf_of "$TMP/iv"); b=$(logelf_of "$TMP/pstep")
if [ -n "$a" ] && [ "$a" = "$b" ]; then
    ok "the deterministic terms land in the same place as -interv puts them ($a)"
else bad ".pre deterministics" "-interv gives $a and the .pre route gives $b"; fi

#  And they are actually being subtracted: a step of 5 cannot leave the fit
#  where it was.
c=$(logelf_of "$TMP/proute")
if [ "$b" != "$c" ]; then
    ok "and a step of 5 does move the fit ($c -> $b), so it is not a no-op"
else bad ".pre deterministics" "the fit did not move at all"; fi

#  3. The products are named after the model, and the default name is built
#     from the files -- which is drtran's <output>_<input> for M series.
rm -f "$TMP/proute.forecast"
"$DRVEC" "$MUS" "$MNK" 2 1 1 -mean -case 2 -f 4 -name "$TMP/proute" >/dev/null 2>&1
if [ -f "$TMP/proute.out" ] && [ -f "$TMP/proute.forecast" ]; then
    ok "-name governs every product: .out and .forecast"
else bad ".pre route naming" "the products are not named after -name"; fi

nm=$("$DRVEC" "$MUS" "$MNK" 2 1 1 -mean -case 2 2>/dev/null \
     | awk '/^Output/{print $3}')
rm -f mmpre.muskrat_mmpre.mink.out
if [ "$nm" = "mmpre.muskrat_mmpre.mink.out" ]; then
    ok "and with no -name it is the file stems joined: $nm"
else bad ".pre default name" "got '$nm'"; fi

#  4. The files are lined up by DATE.  One of them starts three years later, so
#     the common sample has to be 59 observations from 1853 and not 62 from
#     wherever the arrays happen to begin.
out=$("$DRVEC" "$MUS" tests/fixtures/mmpre.late.pre 2 1 1 -mean -case 2 \
      -name "$TMP/plate" 2>/dev/null | grep 'Common sample')
if printf '%s' "$out" | grep -q '59 observations, 1/1853'; then
    ok "the common sample is the intersection of the calendars: $out"
else bad ".pre date alignment" "$out"; fi

#  5. What it refuses, and it exits 1 each time.
for probe in "one file:$MUS" \
             "mixed frequencies:$MUS tests/fixtures/mmpre.monthly.pre" ; do
    lbl=${probe%%:*}; args=${probe#*:}
    "$DRVEC" $args 2 1 1 >/dev/null 2>&1
    if [ $? -eq 1 ]; then ok "refused, with exit 1: $lbl"
    else bad ".pre guard" "$lbl was not refused with exit 1"; fi
done

for opt in "-interv $TMP/iv" "-differenced"; do
    "$DRVEC" "$MUS" "$MNK" 2 1 1 $opt >/dev/null 2>&1
    if [ $? -eq 1 ]; then ok "refused, with exit 1: ${opt%% *} on the .pre route"
    else bad ".pre guard" "${opt%% *} was accepted on the .pre route"; fi
done
echo

# 8n. P8 — THE GOLDEN SET.  A refactor claims to move NOTHING, and that claim is
#     stronger than the ones the rest of this suite checks, so it needs a
#     stronger check: every byte of every report, over the configurations that
#     reach each mode.  tools/golden.sh stores the hashes; this runs them.
#
#     It is not a substitute for anything above it.  An invariant says what must
#     be true of any correct version; a golden hash says only that today's
#     output equals yesterday's, and it is worth exactly as much as the day it
#     was captured.  What it is for is a change whose whole point is that
#     nothing moves -- and for that it is the only honest net.
echo "[8n] the golden set: 24 reports, byte for byte (P8)"

if ./tools/golden.sh verify > "$TMP/golden.txt" 2>&1; then
    ok "$(cat "$TMP/golden.txt")"
else
    bad "golden set" "$(head -4 "$TMP/golden.txt" | tr '\n' ' ')"
fi
echo

# 8o. P11 — THE LEVEL IMPULSE RESPONSES, AND THE IDENTITY THAT TIES THEM TO THE
#     FORECAST.  Both blocks are built from the same map -- level_error_map(),
#     the single source of truth of docs/FORECAST.md 4 -- so they cannot be
#     independently wrong: with R_k = G_k P D^(1/2), the h-step forecast
#     variance is sum_{k<h} R_k R_k', hence
#
#         s.e.(h) = sqrt( sum_{k<h} sum_j R_k[i][j]^2 )
#
#     and that is checked against the forecast table of the SAME run.  If the
#     orthogonalisation, the map or the accumulation were wrong, this breaks.
echo "[8o] the level impulse responses (P11)"

run "$MM" 2 1 1 -case 2 -f 4
if ! grep -aq 'Impulse Response of the Levels' "$TMP/case.out"; then
    bad "impulse responses" "not emitted"
else
    ok "the level impulse responses are reported by default"

    #  s.e.(h) de la tabla de prevision, contra la suma de respuestas al
    #  cuadrado hasta k = h-1.  Se comprueba en las dos series y a h = 1..4.
    worst=$(awk '
        /^Shock to /   {sh++; k=-1; next}
        sh && /^ *[0-9]+ / && NF>=3 {k=$1; R[sh","k","1]=$2; R[sh","k","2]=$3;
                                     if (k>kmax) kmax=k; next}
        /^   h  / {mode="F"; next}
        mode=="F" && /^ *[0-9]+ / && NF>=5 {h=$1; SE[h","1]=$3; SE[h","2]=$5;
                                            if (h>hmax) hmax=h}
        END{
            worst=0
            for (h=1; h<=hmax; h++)
                for (i=1; i<=2; i++) {
                    acc=0
                    for (k=0; k<h; k++)
                        for (j=1; j<=2; j++) acc += R[j","k","i]*R[j","k","i]
                    d = sqrt(acc) - SE[h","i]; if (d<0) d=-d
                    if (d>worst) worst=d
                }
            printf "%.9f", worst
        }' "$TMP/case.out")
    if [ -n "$worst" ] && awk -v w="$worst" 'BEGIN{exit !(w < 1e-6)}'; then
        ok "the forecast s.e. IS the root of the summed squared responses (worst $worst)"
    else
        bad "IRF vs forecast s.e." "they disagree by $worst"
    fi

    #  Y la descomposicion tiene que sumar 100 en cada fila: es una particion.
    if awk '/Forecast Error Variance Decomposition/{f=1}
            f && /^ *[0-9]+ +[0-9.]+% /{
                t=0; for(j=2;j<=NF;j++){v=$j; sub("%","",v); t+=v}
                if (t < 99.9 || t > 100.1) bad=1 }
            END{exit bad?1:0}' "$TMP/case.out"; then
        ok "every row of the decomposition adds to 100%"
    else
        bad "FEVD" "a row does not add to 100%"
    fi
fi
echo

# ============================================================= 8p THE SEARCH ==
#  P12 (2026-09-23).  Every fit is the best of several starts: the caller's seed,
#  the cold one, Johansen's, the NESTED CHAIN (q = 0, -marow, -matri, free, each
#  from the optimum of the one below), and -multistart's perturbations.  These
#  are the properties that make it worth having, each written so it can fail:
#  all four held false on 2026-09-23 before the search existed (BUG-25, 33, 35).
echo "[8p] the search (P12)"

# 8p.1  Nested classes come out nested: logL(marow) <= logL(matri) <= logL(free).
#       On PLL the single cold start gave -6.97, -8.85, -8.65: the richer class
#       BELOW the poorer one it contains, which cannot happen at a maximum.
for d in PLL VILL; do
    lls=""
    for c in -marow -matri -mafree; do
        run "data/$d.inp" 2 1 1 -case 2 $c
        lls="$lls $(logelf_of "$TMP/case")"
    done
    if printf '%s\n' $lls | awk 'NR==1{p=$1;next} {if($1<p-1e-8){exit 1} p=$1} END{exit (NR==3)?0:1}'; then
        ok "$d: marow <= matri <= free ($lls )"
    else
        bad "$d: nested classes out of order" "marow matri free =$lls"
    fi
done

# 8p.2  The UKconsumption rank sequence: 70.13 then 25.51.  From one cold start
#       the r = 1 fit stopped 22 log-units short and the two LRs swapped rows
#       (BUG-25) -- and the register marked the swapped pair as reproduced.
run "$UK" 2 0 1 -case 2 -lrtest
lr0=$(sed -n '/M-r        LR/,/^$/p' "$TMP/case.out" | awk '$1=="0"{print $3; exit}')
lr1=$(sed -n '/M-r        LR/,/^$/p' "$TMP/case.out" | awk '$1=="1"{print $3; exit}')
if [ -n "$lr0" ] && [ -n "$lr1" ] && \
   awk -v a="$lr0" -v b="$lr1" 'BEGIN{exit !((a-70.1280)^2 < 1e-6 && (b-25.5141)^2 < 1e-6)}'; then
    ok "UKconsumption -lrtest: LR(0) = $lr0, LR(1) = $lr1, in that order"
else
    bad "UKconsumption -lrtest" "LR(0) = $lr0, LR(1) = $lr1 (expected 70.1280, 25.5141)"
fi

# 8p.3  -fixb2 v does not depend on which column is demeaned: under case 2 the
#       two data sets have the same likelihood function (BUG-33: -275.36 vs
#       491.51 before the seed was built from W = Y1 + v'Y2).
awk -F, 'NR>1{printf "%.10f %.10f\n",$2,$1}' datasets/urca_UKconinc.csv > "$TMP/ucbody"
nuc=$(wc -l < "$TMP/ucbody"); muc=$(awk '{s+=$1}END{printf "%.12f", s/NR}' "$TMP/ucbody")
printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" "$nuc" > "$TMP/uc.inp"; cat "$TMP/ucbody" >> "$TMP/uc.inp"
printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" "$nuc" > "$TMP/ucd.inp"
awk -v m="$muc" '{printf "%.10f %.10f\n",$1-m,$2}' "$TMP/ucbody" >> "$TMP/ucd.inp"
run "$TMP/uc.inp"  2 0 1 -case 2 -fixb2 0; l_u=$(logelf_of "$TMP/case")
run "$TMP/ucd.inp" 2 0 1 -case 2 -fixb2 0; l_d=$(logelf_of "$TMP/case")
if [ -n "$l_u" ] && near "$l_u" "$l_d"; then
    ok "-fixb2 0 is invariant to demeaning Y2 under case 2 ($l_u)"
else
    bad "-fixb2 0 depends on the start" "raw $l_u, demeaned $l_d"
fi

# 8p.4  -artest runs on the canonical case (BUG-35: its restricted -warma fit
#       skipped the admissibility ladder and the test never ran).
run "$MM" 2 1 1 -case 2 -artest 10
if grep -aq "LR = 2\*\[L(H1) - L(H0)\]" "$TMP/case.out" && \
   ! grep -aq "one of the two fits failed" "$TMP/case.out"; then
    ok "-artest runs on mink-muskrat"
else
    bad "-artest" "the restricted or the unrestricted fit failed"
fi

# 8p.5  A start the engine refuses is recorded as failed, not as converged with
#       logL 0 (the deallocating cast used to overwrite est()'s ifault).
run "$UK" 2 0 2 -case 2
if grep -aq "^  johansen *(failed)" "$TMP/case.out"; then
    ok "an inadmissible start is reported as failed"
elif grep -aq "^  johansen " "$TMP/case.out"; then
    ok "the Johansen start was admissible here"
else
    bad "search table" "no Johansen row in the search table"
fi

# 8p.7  BUG-49: an MA root on the invertibility boundary is diagnosed, not just
#       starred -- the report says which combination of Ybar carries it, and
#       -lrtest marks the LR that uses such a fit.  mink-muskrat's free fit sits
#       there (modulus 0.99995).
run "$MM" 2 1 1 -case 2
if grep -aq "left$" "$TMP/case.out" || grep -aq "null vector u of Theta\*(1)" "$TMP/case.out"; then
    ok "BUG-49: the boundary MA root is diagnosed with its direction"
else bad "BUG-49" "the boundary MA root is starred but not diagnosed"; fi
run "$MM" 2 1 0 -case 2 -lrtest
if grep -aq "MA on the boundary at rank 1" "$TMP/case.out"; then
    ok "BUG-49: -lrtest marks the LR built on a boundary fit"
else bad "BUG-49" "-lrtest does not mark the boundary fit"; fi

# 8p.6  -lrtest honours -multistart.  OPT-IN (SLOW=1): it is 30 starts per rank.
if [ "${SLOW:-0}" = "1" ]; then
    run "$UK" 2 0 1 -case 2 -lrtest -multistart 30
    l2=$(sed -n '/r    npar        logL/,/^$/p' "$TMP/case.out" | awk '$1=="2"{print $3}')
    if [ -n "$l2" ] && awk -v a="$l2" 'BEGIN{exit !(a > 573.9)}'; then
        ok "-lrtest -multistart 30 reaches the r = 2 optimum ($l2)"
    else
        bad "-lrtest ignores -multistart" "r = 2 logL $l2 (the 30-start optimum is 573.96)"
    fi
fi
echo

# ============================================================ 8q THE LABELS ==
#  BUG-17, 18, 19, 23: drvec carries two row orders, the internal [Y1 ; Y2] of
#  the VEC parameters and the .inp's [Y2 ; Y1], and the report mixed them in
#  block after block.  255 checks passed with every one of those defects in,
#  because none asked WHICH SERIES a label names.  These do, on a DGP whose
#  answer is known: tests/repro/fixtures/sim2.inp, x = column 1 (Y2), y =
#  column 2 (Y1), nabla x = 0.4 nabla y(-1) + e1, var e1 = 1, var e2 = 4.
echo "[8q] the labels name the right series (BUG-17 family)"
run tests/repro/fixtures/sim2.inp 2 0 1 -case 1
g=$(grep -a 'D.xY2 <- D.yY1(-1)' "$TMP/case.out" | awk '{print $4}')
if [ -n "$g" ] && awk -v v="$g" 'BEGIN{exit !(v > 0.35 && v < 0.45)}'; then
    ok "Gamma: D.x <- D.y(-1) = $g (truth 0.4)"
else bad "Gamma labels" "D.x <- D.y(-1) = $g, truth 0.4"; fi
if grep -aq "REJECT H0 -> D.xY2 is driven by the others" "$TMP/case.out" && \
   grep -aq "REJECT H0 -> D.yY1 drives the others" "$TMP/case.out"; then
    ok "Wald: y drives x, as in the DGP"
else bad "Wald labels" "the short-run tests do not say that y drives x"; fi
vx=$(sed -n '/^Sigma = sigma2 \* Q/,+1p' "$TMP/case.out" | awk 'NR==2{print $1}')
vy=$(sed -n '/^Sigma = sigma2 \* Q/,+2p' "$TMP/case.out" | awk 'NR==3{print $2}')
if [ -n "$vx" ] && awk -v a="$vx" -v b="$vy" 'BEGIN{exit !(a > 0.8 && a < 1.2 && b > 3.5 && b < 4.5)}'; then
    ok "Sigma in the .inp's order: var x = $vx, var y = $vy (truth 1, 4)"
else bad "Sigma labels" "var x = $vx, var y = $vy, truth 1 and 4"; fi
if grep -aq "Residual series a\[2\] (ec1)" "$TMP/case.out"; then
    ok "the W block's residual is called ec1, not y"
else bad "residual labels" "the W innovation is labelled with a series name"; fi
echo

# 0b. THE BUILD ITSELF, FROM CLEAN.  Opt-in with CLEANBUILD=1, and it exists
#     because of a real failure: on 2026-08-24 `ObsToDate` moved out of
#     fue_bridge.c into the vendored diagnose.c, which left tests/pre_probe.c
#     without the symbol.  Nothing here noticed, because bin/pre_probe was
#     already built and `make` had no reason to relink it.  The CI, starting
#     from an empty tree, failed on the first push.
#
#     A suite that runs against whatever binaries happen to be lying around is
#     testing the last build, not this one.  Off by default because a full
#     rebuild is slow; on in CI, which is where a clean tree is free.
if [ "${CLEANBUILD:-0}" = "1" ]; then
    echo "[0b] the build, from clean"
    if make distclean >/dev/null 2>&1 && make >/dev/null 2>&1 \
       && make bin/pre_probe bin/chisq_probe >/dev/null 2>&1; then
        ok "everything links from an empty build directory"
    else
        bad "clean build" "something does not link; see make output"
    fi
    echo
fi

# 8r. THE RANK TEST'S REFERENCE (BUG-24, BUG-26).  Case 1 has no deterministic
#     term, so its table is MacKinnon-Haug-Michelis' no-constant one (11.22 at
#     5 % for M-r = 2), not urca's ecdet="none" (14.90), which still fits an
#     intercept.  And a negative exact LR -- the fixture is a true optimum at
#     both ranks, under H0 -- is a statistic, not a failed fit.
echo "[8r] the rank test's reference (BUG-24, BUG-26)"

run tests/repro/fixtures/neg.inp 1 0 1 -case 1 -lrtest
row=$(awk '/^  r    M-r/{t=1; next} t && /^ +0 +2 /{print; exit}' "$TMP/case.out")
cv5=$(echo "$row" | awk '{print $5}')
[ "$cv5" = "11.22" ] && ok "case 1 uses the no-constant table (5 % = 11.22 at M-r = 2)" \
                     || bad "case-1 table" "5 % value $cv5, expected 11.22"
if echo "$row" | grep -q "H0 not rejected" && ! grep -q "did not converge" "$TMP/case.out"; then
    ok "a negative exact LR reads as not rejected, not as a failed fit"
else
    bad "negative LR" "$row"
fi
echo

# 8s. -m AND THE ENTRY GATE (BUG-47).  -m 2 only switches the xi truncation off:
#     both methods are the EXACT likelihood, and the report must say so.  And
#     the gate certifies with both sides untruncated, so a correct independent
#     pair with theta = 0.9 / 0.95 -- which failed by 1.8e-3 against a tolerance
#     of xitol = 1e-3 -- passes at 1e-6 under either method.
echo "[8s] -m and the entry gate (BUG-47)"
for m in 1 2; do
    run tests/repro/fixtures/gate_hi.inp 1 1 0 -diagar -diagma -diagcov -m $m
    if grep -aq "crossing identity.*VERIFIED" "$TMP/case.out" &&
       ! grep -aq "NOT VERIFIED" "$TMP/case.out"; then
        ok "gate: a correct pair with theta near 1 verifies under -m $m"
    else bad "gate -m $m" "$(grep -a 'crossing identity' "$TMP/case.out")"; fi
done
if grep -aq "^Estimation .*Exact.*NOT truncated" "$TMP/case.out" &&
   ! grep -aq "Conditional (Approximate)" "$TMP/case.out"; then
    ok "-m 2 is labelled as what it is: exact ML, xi not truncated"
else bad "-m 2 label" "$(grep -a '^Estimation' "$TMP/case.out")"; fi
echo

# 8t. OPTION COMBINATIONS THAT USED TO CRASH OR LIE (BUG-30, BUG-31).  Two are
#     refused -- -warma does not impose alpha = A psi; -matest simulates levels,
#     which -differenced does not carry -- and one is fixed: -lrtest -fixb2
#     -bootstrap with M = 3 rebuilds the H0 rank's fixed B2 (it crashed, 139).
echo "[8t] option combinations (BUG-30, BUG-31)"
run "$MM" 2 1 1 -case 2 -warma -weakex 2
printf '%s' "$STDERR" | grep -q "cannot be combined with -alpha or -weakex" \
  && ok "-warma with -weakex is refused, not faked" \
  || bad "-warma -weakex" "not refused: $STDERR"
run tests/repro/fixtures/mmd.inp 2 1 1 -case 2 -differenced -matest 3
printf '%s' "$STDERR" | grep -q "incompatible with -differenced" \
  && ok "-differenced with -matest is refused" \
  || bad "-differenced -matest" "not refused: $STDERR"
cp datasets/synthetic/rank2.inp "$TMP/case.inp"
timeout "$RUN_TIMEOUT" "$DRVEC" "$TMP/case" 1 0 1 -case 2 -lrtest -fixb2 -bootstrap 10 >/dev/null 2>&1
rc=$?
if [ $rc -eq 0 ] && grep -aq "p-value  reps" "$TMP/case.out"; then
    ok "-lrtest -fixb2 -bootstrap with M = 3 runs (it crashed)"
else bad "-lrtest -fixb2 -bootstrap M=3" "exit $rc"; fi
echo

# 8u. THE .pre READER REFUSES MALFORMED FILES (BUG-39).  An empty ifadf line
#     crashed; a truncated file was estimated with zeros; the DRVUS date line
#     `62 1850' left the start year as garbage; and a name longer than 80
#     characters shifted the file by one line (MAXSTR was main.h's 80), so a
#     lambda = 1 file read as lambda = 0.  The first three must fail with a
#     message, the last must fit exactly as the same file with a short name.
echo "[8u] the .pre reader and malformed files (BUG-39)"
R=tests/repro/fixtures
ABSDRVEC=$(cd "$(dirname "$DRVEC")" && pwd)/$(basename "$DRVEC")
for f in ifempty trunc shortdate; do
    cp "$R/$f.pre" "$TMP/$f.pre"; cp tests/fixtures/mmdiag.2.pre "$TMP/"
    msg=$( (cd "$TMP" && timeout "$RUN_TIMEOUT" "$ABSDRVEC" $f.pre mmdiag.2.pre 2 0 1) 2>&1 >/dev/null)
    rc=$?
    if [ $rc -eq 1 ] && printf '%s' "$msg" | grep -q "ERROR: $f.pre"; then
        ok "reader: $f.pre is refused with a message"
    else bad "reader: $f.pre" "exit $rc: $msg"; fi
done
cp "$R/long100.pre" "$TMP/"
(cd "$TMP" && timeout "$RUN_TIMEOUT" "$ABSDRVEC" long100.pre mmdiag.2.pre 2 0 1 >/dev/null 2>&1)
ll=$(awk '/^logelf/{print $3}' "$TMP/long100_mmdiag.2.out" 2>/dev/null)
[ "$ll" = "64.9983443540" ] && ok "reader: a 100-character name reads like a short one (logL $ll)" \
                            || bad "reader: long name" "logL '$ll', expected 64.9983443540"
echo

# 8v. THE SAMPLE IS DATED FROM ITS FIRST ESTIMATED OBSERVATION (BUG-37).
#     data/synth.inp: 99 annual observations, 2000-2098; nabla Y2 consumes the
#     first, so the estimated sample is 2001-2098.  Header and diagnosis both
#     said 2000, and every date of the diagnosis was one period early.
echo "[8v] the sample's dates (BUG-37)"
run data/synth.inp 2 0 1
grep -aq "^Sample .*98 observations from 2001" "$TMP/case.out" \
  && ok "header: the sample starts at its first estimated observation" \
  || bad "header date" "$(grep -a '^Sample' "$TMP/case.out")"
grep -aq "98 observations: from 2001 to 2098" "$TMP/case.out" \
  && ok "diagnosis: residuals dated 2001-2098" \
  || bad "diagnosis dates" "$(grep -a 'observations: from' "$TMP/case.out" | head -1)"
echo

# ================================================== 9 MEMORY (opt-in) ==
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
    echo "[9] memory (valgrind)"
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
        vg_clean "no leaks: q=2 (negative tensor bound)" "$MM" 2 2 1 -case 2
        vg_clean "no leaks: seeding from .pre" "$MM" 2 1 0 -case 1 \
                 -diagar -diagma -diagcov -seedybar tests/fixtures/mmdiag
        vg_clean "no leaks: -seedgate"        "$MM" 2 1 1 -case 2 -mean -seedgate
        vg_clean "no leaks: -rungs"           "$MM" 2 1 0 -case 1 -rungs
        vg_clean "no leaks: -seedjoh"         data/pairs/milan.inp 2 1 1 \
                 -case 2 -mean -seedjoh
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
