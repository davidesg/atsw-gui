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
    if "$cc" -O0 -w -Iinclude -I../../lib/fuepre -I../../lib/lik -o "$TMP/sonda" "$TMP/sonda.c" ../../lib/optim/lnsrch.c \
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

# 10d. -zero: un coeficiente cruzado suelto a cero (la simplificacion de Tiao y
#      Box por coeficiente; sim_gui S7). Sale de la tabla, el modelo tiene un
#      parametro menos, la LR contra los univariantes un grado menos, y la
#      cabecera lo dice. Lo que no esta en el modelo es un error, no un
#      silencio: un retardo fuera de p, o un par sin enlace.
"$BIN" "$ES" "$FR" 1 0 -o "$TMP/zf" > /dev/null 2>&1
"$BIN" "$ES" "$FR" 1 0 -zero "AR1[IPC_ES<-IPC_FR]" -o "$TMP/zz" > /dev/null 2>&1
rc=$?
nf=$(awk '/^Number of parameters:/ {print $4; exit}' "$TMP/zf.out")
nz=$(awk '/^Number of parameters:/ {print $4; exit}' "$TMP/zz.out")
df_f=$(awk '/LR = .* df = / {for (i = 1; i <= NF; i++) if ($i == "df") print $(i + 2); exit}' "$TMP/zf.out")
df_z=$(awk '/LR = .* df = / {for (i = 1; i <= NF; i++) if ($i == "df") print $(i + 2); exit}' "$TMP/zz.out")
[ $rc -eq 0 ] && [ -n "$nf" ] && [ "$nz" = "$((nf - 1))" ] \
    && bien "-zero: un parametro menos ($nf -> $nz)" \
    || falla "-zero: rc=$rc, parametros $nf -> $nz"
[ -n "$df_f" ] && [ "$df_z" = "$((df_f - 1))" ] \
    && bien "-zero: la LR contra los univariantes, un grado menos ($df_f -> $df_z)" \
    || falla "-zero: gl de la LR $df_f -> $df_z"
awk '$1 == "AR1[IPC_ES<-IPC_FR]"' "$TMP/zz.out" | grep -q . \
    && falla "-zero: el coeficiente sigue en la tabla" \
    || bien "-zero: el coeficiente sale de la tabla"
awk '$1 == "AR1[IPC_FR<-IPC_ES]"' "$TMP/zz.out" | grep -q . \
    && bien "-zero: el del otro sentido sigue libre" \
    || falla "-zero: se fue tambien el del otro sentido"
grep -q "^Zero coefficients: AR1\[IPC_ES<-IPC_FR\]$" "$TMP/zz.out" \
    && bien "-zero: la cabecera lo dice" || falla "-zero: la cabecera no lo dice"
grep -q "^Zero coefficients" "$TMP/zf.out" \
    && falla "sin -zero la cabecera habla de ceros" || bien "sin -zero, la cabecera no cambia"
"$BIN" "$ES" "$FR" 1 0 -zero "AR2[IPC_ES<-IPC_FR]" -o "$TMP/zb" > "$TMP/zb.log" 2>&1
[ $? -ne 0 ] && grep -q "is not in the model" "$TMP/zb.log" \
    && bien "-zero con un retardo fuera de p es un error" \
    || falla "-zero acepta un retardo fuera de p"
"$BIN" "$ES" "$FR" 1 0 -links "IPC_FR<-IPC_ES" -zero "AR1[IPC_ES<-IPC_FR]" -o "$TMP/zl" > "$TMP/zl.log" 2>&1
[ $? -ne 0 ] && grep -q "not a linked pair" "$TMP/zl.log" \
    && bien "-zero sobre un par sin enlace es un error" \
    || falla "-zero acepta un par sin enlace"
"$BIN" "$ES" "$FR" 1 0 -zero "XX1[IPC_ES<-IPC_FR]" -o "$TMP/zx" > "$TMP/zx.log" 2>&1
[ $? -ne 0 ] && grep -q "is not of the form" "$TMP/zx.log" \
    && bien "-zero mal escrito es un error" || falla "-zero mal escrito se acepta"

# 10c. LOS ERRORES TIPICOS DE LA IRF (metodo delta). En el sistema diagonal
#      con Sigma diagonal, IPC_ES es un AR(1): su respuesta propia es
#      O_h = phi^h L, con L = sqrt(Sigma11), y su error tipico tiene forma
#      cerrada: Var = (h phi^(h-1) L)^2 Var(phi) + phi^(2h) Sigma11/(2n),
#      porque Var(Sigma11) = 2 Sigma11^2/n y dL/dSigma11 = 1/(2L). La
#      acumulada a h = 1, L(1+phi): Var = L^2 Var(phi) + (1+phi)^2 Sigma11/(2n).
#      Prueba las dos partes: la dinamica (por shootx_esc) y Sigma.
r=$(python3 - "$TMP/fc.out" <<'PY'
import math, re, sys
t = open(sys.argv[1], errors="replace").read()
phi, sphi = map(float, re.search(r"phi_IPC_ES\[B\^1\]\s+(\S+)\s+(\S+)", t).groups())
s11 = float(re.search(r"Innovation covariance Sigma.*\n\s*IPC_ES\s+(\S+)", t).group(1))
n = int(re.search(r"\((\d+) stationary observations\)", t).group(1))
def tabla(titulo):
    b = t[t.index(titulo):]
    b = b[b.index("Shock to IPC_ES:"):]
    return [float(l.split()[1]) for l in b.split("\n")[2:5]]
se, sea = tabla("STD. ERRORS OF THE ORTHOGONALIZED"), tabla("STD. ERRORS OF THE ACCUMULATED")
L, vs = math.sqrt(s11), s11 / (2 * n)
want = [math.sqrt((h * phi ** (h - 1) * L) ** 2 * sphi ** 2 + phi ** (2 * h) * vs) if h else math.sqrt(vs)
        for h in range(3)]
wa1 = math.sqrt(L ** 2 * sphi ** 2 + (1 + phi) ** 2 * vs)
err = max(abs(a - b) for a, b in zip(se, want))
err = max(err, abs(sea[1] - wa1))
print("%.6f %s %s %.4f %.4f" % (err, " ".join("%.4f" % x for x in se), " ".join("%.4f" % x for x in want), sea[1], wa1))
PY
)
set -- $r
[ -n "$1" ] && python3 -c "import sys; sys.exit(0 if float('$1') <= 1.5e-4 else 1)" \
    && bien "IRF, metodo delta: AR(1) con forma cerrada, h = 0..2 y la acumulada ($2 $3 $4)" \
    || falla "IRF, metodo delta: no casa con la forma cerrada ($r)"

