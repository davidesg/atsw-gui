#!/bin/bash
# drtran -- Box-Jenkins transfer function models.
# Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.  Distributed WITHOUT ANY WARRANTY; see COPYING for details.
# drtran — Batería de tests
#
# Se apoya en TRES fuentes de verdad, en este orden de importancia:
#
#   1. HOMOLOGACIÓN CON FUE. Estimar conjuntamente dos modelos de fue con
#      estructura diagonal y sin transferencia debe reproducir a fue ejecutado
#      sobre cada serie por separado (coeficientes Y suma de log-verosimilitudes).
#      Si esto falla, nada de lo demás es creíble.
#   2. VERDAD SINTÉTICA. Simular Y = nu(B)X + N con (b, r, s, omega) conocidos y
#      exigir que se recuperen.
#   3. PASS-THROUGH. Con Y = X la verdad es omega_0 = 1 (NO cero).
#
# La batería anterior afirmaba que el pass-through debía dar omega_0 ~ 0 y daba
# PASS: certificaba el bug en lugar de detectarlo.

set -uo pipefail

DRTRAN="./bin/drtran"
WORK="tests/cases"     # los .pre de trabajo viven aquí; examples/ es solo ilustrativo
SYN="tests/data"
PASS=0; FAIL=0
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS=$((PASS+1)); }
fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL=$((FAIL+1)); }

# check <desc> <esperado> <obtenido> [tol]
check() {
    local desc="$1" exp="$2" got="$3" tol="${4:-0.01}"
    if [ -z "$got" ]; then fail "$desc (sin valor)"; return; fi
    if python3 -c "import sys; sys.exit(0 if abs($got - ($exp)) < $tol else 1)"; then
        pass "$desc = $got  (esperado ~$exp)"
    else
        fail "$desc = $got  (ESPERADO ~$exp, tol $tol)"
    fi
}

# valor de un parámetro en la tabla de resultados
val() { grep -E "^$2" "$1" | head -1 | awk '{print $2}'; }

# sig <fichero> <i> <j>: elemento (i,j) de la matriz Sigma = sigma2*Q del .out
sig() {
    grep -A"$2" "Sigma = sigma2 \* Q" "$1" | tail -1 | awk -v c="$3" '{print $c}'
}

# El optimizador no debe FALLAR (límite de iteraciones / pasos máximos).
# "PARADA en un punto sin mejora" es legítimo: es lo que ocurre al arrancar ya
# en el óptimo, y no impide que el punto sea el correcto.
opt_ok() {
    if grep -q "NO CONVERGENCE" "$1"; then fail "$2 (el optimizador FALLÓ)"
    else pass "$2"; fi
}

echo "============================================"
echo "  DRTRAN — Batería de tests"
echo "  $(date)"
echo "============================================"

[ -x "$DRTRAN" ] || { echo "No existe $DRTRAN — ejecuta 'make'"; exit 1; }

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 1. HOMOLOGACIÓN CON FUE (la puerta de entrada) ──"
echo "   Estimación conjunta diagonal, sin transferencia, de dos modelos de fue."
echo "   Debe reproducir a fue ejecutado por separado sobre cada serie."
echo ""
echo "   fue: ES_CPI  phi=0.402839  mu=0.154472  sigma2=0.062666  logL=  -7.3917"
echo "        WTI     phi=0.299193  mu=0 (fija)  sigma2=68.8381   logL=-760.0326"
echo "        suma logL = -767.4243"
echo ""

OUT="$TMPDIR/homolog.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

opt_ok "$OUT" "el optimizador no falla"

check "phi_N (ES_CPI)"  0.402839 "$(val "$OUT" 'phi_1\[B\^1\]')"  0.0001
check "phi_X (WTI)"     0.299193 "$(val "$OUT" 'phi_2\[B\^1\]')"  0.0001
check "mu_Y   (ES_CPI)" 0.154472 "$(val "$OUT" 'mu\[1\]')"     0.0001

# Sigma = sigma2 * Q  (Q por sí sola NO es la covarianza)
S11=$(sig "$OUT" 1 1)
S22=$(sig "$OUT" 2 2)
check "Sigma[1,1] (ES_CPI)"  0.062666 "$S11" 0.0005
check "Sigma[2,2] (WTI)"    68.838100 "$S22" 0.01

LOGL=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
check "logL conjunta = suma de las univariantes" -767.4243 "$LOGL" 0.01

# El WTI trae mu fija en el .pre: drtran debe respetarlo
grep -q "mu\[2\].*fixed" "$OUT" && pass "mu_X respeta el flag del .pre (fija)" \
                                || fail "mu_X NO respeta el flag del .pre"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 1b. HOMOLOGACIÓN, casos reales de SF_MEG ──"
echo "   Los tres tipos de modelo del estudio de inflación, cada uno ejercitando"
echo "   una parte distinta del motor. Referencias: los .out de fue."
echo ""

CASES="tests/cases"

# --- Airline de Box–Jenkins: ∇∇₁₂ + MA anual ---------------------------------
# Ejercita: diferencia estacional (D=1) y factores ARMA ANUALES (retardo 12).
# Además Y pierde 13 observaciones y X (WTI) solo 1 -> ventana común.
echo "   [airline AR] ES_CPI_airAR_mu: (1-0.3631B)(∇∇₁₂ ln y + 0.0145) = (1-0.8575B¹²)a"
OUT="$TMPDIR/air.txt"
$DRTRAN "$CASES/ES_CPI_airAR_mu.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

check "phi   (B^1)"   0.363119 "$(val "$OUT" 'phi_1\[B\^1\]')"    0.0001
check "Theta (B^12)"  0.857450 "$(val "$OUT" 'theta_1\[B\^12\]')" 0.0001
check "mu"           -0.014475 "$(val "$OUT" 'mu\[1\]')"        0.0001
check "Sigma[1,1]"    0.069328 "$(sig "$OUT" 1 1)" 0.0005

# --- Airline puro: MA regular + MA anual (sin media) --------------------------
echo "   [airline MA] ES_CPI_airline: ∇∇₁₂ ln y = (1+0.4212B)(1-0.8147B¹²)a"
OUT="$TMPDIR/air2.txt"
$DRTRAN "$CASES/ES_CPI_airline.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

check "theta (B^1)"  -0.421156 "$(val "$OUT" 'theta_1\[B\^1\]')"  0.0005
check "Theta (B^12)"  0.814706 "$(val "$OUT" 'theta_1\[B\^12\]')"  0.0005
check "Sigma[1,1]"    0.071022 "$(sig "$OUT" 1 1)" 0.0005

# --- MEG / estacionalidad estocástica -----------------------------------------
# Ejercita: factor irreducible de la diferencia anual (ifadf[3]=1), AR anual y
# factor MA de FRECUENCIA FIJA, cuyo término en B se deriva de c2:
#     c1 = 2·cos(2πf/s)·sqrt(-c2)
echo "   [MEG] ES_CORE_S3: AR(3) + AR₁₂ + MA de frecuencia fija en f=3, ifadf[3]=1"
OUT="$TMPDIR/meg.txt"
$DRTRAN "$CASES/ES_CORE_S3.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

check "phi_1  (B^1)"   0.186067 "$(val "$OUT" 'phi_1\[B\^1\]')"   0.0002
check "phi_2  (B^2)"   0.138391 "$(val "$OUT" 'phi_1\[B\^2\]')"   0.0002
check "phi_3  (B^3)"   0.205844 "$(val "$OUT" 'phi_1\[B\^3\]')"   0.0002
check "Phi    (B^12)"  0.295467 "$(val "$OUT" 'phi_1\[B\^12\]')"   0.0002
check "MA f=3 (c2)"   -0.950159 "$(val "$OUT" 'theta_1\[f=3\]')" 0.0005
check "mu"             0.266056 "$(val "$OUT" 'mu\[1\]')"        0.0002
check "Sigma[1,1]"     0.016968 "$(sig "$OUT" 1 1)" 0.0005

# --- MEG con TRES frecuencias: c1 != 0 y de ambos signos ----------------------
# El término en B de un factor de frecuencia fija se DERIVA de c2:
#     c1 = 2·cos(2πf/s)·sqrt(-c2)
# En f=3 sale 0 (cos(π/2)=0), así que ese caso no distingue una implementación
# correcta de una que ignore c1. En f=1 vale +1.66 y en f=5 vale -1.67: estos SÍ.
echo "   [MEG x3] ES_CORE_S135b: MA de frecuencia fija en f=3, f=1 y f=5"
OUT="$TMPDIR/meg3.txt"
$DRTRAN "$CASES/ES_CORE_S135b.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

check "phi_1 (B^1)"    0.257507 "$(val "$OUT" 'phi_1\[B\^1\]')"   0.0002
check "phi_3 (B^3)"    0.312740 "$(val "$OUT" 'phi_1\[B\^3\]')"   0.0002
check "MA f=3 (c1=0)" -0.908012 "$(val "$OUT" 'theta_1\[f=3\]')"  0.0005
check "MA f=1 (c1>0)" -0.924052 "$(val "$OUT" 'theta_1\[f=1\]')"  0.0005
check "MA f=5 (c1<0)" -0.924685 "$(val "$OUT" 'theta_1\[f=5\]')"  0.0005
check "mu"             0.257980 "$(val "$OUT" 'mu\[1\]')"         0.0002
check "Sigma[1,1]"     0.016176 "$(sig "$OUT" 1 1)" 0.0005

# --- Coeficiente ARMA FIJADO en el .pre ---------------------------------------
# FR_CPI_f5 trae "0.0000  0": el AR(1) está FIJO, no es un valor inicial.
echo "   [f=5 + AR fijo] FR_CPI_f5: AR(1) fijado en 0 por el .pre"
OUT="$TMPDIR/fr5.txt"
$DRTRAN "$CASES/FR_CPI_f5.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

grep -q "phi_1\[B\^1\].*fixed" "$OUT" && pass "el AR(1) respeta el flag del .pre (fijo)" \
                                      || fail "el AR(1) NO respeta el flag del .pre"
check "MA f=5 (c2)"   -0.954212 "$(val "$OUT" 'theta_1\[f=5\]')"  0.0005
check "Sigma[1,1]"     0.046484 "$(sig "$OUT" 1 1)" 0.0005

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 1c. ROBUSTEZ AL ARRANQUE ──"
echo "   El óptimo debe ser un atractor, no un eco de los valores iniciales."
echo "   Se perturban las preestimaciones ARMA del .pre y debe converger al mismo"
echo "   sitio. (El pecado original del proyecto era justo ese: los parámetros no"
echo "   se movían de su valor inicial y eso se leía como 'coincide con fue'.)"
echo ""

PERT="$TMPDIR/S3_perturb.pre"
python3 - "$CASES/ES_CORE_S3.pre" "$PERT" <<'EOF'
import sys
src, dst = sys.argv[1], sys.argv[2]
rep = {'0.1861': '0.0500', '0.1384': '0.0500', '0.2058': '0.0500',
       '0.2955': '0.0500', '-0.9502': '-0.5000'}
out = []
for line in open(src).read().splitlines():
    s = line.strip()
    for k, v in rep.items():
        if s.startswith(k):
            line = v + ' 1'
            break
    out.append(line)
open(dst, 'w').write('\n'.join(out) + '\n')
EOF

OUT="$TMPDIR/pert.txt"
$DRTRAN "$PERT" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

opt_ok "$OUT" "el optimizador no falla desde un arranque lejano"

# ...y al MISMO óptimo que con las preestimaciones de fue
check "phi_1  (perturbado)"  0.186067 "$(val "$OUT" 'phi_1\[B\^1\]')"   0.0002
check "phi_3  (perturbado)"  0.205844 "$(val "$OUT" 'phi_1\[B\^3\]')"   0.0002
check "Phi    (perturbado)"  0.295467 "$(val "$OUT" 'phi_1\[B\^12\]')"  0.0002
check "MA f=3 (perturbado)" -0.950159 "$(val "$OUT" 'theta_1\[f=3\]')"  0.0005
check "mu     (perturbado)"  0.266056 "$(val "$OUT" 'mu\[1\]')"         0.0002

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 1d. ERRORES ESTÁNDAR ──"
echo "   No basta con que los PUNTOS coincidan: las SE también deben ser correctas."
echo "   Referencia: GLS exacto sobre los datos (mu + 11 deterministas + AR(1)) y"
echo "   la teoría del AR(1), SE(phi) = sqrt((1-phi²)/n)."
echo ""
echo "   Dos bugs vivían aquí y esta sección los cierra:"
echo "     - el hessiano venía ACUMULADO por BFGS, no calculado en el óptimo;"
echo "     - Q tenía la escala redundante con la sigma2 concentrada -> hessiano"
echo "       SINGULAR (se veían SE de 4·10^5)."
echo ""

