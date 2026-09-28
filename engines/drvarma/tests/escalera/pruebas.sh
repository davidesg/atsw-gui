#!/bin/sh
# pruebas.sh -- la escalera de drvarma 5.0: los .pre de fue como entrada.
#
#   sh tests/escalera/pruebas.sh             (desde engines/drvarma)
#   sh tests/escalera/pruebas.sh --generar   reescribe ref/ (solo si un cambio
#                                            de la salida es deliberado)
#
# Las comprobaciones con numero son AFIRMACIONES, no recuerdos: la puerta, que
# el univariante de drvarma es el de fue (sigma2 y coeficientes del .out de
# fue), BUG-2, las opciones que la escalera todavia no admite, y la busqueda
# lineal con un objetivo no finito. Lo ultimo es la regresion byte a byte de
# la salida de la 5.0 (ref/), con las dos lineas que llevan rutas o version
# fuera.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
RAIZ=$(cd "$AQUI/../.." && pwd)
BIN=${DRVARMA:-$RAIZ/bin/drvarma}
MODO=${1:-comparar}
[ -x "$BIN" ] || { echo "no encuentro $BIN (make)"; exit 2; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$RAIZ"

C=cases
ES=$C/IPC_ES/work/IPC_ES_m10.pre
FR=$C/IPC_FR/work/IPC_FR_msar.pre
DE=$C/IPC_DE/work/IPC_DE_mar3sar.pre
WTI=$C/WTI/work/WTI_ar1.pre

ok=0; mal=0
bien()  { echo "  ok     $1"; ok=$((ok+1)); }
falla() { echo "  FALLA  $1"; mal=$((mal+1)); }
# cerca <desc> <esperado> <obtenido> <tol>
cerca() {
    if [ -n "$3" ] && python3 -c "import sys; sys.exit(0 if abs(float('$3')-float('$2'))<=$4 else 1)"; then
        bien "$1 = $3"
    else
        falla "$1 = '$3' (esperado $2 +- $4)"
    fi
}
# valor <fichero .out> <nombre del parametro> -> la estimacion
valor() { awk -v n="$2" '$1==n {print $2; exit}' "$1"; }

echo "drvarma, la escalera"

# 1. la version, en un solo sitio
v=$("$BIN" -version)
case "$v" in "drvarma 5.0.0 (git "*")") true;; *) false;; esac && bien "-version: $v" || falla "-version: '$v'"

# 2. la puerta, sobre los tres IPC del banco
"$BIN" "$ES" "$FR" "$DE" 0 0 -diagcov -o "$TMP/g1" > "$TMP/g1.log" 2>&1
rc=$?
[ $rc -eq 0 ] && bien "tres IPC, diagonal: rc=0" || falla "tres IPC, diagonal: rc=$rc"
grep -q "GATE: PASSED" "$TMP/g1.out" && bien "la puerta pasa" || falla "la puerta no pasa"
d=$(awk '$1=="difference" {print $2}' "$TMP/g1.out")
cerca "logL conjunto - SUM logL_i" 0 "$d" 1e-9

# 3. el univariante de drvarma ES el de fue: sigma2 del .out de fue
for pre in "$ES" "$FR" "$DE"; do
    s=$(basename "$pre" .pre | cut -d_ -f1-2)
    fue=$(sed -n 's/^sigma2: *\([0-9.eE+-]*\).*/\1/p' "${pre%.pre}.out")
    got=$(awk -v n="$s" '$1==n && NF>=4 {print $3; exit}' "$TMP/g1.out")
    cerca "sigma2 $s contra fue" "$fue" "$got" 1e-7
done

# 4. y sus coeficientes: el AR(3)xSAR(1)_12 de Alemania, como en fue
cerca "phi_IPC_DE[B^1]"  0.041309  "$(valor "$TMP/g1.out" 'phi_IPC_DE[B^1]')"  2e-6
cerca "phi_IPC_DE[B^3]" -0.147537  "$(valor "$TMP/g1.out" 'phi_IPC_DE[B^3]')"  2e-6
cerca "Phi_IPC_DE[B^12]" 0.165910  "$(valor "$TMP/g1.out" 'Phi_IPC_DE[B^12]')" 2e-6
cerca "mu_IPC_DE"        0.114851  "$(valor "$TMP/g1.out" 'mu_IPC_DE')"        2e-6

# 5. un .pre genuino es un punto fijo: se mueve del orden de sus 6 decimales
for s in IPC_ES IPC_FR IPC_DE; do
    mv=$(awk -v n="$s" '$1==n && NF>=4 {print $4; exit}' "$TMP/g1.out")
    cerca "lo que se mueve el .pre de $s" 0 "$mv" 1e-4
done