# 10d. -irfboot: LAS BANDAS POR BOOTSTRAP. El .irfboot entero (irf y acc, m*m
#      pares, h = 0..H), el mismo fichero en dos corridas (el generador es
#      determinista, el de drvec), y, en un modelo sin raices cerca de la
#      unidad, la banda del 90 % del bootstrap casa con la del metodo delta
#      (2 * 1.645 d.t.) a menos de un 30 % en h = 0..2: dos calculos que no
#      comparten nada salvo el modelo.
"$BIN" "$WTI" "$ES" 1 0 -o "$TMP/bt1" -irfboot 100 > "$TMP/bt1.log" 2>&1
"$BIN" "$WTI" "$ES" 1 0 -o "$TMP/bt2" -irfboot 100 > "$TMP/bt2.log" 2>&1
r=$(python3 - "$TMP/bt1" <<'PY'
import csv, sys
b = sys.argv[1]
t = open(b + ".out", errors="replace").read()
rows = [r for r in csv.reader(l for l in open(b + ".irfboot") if not l.startswith("#"))][1:]
H = max(int(r[3]) for r in rows)
blk = t[t.index("STD. ERRORS OF THE ORTHOGONALIZED"):]
blk = blk[blk.index("Shock to WTI:"):].split("\n")
se = [[float(x) for x in l.split()[1:]] for l in blk[2:5]]
peor = 0.0
for r in rows:
    if r[0] == "irf" and r[1] == "WTI" and int(r[3]) <= 2:
        h, j = int(r[3]), (0 if r[2] == "WTI" else 1)
        d = 2 * 1.645 * se[h][j]
        peor = max(peor, abs((float(r[8]) - float(r[6])) / d - 1))
print(len(rows), 2 * 4 * (H + 1), "%.3f" % peor)
PY
)
set -- $r
[ -n "$1" ] && [ "$1" = "$2" ] && bien "-irfboot: el .irfboot entero ($1 filas)" \
    || falla "-irfboot: el .irfboot no tiene sus filas ($r)"
cmp -s "$TMP/bt1.irfboot" "$TMP/bt2.irfboot" \
    && bien "-irfboot: dos corridas, el mismo fichero (el generador de drvec)" \
    || falla "-irfboot: dos corridas dan bandas distintas"
[ -n "$3" ] && python3 -c "import sys; sys.exit(0 if float('$3') < 0.30 else 1)" \
    && bien "-irfboot: la banda del 90 % casa con la del metodo delta (peor diferencia $3)" \
    || falla "-irfboot: la banda no casa con la del metodo delta ($r)"
"$BIN" "$WTI" "$ES" 1 0 -redet -irfboot 10 -o "$TMP/btr" > "$TMP/btr.log" 2>&1
[ $? -ne 0 ] && grep -q "not -redet" "$TMP/btr.log" \
    && bien "-irfboot con -redet es un error" || falla "-irfboot acepta -redet"

# 10b. -start ols: el arranque de drvarma original (init_varma) en la escalera.
#     El VAR monetario de EE. UU. (FRED; IPI, IPC, WTI, GS1, ruta U): desde
#     cero el VAR(2) se para a las 2 iteraciones por debajo del VAR(1)
#     (drvarma-python BUG-0027); desde MCO llega al optimo, el mismo que el
#     porte (Ladder(start="ols")) a 6 decimales.
MON=$AQUI/data/monetario
( cd "$TMP" && "$BIN" "$MON/IPI_m07.pre" "$MON/IPC_m07.pre" "$MON/WTI_m03.pre" "$MON/GS1_m01.pre" \
    2 0 -start ols -o "$TMP/mols" ) > "$TMP/mols.log" 2>&1
grep -q "^Start of the cross terms: ols" "$TMP/mols.log" \
    && bien "-start ols: se usa" || falla "-start ols: no se usa ($(grep -a 'Start of' "$TMP/mols.log"))"
cerca "-start ols: logL del VAR(2) monetario" -1741.275477 \
    "$(awk '/^Exact log-likelihood/ {print $3; exit}' "$TMP/mols.log")" 1e-5