# se <fichero> <patron> : error estandar (columna 3) de un parametro
se() { grep -E "^$2" "$1" | head -1 | awk '{print $3}'; }

OUT="$TMPDIR/homolog.txt"   # el caso canónico, ya estimado arriba

check "SE(phi_N)  [teoría 0.06242]"  0.062421 "$(se "$OUT" 'phi_1\[B\^1\]')" 0.001
check "SE(phi_X)  [teoría 0.06508]"  0.065075 "$(se "$OUT" 'phi_2\[B\^1\]')" 0.001
check "SE(mu)     [GLS 0.028502]"    0.028502 "$(se "$OUT" 'mu\[1\]')"       0.001

# los deterministas: GLS exacto sobre el diseño diferenciado
check "SE(det f=1 cos) [GLS 0.06833]" 0.068328 "$(se "$OUT" 'omega_d1\[1,0\]')"  0.002
check "SE(det f=1 sin) [GLS 0.06829]" 0.068294 "$(se "$OUT" 'omega_d1\[2,0\]')"  0.002
check "SE(det f=2 cos) [GLS 0.02769]" 0.027692 "$(se "$OUT" 'omega_d1\[3,0\]')"  0.001
check "SE(alternador)  [GLS 0.00609]" 0.006094 "$(se "$OUT" 'omega_d1\[11,0\]')" 0.001

# ninguna SE puede ser absurda (la firma del hessiano singular)
if grep -qE "^(phi|theta|mu|omega|delta|log)" "$OUT" | true; then :; fi
BIG=$(grep -E "^(phi_|theta_|mu\[1|omega_|delta_|log\()" "$OUT" \
      | awk '{if ($3+0 > 10) print $1}' | head -1)
[ -z "$BIG" ] && pass "ninguna SE absurda (hessiano no singular)" \
              || fail "SE absurda en $BIG (¿hessiano singular?)"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 1e. EL CAST EMPOTRADO (-V): la transferencia DENTRO del VARMA ──"
echo "   El cast por RESTA construye el ruido fuera de elf:"
echo "       N_t = w_Y,t - SUM_k nu_k w_X,{t-k}"
echo "   y en t=1 necesita w_X en instantes que NO EXISTEN. Los pone a cero. elf no"
echo "   puede arreglarlo porque nunca ve esas X: recibe el ruido ya contaminado."
echo ""
echo "   El cast EMPOTRADO no resta nada. Escribe la transferencia como coeficientes"
echo "   FUERA DE LA DIAGONAL del VARMA:"
echo "       [phi_i*D_i] w_i - SUM_k [phi_i*omega_k*B^bk*(D_i/delta_k)] w_inp = [D_i*theta_i] a_i"
echo "   y elf, que ve el sistema entero, hace la inicialización pre-muestral EXACTA."
echo "   El truncamiento no se arregla: DESAPARECE. Es lo que hacen los m6 de Mauricio."
echo ""

# --- LA PUERTA: sin transferencia, -V debe seguir homologando con fue ---
OUT="$TMPDIR/v_hom.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -0 -V -o "$OUT" >/dev/null 2>&1
check "-V homologa con fue: phi_N"  0.402839 "$(val "$OUT" 'phi_1\[B\^1\]')" 0.0001
check "-V homologa con fue: phi_X"  0.299193 "$(val "$OUT" 'phi_2\[B\^1\]')" 0.0001
check "-V homologa con fue: mu"     0.154472 "$(val "$OUT" 'mu\[1\]')"       0.0001
check "-V homologa con fue: logL"  -767.4243 "$(grep 'Log-likelihood =' "$OUT" | awk '{print $3}')" 0.01

# --- los DOS cast deben dar lo MISMO (la diferencia es solo el pre-muestral) ---
A="$TMPDIR/v_sub.txt"; B="$TMPDIR/v_emb.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0    -o "$A" >/dev/null 2>&1
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -V -o "$B" >/dev/null 2>&1
check "los dos cast coinciden en omega_0" "$(val "$A" 'omega1\[0\]')" "$(val "$B" 'omega1\[0\]')" 0.005
check "los dos cast coinciden en delta_1" "$(val "$A" 'delta1\[1\]')" "$(val "$B" 'delta1\[1\]')" 0.005
check "y recuperan la verdad (omega=0.600)" 0.600 "$(val "$B" 'omega1\[0\]')" 0.03
check "y recuperan la verdad (delta=0.600)" 0.600 "$(val "$B" 'delta1\[1\]')" 0.03

# --- la verosimilitud EMPOTRADA es la EXACTA: no es la del cast por resta ---
LA=$(grep "Log-likelihood =" "$A" | awk '{print $3}')
LB=$(grep "Log-likelihood =" "$B" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if abs($LB - ($LA)) > 1e-6 else 1)" \
    && pass "las dos verosimilitudes DIFIEREN ($LB vs $LA): la del cast por resta no es exacta" \
    || fail "las verosimilitudes coinciden: el pre-muestral no se está tratando distinto"

# --- aguanta los casos DIFÍCILES (órdenes altos) ---
for C in ES_CORE_S135b ES_CPI_airline FR_CPI_msar DE_CPI_mar3sar; do
    $DRTRAN "$WORK/$C.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -o "$TMPDIR/v_$C.txt" >/dev/null 2>&1
    grep -q "Log-likelihood =" "$TMPDIR/v_$C.txt" \
        && pass "-V aguanta $C" \
        || fail "-V falla en $C"
done



# --- ¿SON ORTOGONALES LOS RESIDUOS? ---
# Los ESTRUCTURALES sí: Q es diagonal, y ése es el supuesto del modelo.
# Los de la FORMA REDUCIDA no, cuando b=0: la representación VARMA pone omega_0 en
# el retardo cero, Phi(0) != I, y Sigma_12 = omega_0 * Sigma_22 aparece SOLA.
OB0="$TMPDIR/orth_b0.txt"; OB1="$TMPDIR/orth_b1.txt"; OSU="$TMPDIR/orth_sub.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -o "$OB0" >/dev/null 2>&1
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 1 -r 0 -s 0 -V -o "$OB1" >/dev/null 2>&1
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1    -o "$OSU" >/dev/null 2>&1

rho() { grep -A1 "Innovation correlations" "$1" | tail -1 | awk '{print $2}'; }
check "b=0 empotrado: los residuos REDUCIDOS NO son ortogonales" 0.5639 "$(rho "$OB0")" 0.01
check "b=1 empotrado: Phi(0)=I, ya son ortogonales"              0.0    "$(rho "$OB1")" 1e-9
check "cast por resta: la serie 1 ES el ruido, ortogonales"      0.0    "$(rho "$OSU")" 1e-9

# LA CLAVE: des-normalizar con Phi(0) ES un Cholesky con el INPUT ordenado PRIMERO.
# Si es cierto, el factor de Cholesky de Sigma debe devolver omega_0 exactamente.
python3 - "$OB0" <<'PYCH'
import math, re, sys
t = open(sys.argv[1]).read()
S = [[float(x) for x in r.split()] for r in
     re.search(r'Sigma = sigma2 \* Q.*\n((?: +[-\d.]+ +[-\d.]+\n){2})', t).group(1).strip().split('\n')]
w0 = float(re.search(r'^omega1\[0\]\s+(\S+)', t, re.M).group(1))
sYY, sYX, sXX = S[0][0], S[0][1], S[1][1]
L11 = math.sqrt(sXX); L21 = sYX / L11          # Cholesky con X (el input) PRIMERO
sys.exit(0 if abs(L21 / L11 - w0) < 1e-5 else (print("%.6f vs %.6f" % (L21/L11, w0)) or 1))
PYCH
[ $? -eq 0 ] \
    && pass "el Cholesky de Sigma con el INPUT primero devuelve exactamente omega_0" \
    || fail "el Cholesky no reproduce omega_0"

grep -q "ARBITRARINESS" "$OB0" \
    && pass "y el informe lo dice: no se escapa de ortogonalizar, sino de ELEGIR el orden" \
    || fail "el informe no advierte de que la Sigma es la REDUCIDA"

# --- LA COVARIANZA NO SE ESTIMA: SALE DE LA ESTRUCTURA ---
echo ""
echo "   ¿Hay que estimar la matriz de covarianzas con la FLT dentro del VARMA?"
echo "   NO. La Q ESTRUCTURAL sigue siendo diagonal. La Q REDUCIDA es"
echo "       Q_red = Phi(0)^-1 . Q . Phi(0)^-T,     Phi(0) = [[1, -omega_0],[0,1]]"
echo "   o sea Sigma_12 = omega_0 * Sigma_22: una FUNCIÓN de omega_0, no un"
echo "   parámetro nuevo. Es la identidad del SVAR: un coeficiente estructural"
echo "   contemporáneo GENERA la covarianza de la forma reducida. Por eso no se"
echo "   pueden estimar los dos (sección 2g/3d)."
echo ""

V0="$TMPDIR/v_b0.txt"; V1="$TMPDIR/v_b1.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -o "$V0" >/dev/null 2>&1
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 1 -r 0 -s 0 -V -o "$V1" >/dev/null 2>&1

# con b=1 (sin contemporánea) Phi(0)=I y la covarianza reducida es CERO
check "b=1: sin término contemporáneo, Sigma es DIAGONAL" 0.0 "$(sig "$V1" 1 2)" 1e-9

# con b=0 la covarianza aparece SOLA, y vale exactamente omega_0 * Sigma_22
W0=$(val "$V0" 'omega1\[0\]')
S22=$(sig "$V0" 2 2)
S12=$(sig "$V0" 1 2)
python3 -c "import sys; sys.exit(0 if abs($S12 - $W0*$S22) < 1e-3 else 1)" \
    && pass "b=0: Sigma_12 = omega_0 * Sigma_22 ($S12 = $W0 x $S22): NO se estima, se DEDUCE" \
    || fail "Sigma_12 no coincide con omega_0 * Sigma_22"

# y no cuesta un grado de libertad: la covarianza no es un parámetro
NF0=$(grep "Structural parameters" "$V0" | sed 's/.*free: \([0-9]*\).*/\1/')
NF1=$(grep "Structural parameters" "$V1" | sed 's/.*free: \([0-9]*\).*/\1/')
check "la covarianza NO cuesta un parámetro (b=0 tiene uno más SOLO por omega_1)" \
      "$((NF1 + 1))" "$NF0" 0.5

# --- y la RED ---
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$SYN/SYNC.net" -V \
        -o "$TMPDIR/v_net.txt" >/dev/null 2>&1
check "-V con la RED: omega Y<-M (verdad 0.500)" 0.500 "$(val "$TMPDIR/v_net.txt" 'omega1\[0\]')" 0.05
check "-V con la RED: omega M<-X (verdad 0.700)" 0.700 "$(val "$TMPDIR/v_net.txt" 'omega2\[0\]')" 0.05

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2. VERDAD SINTÉTICA: se recupera la transferencia conocida ──"
echo "   Y = nu(B)X + N  con  b=2, r=0, s=1, omega=(0.8, 0.4)"
echo ""

if [ ! -f "$SYN/SYN_Y.pre" ]; then
    python3 tests/gen_synthetic.py "$SYN" > /dev/null
fi

OUT="$TMPDIR/synth.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -r 0 -s 1 -b 2 -o "$OUT" > /dev/null 2>&1

opt_ok "$OUT" "el optimizador no falla"

# tolerancias ~2 errores estándar: es una muestra finita, no aritmética exacta
check "omega_0"  0.800 "$(val "$OUT" 'omega1\[0\]')"  0.06
check "omega_1"  0.400 "$(val "$OUT" 'omega1\[1\]')"  0.06
check "phi_N"    0.300 "$(val "$OUT" 'phi_1\[B\^1\]')"  0.12
check "phi_X"    0.500 "$(val "$OUT" 'phi_2\[B\^1\]')"  0.12

S11=$(sig "$OUT" 1 1)
S22=$(sig "$OUT" 2 2)
check "Sigma ruido"    0.25 "$S11" 0.06
check "Sigma entrada"  1.00 "$S22" 0.20

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2b. IDENTIFICACIÓN: el preblanqueo debe PROPONER los órdenes verdaderos ──"
echo "   Sin órdenes en la línea de comandos, drtran preblanquea la entrada, filtra"
echo "   la salida con el mismo filtro y lee (b, r, s) de la CCF."
echo ""