# 6. BUG-2: la misma serie declarada catorce anos despues se RECHAZA
sed '9s/ 2002 / 2016 /' "$WTI" > "$TMP/WTI_desplazado.pre"
"$BIN" "$TMP/WTI_desplazado.pre" "$ES" 1 0 -o "$TMP/b2" > "$TMP/b2.log" 2>&1
rc=$?
if [ $rc -eq 4 ] && grep -q "do NOT end on the same date" "$TMP/b2.log"; then
    bien "BUG-2: series con catorce anos de desfase, rechazadas (rc=4)"
else
    falla "BUG-2: rc=$rc, $(head -c 200 "$TMP/b2.log")"
fi

# 7. distinto nobs y la MISMA fecha final: se alinea por el final
awk 'NR==9{sub(/216  1 2002/,"204  1 2003")} NR>=73 && NR<=84 {next} {print}' \
    "$ES" > "$TMP/ES_2003.pre"
"$BIN" "$TMP/ES_2003.pre" "$FR" 0 0 -diagcov -o "$TMP/corto" > "$TMP/corto.log" 2>&1
rc=$?
if [ $rc -eq 0 ] && grep -q "2/2003 - 12/2019  (203 stationary" "$TMP/corto.out" \
   && grep -q "IPC_FR .*trimmed sample" "$TMP/corto.out"; then
    bien "distinto nobs, mismo final: ventana 2/2003-12/2019, FR recortada"
else
    falla "distinto nobs, mismo final: rc=$rc"
fi

# 8. lo que la escalera todavia no hace es un error, no un silencio
"$BIN" "$ES" "$FR" 1 0 -deseason > "$TMP/ds.log" 2>&1
rc=$?
[ $rc -eq 1 ] && grep -q "not available in ladder mode" "$TMP/ds.log" \
    && bien "-deseason en la escalera: error explicito (la transformacion es del .pre)" \
    || falla "-deseason en la escalera: rc=$rc"

# 9. el pass-through WTI -> IPC_ES, como lo midio el banco por la via .inp
"$BIN" "$WTI" "$ES" 1 0 -o "$TMP/pt" > "$TMP/pt.log" 2>&1
grep -q "GATE: PASSED" "$TMP/pt.out" && bien "WTI + IPC_ES: la puerta pasa" \
                                    || falla "WTI + IPC_ES: la puerta"
cerca "AR1[IPC_ES<-WTI]" 0.0104 "$(valor "$TMP/pt.out" 'AR1[IPC_ES<-WTI]')" 1e-3

# 10. la busqueda lineal con un objetivo NaN o infinito tiene que VOLVER
cc=$(command -v gcc || command -v cc)
if [ -n "$cc" ]; then
    cat > "$TMP/sonda.c" <<'EOF'
#include "main.h"
real macheps; FILE *outputv; int quiet_mode = 1; static int kind;
void raxopt(real (*)(real []), real *, int, real *, real **, int, int, real, real);
real f(real *x) { if (x[1] > 2.0) return kind ? INFINITY : NAN;
                  return 0.1*(x[1]-3)*(x[1]-3)/0.9; }
int main(int argc, char **argv) { kind = (argc > 1 && argv[1][0] == 'i');
  macheps = cmacheps(); outputv = stdout;
  real *x = vector(1,1); x[1] = 0.0; real **b = matrix(1,1,1,1), fk;
  raxopt(f, &fk, 1, x, b, 100, 1, 1e-6, 1e-8);
  printf("%.10g\n", x[1]); return 0; }
EOF
    if "$cc" -O0 -w -Iinclude -I../../lib/fuepre -o "$TMP/sonda" "$TMP/sonda.c" ../../lib/optim/lnsrch.c \
          src/qnewtopt.c src/nlatools.c -lgsl -lgslcblas -lm 2>/dev/null; then
        for k in nan inf; do
            x=$(timeout 20 "$TMP/sonda" $k); rc=$?
            if [ $rc -eq 0 ] && python3 -c "import sys; sys.exit(0 if float('$x')<=2.0+1e-9 else 1)"; then
                bien "lnsrch vuelve con un objetivo $k (x=$x)"
            else
                falla "lnsrch con un objetivo $k: rc=$rc (124 = sigue colgada)"
            fi
        done
    else
        falla "no compila la sonda de lnsrch"
    fi
fi

# 10b. LA PREVISION (fase 2): el sistema diagonal con -estwin 216 tiene que
#      dar las previsiones de fue con parametros fijos, en todos los origenes
#      (12/2019..11/2023) y horizontes (1..24): 3456 valores. La referencia es
#      fue de hoy (data/make_fue_reference.py); los CSV de cases/ son de un
#      fue anterior y no coinciden con el actual.
DX=tests/escalera/data
"$BIN" $DX/IPC_ES_ext.pre $DX/IPC_FR_ext.pre $DX/IPC_DE_ext.pre 0 0 -diagcov \
    -forecast 24 -estwin 216 -o "$TMP/fc" > "$TMP/fc.log" 2>&1