"$BIN" "$MON/IPI_m07.pre" "$MON/GS1_m01.pre" 1 0 -start mco -o "$TMP/mmal" > "$TMP/mmal.log" 2>&1
[ $? -ne 0 ] && bien "-start desconocido es un error" || falla "-start acepta un valor desconocido"

# 10e. La mascara de las deterministas: -det y -detbase.
#     Los IPC de Espana y Francia (11 armonicos libres cada uno) y, para las
#     intervenciones, el IPC y el WTI del monetario (el escalon del Golfo y
#     Katrina en el IPC; el impulso del Golfo en el WTI).
det() { sed -n 's/^Deterministics   : //p' "$1" | head -1; }
ll()  { awk '/^Exact log-likelihood/ {print $3; exit}' "$1"; }
np()  { awk '/^Number of parameters/ {print $4; exit}' "$1"; }
"$BIN" "$ES" "$FR" 1 0 -redet   -o "$TMP/dr" > /dev/null 2>&1
"$BIN" "$ES" "$FR" 1 0 -det all -o "$TMP/da" > /dev/null 2>&1
filtro2() { grep -av '^Program          : \|^Output File      : ' "$1"; }
filtro2 "$TMP/dr.out" > "$TMP/dr.f"; filtro2 "$TMP/da.out" > "$TMP/da.f"
[ -s "$TMP/dr.f" ] && cmp -s "$TMP/dr.f" "$TMP/da.f" && [ "$(det "$TMP/dr.out")" = "re-estimated" ] \
    && bien "-det all es -redet (la misma salida)" || falla "-det all no es -redet"
"$BIN" "$ES" "$FR" 1 0 -det IPC_FR:seasonal -o "$TMP/df" > /dev/null 2>&1
[ "$(np "$TMP/df.out")" = 19 ] && [ "$(np "$TMP/dr.out")" = 30 ] \
    && det "$TMP/df.out" | grep -q "IPC_FR:seasonal (11 of 22); the rest fixed at the .pre" \
    && bien "-det SERIE:clase: solo los 11 armonicos de Francia (19 parametros de 30)" \
    || falla "-det IPC_FR:seasonal: $(np "$TMP/df.out") parametros, $(det "$TMP/df.out")"
"$BIN" "$ES" "$FR" 1 0 -o "$TMP/dn" > /dev/null 2>&1
"$BIN" "$ES" "$FR" 1 0 -detbase gate -o "$TMP/dg" > /dev/null 2>&1
cerca "-detbase gate con .pre genuinos sin recortar: el mismo logL (son un punto fijo)" \
    "$(ll "$TMP/dn.out")" "$(ll "$TMP/dg.out")" 1e-5
det "$TMP/dg.out" | grep -q "fixed at the gate's values (22 estimated there" \
    && [ "$(det "$TMP/dn.out")" = "fixed at the .pre" ] \
    && bien "-detbase gate: la cabecera dice de donde sale lo fijo" \
    || falla "-detbase gate: $(det "$TMP/dg.out")"
# la ventana recortada (el caso 7): por defecto, los de la serie recortada
# los estima la puerta; -detbase file los deja en los del fichero
"$BIN" "$TMP/ES_2003.pre" "$FR" 0 0 -diagcov -detbase file -o "$TMP/cf" > /dev/null 2>&1
"$BIN" "$TMP/ES_2003.pre" "$FR" 0 0 -diagcov -detbase gate -o "$TMP/cg" > /dev/null 2>&1
"$BIN" "$TMP/ES_2003.pre" "$FR" 0 0 -diagcov -redet        -o "$TMP/cr" > /dev/null 2>&1
det "$TMP/corto.out" | grep -q "fixed at the gate's values (11 estimated there" \
    && grep -q "re-estimated here too" "$TMP/corto.out" \
    && bien "ventana recortada: por defecto la puerta reestima las deterministas de la serie recortada" \
    || falla "ventana recortada, por defecto: $(det "$TMP/corto.out")"
python3 -c "
import sys
f, d, g, r = map(float, sys.argv[1:5])
sys.exit(0 if f < d < g and abs(g - r) < 1e-5 else 1)" \
    "$(ll "$TMP/cf.out")" "$(ll "$TMP/corto.out")" "$(ll "$TMP/cg.out")" "$(ll "$TMP/cr.out")" \
    && bien "ventana recortada: file < por defecto < gate, y gate = -redet en el diagonal" \
    || falla "ventana recortada: logL $(ll "$TMP/cf.out") $(ll "$TMP/corto.out") $(ll "$TMP/cg.out") $(ll "$TMP/cr.out")"
[ "$(np "$TMP/cg.out")" = 5 ] && [ "$(np "$TMP/cr.out")" = 27 ] \
    && bien "-detbase gate las fija despues: 5 parametros en el conjunto; -redet, 27" \
    || falla "-detbase gate: $(np "$TMP/cg.out") parametros; -redet, $(np "$TMP/cr.out")"