# --- caso simple: b=2, r=0, s=1 ---
OUT="$TMPDIR/id1.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -o "$OUT" > /dev/null 2>&1
REC=$(grep "RECOMMENDED" "$OUT" | head -1)
echo "   [b=2, r=0, s=1] $REC"
echo "$REC" | grep -q "b=2, r=0, s=1" && pass "identifica b=2, r=0, s=1" \
                                      || fail "NO identifica b=2, r=0, s=1 -> $REC"

# los pesos nu(k) deben preestimar los omega verdaderos (0.8 y 0.4).
# Se leen SOLO de la tabla de nu (el gráfico de la CCF también empieza por el lag).
nuval() {
    awk -v k="$2" '
        /k      r\(k\)      nu\(k\)/ { inb = 1; next }
        inb && /^ *$/                   { inb = 0 }
        inb && $1 == k && NF >= 3       { print $3; exit }
    ' "$1"
}
NU2=$(nuval "$OUT" 2)
NU3=$(nuval "$OUT" 3)
check "nu(2) preestima omega_0" 0.80 "$NU2" 0.08
check "nu(3) preestima omega_1" 0.40 "$NU3" 0.08

# la CCF no debe tener picos en k<0 (X es exógena por construcción)
grep -q "X behaves as exogenous" "$OUT" && pass "no detecta retroalimentación (X exógena)" \
                                  || fail "detecta retroalimentación donde no la hay"

# --- caso racional: b=1, r=1, s=0 (la cola decae con razón delta=0.6) ---
OUT="$TMPDIR/id2.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -o "$OUT" > /dev/null 2>&1
REC=$(grep "RECOMMENDED" "$OUT" | head -1)
echo "   [b=1, r=1, s=0] $REC"
echo "$REC" | grep -q "b=1, r=1, s=0" && pass "identifica el DENOMINADOR (r=1)" \
                                      || fail "NO identifica r=1 -> $REC"

grep -q "\[A\]" "$OUT" && grep -q "\[B\]" "$OUT" \
    && pass "ofrece las dos propuestas (omegas libres vs denominador)" \
    || fail "no ofrece ambas propuestas"

# --- caso real: no debe proponer órdenes absurdos por un pico espurio lejano ---
OUT="$TMPDIR/id3.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -o "$OUT" > /dev/null 2>&1
REC=$(grep "RECOMMENDED" "$OUT" | head -1)
echo "   [ES_CPI <- WTI] $REC"
echo "$REC" | grep -q "b=0, r=0, s=1" && pass "IPC<-WTI: propone b=0, r=0, s=1" \
                                      || fail "IPC<-WTI: propuesta inesperada -> $REC"
grep -q "exceeds MAX_S" "$OUT" && fail "un pico espurio lejano dispara s absurdo" \
                              || pass "los picos espurios lejanos no inflan s"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2c. ADECUACIÓN: la CCF ruido-vs-entrada debe delatar una mala (b,r,s) ──"
echo "   En el cast, el residuo de la serie 2 ES la entrada preblanqueada y el de"
echo "   la serie 1 es la innovación del ruido. Si (b,r,s) es correcta, su CCF debe"
echo "   ser ruido blanco: es el chequeo de adecuación de Box–Jenkins."
echo ""

# --- órdenes CORRECTOS -> adecuada ---
OUT="$TMPDIR/adq_ok.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 2 -r 0 -s 1 -o "$OUT" > /dev/null 2>&1
grep -q "transfer is ADEQUATE" "$OUT" \
    && pass "con (b=2,r=0,s=1) [la verdad] declara la transferencia ADECUADA" \
    || fail "con los órdenes correctos NO la declara adecuada"

# no debe inventarse retroalimentación: X es exógena por construcción
grep -q "X behaves as exogenous" "$OUT" \
    && pass "no inventa retroalimentación (X exógena por construcción)" \
    || fail "detecta retroalimentación donde no la hay"

# --- órdenes MAL -> inadecuada, y debe señalar DÓNDE ---
OUT="$TMPDIR/adq_bad.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 0 -r 0 -s 0 -o "$OUT" > /dev/null 2>&1
grep -q "is NOT adequate" "$OUT" \
    && pass "con (b=0,r=0,s=0) [mal] declara la transferencia INADECUADA" \
    || fail "no detecta una transferencia mal especificada"

# y los retardos que señala deben ser los de la transferencia verdadera (2 y 3)
awk '/carries a trace of the input at:/{f=1;next} f&&/^ *k =/{print $3}' "$OUT" \
    | head -2 | tr '\n' ' ' | grep -q "2 3" \
    && pass "señala los retardos correctos (k=2 y k=3, donde vive nu(B))" \
    || fail "no señala los retardos de la transferencia verdadera"

# --- caso real: con los órdenes identificados, debe ser adecuada ---
OUT="$TMPDIR/adq_real.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -o "$OUT" > /dev/null 2>&1
grep -q "transfer is ADEQUATE" "$OUT" \
    && pass "IPC<-WTI: el modelo identificado resulta adecuado" \
    || fail "IPC<-WTI: el modelo identificado NO resulta adecuado"
grep -q "X behaves as exogenous" "$OUT" \
    && pass "IPC<-WTI: el WTI se comporta como exógeno" \
    || fail "IPC<-WTI: detecta retroalimentación IPC -> WTI"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2d. MÚLTIPLES ENTRADAS (m > 2) ──"
echo "   Y = nu1(B)·X1 + nu2(B)·X2 + N, con DOS transferencias distintas."
echo "   VERDAD:  X1: b=1, r=0, s=0, omega=0.70"
echo "            X2: b=0, r=0, s=1, omega=(0.50, 0.30)"
echo ""

OUT="$TMPDIR/m3.txt"
$DRTRAN "$SYN/SYN2_Y.pre" "$SYN/SYN2_X1.pre" "$SYN/SYN2_X2.pre" \
        -b 1,0 -r 0,0 -s 0,1 -o "$OUT" > /dev/null 2>&1

opt_ok "$OUT" "el optimizador no falla con 3 series"

grep -q "Series           : 3 (1 output + 2 input(s))" "$OUT" \
    && pass "el cast es de 3 series (m = 1 + 2)" \
    || fail "el cast NO es de 3 series"

# se recuperan LAS DOS transferencias a la vez (tolerancia ~2-3 SE)
check "omega1[0]  (X1)"  0.700 "$(val "$OUT" 'omega1\[0\]')"  0.06
check "omega2[0]  (X2)"  0.500 "$(val "$OUT" 'omega2\[0\]')"  0.06
check "omega2[1]  (X2)"  0.300 "$(val "$OUT" 'omega2\[1\]')"  0.06

# y el ARMA propio de cada serie
check "phi serie 1 (ruido)" 0.300 "$(val "$OUT" 'phi_1\[B\^1\]')" 0.12
check "phi serie 2 (X1)"    0.500 "$(val "$OUT" 'phi_2\[B\^1\]')" 0.12
check "phi serie 3 (X2)"    0.200 "$(val "$OUT" 'phi_3\[B\^1\]')" 0.12

# la identificación debe proponer los órdenes de CADA entrada por separado
OUT="$TMPDIR/m3id.txt"
$DRTRAN "$SYN/SYN2_Y.pre" "$SYN/SYN2_X1.pre" "$SYN/SYN2_X2.pre" \
        -p -o "$OUT" > /dev/null 2>&1
[ "$(grep -c 'RECOMMENDED' "$OUT")" = "2" ] \
    && pass "el preblanqueo identifica CADA entrada por separado" \
    || fail "no identifica las dos entradas"
grep -q "RECOMMENDED: b=1, r=0, s=0" "$OUT" \
    && pass "identifica el retardo b=1 de X1" \
    || fail "no identifica el retardo de X1"

# previsión con dos entradas: prever Y exige prever LAS DOS
OUT="$TMPDIR/m3fc.txt"
$DRTRAN "$SYN/SYN2_Y.pre" "$SYN/SYN2_X1.pre" "$SYN/SYN2_X2.pre" \
        -b 1,0 -r 0,0 -s 0,1 -f 6 -o "$OUT" > /dev/null 2>&1
grep -q "THE 2 INPUT(S)" "$OUT" \
    && pass "prevé con las dos entradas" \
    || fail "la previsión no contempla las dos entradas"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2e. PARÁMETROS COMPARTIDOS Y FIJOS (-c) ──"
echo "   Un parámetro puede aparecer en VARIOS sitios de la estructura con un"
echo "   solo grado de libertad. Es lo que hace racional a una transferencia"
echo "   dentro de un sistema: en los m6, el mismo x6 está en la dinámica propia"
echo "   de EI y en la transferencia EI->EP."
echo ""

# --- referencia: todo libre ---
OUT="$TMPDIR/cns_free.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -o "$OUT" > /dev/null 2>&1
LL_FREE=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
NF_FREE=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')

# --- COMPARTIR: delta de la transferencia = AR propio de la entrada ---
CNS="$TMPDIR/share.cns"
printf 'delta1[1] = phi_2[B^1]\n' > "$CNS"
OUT="$TMPDIR/cns_share.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -c "$CNS" -o "$OUT" > /dev/null 2>&1

NF_SH=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')
check "compartir quita UN grado de libertad" "$((NF_FREE - 1))" "$NF_SH" 0.5

D1=$(val "$OUT" 'delta1\[1\]')
P2=$(val "$OUT" 'phi_2\[B\^1\]')
check "delta1[1] y phi_2[B^1] valen LO MISMO" "$P2" "$D1" 0.000001
grep -q "delta1\[1\].*(= phi_2\[B\^1\])" "$OUT" \
    && pass "el informe dice con quién comparte" \
    || fail "el informe no indica el parámetro compartido"

# la restricción no puede MEJORAR la verosimilitud
LL_SH=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if $LL_SH <= $LL_FREE + 1e-6 else 1)" \
    && pass "la restricción no aumenta la verosimilitud (como debe)" \
    || fail "compartir AUMENTA la verosimilitud: imposible"

# --- FIJAR un coeficiente a un valor ---
CNS="$TMPDIR/fix.cns"
printf '# fijar el denominador en su valor verdadero\ndelta1[1] = 0.6\n' > "$CNS"
OUT="$TMPDIR/cns_fix.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -c "$CNS" -o "$OUT" > /dev/null 2>&1

check "delta1[1] queda FIJO en 0.6" 0.600000 "$(val "$OUT" 'delta1\[1\]')" 0.000001
NF_FX=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')
check "fijar quita UN grado de libertad" "$((NF_FREE - 1))" "$NF_FX" 0.5
grep -q "delta1\[1\].*(fixed)" "$OUT" && pass "el informe lo marca como fijo" \
                                      || fail "el informe no lo marca como fijo"

# --- un nombre inexistente debe fallar con un mensaje claro ---
CNS="$TMPDIR/bad.cns"
printf 'delta9[3] = 0.5\n' > "$CNS"
if $DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -c "$CNS" \
        -o "$TMPDIR/bad.txt" > "$TMPDIR/bad_console.txt" 2>&1; then
    fail "acepta una restricción sobre un parámetro inexistente"
else
    grep -qi "unknown parameter" "$TMPDIR/bad_console.txt" \
        && pass "rechaza un parámetro inexistente con un mensaje claro" \
        || fail "falla, pero sin explicar por qué"
fi


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2f. LA RED: un DAG de transferencias (-n) ──"
echo "   El modelo general no es UNA salida y k entradas, sino una RED: una serie"
echo "   puede RECIBIR transferencias y ser a la vez ENTRADA de otra. Es lo que"
echo "   son de verdad los sistemas de Mauricio (en m6: EC -> EU -> EI -> EP)."
echo ""
echo "   Cadena sintética  X -> M -> Y   (verdad: Y<-M b=2 w=0.500; M<-X b=1 w=0.700)"
echo "   X influye en Y solo INDIRECTAMENTE, a través de M."
echo ""

NET="$SYN/SYNC.net"
OUT="$TMPDIR/net_chain.txt"
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$NET" \
        -o "$OUT" -f 6 > /dev/null 2>&1

check "omega Y<-M  (verdad 0.500)" 0.500 "$(val "$OUT" 'omega1\[0\]')" 0.05
check "omega M<-X  (verdad 0.700)" 0.700 "$(val "$OUT" 'omega2\[0\]')" 0.05
check "phi del ruido de Y (verdad 0.400)" 0.400 "$(val "$OUT" 'phi_1\[B\^1\]')" 0.06
check "phi del ruido de M (verdad 0.300)" 0.300 "$(val "$OUT" 'phi_2\[B\^1\]')" 0.06
check "phi de X           (verdad 0.500)" 0.500 "$(val "$OUT" 'phi_3\[B\^1\]')" 0.06