rc=$?
[ $rc -eq 0 ] && bien "prevision recursiva del diagonal: rc=0" || falla "prevision recursiva: rc=$rc"
r=$(python3 - "$TMP/fc.recursive" $DX/fue_recursive_reference.csv <<'PY'
import sys
def load(p):
    d = {}
    for l in open(p).read().splitlines():
        t = l.replace(',', ' ').split()
        if len(t) == 4 and t[2].isdigit():
            d[(t[0], t[1], int(t[2]))] = float(t[3])
    return d
our, ref = load(sys.argv[1]), load(sys.argv[2])
miss = [k for k in ref if k not in our]
worst = max(abs(our[k] - v) / abs(v) for k, v in ref.items() if k in our)
print(len(ref), len(miss), "%.3g" % worst)
PY
)
set -- $r
if [ "$1" = 3456 ] && [ "$2" = 0 ] && python3 -c "import sys; sys.exit(0 if $3 < 1e-5 else 1)"; then
    bien "las 3456 previsiones coinciden con fue (error relativo maximo $3)"
else
    falla "previsiones contra fue: $r (n, faltan, error relativo maximo)"
fi
"$BIN" "$ES" "$FR" 0 0 -estwin 100 > "$TMP/ew.log" 2>&1
rc=$?
[ $rc -eq 1 ] && grep -q "needs a horizon" "$TMP/ew.log" \
    && bien "-estwin sin -forecast: error explicito" || falla "-estwin sin -forecast: rc=$rc"

# 10c. UN SOLO FORMATO. El .inp multivariante esta obsoleto desde la 5.0:
#      -split lo convierte en un .inp univariante de fue por serie, y la
#      escalera los toma por su CONTENIDO (el validador de fue), no por el
#      nombre. La equivalencia: la escalera sobre los ficheros de -split
#      -mean -ar 1, con p = 1, es el VAR(1) completo con -mean de la via .inp.
mkdir -p "$TMP/split"
"$BIN" -split data/models_group1/IPC3 -mean -ar 1 -dir "$TMP/split" > "$TMP/split.log" 2>&1
rc=$?
[ $rc -eq 0 ] && [ -f "$TMP/split/IPC_ES.inp" ] && [ -f "$TMP/split/IPC_DE.inp" ] \
    && bien "-split: un .inp de fue por serie" || falla "-split: rc=$rc"
"$BIN" -split data/models_group1/IPC3 -dir "$TMP/split" > "$TMP/split2.log" 2>&1
rc=$?
[ $rc -eq 2 ] && grep -q "is not overwritten" "$TMP/split2.log" \
    && bien "-split no sobrescribe" || falla "-split sobrescribio (rc=$rc)"
cp data/models_group1/IPC3.inp "$TMP/"
( cd "$TMP" && "$BIN" IPC3 1 0 -mean > legacy.log 2>&1 )
grep -q "^Note: the multivariate .inp is deprecated" "$TMP/legacy.log" \
    && bien "la via .inp avisa de que su formato esta obsoleto" \
    || falla "la via .inp no avisa de la obsolescencia"
"$BIN" "$TMP/split/IPC_ES.inp" "$TMP/split/IPC_FR.inp" "$TMP/split/IPC_DE.inp" 1 0 \
    -o "$TMP/lad" > "$TMP/lad.log" 2>&1
sed -n '/^Normalized model:/,/^Q matrix:/p' "$TMP/IPC3.out" > "$TMP/leg.blk"
sed -n '/^Normalized model:/,/^Q matrix:/p' "$TMP/lad.out" > "$TMP/lad.blk"
if [ -s "$TMP/leg.blk" ] && cmp -s "$TMP/leg.blk" "$TMP/lad.blk"; then
    bien "la escalera sobre los .inp de -split es el VAR(1) de la via .inp (mu y phi)"
else
    falla "la escalera y la via .inp no llegan al mismo VAR(1)"
    diff "$TMP/leg.blk" "$TMP/lad.blk" | head -6
fi
"$BIN" data/models_group1/IPC3.inp 1 0 > "$TMP/mv.log" 2>&1
rc=$?
[ $rc -eq 2 ] && grep -q "drvarma -split" "$TMP/mv.log" \
    && bien "un .inp multivariante en la escalera: error que dice como convertirlo" \
    || falla "un .inp multivariante en la escalera: rc=$rc"