# las intervenciones: una clase, un nombre, y el shock comun (el Golfo en los dos)
MI=$MON/IPC_m07.pre; MW=$MON/WTI_m03.pre
"$BIN" "$MI" "$MW" 1 0                 -o "$TMP/i0" > /dev/null 2>&1
"$BIN" "$MI" "$MW" 1 0 -det interv     -o "$TMP/i1" > /dev/null 2>&1
"$BIN" "$MI" "$MW" 1 0 -det "w0_IPC[step 8 1990],w0_WTI[impulse 8 1990]" -o "$TMP/i2" > /dev/null 2>&1
"$BIN" "$MI" "$MW" 1 0 -redet          -o "$TMP/i3" > /dev/null 2>&1
[ "$(np "$TMP/i0.out")" = 9 ] && [ "$(np "$TMP/i1.out")" = 21 ] && [ "$(np "$TMP/i2.out")" = 18 ] \
    && [ "$(np "$TMP/i3.out")" = 32 ] \
    && bien "-det interv (12 coeficientes) y -det con dos nombres, el Golfo en las dos series (9)" \
    || falla "-det interv / nombres: parametros $(np "$TMP/i0.out") $(np "$TMP/i1.out") $(np "$TMP/i2.out") $(np "$TMP/i3.out")"
python3 -c "
import sys
a, b, c, d = map(float, sys.argv[1:5])
sys.exit(0 if a <= b + 1e-6 and b <= c + 1e-6 and c <= d + 1e-6 else 1)" \
    "$(ll "$TMP/i0.out")" "$(ll "$TMP/i2.out")" "$(ll "$TMP/i1.out")" "$(ll "$TMP/i3.out")" \
    && bien "-det: los modelos anidan (fijo <= el Golfo <= interv <= todo)" \
    || falla "-det: no anidan: $(ll "$TMP/i0.out") $(ll "$TMP/i2.out") $(ll "$TMP/i1.out") $(ll "$TMP/i3.out")"
grep -q "w0_IPC\[step 8 1990\]" "$TMP/i2.out" && ! grep -q "w0_IPC\[impulse 9 2005\]" "$TMP/i2.out" \
    && bien "-det con un nombre: ese termino en la tabla, y no los demas" \
    || falla "-det con un nombre: la tabla no es la pedida"
"$BIN" "$MI" "$MW" 1 0 -det estacional -o "$TMP/ix" > "$TMP/ix.log" 2>&1
[ $? -ne 0 ] && grep -q "does not match any deterministic term" "$TMP/ix.log" \
    && bien "-det con un selector que no casa: error que dice como es" || falla "-det acepta un selector desconocido"
"$BIN" "$MI" "$MW" 1 0 -det interv -irfboot 10 -o "$TMP/iy" > "$TMP/iy.log" 2>&1
[ $? -ne 0 ] && bien "-irfboot con -det es un error" || falla "-irfboot acepta -det"

# 10f. La ruta V (-route v): el vector primero, por la puerta. Pasada la
#     puerta, el ARMA regular de cada serie es un AR(p)/MA(q) libre y sus
#     factores estacionales multiplican la fila.
diag() { awk '/logL diagonal/ {gsub(",", ""); print $3; exit}' "$1"; }
suma() { awk '/^  SUM / {print $2; exit}' "$1"; }
tabla() { sed -n '/ESTIMATED MODEL/,/^Exact log-likelihood/p' "$1"; }
# caso golden: univariantes cuyo ARMA regular ES un AR(p), sin factores
# estacionales. La V sustituye un AR(p) por un AR(p) libre: es la U.
# Desde cero el punto de partida es el mismo (lo propio del univariante, los
# cruzados a cero): la misma tabla, byte a byte. Con el arranque por defecto
# la V siembra toda la matriz y la U solo los cruzados: el mismo optimo.
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0          -start zero -o "$TMP/vu1" > /dev/null 2>&1
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0 -route v -start zero -o "$TMP/vv1" > /dev/null 2>&1
tabla "$TMP/vu1.out" > "$TMP/vu1.t"; tabla "$TMP/vv1.out" > "$TMP/vv1.t"
[ -s "$TMP/vu1.t" ] && cmp -s "$TMP/vu1.t" "$TMP/vv1.t" \
    && bien "ruta V, caso golden (WTI y GS2, AR(1); VAR(1)): la misma tabla que la ruta U" \
    || falla "ruta V, caso golden VAR(1): la tabla no es la de la U"
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0 -route v -o "$TMP/vv1d" > /dev/null 2>&1
grep -q "^Start of the cross terms: ols, the whole matrix" "$TMP/vv1d.out" \
    && bien "ruta V: el arranque por defecto con q = 0 siembra toda la matriz, y lo dice" \
    || falla "ruta V: $(grep -a 'Start of' "$TMP/vv1d.out")"
cerca "ruta V, caso golden con la siembra de toda la matriz: el mismo logL" \
    "$(ll "$TMP/vu1.out")" "$(ll "$TMP/vv1d.out")" 1e-5
"$BIN" "$MON/GS1_m01.pre" "$MON/FF_m03.pre" 3 0          -o "$TMP/vu3" > /dev/null 2>&1
"$BIN" "$MON/GS1_m01.pre" "$MON/FF_m03.pre" 3 0 -route v -o "$TMP/vv3" > /dev/null 2>&1
cerca "ruta V, caso golden (GS1 y FF, AR(3); VAR(3)): el logL de la ruta U" \
    "$(ll "$TMP/vu3.out")" "$(ll "$TMP/vv3.out")" 1e-5