LL_NET=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
NP_NET=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')

# La ESTRELLA: lo único que drtran sabía hacer antes. MISMO número de parámetros,
# pero no puede decir que M depende de X: trata a M como autónoma.
OUT2="$TMPDIR/net_star.txt"
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" \
        -b 2,3 -r 0,0 -s 0,0 -o "$OUT2" > /dev/null 2>&1
LL_STAR=$(grep "Log-likelihood =" "$OUT2" | awk '{print $3}')
NP_STAR=$(grep "Structural parameters" "$OUT2" | sed 's/.*free: \([0-9]*\).*/\1/')

check "la estrella tiene los MISMOS parámetros libres" "$NP_NET" "$NP_STAR" 0.5
python3 -c "import sys; sys.exit(0 if $LL_NET > $LL_STAR + 50 else 1)" \
    && pass "la RED gana a la estrella por >50 en logL con los mismos parámetros ($LL_NET vs $LL_STAR)" \
    || fail "la red no mejora sobre la estrella ($LL_NET vs $LL_STAR)"

# En la estrella el enlace X->Y sale insignificante: el efecto es indirecto.
T_XY=$(grep -E "^omega2\[0\]" "$OUT2" | awk '{print $4}')
python3 -c "import sys; sys.exit(0 if abs($T_XY) < 2.0 else 1)" \
    && pass "en la estrella el enlace directo X->Y es insignificante (t = $T_XY)" \
    || fail "la estrella declara significativo un enlace X->Y que NO existe (t = $T_XY)"

grep -q "Transfer network (2 link(s))" "$OUT" \
    && pass "el informe declara la red" \
    || fail "el informe no declara la red"
grep -q "Output: SYNC_M" "$OUT" \
    && pass "prevé TODAS las series que reciben transferencia, no solo la 1" \
    || fail "no prevé la serie intermedia M"

# --- un CICLO debe rechazarse: el sistema sería simultáneo ---
CYC="$TMPDIR/cycle.net"
printf 'SYNC_Y <- SYNC_M  1 0 0\nSYNC_M <- SYNC_Y  1 0 0\n' > "$CYC"
# Ojo: drtran sale con codigo != 0 al rechazar, y con 'set -o pipefail' eso
# tumbaria el pipeline aunque el grep acierte. Se captura y luego se busca.
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$CYC" \
        -o "$TMPDIR/cyc.txt" > "$TMPDIR/cyc.log" 2>&1
grep -q "CYCLE" "$TMPDIR/cyc.log" \
    && pass "un ciclo en la red se RECHAZA (sistema simultáneo)" \
    || fail "acepta una red con ciclo"

# --- serie desconocida en el fichero de red ---
BAD="$TMPDIR/bad.net"
printf 'NO_EXISTE <- SYNC_X  1 0 0\n' > "$BAD"
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$BAD" \
        -o "$TMPDIR/bad.txt" > "$TMPDIR/bad.log" 2>&1
grep -q "unknown series" "$TMPDIR/bad.log" \
    && pass "una serie inexistente en la red se rechaza" \
    || fail "acepta una serie inexistente"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2g. COVARIANZA NO DIAGONAL (q[i,j] = free) ──"
echo "   Sigma = sigma2*Q es una descomposición NO ÚNICA (Mauricio 1995, ec. 2.1):"
echo "   la verosimilitud concentrada es invariante ante Q -> cQ. Por eso Q se"
echo "   normaliza con Q[1,1]=1 y sigma2 se queda la escala; si no, el hessiano es"
echo "   exactamente singular. El legacy NO normaliza: sus errores estándar de las"
echo "   sigmas son finitos solo porque salen del hessiano de BFGS."
echo ""
echo "   Caso sintético SYNQ: rho(a_N, a_X) = 0.600, sin transferencia."
echo ""

OUT="$TMPDIR/q_diag.txt"
$DRTRAN "$SYN/SYNQ_Y.pre" "$SYN/SYNQ_X.pre" -0 -o "$OUT" > /dev/null 2>&1
LL_D=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
NF_D=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')
R12_D=$(grep -A1 "Innovation correlations" "$OUT" | tail -1 | awk '{print $2}')
check "por defecto la covarianza es DIAGONAL (corr = 0)" 0.0 "$R12_D" 1e-9

OUT="$TMPDIR/q_full.txt"
$DRTRAN "$SYN/SYNQ_Y.pre" "$SYN/SYNQ_X.pre" -0 -c "$SYN/SYNQ.cns" -o "$OUT" > /dev/null 2>&1
LL_F=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
NF_F=$(grep "Structural parameters" "$OUT" | sed 's/.*free: \([0-9]*\).*/\1/')
R12=$(grep -A1 "Innovation correlations" "$OUT" | tail -1 | awk '{print $2}')

check "liberar q[2,1] añade UN grado de libertad" "$((NF_D + 1))" "$NF_F" 0.5
check "recupera la correlación (verdad 0.600)" 0.600 "$R12" 0.05

python3 -c "import sys; sys.exit(0 if 2*($LL_F - ($LL_D)) > 3.84 else 1)" \
    && pass "LR de la covarianza = $(python3 -c "print('%.1f' % (2*($LL_F-($LL_D))))") > chi2(1) = 3.84" \
    || fail "la covarianza no mejora la verosimilitud"

# EL error estándar: la prueba de que el hessiano NO es singular. Con la escala
# de Q sin normalizar esto daba 408901 (M0.8).
SD_Q=$(grep -E "^q\[2,1\]" "$OUT" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if 0.0 < $SD_Q < 1.0 else 1)" \
    && pass "el error estándar de q[2,1] es finito y razonable ($SD_Q)" \
    || fail "error estándar de q[2,1] degenerado ($SD_Q): ¿hessiano singular?"

# Sigma debe ser simétrica
S12=$(sig "$OUT" 1 2); S21=$(sig "$OUT" 2 1)
check "Sigma es simétrica" "$S12" "$S21" 1e-9

# Fijar una covarianza a un valor también debe poder hacerse
CNS="$TMPDIR/qfix.cns"
printf 'q[2,1] = 0.0\n' > "$CNS"
OUT="$TMPDIR/q_fix0.txt"
$DRTRAN "$SYN/SYNQ_Y.pre" "$SYN/SYNQ_X.pre" -0 -c "$CNS" -o "$OUT" > /dev/null 2>&1
LL_0=$(grep "Log-likelihood =" "$OUT" | awk '{print $3}')
check "fijar q[2,1]=0 reproduce el caso diagonal" "$LL_D" "$LL_0" 0.0001


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2h. CARACTERIZACIÓN DE LA FLT: ganancia y retardo medio ──"
echo "   Una transferencia estimada no está DESCRITA hasta que se dice cuánto"
echo "   responde en total (la GANANCIA) y cuánto tarda (el RETARDO MEDIO). Los"
echo "   omegas y deltas sueltos son la parametrización, no la respuesta."
echo ""
echo "     g = nu(1) = omega(1)/delta(1)"
echo "     m = nu'(1)/nu(1) = b + [SUM k*omega_k]/omega(1) + [SUM j*delta_j]/delta(1)"
echo ""

gain()  { grep -E "^  gain " "$1"     | awk '{print $2}'; }
gainse(){ grep -E "^  gain " "$1"     | awk '{print $3}'; }
mlag()  { grep -E "^  mean lag " "$1" | awk '{print $3}'; }
mlagse(){ grep -E "^  mean lag " "$1" | awk '{print $4}'; }

# --- AUTOCOMPROBACIÓN EXACTA: con s=0 y r=0, la ganancia ES omega_0 y el
#     retardo medio ES b. Si el método delta está bien, coinciden AL BIT.
OUT="$TMPDIR/ch_trivial.txt"
$DRTRAN "$SYN/SYN2_Y.pre" "$SYN/SYN2_X1.pre" -b 1 -r 0 -s 0 -o "$OUT" >/dev/null 2>&1
W0=$(val "$OUT" 'omega1\[0\]')
S0=$(grep -E "^omega1\[0\]" "$OUT" | awk '{print $3}')
check "s=0,r=0: la ganancia ES omega_0"          "$W0" "$(gain "$OUT")"   1e-9
check "s=0,r=0: y su error estándar TAMBIÉN"     "$S0" "$(gainse "$OUT")" 1e-9
check "s=0,r=0: el retardo medio ES b"           1.0   "$(mlag "$OUT")"   1e-9
check "s=0,r=0: sin incertidumbre (b es entero)" 0.0   "$(mlagse "$OUT")" 1e-9

# --- VERDAD SINTÉTICA: b=2, omega=(0.8,0.4)
#     g = 1.2 ;  m = 2 + 0.4/1.2 = 2.3333
OUT="$TMPDIR/ch_syn.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 2 -r 0 -s 1 -o "$OUT" >/dev/null 2>&1
check "SYN: ganancia (verdad 1.200)"       1.200 "$(gain "$OUT")" 0.06
check "SYN: retardo medio (verdad 2.333)"  2.333 "$(mlag "$OUT")" 0.06

# --- VERDAD SINTÉTICA RACIONAL: b=1, omega=0.6, delta=0.6
#     g = 0.6/0.4 = 1.5 ;  m = 1 + 0.6/0.4 = 2.5   <- el denominador ALARGA la respuesta
OUT="$TMPDIR/ch_synr.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -o "$OUT" >/dev/null 2>&1
check "SYNR: ganancia (verdad 1.500)"      1.500 "$(gain "$OUT")" 0.08
check "SYNR: retardo medio (verdad 2.500)" 2.500 "$(mlag "$OUT")" 0.10

# --- PARÁMETRO COMPARTIDO: el gradiente debe pasar POR EL ALIAS.
#     Tratar delta y phi_2 como independientes daría una SE distinta.
CNS="$TMPDIR/sh.cns"
printf 'delta1[1] = phi_2[B^1]\n' > "$CNS"
OUT="$TMPDIR/ch_share.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -c "$CNS" -o "$OUT" >/dev/null 2>&1
SEG=$(gainse "$OUT")
python3 -c "import sys; sys.exit(0 if 0.0 < $SEG < 0.5 else 1)" \
    && pass "con delta COMPARTIDO, la ganancia tiene SE por el alias ($SEG)" \
    || fail "la SE de la ganancia con parámetro compartido es degenerada ($SEG)"


# --- LOS DIAGNOSTICOS CON EL CAST EMPOTRADO: residuos ESTRUCTURALES ---
# Con -V la serie 1 es w_Y, no el ruido, y tras normalizar los residuos son los de
# la FORMA REDUCIDA -- correlacionados con los del input POR CONSTRUCCION
# (Sigma_12 = omega_0 * sigma_X^2). Si no se deshace la normalizacion, la prueba de
# adecuacion mide esa correlacion y la llama mala especificacion: los dos modelos
# del IPC salian "NO adecuados" con p = 0.0000. Hay que aplicar a = Phi(0) * a_red.
DS="$TMPDIR/diag_sub.txt"; DV="$TMPDIR/diag_emb.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1    -o "$DS" >/dev/null 2>&1
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -o "$DV" >/dev/null 2>&1
PS=$(grep -oE "is ADEQUATE \(p = [0-9.]+" "$DS" | grep -oE "[0-9.]+$")
PV=$(grep -oE "is ADEQUATE \(p = [0-9.]+" "$DV" | grep -oE "[0-9.]+$")
[ -n "$PV" ] \
    && pass "con -V la transferencia sale ADECUADA (p = $PV): los residuos se des-normalizan" \
    || fail "con -V la adecuación falla: ¿se están usando los residuos de la forma reducida?"
check "y el p-valor coincide con el del cast por resta" "$PS" "$PV" 0.01

# --- CASO REAL: el crudo se traslada al IPC de forma casi inmediata
OUT="$TMPDIR/ch_ipc.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -o "$OUT" >/dev/null 2>&1
GI=$(gain "$OUT"); MI=$(mlag "$OUT")
check "IPC<-WTI: ganancia = omega_0 + omega_1" 0.027194 "$GI" 0.0005
python3 -c "import sys; sys.exit(0 if 0.0 <= $MI < 1.0 else 1)" \
    && pass "IPC<-WTI: el retardo medio es de MEDIO MES ($MI): traslado inmediato" \
    || fail "el retardo medio del IPC no es inmediato ($MI)"