# 10b. standard errors: fdhess at the optimum by default, BFGS on request,
#      and the method is always written (docs/DESIGN-v5-ladder.md; the study
#      is drvarma-python docs/STUDY-standard-errors.md).
"$BIN" "$ES" "$WTI" 0 0 -diagcov -o "$TMP/sefd" > /dev/null 2>&1
"$BIN" "$ES" "$WTI" 0 0 -diagcov -hessian bfgs -o "$TMP/sebf" > /dev/null 2>&1
grep -q "^  Standard errors: fdhess$" "$TMP/sefd.out" \
    && grep -q "^  Standard errors: bfgs$" "$TMP/sebf.out" \
    && bien "standard errors: fdhess by default, -hessian bfgs on request, both said" \
    || falla "standard errors: the method is not the one asked for, or not said"
"$BIN" "$ES" "$WTI" 0 0 -hessian exact > "$TMP/sebad.log" 2>&1
[ $? -ne 0 ] && grep -q "not available" "$TMP/sebad.log" \
    && bien "-hessian with an unknown value is an error" \
    || falla "-hessian with an unknown value is accepted"
T2=$(mktemp -d); cp data/IPC.inp "$T2/"
( cd "$T2" && "$BIN" IPC 1 0 -mean > /dev/null 2>&1 )
grep -q "^cov\[1,1\] .*(normalised)" "$T2/IPC.out" \
    && grep -q "^Standard errors: fdhess$" "$T2/IPC.out" \
    && bien ".inp path: fdhess holds qq[1,1] (the flat direction) and says so" \
    || falla ".inp path: qq[1,1] not held, or the method not said"
( cd "$T2" && "$BIN" IPC 1 0 -mean -hessian nope > bad.log 2>&1 )
[ $? -ne 0 ] && bien ".inp path: -hessian with an unknown value is an error" \
    || falla ".inp path: -hessian with an unknown value is accepted"
rm -r "$T2"

# 10c. Shea (AS 242) against elf (AS 311): two independent exact likelihoods.
#      With -m 2 (no xi truncation) they must agree to rounding at every point
#      the optimizer visits; measured 1e-9..1e-13 (lib.c, -lik both).
shea_ok() {   # file: the "Shea check" line must show both maxima below 1e-8
    python3 - "$1" <<'PY'
import re, sys
t = open(sys.argv[1], encoding="latin-1").read()
m = re.search(r"Shea check.*?(\d+) points; max \|dlogL\| = ([0-9.e+-]+), at the optimum ([0-9.e+-]+)", t)
sys.exit(0 if m and int(m.group(1)) > 0 and float(m.group(2)) < 1e-8 and float(m.group(3)) < 1e-8 else 1)
PY
}
"$BIN" "$ES" "$FR" 1 0 -diagcov -m 2 -lik both -o "$TMP/shl" > /dev/null 2>&1
shea_ok "$TMP/shl.out" \
    && bien "ladder: elf and Shea give the same likelihood at every point (-m 2)" \
    || falla "ladder: elf and Shea disagree (-lik both -m 2)"
T3=$(mktemp -d); cp data/PSW.inp "$T3/"
( cd "$T3" && "$BIN" PSW 1 1 -m 2 -lik both > /dev/null 2>&1 )
shea_ok "$T3/PSW.out" \
    && bien ".inp path, VARMA(1,1): elf and Shea agree at every point (-m 2)" \
    || falla ".inp path, VARMA(1,1): elf and Shea disagree (-lik both -m 2)"
( cd "$T3" && "$BIN" PSW 1 1 -lik nope > bad.log 2>&1 )
[ $? -ne 0 ] && bien "-lik with an unknown value is an error" \
    || falla "-lik with an unknown value is accepted"
rm -r "$T3"
"$BIN" "$ES" "$WTI" 0 0 -diagcov -lik shea -o "$TMP/shg" > /dev/null 2>&1
[ $? -eq 0 ] && grep -q "GATE: PASSED" "$TMP/shg.out" \
    && bien "the diagonal gate closes with Shea's likelihood too" \
    || falla "the diagonal gate does not close with -lik shea"

# 11. la regresion de la salida de la 5.0
REF=$AQUI/ref
filtro() { grep -av '^Program          : \|^Output File      : \|^  \[[0-9]*\] \|^Full results written to \|^Forecasts written to \|^Recursive forecasts written to ' "$1"; }
for caso in g1 pt corto fc; do
    for ext in out log forecast; do
        f=$TMP/$caso.$ext
        [ -f "$f" ] || continue
        if [ "$MODO" = --generar ]; then
            mkdir -p "$REF"; filtro "$f" > "$REF/$caso.$ext"
        elif filtro "$f" | cmp -s - "$REF/$caso.$ext"; then
            bien "regresion $caso.$ext"
        else
            falla "regresion $caso.$ext"; filtro "$f" | diff "$REF/$caso.$ext" - | head -6
        fi
    done
done
[ "$MODO" = --generar ] && echo "  referencia regenerada en $REF"

echo "$ok ok, $mal fallos"
[ $mal -eq 0 ]