# el factor de fila: el IPI es AR(3) x AR(1)_12. Con p = 3 la V sin
# cruzados es el univariante (la suma de la puerta); con cruzados difiere de
# la U, porque en la V el AR(1)_12 multiplica tambien los cruzados de su fila
"$BIN" "$MON/IPI_m07.pre" "$MON/FF_m03.pre" 3 0          -o "$TMP/fu" > /dev/null 2>&1
"$BIN" "$MON/IPI_m07.pre" "$MON/FF_m03.pre" 3 0 -route v -o "$TMP/fv" > /dev/null 2>&1
cerca "ruta V con factor de fila: sin cruzados es la suma de la puerta" \
    "$(suma "$TMP/fv.out")" "$(diag "$TMP/fv.out")" 1e-5
python3 -c "
import sys
u, v = float(sys.argv[1]), float(sys.argv[2])
sys.exit(0 if abs(u - v) > 0.5 else 1)" "$(ll "$TMP/fu.out")" "$(ll "$TMP/fv.out")" \
    && grep -q "IPI .*AR(3) MA(0) .*AR 12  MA 0" "$TMP/fv.out" \
    && bien "ruta V con factor de fila: con cruzados no es la U (el AR(1)_12 va en toda la fila)" \
    || falla "ruta V con factor de fila: logL U $(ll "$TMP/fu.out"), V $(ll "$TMP/fv.out")"
# el VARMA(1,1) del IPI y el WTI: el mismo numero que la forma echelon del
# porte con indices iguales, Ladder(echelon=(1,1), own=\"free\") (13
# parametros, logL -924.846133, sin cruzados -929.168188)
"$BIN" "$MON/IPI_m07.pre" "$MON/WTI_m03.pre" 1 1 -route v -o "$TMP/v11" > /dev/null 2>&1
cerca "ruta V, VARMA(1,1) con factor de fila: el logL del porte (echelon (1,1))" \
    -924.846133 "$(ll "$TMP/v11.out")" 1e-5
cerca "ruta V, VARMA(1,1): la V sin cruzados, la del porte" \
    -929.168188 "$(diag "$TMP/v11.out")" 1e-5
[ "$(np "$TMP/v11.out")" = 13 ] \
    && grep -q "^Cross dynamics against the same model without cross terms" "$TMP/v11.out" \
    && grep -q "^Univariate ARMA  : replaced by the vector's" "$TMP/v11.out" \
    && grep -q "IPI's univariate model has a regular AR(3)" "$TMP/v11.out" \
    && bien "ruta V: 13 parametros; dice contra que es el LR y que el AR(3) del IPI no cabe en p = 1" \
    || falla "ruta V: la salida del VARMA(1,1) no dice lo que debe"
# la siembra de toda la matriz: el horno de gas, VAR(6) (los ficheros de
# BUG-0006). Sembrando solo los cruzados sobre lo propio del ajuste diagonal
# (la U) el punto no es admisible y se vuelve a cero; sembrando toda la
# matriz (la V) si lo es. El mismo optimo, -2638.762693.
GAS=bugs/BUG-0006-repro
"$BIN" "$GAS/GAS_v.inp" "$GAS/CO2_v.inp" 6 0          -o "$TMP/gu" > /dev/null 2>&1
"$BIN" "$GAS/GAS_v.inp" "$GAS/CO2_v.inp" 6 0 -route v -o "$TMP/gv" > /dev/null 2>&1
grep -q "^Start of the cross terms: zero (the ols start was not admissible)" "$TMP/gu.out" \
    && grep -q "^Start of the cross terms: ols, the whole matrix" "$TMP/gv.out" \
    && bien "horno de gas, VAR(6): la siembra de toda la matriz es admisible donde la de los cruzados no" \
    || falla "horno de gas: U '$(grep -a 'Start of' "$TMP/gu.out")', V '$(grep -a 'Start of' "$TMP/gv.out")'"
cerca "horno de gas, VAR(6), ruta V: logL" -2638.762693 "$(ll "$TMP/gv.out")" 1e-5
cerca "horno de gas, VAR(6), ruta U: el mismo logL" "$(ll "$TMP/gv.out")" "$(ll "$TMP/gu.out")" 1e-5
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0 -route v -fixarma -o "$TMP/ve" > "$TMP/ve.log" 2>&1
[ $? -ne 0 ] && grep -q "no univariate ARMA to hold" "$TMP/ve.log" \
    && bien "-route v con -fixarma es un error" || falla "-route v acepta -fixarma"
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 0 0 -route v -o "$TMP/ve" > "$TMP/ve.log" 2>&1
[ $? -ne 0 ] && bien "-route v con p = q = 0 es un error" || falla "-route v acepta p = q = 0"