# --- el retardo medio NUNCA puede caer por debajo del retardo puro b
for f in "$TMPDIR/ch_syn.txt" "$TMPDIR/ch_synr.txt" "$TMPDIR/ch_trivial.txt"; do
    B=$(grep -E "^  pure delay b" "$f" | awk '{print $4}')
    M=$(mlag "$f")
    python3 -c "import sys; sys.exit(0 if $M >= $B - 1e-9 else 1)" || {
        fail "retardo medio ($M) por debajo del retardo puro ($B): imposible"; break; }
done
pass "el retardo medio nunca cae por debajo del retardo puro b"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 2i. RESPUESTA AL IMPULSO Y DESCOMPOSICIÓN DE LA VARIANZA ──"
echo "   Los pesos nu_k YA son la respuesta al impulso -- ésa es la comodidad de un"
echo "   modelo de transferencia. Pero sueltos no son comparables con nada: hacen"
echo "   falta su error estándar y la respuesta ACUMULADA, que converge a la"
echo "   ganancia. Y la descomposición de la varianza, que es lo que se compara"
echo "   con un VAR."
echo ""

OUT="$TMPDIR/irf.txt"
$DRTRAN "$SYN/SYNR_Y.pre" "$SYN/SYNR_X.pre" -b 1 -r 1 -s 0 -f 12 -o "$OUT" >/dev/null 2>&1

irf()  { grep -E "^  +$2  " "$1" | head -1 | awk '{print $2}'; }
icum() { grep -E "^  +$2  " "$1" | head -1 | awk '{print $6}'; }

# VERDAD (b=1, omega=0.6, delta=0.6): nu_0=0, nu_1=0.6, nu_2=0.36, nu_3=0.216
check "SYNR nu_0 = 0 (retardo puro b=1)"    0.0   "$(irf "$OUT" 0)" 1e-9
check "SYNR nu_1 (verdad 0.600)"            0.600 "$(irf "$OUT" 1)" 0.03
check "SYNR nu_2 (verdad 0.360)"            0.360 "$(irf "$OUT" 2)" 0.03
check "SYNR nu_3 (verdad 0.216)"            0.216 "$(irf "$OUT" 3)" 0.03

# la ACUMULADA converge a la GANANCIA: es la misma cantidad por otro camino
G=$(gain "$OUT")
C=$(awk '/k      nu_k/{f=1;next} f&&/^=+$/{exit} f&&/^ +[0-9]+ /{v=$6} END{print v}' "$OUT")
python3 -c "import sys; sys.exit(0 if abs($C - $G) < 0.02 else 1)" \
    && pass "la respuesta ACUMULADA converge a la ganancia ($C vs $G)" \
    || fail "la acumulada no converge a la ganancia ($C vs $G)"

# el error estandar de nu_0 con s=0,r=0 DEBE ser el de omega_0
OUT2="$TMPDIR/irf0.txt"
$DRTRAN "$SYN/SYN2_Y.pre" "$SYN/SYN2_X1.pre" -b 1 -r 0 -s 0 -o "$OUT2" >/dev/null 2>&1
SW=$(grep -E "^omega1\[0\]" "$OUT2" | awk '{print $3}')
SN=$(grep -E "^    1  " "$OUT2" | head -1 | awk '{print $3}')
check "SE(nu_1) = SE(omega_0) cuando la transferencia es un solo peso" "$SW" "$SN" 1e-9

# --- DESCOMPOSICIÓN DE LA VARIANZA ---
OUT="$TMPDIR/fevd.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -f 12 -o "$OUT" >/dev/null 2>&1
fevd() { grep -A"$((3 + $2))" "ES_CPI  (%" "$1" | tail -1 | awk '{print $3}' | tr -d '%'; }
S1=$(fevd "$OUT" 1); S3=$(fevd "$OUT" 3)
check "ES: a h=1 el crudo explica ~32% de la varianza del error" 31.8 "$S1" 1.0
check "ES: a h=3 ya explica la MITAD"                            50.1 "$S3" 1.5
python3 -c "import sys; sys.exit(0 if $S3 > $S1 else 1)" \
    && pass "la aportación del crudo CRECE con el horizonte (hay que preverlo)" \
    || fail "la aportación del crudo no crece con el horizonte"

# --- LA HONESTIDAD: con Sigma NO diagonal, la descomposición NO es única ---
CNS="$TMPDIR/qq.cns"; printf 'q[2,1] = free\n' > "$CNS"
OUT="$TMPDIR/fevd_nd.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 1 -r 0 -s 0 -c "$CNS" -f 6 -o "$OUT" >/dev/null 2>&1
grep -q "NOT UNIQUE" "$OUT" \
    && pass "con Sigma no diagonal NO la calcula: haría falta una ORDENACIÓN (el problema del VAR)" \
    || fail "fabrica una descomposición dependiente de un orden arbitrario"
grep -q "exactly the VAR's problem" "$OUT" \
    && pass "y lo dice: drtran no lo resuelve por magia, lo EVITA mientras puede" \
    || fail "no explica por qué no la da"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3. PASS-THROUGH: con Y = X la verdad es omega_0 = 1 ──"
echo ""

OUT="$TMPDIR/passthru.txt"
$DRTRAN "$WORK/WTI_ar1.pre" "$WORK/WTI_ar1.pre" -r 0 -s 0 -b 0 -o "$OUT" > /dev/null 2>&1
check "omega_0 (Y=X)"  1.0 "$(val "$OUT" 'omega1\[0\]')"  0.01


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3b. CASO REAL: IPC_ES <- WTI. ¿La transferencia es RACIONAL? ──"
echo "   Los datos deciden, no la costumbre. Tres ajustes, mismo output y mismo"
echo "   input, y dos de ellos con EXACTAMENTE los mismos parámetros libres."
echo ""

CASES="tests/cases"
R1="$TMPDIR/ipc_rat.txt"     # omega_0 / (1 - delta_1 B)   -- racional
R2="$TMPDIR/ipc_s1.txt"      # omega_0 + omega_1 B         -- dos omegas
R3="$TMPDIR/ipc_rat11.txt"   # omega_0 + omega_1 B, / (1 - delta_1 B)  -- anida a R2

$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 1 -s 0 -o "$R1" >/dev/null 2>&1
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 0 -s 1 -o "$R2" >/dev/null 2>&1
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 1 -s 1 -o "$R3" >/dev/null 2>&1

LL1=$(grep "Log-likelihood =" "$R1" | awk '{print $3}')
LL2=$(grep "Log-likelihood =" "$R2" | awk '{print $3}')
LL3=$(grep "Log-likelihood =" "$R3" | awk '{print $3}')
NF1=$(grep "Structural parameters" "$R1" | sed 's/.*free: \([0-9]*\).*/\1/')
NF2=$(grep "Structural parameters" "$R2" | sed 's/.*free: \([0-9]*\).*/\1/')

check "racional y dos-omegas tienen los MISMOS parámetros libres" "$NF2" "$NF1" 0.5
python3 -c "import sys; sys.exit(0 if $LL2 > $LL1 else 1)" \
    && pass "con los mismos parámetros, DOS OMEGAS gana al racional ($LL2 vs $LL1)" \
    || fail "el racional gana al de dos omegas"

grep -q "NOT adequate" "$R1" \
    && pass "la adecuación DELATA al racional (deja rastro del input en el ruido)" \
    || fail "la adecuación no detecta la mala especificación del racional"
grep -q "is ADEQUATE" "$R2" \
    && pass "el de dos omegas es adecuado" \
    || fail "el de dos omegas sale inadecuado"

# R3 anida a R2: el contraste honesto del denominador
D1=$(val "$R3" 'delta1\[1\]')
T3=$(grep -E "^delta1\[1\]" "$R3" | awk '{print $4}')
python3 -c "import sys; sys.exit(0 if abs($T3) < 2.0 else 1)" \
    && pass "en el modelo que los ANIDA, delta_1 es insignificante (t = $T3): no hay denominador" \
    || fail "delta_1 sale significativo en el modelo anidante (t = $T3)"
python3 -c "import sys; sys.exit(0 if 2*($LL3 - ($LL2)) < 3.84 else 1)" \
    && pass "LR del denominador = $(python3 -c "print('%.3f' % (2*($LL3-($LL2))))") < chi2(1) = 3.84" \
    || fail "el denominador mejora significativamente"

# El t = 6.4 del delta en el modelo R1 es un ESPEJISMO: con el denominador
# forzado, delta es el unico camino para dar peso al retardo 1.
TD1=$(grep -E "^delta1\[1\]" "$R1" | awk '{print $4}')
python3 -c "import sys; sys.exit(0 if $TD1 > 4.0 else 1)" \
    && pass "aviso: en el racional forzado delta_1 parece contundente (t = $TD1) y es un espejismo" \
    || fail "el racional forzado no reproduce el t alto de delta_1"


# --- LOS DOS MODELOS CON EL CAST EMPOTRADO: AIC y BIC ya SIGNIFICAN algo ---
# Con -V la verosimilitud es la EXACTA de los datos, asi que los criterios de
# informacion y los LR son validos (con el cast por resta, elf calculaba una
# verosimilitud exacta... de la serie equivocada).
V1="$TMPDIR/ipc_v_w.txt"; V2="$TMPDIR/ipc_v_r.txt"; V3="$TMPDIR/ipc_v_n.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -o "$V1" >/dev/null 2>&1
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 1 -s 0 -V -o "$V2" >/dev/null 2>&1
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 1 -s 1 -V -o "$V3" >/dev/null 2>&1
LV1=$(grep "Log-likelihood =" "$V1" | awk '{print $3}')
LV2=$(grep "Log-likelihood =" "$V2" | awk '{print $3}')
LV3=$(grep "Log-likelihood =" "$V3" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if $LV1 > $LV2 + 3.0 else 1)" \
    && pass "con -V: dos omegas bate al racional por $(python3 -c "print('%.2f' % ($LV1-($LV2)))") de logL (mismos 17 parámetros)" \
    || fail "con -V el racional no queda por debajo"
grep -q "is ADEQUATE" "$V1" && pass "con -V: el de dos omegas es ADECUADO" || fail "el de dos omegas falla la adecuación con -V"
grep -q "NOT adequate" "$V2" && pass "con -V: el racional es INADECUADO (rastro del input en k=2)" || fail "el racional no se delata con -V"
python3 -c "import sys; sys.exit(0 if 2*($LV3 - ($LV1)) < 3.84 else 1)" \
    && pass "con -V: el LR del denominador = $(python3 -c "print('%.4f' % (2*($LV3-($LV1))))") < chi2(1): no hay denominador" \
    || fail "el denominador sale significativo con -V"

# --- EL RETARDO MEDIO EXPLICA LA PREVISIÓN ---
# El racional pone la masa de la respuesta MAS ATRAS en el tiempo (retardo medio
# 0.84 frente a 0.40), o sea en retardos YA OBSERVADOS en el origen. Por eso preve
# mejor a UN paso, aunque su cola sea falsa. Y por eso pierde en cuanto el horizonte
# crece: la mala especificación se cobra la factura.
M1=$(mlag "$V1"); M2=$(mlag "$V2")
python3 -c "import sys; sys.exit(0 if $M2 > $M1 + 0.2 else 1)" \
    && pass "el racional tiene el retardo medio MÁS LARGO ($M2 vs $M1): su cola falsa empuja la masa hacia atrás" \
    || fail "el retardo medio del racional no es más largo"