# 10g. -factortest: el contraste del factor de fila (ruta V). El IPI lleva
#     un AR(1)_12; el WTI, nada. VAR(1): la restriccion son 2 (el retardo 13
#     propio y el 13 cruzado del IPI salen del producto).
lrf() { sed -n 's/.*row factor: LR = \([-0-9.]*\)   df = \([0-9]*\).*/\1 \2/p' "$1" | head -1; }
sinf() { awk '/^    without it / {print $4; exit}' "$1"; }
"$BIN" "$MON/IPI_m07.pre" "$MON/WTI_m03.pre" 1 0 -route v -factortest -o "$TMP/ft1" > /dev/null 2>&1
set -- $(lrf "$TMP/ft1.out")
[ "${2:-}" = 2 ] && bien "-factortest, VAR(1): 2 grados de libertad" || falla "-factortest, VAR(1): g.l. ${2:-?}"
cerca "-factortest, VAR(1): LR" 1.5532 "${1:-nan}" 1e-3
# el mismo modelo sin el factor, por otro camino: un fichero del IPI con un
# AR regular de orden 13 libre en 1, 12 y 13, y -zero en los cruzados que el
# producto no alcanza. La escalera de siempre, sin -route v.
python3 - "$MON/IPI_m07.pre" "$TMP/IPI_libre.inp" <<'PY'
import sys
L = open(sys.argv[1], errors="replace").read().split("\n")
i = [k for k, l in enumerate(L) if "regular AR operators" in l][0]
j = [k for k, l in enumerate(L) if "regular MA operators" in l][0]
b = ["**Number and orders of regular AR operators:", "1 13", "**"]
b += ["0.1 1" if lag in (1, 12, 13) else "0.000000 0" for lag in range(1, 14)]
b += ["** Number and orders of annual AR operators:", "0"]
open(sys.argv[2], "w").write("\n".join(L[:i] + b + L[j:]))
PY
Z=""
for k in 2 3 4 5 6 7 8 9 10 11 12; do Z="$Z,AR$k[IPI<-WTI]"; done
for k in 2 3 4 5 6 7 8 9 10 11 12 13; do Z="$Z,AR$k[WTI<-IPI]"; done
"$BIN" "$TMP/IPI_libre.inp" "$MON/WTI_m03.pre" 13 0 -zero "${Z#,}" -o "$TMP/ft1i" > /dev/null 2>&1
cerca "-factortest, VAR(1): el modelo sin el factor es el del camino independiente" \
    "$(ll "$TMP/ft1i.out")" "$(sinf "$TMP/ft1.out")" 1e-5
[ "$(np "$TMP/ft1i.out")" = 11 ] && grep -q "without it .* 11 parameters" "$TMP/ft1.out" \
    && bien "-factortest, VAR(1): 11 parametros sin el factor, por los dos caminos" \
    || falla "-factortest, VAR(1): parametros del modelo sin el factor"
"$BIN" "$MON/IPI_m07.pre" "$MON/WTI_m03.pre" 3 0 -route v -factortest -o "$TMP/ft3" > /dev/null 2>&1
set -- $(lrf "$TMP/ft3.out")
[ "${2:-}" = 6 ] && bien "-factortest, VAR(3): 6 grados de libertad" || falla "-factortest, VAR(3): g.l. ${2:-?}"
"$BIN" "$MON/IPI_m07.pre" "$MON/WTI_m03.pre" 1 0 -route v -factortest -forecast 6 -o "$TMP/ftf" > /dev/null 2>&1
[ "$(lrf "$TMP/ftf.out")" = "$(lrf "$TMP/ft1.out")" ] && [ -s "$TMP/ftf.forecast" ] \
    && bien "-factortest con -forecast: el mismo contraste, despues de prever" \
    || falla "-factortest con -forecast: '$(lrf "$TMP/ftf.out")' frente a '$(lrf "$TMP/ft1.out")'"
"$BIN" "$MON/IPI_m07.pre" "$MON/WTI_m03.pre" 1 1 -route v -factortest -o "$TMP/ft11" > /dev/null 2>&1
set -- $(lrf "$TMP/ft11.out")
[ "${2:-}" = 2 ] && grep -q "with the row factor      logL    -924.846133    13 parameters" "$TMP/ft11.out" \
    && python3 -c "import sys; sys.exit(0 if float(sys.argv[1]) >= 0 else 1)" "${1:-nan}" \
    && bien "-factortest, VARMA(1,1): parte del optimo del porte, 2 g.l., LR >= 0" \
    || falla "-factortest, VARMA(1,1): $(lrf "$TMP/ft11.out")"
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0 -route v -factortest -o "$TMP/ftn" > /dev/null 2>&1
grep -q "there is no" "$TMP/ftn.out" && grep -q "row factor to test" "$TMP/ftn.out" \
    && bien "-factortest sin factores estacionales: lo dice" || falla "-factortest sin factor: no lo dice"
"$BIN" "$MON/WTI_m03.pre" "$MON/GS2_m05.pre" 1 0 -factortest -o "$TMP/fte" > "$TMP/fte.log" 2>&1
[ $? -ne 0 ] && bien "-factortest sin -route v es un error" || falla "-factortest sin -route v pasa"

# 10h. -echelon: la forma echelon con los indices de Kronecker dados. Los
#     numeros son los del porte, Ladder(echelon=n, own="free", start="zero"),
#     en los casos en que los dos certifican el optimo.
EI=$MON/IPC_m07.pre; EW=$MON/WTI_m03.pre
ech() { "$BIN" "$EI" "$EW" 0 0 -echelon "$1" -o "$TMP/e$2" > "$TMP/e$2.log" 2>&1; }
ech 1,0 10; ech 1,1 11; ech 2,1 21; ech 1,2 12
cerca "echelon (1,0), IPC y WTI: el logL del porte" -638.329701 "$(ll "$TMP/e10.out")" 1e-5
cerca "echelon (1,1): el logL del porte"            -630.843671 "$(ll "$TMP/e11.out")" 1e-5
cerca "echelon (2,1): el logL del porte"            -629.155884 "$(ll "$TMP/e21.out")" 1e-5
cerca "echelon (1,2): el logL del porte"            -628.703020 "$(ll "$TMP/e12.out")" 1e-5
[ "$(np "$TMP/e10.out")" = 8 ] && [ "$(np "$TMP/e11.out")" = 12 ] && [ "$(np "$TMP/e21.out")" = 16 ] \
    && [ "$(np "$TMP/e12.out")" = 15 ] \
    && bien "echelon: 8, 12, 16 y 15 parametros, los del porte" \
    || falla "echelon: parametros $(np "$TMP/e10.out") $(np "$TMP/e11.out") $(np "$TMP/e21.out") $(np "$TMP/e12.out")"
# A_0: el termino contemporaneo r <- c existe solo si c < r y n_c > n_r
grep -q "AR0\[WTI<-IPC\]" "$TMP/e21.out" && grep -q "AR0\[WTI<-IPC\]" "$TMP/e10.out" \
    && ! grep -q "AR0\[" "$TMP/e12.out" && ! grep -q "AR0\[" "$TMP/e11.out" \
    && bien "echelon: AR0[WTI<-IPC] en (2,1) y (1,0); ninguno en (1,2) ni en (1,1)" \
    || falla "echelon: los terminos de A_0 no son los de la tabla de indices"
# con los indices iguales es el VARMA(n,n) de la ruta V
"$BIN" "$EI" "$EW" 1 1 -route v -o "$TMP/ev11" > /dev/null 2>&1
cerca "echelon (1,1) es -route v 1 1" "$(ll "$TMP/ev11.out")" "$(ll "$TMP/e11.out")" 1e-5
# tres series, con el factor de fila del IPI: (0,1,1)
"$BIN" "$MON/IPI_m07.pre" "$EI" "$EW" 0 0 -echelon 0,1,1 -o "$TMP/e011" > /dev/null 2>&1
cerca "echelon (0,1,1), IPI, IPC y WTI, con factor de fila: el logL del porte" \
    -835.732462 "$(ll "$TMP/e011.out")" 1e-5
[ "$(np "$TMP/e011.out")" = 19 ] && grep -q "^Model            : echelon form, Kronecker indices 0,1,1" "$TMP/e011.out" \
    && bien "echelon (0,1,1): 19 parametros, y la cabecera dice los indices" \
    || falla "echelon (0,1,1): $(np "$TMP/e011.out") parametros"
"$BIN" "$EI" "$EW" 0 0 -echelon 1,1,0 -o "$TMP/ex" > "$TMP/ex.log" 2>&1
[ $? -ne 0 ] && grep -q "give 2 Kronecker indices" "$TMP/ex.log" \
    && bien "-echelon con otro numero de indices que de series es un error" || falla "-echelon acepta 3 indices para 2 series"
"$BIN" "$EI" "$EW" 1 0 -echelon 2,1 -o "$TMP/ex" > "$TMP/ex.log" 2>&1
[ $? -ne 0 ] && bien "-echelon con ordenes cruzados que no son 0 0 ni el mayor indice es un error" \
    || falla "-echelon acepta p = 1 con indices 2,1"
"$BIN" "$MON/IPI_m07.pre" "$EW" 0 0 -echelon 2,1 -factortest -o "$TMP/ex" > "$TMP/ex.log" 2>&1
[ $? -ne 0 ] && bien "-factortest con indices desiguales: todavia no, y lo dice" \
    || falla "-factortest con indices desiguales pasa"

# 10i. -echelon search: la busqueda de los indices de Kronecker (etapas I y
#     II de Lutkepohl y Poskitt). Los numeros son los de echelon.search del
#     porte (search_row_factors, una pasada): la tabla entera coincide a seis
#     decimales en los seis casos comparados. Aqui se fijan los indices
#     elegidos y las filas bien condicionadas.
elig() { sed -n 's/^  Chosen by [A-Z]*: \([0-9,]*\) .*/\1/p' "$1" | head -1; }
otro() { sed -n 's/^  Chosen by .*(by [A-Z]*: \([0-9,]*\)).*/\1/p' "$1" | head -1; }
fila() { awk -v n="$2" '$1 == n && NF >= 5 {print $2, $3; exit}' "$1"; }
ncan() { sed -n 's/.* \([0-9]*\) candidates\..*/\1/p' "$1" | head -1; }
"$BIN" "$EI" "$EW" 0 0 -echelon search -o "$TMP/s1" > /dev/null 2>&1
[ "$(elig "$TMP/s1.out")" = "1,0" ] && [ "$(otro "$TMP/s1.out")" = "1,1" ] && [ "$(ncan "$TMP/s1.out")" = 16 ] \
    && bien "echelon search, IPC y WTI: 16 candidatos, (1,0) por SC y (1,1) por HQ, como el porte" \
    || falla "echelon search, IPC y WTI: $(elig "$TMP/s1.out") / $(otro "$TMP/s1.out"), $(ncan "$TMP/s1.out") candidatos"
set -- $(fila "$TMP/s1.out" 1,0)
[ "${1:-}" = 4 ] && bien "echelon search: (1,0) tiene 4 coeficientes" || falla "echelon search: dim de (1,0) ${1:-?}"
cerca "echelon search: log|S| de (1,0), el del porte" 0.282751 "${2:-nan}" 1e-5
set -- $(fila "$TMP/s1.out" 1,1)
cerca "echelon search: log|S| de (1,1), el del porte" 0.202069 "${2:-nan}" 1e-5
grep -q "long VAR(1) by least squares" "$TMP/s1.out" && ! grep -q "ESTIMATED MODEL" "$TMP/s1.out" \
    && bien "echelon search: VAR(1) en la etapa I; da la tabla y no estima por MV" \
    || falla "echelon search: la salida no es solo la tabla"