O1="$TMPDIR/ipc_o_w.txt"; O2="$TMPDIR/ipc_o_r.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 0 -s 1 -V -R 168 -f 12 -o "$O1" >/dev/null 2>&1
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 1 -s 0 -V -R 168 -f 12 -o "$O2" >/dev/null 2>&1
python3 - "$O1" "$O2" <<'PYRAT'
import re, sys
def rmse(f):
    t=open(f).read()
    m=re.search(r'h      n        MAE.*?\n  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): float(r.split()[3]) for r in m.group(1).strip().split('\n')}
W, R = rmse(sys.argv[1]), rmse(sys.argv[2])
# a h=1 el racional gana (masa mas atras = mas input YA OBSERVADO)
# a h>=3 pierde (la cola es falsa y la mala especificacion se paga)
ok = R[1] < W[1] and all(R[h] > W[h] for h in (3, 6, 12))
sys.exit(0 if ok else (print("h=1 %.4f/%.4f  h=3 %.4f/%.4f  h=12 %.4f/%.4f"
        % (R[1],W[1],R[3],W[3],R[12],W[12])) or 1))
PYRAT
[ $? -eq 0 ] \
    && pass "fuera de muestra: el racional gana a h=1 (por el motivo equivocado) y PIERDE de h=3 en adelante" \
    || fail "no se reproduce el patrón de previsión racional/dos-omegas"

echo ""
echo "   Los pesos: el racional impone cola geométrica; los datos quieren dos y parar."
echo ""

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3c. SEPARABILIDAD: por qué Box-Jenkins puede dejar FIJO el modelo del input ──"
echo "   La tesis de Muñoz Polo (2001, sec. 2.6): «El modelo U del input permanece"
echo "   inalterado desde el inicio hasta el fin del proceso». drtran, en cambio, lo"
echo "   estima CONJUNTAMENTE. ¿Cambia algo? Con Sigma diagonal y sin realimentación"
echo "   la verosimilitud se FACTORIZA, así que no: no es una aproximación, es"
echo "   exactamente óptimo. Y por eso drtran homologa con fue."
echo ""

FX="$TMPDIR/ipc_fixX.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 0 -s 1 -X -o "$FX" >/dev/null 2>&1
LLX=$(grep "Log-likelihood =" "$FX" | awk '{print $3}')
check "fijar el ARMA del input NO cambia la verosimilitud" "$LL2" "$LLX" 1e-6
check "ni el omega_0" "$(val "$R2" 'omega1\[0\]')" "$(val "$FX" 'omega1\[0\]')" 1e-6
check "ni el omega_1" "$(val "$R2" 'omega1\[1\]')" "$(val "$FX" 'omega1\[1\]')" 1e-6

echo ""
echo "── 3d. omega_0 vs sigma_12: dos formas de explicar LO MISMO en k=0 ──"
echo "   Una transferencia CONTEMPORÁNEA (b=0) y la covarianza de las innovaciones"
echo "   producen la misma covarianza cruzada en el retardo 0. Solo se separan por"
echo "   cómo decae en k>0: phi_X^k la transferencia, phi_N^k la covarianza. Si los"
echo "   dos AR se parecen, la identificación es débil. Es la razón de que m6-1 tenga"
echo "   covarianzas y NINGUNA estructura contemporánea."
echo ""

CQ="$TMPDIR/q21.cns"
printf 'q[2,1] = free\n' > "$CQ"

# (a) IPC<-WTI: b=0 y phi_X=0.30 ~ phi_N=0.40. Cresta.
PAT="$TMPDIR/ipc_ridge.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -b 0 -r 0 -s 1 -c "$CQ" -o "$PAT" >/dev/null 2>&1
LLP=$(grep "Log-likelihood =" "$PAT" | awk '{print $3}')
RHO=$(grep -A1 "Innovation correlations" "$PAT" | tail -1 | awk '{print $2}')
TQ=$(grep -E "^q\[2,1\]" "$PAT" | awk '{print $4}')

grep -q "near-collinearity" "$PAT" \
    && pass "drtran AVISA de la casi-colinealidad (b=0 + covarianza libre)" \
    || fail "no avisa de la casi-colinealidad"
python3 -c "import sys; sys.exit(0 if 2*($LLP - ($LL2)) < 3.84 else 1)" \
    && pass "y tiene razón: la verosimilitud NO mejora (LR = $(python3 -c "print('%.3f' % (2*($LLP-($LL2))))"))" \
    || fail "la covarianza sí mejora significativamente"
python3 -c "import sys; sys.exit(0 if abs($RHO) > 0.9 and abs($TQ) > 100 else 1)" \
    && pass "pero los parámetros huyen a una esquina: corr = $RHO, t = $TQ" \
    || fail "no se reproduce la patología (corr = $RHO, t = $TQ)"

# (b) SYN: b=2 (no contemporánea) y phi_X=0.50 != phi_N=0.30. Sin patología.
CLEAN="$TMPDIR/syn_q.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 2 -r 0 -s 1 -c "$CQ" -o "$CLEAN" >/dev/null 2>&1
TQ2=$(grep -E "^q\[2,1\]" "$CLEAN" | awk '{print $4}')
grep -q "near-collinearity" "$CLEAN" \
    && fail "avisa de colinealidad donde NO la hay (b=2)" \
    || pass "con b=2 NO avisa: la transferencia no toca el retardo 0"
python3 -c "import sys; sys.exit(0 if abs($TQ2) < 2.0 else 1)" \
    && pass "y la covarianza sale correctamente NO significativa (t = $TQ2, verdad = 0)" \
    || fail "la covarianza sale significativa siendo cero (t = $TQ2)"
check "los omegas se recuperan igual (verdad 0.800)" 0.800 "$(val "$CLEAN" 'omega1\[0\]')" 0.05


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3e. EL ORDEN DE REFORMULACIÓN: la relación antes que el ruido ──"
echo "   Muñoz Polo (2001, sec. 2.6): la contaminación va en UN SOLO SENTIDO."
echo "   Una relación mal especificada deja parte del input DENTRO del ruido, así"
echo "   que SÍ ensucia la ACF residual. Un ruido mal especificado NO PUEDE"
echo "   ensuciar la CCF. Luego: arreglar la RELACIÓN primero. Una ACF residual"
echo "   fea no es evidencia contra el ruido mientras la CCF siga hablando."
echo ""

# (a) orden EQUIVOCADO (b=0,s=0; la verdad es b=2,s=1) -> arreglar la RELACIÓN
OUT="$TMPDIR/adv_bad.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 0 -r 0 -s 0 -o "$OUT" >/dev/null 2>&1
grep -q "REFORMULATE THE RELATION" "$OUT" \
    && pass "con (b,r,s) equivocados manda arreglar la RELACIÓN" \
    || fail "no manda arreglar la relación cuando está mal especificada"

# (b) orden CORRECTO -> ya no manda arreglar la relación
OUT="$TMPDIR/adv_ok.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 2 -r 0 -s 1 -o "$OUT" >/dev/null 2>&1
grep -q "REFORMULATE THE RELATION" "$OUT" \
    && fail "sigue mandando arreglar la relación con los órdenes correctos" \
    || pass "con los órdenes correctos ya no manda arreglar la relación"

# (c) caso real bien especificado -> no hay nada que reformular
OUT="$TMPDIR/adv_ipc.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -o "$OUT" >/dev/null 2>&1
grep -q "Nothing to reformulate" "$OUT" \
    && pass "IPC<-WTI bien especificado: nada que reformular" \
    || fail "declara algo que reformular en un modelo adecuado"

# (d) el consejo cita la asimetría, que es la razón de todo
grep -q "A badly specified noise CANNOT dirty the CCF" "$OUT" \
    && pass "el informe explica POR QUÉ ese orden y no el contrario" \
    || fail "no explica la asimetría"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3f. PASS-THROUGH del crudo a la inflación: ES, FR y DE ──"
echo "   El ejercicio de SF_MEG/drvarma: ¿mejora el modelo multivariante la"
echo "   previsión del univariante? Allí se usó un VAR, y NO era adecuado por una"
echo "   razón concreta: el VAR no podía llevar la estructura univariante que las"
echo "   series necesitan (FR un SAR(1)_12; DE un AR(3)+SAR), así que dejaba"
echo "   autocorrelación residual (Q falla: FR p=0.006, DE p=0.018), y subir el"
echo "   orden p introducía realimentación espuria IPC->WTI."
echo ""
echo "   drtran no tiene ese problema: CADA SERIE CONSERVA SU MODELO DE FUE, y la"
echo "   transferencia es unidireccional por construcción."
echo ""

# --- las tres elasticidades. drvarma documenta ES 2.7% > FR 1.35% > DE 1.1%
for P in ES:ES_CPI_m10 FR:FR_CPI_msar DE:DE_CPI_mar3sar; do
    K=${P%%:*}; M=${P##*:}
    $DRTRAN "$WORK/$M.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -f 24 \
            -o "$TMPDIR/pt_$K.txt" >/dev/null 2>&1
    $DRTRAN "$WORK/$M.pre" "$WORK/WTI_ar1.pre" -0 -f 24 \
            -o "$TMPDIR/pu_$K.txt" >/dev/null 2>&1
    $DRTRAN "$WORK/$M.pre" "$WORK/WTI_ar1.pre" -b 1 -r 0 -s 0 -f 24 \
            -o "$TMPDIR/pb1_$K.txt" >/dev/null 2>&1
done

GES=$(gain "$TMPDIR/pt_ES.txt"); GFR=$(gain "$TMPDIR/pt_FR.txt"); GDE=$(gain "$TMPDIR/pt_DE.txt")
check "elasticidad de largo plazo ES (drvarma: 2.7%)" 0.027 "$GES" 0.004
check "elasticidad de largo plazo FR (drvarma: 1.35%)" 0.0135 "$GFR" 0.004
check "elasticidad de largo plazo DE (drvarma: 1.1%)" 0.011 "$GDE" 0.004
python3 -c "import sys; sys.exit(0 if $GES > $GFR > $GDE else 1)" \
    && pass "el ORDEN se mantiene: España > Francia > Alemania" \
    || fail "el orden de las elasticidades no coincide con drvarma"

# --- DE: el retardo NO es significativo, como en drvarma ---
TD=$(grep -E "^omega1\[1\]" "$TMPDIR/pt_DE.txt" | awk '{print $4}')
python3 -c "import sys; sys.exit(0 if abs($TD) < 2.0 else 1)" \
    && pass "DE: el coeficiente retardado NO es significativo (t = $TD), como en drvarma" \
    || fail "DE: el retardado sale significativo (t = $TD)"

# --- LO QUE EL VAR NO PODÍA: el ruido de FR ya no queda autocorrelacionado ---
grep -A3 "relation (CCF" "$TMPDIR/pt_FR.txt" | grep -q "Nothing to reformulate" \
    && pass "FR: relación Y ruido pasan (el VAR fallaba el ruido con p=0.006)" \
    || fail "FR: sigue quedando estructura sin modelar"
grep -A3 "relation (CCF" "$TMPDIR/pt_ES.txt" | grep -q "Nothing to reformulate" \
    && pass "ES: relación y ruido pasan" \
    || fail "ES: queda estructura sin modelar"

# --- LA COMPARACIÓN DE PREVISIÓN ---
python3 - "$TMPDIR" <<'PYFC'
import re, sys
T = sys.argv[1]
def sd(f, tag):
    t = open(f).read()
    m = re.search(r'Output: %s.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)' % tag, t)
    return {int(r.split()[0]): float(r.split()[2]) for r in m.group(1).strip().split('\n')}
names = {'ES': 'ES_CPI', 'FR': 'IPC_FR', 'DE': 'IPC_DE'}
bad = []
for k, tag in names.items():
    u  = sd('%s/pu_%s.txt'  % (T, k), tag)
    t0 = sd('%s/pt_%s.txt'  % (T, k), tag)
    b1 = sd('%s/pb1_%s.txt' % (T, k), tag)
    # (a) a h=1 la transferencia AYUDA: el retardo del crudo ya se observo
    if not t0[1] < u[1]:
        bad.append("%s: a h=1 la transferencia no mejora (%.4f vs %.4f)" % (k, t0[1], u[1]))
    # (b) con b=0, a horizontes largos EMPEORA: hay que prever el crudo
    if k == 'ES' and not t0[12] > u[12]:
        bad.append("ES: con b=0 no empeora a h=12, y deberia (prever el crudo inyecta ruido)")
    # (c) OJO: b=1 no es "b=0 sin el contemporaneo". Es OTRO modelo: al quitar
    #     omega_0 el ruido absorbe la covariacion contemporanea y sigma_N crece.
    #     Hay un INTERCAMBIO -- se pierde ajuste, pero se evita tener que prever
    #     el crudo para ese termino. Solo compensa CLARAMENTE en ES; en FR y DE
    #     es un empate (diferencias por debajo del 1%). No se afirma dominancia.
    if k == 'ES':
        for h in (1, 2, 3, 12):
            if b1[h] >= t0[h]:
                bad.append("ES: b=1 no mejora a b=0 en h=%d (%.4f vs %.4f)"
                           % (h, b1[h], t0[h]))
    else:
        for h in (1, 2, 3, 12):
            if abs(b1[h] - t0[h]) / t0[h] > 0.01:
                bad.append("%s: b=1 y b=0 deberian empatar (<1%%) y difieren en h=%d "
                           "(%.4f vs %.4f)" % (k, h, b1[h], t0[h]))
sys.exit(0 if not bad else (print('\n'.join(bad)) or 1))
PYFC

if [ $? -eq 0 ]; then
    pass "a h=1 la transferencia MEJORA: el crudo del origen ya está observado"
    pass "con b=0 y horizonte largo EMPEORA: hay que prever el crudo, y su varianza es ~1000x"
    pass "en ES, tirar el término contemporáneo (b=1) mejora a b=0 a todo horizonte"
    pass "en FR y DE es un empate (<1%): quitar omega_0 sube el ruido tanto como ahorra"
else
    fail "la comparación de previsión no da lo esperado (ver arriba)"
fi

# ES con b=1 bate al univariante a TODO horizonte: el resultado accionable
python3 - "$TMPDIR" <<'PYES'
import re, sys
T = sys.argv[1]
def sd(f):
    t = open(f).read()
    m = re.search(r'Output: ES_CPI.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): float(r.split()[2]) for r in m.group(1).strip().split('\n')}
u, b = sd('%s/pu_ES.txt' % T), sd('%s/pb1_ES.txt' % T)
sys.exit(0 if all(b[h] < u[h] for h in (1, 2, 3, 6, 12, 24)) else 1)
PYES
[ $? -eq 0 ] \
    && pass "ES con b=1 bate al univariante a TODO horizonte (h=1..24)" \
    || fail "ES con b=1 no bate al univariante en todos los horizontes"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 3g. NO IDENTIFICACIÓN, con verdad construida ──"
echo "   En la sección 3d medimos la PATOLOGÍA (la cresta). Aquí se mide la CAUSA,"
echo "   sobre datos generados con la verdad conocida."
echo ""
echo "   Una transferencia CONTEMPORÁNEA y una covarianza de innovaciones explican"
echo "   lo mismo en el retardo 0. Solo las separa la dinámica, y la señal que las"
echo "   separa es proporcional a (phi_X - phi_N). El álgebra del cast empotrado lo"
echo "   dice sin que se le pregunte: tras normalizar por Phi(0), el AR fuera de la"
echo "   diagonal en el retardo 1 vale exactamente"
echo ""
echo "       [Phi(0)^-1 Phi_1]_12 = omega_0 * (phi_X - phi_N)"
echo ""
echo "   que es EL ÚNICO RASTRO que queda, en forma reducida, de que la relación es"
echo "   una TRANSFERENCIA y no una simple covarianza. Se anula si phi_N = phi_X."
echo ""

CQ="$TMPDIR/q21.cns";  printf 'q[2,1] = free\n' > "$CQ"
CE="$TMPDIR/eq.cns";   printf 'phi_2[B^1] = phi_1[B^1]\n' > "$CE"
CEQ="$TMPDIR/eqq.cns"; printf 'phi_2[B^1] = phi_1[B^1]\nq[2,1] = free\n' > "$CEQ"

# ── SYNI: phi_N = phi_X = 0.5. Los dos modelos son EL MISMO. ──
# Se impone phi_N = phi_X en ambos, que es el subconjunto donde coinciden, y se
# comparan con el MISMO número de parámetros libres.
A="$TMPDIR/id_A.txt"; B="$TMPDIR/id_B.txt"
$DRTRAN "$SYN/SYNI_Y.pre" "$SYN/SYNI_X.pre" -b 0 -r 0 -s 0 -V -c "$CE"  -o "$A" >/dev/null 2>&1
$DRTRAN "$SYN/SYNI_Y.pre" "$SYN/SYNI_X.pre" -0              -c "$CEQ" -o "$B" >/dev/null 2>&1

NA=$(grep "Structural parameters" "$A" | sed 's/.*free: \([0-9]*\).*/\1/')
NB=$(grep "Structural parameters" "$B" | sed 's/.*free: \([0-9]*\).*/\1/')
check "mismo número de parámetros libres" "$NA" "$NB" 0.5

LA=$(grep "Log-likelihood =" "$A" | awk '{print $3}')
LB=$(grep "Log-likelihood =" "$B" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if abs($LA - ($LB)) < 1e-6 else 1)" \
    && pass "con phi_N = phi_X, transferencia y covarianza dan la MISMA verosimilitud ($LA): son EL MISMO MODELO" \
    || fail "deberían ser indistinguibles y difieren ($LA vs $LB)"
check "y hasta la phi estimada coincide" "$(val "$A" 'phi_1\[B\^1\]')" "$(val "$B" 'phi_1\[B\^1\]')" 1e-6

# ── SYNJ: phi_N = 0.2, phi_X = 0.7. Ahora SÍ se distinguen. ──
# La verdad es una TRANSFERENCIA. Con las phi libres y los MISMOS parámetros,
# el modelo verdadero debe ganar, y por mucho.
A2="$TMPDIR/id_A2.txt"; B2="$TMPDIR/id_B2.txt"
$DRTRAN "$SYN/SYNJ_Y.pre" "$SYN/SYNJ_X.pre" -b 0 -r 0 -s 0 -V -o "$A2" >/dev/null 2>&1
$DRTRAN "$SYN/SYNJ_Y.pre" "$SYN/SYNJ_X.pre" -0 -c "$CQ"       -o "$B2" >/dev/null 2>&1

check "SYNJ: recupera omega_0 (verdad 0.500)" 0.500 "$(val "$A2" 'omega1\[0\]')" 0.03
check "SYNJ: recupera phi_X   (verdad 0.700)" 0.700 "$(val "$A2" 'phi_2\[B\^1\]')" 0.05

LA2=$(grep "Log-likelihood =" "$A2" | awk '{print $3}')
LB2=$(grep "Log-likelihood =" "$B2" | awk '{print $3}')
python3 -c "import sys; sys.exit(0 if $LA2 - ($LB2) > 10.0 else 1)" \
    && pass "con phi_N != phi_X la TRANSFERENCIA (la verdad) gana por $(python3 -c "print('%.1f' % ($LA2-($LB2)))") puntos" \
    || fail "con las phi separadas los datos no distinguen los dos mecanismos"

# el modelo EQUIVOCADO se delata: deforma las dos phi para imitar la transferencia
P1=$(val "$B2" 'phi_1\[B\^1\]'); P2=$(val "$B2" 'phi_2\[B\^1\]')
python3 -c "import sys; sys.exit(0 if abs($P1 - 0.2) > 0.1 and abs($P2 - 0.7) > 0.1 else 1)" \
    && pass "y el modelo de covarianza se delata: deforma las dos phi ($P1, $P2 frente a 0.2, 0.7)" \
    || fail "el modelo de covarianza no deforma las phi"

echo ""
echo "   Resumen: phi_N = phi_X -> diferencia 0.00 (no identificado)."
echo "            phi_N != phi_X -> la verdad gana por 26 puntos."
echo "   La identificación NACE de que los AR sean distintos."
echo ""

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 4. DETERMINISTAS: tipos de fue e intervenciones racionales ──"
echo ""

# .pre con un step de estructura racional (omega_0, omega_1, delta_1) y un impulse
SYNDET="$TMPDIR/WTI_det.pre"
python3 - "$WORK/WTI_ar1.pre" "$SYNDET" <<'EOF'
import sys
src, dst = sys.argv[1], sys.argv[2]
L = open(src).read().splitlines()
i = L.index('** Number of deterministic variables (including seasonal components):')
j = L.index('**Number and orders of regular AR operators:')
det = ['** Number of deterministic variables (including seasonal components):', '2', '**',
       'step 6 2008', 'impulse 1 2015', '**',
       '1 0',                          # Nomega: el step tiene omega_0 y omega_1
       '**', '0.5000  1', '0.2000  1', # omegas del step (una por línea)
       '**', '1.5000  1',              # omega del impulse
       '**',
       '1 0',                          # Ndelta: el step tiene delta_1
       '**', '0.4000  1']
open(dst, 'w').write('\n'.join(L[:i] + det + L[j:]) + '\n')
EOF

OUT="$TMPDIR/det.txt"
timeout 120 $DRTRAN "$SYNDET" "$WORK/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1
RC=$?

if [ $RC -eq 124 ]; then
    fail "se cuelga con deterministas racionales (¿falta la guarda de delta?)"
else
    opt_ok "$OUT" "no falla con step racional + impulse"
    grep -q "omega_d1\[1,1\]" "$OUT" && pass "estima omega_1 del step (Nomega>0)" \
                                    || fail "no estima omega_1 del step"
    grep -q "delta_d1\[1,1\]" "$OUT" && pass "estima delta_1 del step (Ndelta>0)" \
                                    || fail "no estima delta_1 del step"
    grep -q "omega_d1\[2,0\]" "$OUT" && pass "estima el impulse" \
                                    || fail "no estima el impulse"
fi

# Una variable determinista no estándar debe rechazarse con un error claro
BADPRE="$TMPDIR/bad.pre"
sed 's/^step 6 2008$/basura_no_estandar/' "$SYNDET" > "$BADPRE"
if $DRTRAN "$BADPRE" "$WORK/WTI_ar1.pre" -r 0 -s 0 -b 0 > "$TMPDIR/bad.txt" 2>&1; then
    fail "acepta una determinista no estándar (debería rechazarla)"
else
    grep -qi "no reconocida" "$TMPDIR/bad.txt" \
        && pass "rechaza deterministas no estándar con un error claro" \
        || fail "falla, pero sin explicar por qué"
fi

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 4b. PREVISIÓN: la varianza del error debe descomponerse como manda la teoría ──"
echo "   Prever Y exige prever X, así que el error de Y tiene DOS fuentes: la"
echo "   innovación del ruido y la de la entrada, propagada por nu(B)."
echo "   Con retardo puro b=2, los DOS primeros pasos solo usan X ya observada:"
echo "     sd(1) = sqrt(Sigma_N)              [sin error de X]"
echo "     sd(2) = sqrt(Sigma_N*(1+phi_N^2))  [sin error de X]"
echo "     sd(3) >> sd(2)                     [ya entra el error de X]"
echo ""

OUT="$TMPDIR/fc.txt"
$DRTRAN "$SYN/SYN_Y.pre" "$SYN/SYN_X.pre" -b 2 -r 0 -s 1 -f 6 -o "$OUT" > /dev/null 2>&1

SN=$(sig "$OUT" 1 1)
PHIN=$(val "$OUT" 'phi_1\[B\^1\]')

# sd(w) del paso l: columna 3 de la tabla de previsión
sdw() { awk -v l="$2" '/w_Y fcst/{f=1;next} f&&$1==l{print $3;exit}' "$1"; }

SD1=$(sdw "$OUT" 1); SD2=$(sdw "$OUT" 2); SD3=$(sdw "$OUT" 3)

EXP1=$(python3 -c "import math; print(math.sqrt($SN))")
EXP2=$(python3 -c "import math; print(math.sqrt($SN*(1+$PHIN**2)))")

check "sd(1) = sqrt(Sigma_N)"             "$EXP1" "$SD1" 0.002
check "sd(2) = sqrt(Sigma_N*(1+phi_N^2))" "$EXP2" "$SD2" 0.002

# el retardo puro b=2 debe verse: al llegar a l=3 entra el error de X y sd salta
python3 -c "import sys; sys.exit(0 if $SD3 > 1.5*$SD2 else 1)" \
    && pass "sd(3) salta al entrar el error de previsión de X (retardo b=2)" \
    || fail "el error de previsión de X no se propaga por nu(B)"

# las bandas del nivel deben ensancharse monótonamente
python3 - "$OUT" <<'EOF'
import sys, re
lines = open(sys.argv[1]).read().splitlines()
i = next(k for k,l in enumerate(lines) if 'w_Y fcst' in l)
w = []
for l in lines[i+2:]:
    p = l.split()
    if len(p) < 7: break
    w.append(float(p[-1]) - float(p[-2]))     # anchura de la banda
sys.exit(0 if all(w[k] < w[k+1] for k in range(len(w)-1)) else 1)
EOF
[ $? -eq 0 ] && pass "las bandas del nivel se ensanchan con el horizonte" \
             || fail "las bandas no se ensanchan monótonamente"

# --- caso real: el determinista futuro se aplica (la caída de enero) ---
OUT="$TMPDIR/fc_real.txt"
$DRTRAN "$CASES/ES_CPI_m10.pre" "$CASES/WTI_ar1.pre" -f 12 -o "$OUT" > /dev/null 2>&1
LVL1=$(awk '/w_Y fcst/{f=1;next} f&&$1==1{print $5;exit}' "$OUT")
# el último dato observado es de diciembre; enero trae la caída de rebajas,
# así que la previsión de enero debe quedar POR DEBAJO del último nivel (82.84)
python3 -c "import sys; sys.exit(0 if $LVL1 < 82.84 else 1)" \
    && pass "IPC: enero cae bajo el último dato (los armónicos futuros se aplican)" \
    || fail "IPC: el componente determinista futuro no se está aplicando"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 4c. AGREGADOS: identidades contables con varianza c'Vc (-a) ──"
echo "   La identidad (OCUPADOS = suma de sectores; PARADOS = ACTIVOS - OCUPADOS)"
echo "   NO entra en el modelo: se calcula DESPUÉS de prever, como hacía el legacy."
echo "   Lo que no es trivial es su BANDA: los errores de previsión de las series"
echo "   están CORRELACIONADOS -- comparten innovaciones a través de la red -- así"
echo "   que la varianza del agregado NO es la suma de las varianzas. Es c'Vc."
echo ""
echo "   Sobre la cadena X -> M -> Y, con la transferencia M->Y de retardo b=2."
echo ""

AG="$TMPDIR/ag.txt"
printf 'TOTAL = + SYNC_Y + SYNC_M\nGAP   = + SYNC_Y - SYNC_M\n' > "$AG"
OUT="$TMPDIR/aggr.txt"
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$SYN/SYNC.net" \
        -a "$AG" -f 6 -o "$OUT" >/dev/null 2>&1

python3 - "$OUT" <<'PYCHK'
import re, sys
t = open(sys.argv[1]).read()

def series_sd(tag):
    m = re.search(r'Output: %s.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)' % tag, t)
    return {int(r.split()[0]): (float(r.split()[6]) - float(r.split()[5])) / (2 * 1.96)
            for r in m.group(1).strip().split('\n')}

def aggr_sd(tag):
    m = re.search(r'Aggregate: %s.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)' % tag, t)
    return {int(r.split()[0]): float(r.split()[2])
            for r in m.group(1).strip().split('\n')}

Y, M = series_sd('SYNC_Y'), series_sd('SYNC_M')
T, G = aggr_sd('TOTAL'),   aggr_sd('GAP')

ok, msg = True, []
for l in sorted(T):
    indep = (Y[l]**2 + M[l]**2) ** 0.5
    if l <= 2:
        # la transferencia M->Y tiene b=2: la innovacion de M AUN NO ha llegado
        # a Y, luego los errores son INDEPENDIENTES y c'Vc == la suma ingenua.
        if abs(T[l] - indep) > 1e-3 or abs(G[l] - indep) > 1e-3:
            ok = False; msg.append("l=%d: con b=2 los errores aun son independientes, "
                                   "c'Vc deberia coincidir (%.4f vs %.4f)" % (l, T[l], indep))
    else:
        # ya llegada la innovacion, la correlacion es POSITIVA: sumar amplifica,
        # restar cancela.
        if not (G[l] < indep < T[l]):
            ok = False; msg.append("l=%d: no se cumple GAP < indep < TOTAL "
                                   "(%.4f, %.4f, %.4f)" % (l, G[l], indep, T[l]))
sys.exit(0 if ok else (print('\n'.join(msg)) or 1))
PYCHK

if [ $? -eq 0 ]; then
    pass "c'Vc coincide con la suma ingenua mientras b=2 impide la correlación (l<=2)"
    pass "y en cuanto la innovación llega (l>=3): GAP < independientes < TOTAL"
else
    fail "la varianza del agregado no respeta la correlación entre errores"
    fail "(ver arriba)"
fi

grep -q "Aggregate: TOTAL" "$OUT" && pass "el informe da el agregado" || fail "no da el agregado"
grep -q "Aggregate: GAP"   "$OUT" && pass "y admite varios" || fail "no admite varios agregados"

# serie inexistente en el fichero de agregados
BADA="$TMPDIR/bad_ag.txt"
printf 'X = + NO_EXISTE\n' > "$BADA"
$DRTRAN "$SYN/SYNC_Y.pre" "$SYN/SYNC_M.pre" "$SYN/SYNC_X.pre" -n "$SYN/SYNC.net" \
        -a "$BADA" -f 3 -o "$TMPDIR/bad_ag.out" > "$TMPDIR/bad_ag.log" 2>&1
grep -q "unknown series" "$TMPDIR/bad_ag.log" \
    && pass "una serie inexistente en el agregado se rechaza" \
    || fail "acepta una serie inexistente en el agregado"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 4d. PREVISIÓN DE MODELOS CON MA: los residuos pasados ──"
echo "   forecast_model necesita los residuos pasados para la parte MA: la"
echo "   previsión de un MA(q) ES una combinación de las últimas q innovaciones."
echo "   shootx solo ALOJA a[] (a ceros); quien lo calcula es elf. Sin esa llamada,"
echo "   TODO modelo con q>0 se preveía con residuos NULOS -- y la serie estacionaria"
echo "   salía exactamente 0.0000 a todo horizonte. No se notaba porque ningún"
echo "   modelo con MA se preveía en las pruebas. Contra fue 1.13.1 (Python):"
echo ""
echo "     h        fue     drtran(bug)    drtran"
echo "     1    81.8884   81.7807(-0.13%)  81.8947"
echo "     6    83.3690   83.1716(-0.24%)  83.3808"
echo ""

OUT="$TMPDIR/ma_fc.txt"
$DRTRAN "$WORK/ES_CPI_airline.pre" "$WORK/WTI_ar1.pre" -0 -f 6 -o "$OUT" >/dev/null 2>&1

# nivel previsto del airline, contra el de fue
lvl() { grep -A"$((4 + $2))" "Output: ES_CPI" "$1" | tail -1 | awk '{print $5}'; }
check "airline h=1: coincide con fue (81.8884)" 81.8884 "$(lvl "$OUT" 1)" 0.02
check "airline h=2: coincide con fue (81.9091)" 81.9091 "$(lvl "$OUT" 2)" 0.02
check "airline h=6: coincide con fue (83.3690)" 83.3690 "$(lvl "$OUT" 6)" 0.02

# LA REGRESIÓN: la serie estacionaria NO puede ser idénticamente cero
python3 - "$OUT" <<'PYMA'
import re, sys
t = open(sys.argv[1]).read()
m = re.search(r'Output: ES_CPI.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)', t)
w = [float(r.split()[1]) for r in m.group(1).strip().split('\n')]
# con residuos a cero, un MA puro en diferencias preve w = 0 a todo horizonte
sys.exit(0 if any(abs(v) > 1e-6 for v in w) else 1)
PYMA
[ $? -eq 0 ] \
    && pass "la previsión estacionaria de un MA NO es idénticamente cero (los residuos se usan)" \
    || fail "la previsión de un MA sale 0.0000: los residuos NO se están calculando"


# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 4e. PREVISIÓN RECURSIVA FUERA DE MUESTRA (-R) ──"
echo "   Las varianzas que un modelo declara son TEÓRICAS: se calculan SUPONIENDO"
echo "   QUE ESE MODELO ES CIERTO. Cada modelo se pone su propia nota, así que"
echo "   comparar dos varianzas teóricas NO es comparar dos modelos. La única forma"
echo "   honesta: estimar UNA vez, congelar los parámetros, y hacer rodar el origen."
echo ""

# --- el ejercicio del pass-through, ahora de verdad ---
for P in ES:ES_CPI_m10 DE:DE_CPI_mar3sar; do
    K=${P%%:*}; M=${P##*:}
    $DRTRAN "$WORK/$M.pre" "$WORK/WTI_ar1.pre" -0             -R 168 -f 12 \
            -o "$TMPDIR/R_${K}_uni.txt" >/dev/null 2>&1
    $DRTRAN "$WORK/$M.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -R 168 -f 12 \
            -o "$TMPDIR/R_${K}_tf.txt"  >/dev/null 2>&1
done

grep -q "RECURSIVE FORECAST EVALUATION" "$TMPDIR/R_ES_uni.txt" \
    && pass "el informe da la evaluación recursiva" \
    || fail "no hay evaluación recursiva"
NO=$(grep -E "^  Origins" "$TMPDIR/R_ES_uni.txt" | awk '{print $3}')
check "37 orígenes (obs 168..204, H=12, n=216)" 37 "$NO" 0.5

# --- ES: el crudo MEJORA a todo horizonte. Contradice a la varianza teorica.
python3 - "$TMPDIR" <<'PYR'
import re, sys
T = sys.argv[1]
def rmse(f):
    t = open(f).read()
    m = re.search(r'h      n        MAE.*?\n  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): float(r.split()[3]) for r in m.group(1).strip().split('\n')}
u, t = rmse('%s/R_ES_uni.txt' % T), rmse('%s/R_ES_tf.txt' % T)
bad = [h for h in (1, 2, 3, 6, 12) if t[h] >= u[h]]
sys.exit(0 if not bad else (print("ES: el crudo no mejora en h=%s" % bad) or 1))
PYR
[ $? -eq 0 ] \
    && pass "ES: fuera de muestra el crudo MEJORA el RMSE a TODO horizonte" \
    || fail "ES: el crudo no mejora fuera de muestra"

# --- DE: EMPEORA. Es donde el coeficiente retardado no era significativo.
python3 - "$TMPDIR" <<'PYD'
import re, sys
T = sys.argv[1]
def rmse(f):
    t = open(f).read()
    m = re.search(r'h      n        MAE.*?\n  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): float(r.split()[3]) for r in m.group(1).strip().split('\n')}
u, t = rmse('%s/R_DE_uni.txt' % T), rmse('%s/R_DE_tf.txt' % T)
sys.exit(0 if all(t[h] > u[h] for h in (1, 3, 6, 12)) else 1)
PYD
[ $? -eq 0 ] \
    && pass "DE: fuera de muestra el crudo EMPEORA (allí el retardo no era significativo)" \
    || fail "DE: el resultado fuera de muestra no es el esperado"

# --- LA LECCIÓN: la varianza teórica decía lo CONTRARIO para ES a h>=2.
python3 - "$TMPDIR" <<'PYL'
import re, sys
T = sys.argv[1]
def lvl_sd(f):
    t = open(f).read()
    m = re.search(r'Output: ES_CPI.*?\n(?:.*\n)*?  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): (float(r.split()[6]) - float(r.split()[5])) / (2 * 1.96)
            for r in m.group(1).strip().split('\n')}
def rmse(f):
    t = open(f).read()
    m = re.search(r'h      n        MAE.*?\n  ---+\n((?:  +\d+.*\n)+)', t)
    return {int(r.split()[0]): float(r.split()[3]) for r in m.group(1).strip().split('\n')}
lu, lt = lvl_sd('%s/R_ES_uni.txt' % T), lvl_sd('%s/R_ES_tf.txt' % T)
ru, rt = rmse('%s/R_ES_uni.txt' % T),   rmse('%s/R_ES_tf.txt' % T)
# a h=3 la TEORIA dice que la transferencia es PEOR (sd mayor)...
theory_worse = lt[3] > lu[3]
# ...y la REALIDAD dice que es mejor.
reality_better = rt[3] < ru[3]
# y la razon: el univariante es MAS sobreconfiado que la transferencia
uni_worse_calibrated = (ru[3] / lu[3]) > (rt[3] / lt[3])
sys.exit(0 if (theory_worse and reality_better and uni_worse_calibrated) else 1)
PYL
[ $? -eq 0 ] \
    && pass "a h=3 la TEORÍA dice que empeora y la REALIDAD que mejora: cada modelo se pone su nota" \
    || fail "no se reproduce la divergencia teoría/realidad"

# --- el CSV por origen ---
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -b 0 -r 0 -s 1 -R 200 -f 6 \
        -C "$TMPDIR/rec.csv" -o "$TMPDIR/rc.txt" >/dev/null 2>&1
[ -s "$TMPDIR/rec.csv" ] && head -1 "$TMPDIR/rec.csv" | grep -q "origin,horizon,actual,forecast,error" \
    && pass "escribe los errores por origen en CSV (-C)" \
    || fail "no escribe el CSV de errores"

# --- -R sin horizonte debe rechazarse ---
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -0 -R 168 -o "$TMPDIR/rr.txt" \
        > "$TMPDIR/rr.log" 2>&1
grep -q "needs a horizon" "$TMPDIR/rr.log" \
    && pass "-R sin -f se rechaza con un mensaje claro" \
    || fail "acepta -R sin horizonte"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "── 5. Sanidad ──"
echo ""
$DRTRAN -h > /dev/null 2>&1 && pass "drtran -h funciona" || fail "-h falla"

OUT="$TMPDIR/autoid.txt"
$DRTRAN "$WORK/ES_CPI_m10.pre" "$WORK/WTI_ar1.pre" -o "$OUT" > /dev/null 2>&1
grep -q "RECOMMENDED" "$OUT" && pass "la identificación automática corre" \
                             || fail "la identificación automática falla"

# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "============================================"
echo -e "  RESULTADO: ${GREEN}$PASS PASS${NC}, ${RED}$FAIL FAIL${NC}"
echo "============================================"
[ "$FAIL" -gt 0 ] && exit 1
exit 0