"$BIN" ../../lib/outvarma/datos/MUSKRAT_m03.pre ../../lib/outvarma/datos/MINK_m02.pre 0 0 -echelon search -o "$TMP/s2" > /dev/null 2>&1
[ "$(elig "$TMP/s2.out")" = "3,1" ] && bien "echelon search, almizclero y vison: (3,1), como el porte" \
    || falla "echelon search, almizclero y vison: $(elig "$TMP/s2.out")"
"$BIN" "$MON/IPI_m07.pre" "$EI" "$EW" 0 0 -echelon search -o "$TMP/s3" > /dev/null 2>&1
[ "$(elig "$TMP/s3.out")" = "0,1,0" ] && [ "$(ncan "$TMP/s3.out")" = 64 ] && grep -q "filtered by its row factor" "$TMP/s3.out" \
    && bien "echelon search con factor de fila (IPI, IPC, WTI): 64 candidatos, (0,1,0), como el porte" \
    || falla "echelon search con factor de fila: $(elig "$TMP/s3.out"), $(ncan "$TMP/s3.out") candidatos"
"$BIN" "$MON/IPI_m07.pre" "$EW" 0 0 -echelon search -nmax 2 -criterion HQ -o "$TMP/s4" > /dev/null 2>&1
[ "$(elig "$TMP/s4.out")" = "2,0" ] && [ "$(ncan "$TMP/s4.out")" = 9 ] \
    && bien "echelon search -nmax 2 -criterion HQ (IPI, WTI): 9 candidatos, (2,0), como el porte" \
    || falla "echelon search HQ: $(elig "$TMP/s4.out")"
# mas de 256 candidatos: los dos pasos de Hannan y Kavalieris
"$BIN" "$MON/IPI_m07.pre" "$EI" "$EW" "$MON/FF_m03.pre" 0 0 -echelon search -nmax 4 -o "$TMP/s5" > /dev/null 2>&1
[ "$(elig "$TMP/s5.out")" = "0,0,0,1" ] && [ "$(ncan "$TMP/s5.out")" = 8 ] && grep -q "Hannan and Kavalieris' two steps" "$TMP/s5.out" \
    && bien "echelon search, cuatro series y -nmax 4: los dos pasos, 8 candidatos, (0,0,0,1), como el porte" \
    || falla "echelon search en dos pasos: $(elig "$TMP/s5.out"), $(ncan "$TMP/s5.out") candidatos"
"$BIN" "$EI" "$EW" 1 0 -echelon search -o "$TMP/sx" > "$TMP/sx.log" 2>&1
[ $? -ne 0 ] && bien "-echelon search con ordenes cruzados distintos de 0 0 es un error" \
    || falla "-echelon search acepta p = 1"

# 10j. -start echelon: la siembra de la etapa II de Lutkepohl y Poskitt. Es
#     _echelon_start del porte: el mismo punto de partida (las mismas
#     lambdas cuando hay que recoger las raices), las mismas iteraciones y
#     el mismo optimo en los casos que certifican.
ini() { sed -n 's/^Start of the cross terms: \(.*\) (-start echelon.*/\1/p' "$1" | head -1; }
eche() { "$BIN" "$EI" "$EW" 0 0 -echelon "$1" -start echelon -o "$TMP/k$2" > "$TMP/k$2.log" 2>&1; }
eche 1,0 10; eche 1,1 11; eche 2,1 21
[ "$(ini "$TMP/k10.out")" = echelon ] && [ "$(ini "$TMP/k11.out")" = echelon ] \
    && bien "-start echelon: el arranque se usa y lo dice" || falla "-start echelon: '$(ini "$TMP/k10.out")'"
cerca "-start echelon, (1,0): el optimo del porte" -638.329701 "$(ll "$TMP/k10.out")" 1e-5
cerca "-start echelon, (1,1): el optimo del porte" -630.843671 "$(ll "$TMP/k11.out")" 1e-5
[ "$(ini "$TMP/k21.out")" = "echelon (AR x 0.388^k, MA x 0.459^k: inside the unit circle)" ] \
    && bien "-start echelon, (2,1): las raices se recogen dentro con las lambdas del porte (0.388, 0.459)" \
    || falla "-start echelon, (2,1): '$(ini "$TMP/k21.out")'"
"$BIN" ../../lib/outvarma/datos/MUSKRAT_m03.pre ../../lib/outvarma/datos/MINK_m02.pre 0 0 -echelon 2,1 -start echelon -o "$TMP/km" > /dev/null 2>&1
cerca "-start echelon, almizclero y vison (2,1): el logL del porte" -557.637479 "$(ll "$TMP/km.out")" 1e-5
"$BIN" "$MON/IPI_m07.pre" "$EI" "$EW" 0 0 -echelon 0,1,1 -start echelon -o "$TMP/kt" > /dev/null 2>&1
cerca "-start echelon, (0,1,1) con factor de fila: el logL del porte" -835.732462 "$(ll "$TMP/kt.out")" 1e-5
"$BIN" "$EI" "$EW" 1 1 -route v -start echelon -o "$TMP/kx" > "$TMP/kx.log" 2>&1
[ $? -ne 0 ] && grep -q "give -echelon" "$TMP/kx.log" \
    && bien "-start echelon sin -echelon es un error" || falla "-start echelon sin -echelon pasa"

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
