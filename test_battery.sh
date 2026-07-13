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
S11=$(grep "Sigma\[1,1\]" "$OUT" | awk '{print $NF}')
S22=$(grep "Sigma\[2,2\]" "$OUT" | awk '{print $NF}')
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
check "Sigma[1,1]"    0.069328 "$(grep 'Sigma\[1,1\]' "$OUT" | awk '{print $NF}')" 0.0005

# --- Airline puro: MA regular + MA anual (sin media) --------------------------
echo "   [airline MA] ES_CPI_airline: ∇∇₁₂ ln y = (1+0.4212B)(1-0.8147B¹²)a"
OUT="$TMPDIR/air2.txt"
$DRTRAN "$CASES/ES_CPI_airline.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

check "theta (B^1)"  -0.421156 "$(val "$OUT" 'theta_1\[B\^1\]')"  0.0005
check "Theta (B^12)"  0.814706 "$(val "$OUT" 'theta_1\[B\^12\]')"  0.0005
check "Sigma[1,1]"    0.071022 "$(grep 'Sigma\[1,1\]' "$OUT" | awk '{print $NF}')" 0.0005

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
check "Sigma[1,1]"     0.016968 "$(grep 'Sigma\[1,1\]' "$OUT" | awk '{print $NF}')" 0.0005

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
check "Sigma[1,1]"     0.016176 "$(grep 'Sigma\[1,1\]' "$OUT" | awk '{print $NF}')" 0.0005

# --- Coeficiente ARMA FIJADO en el .pre ---------------------------------------
# FR_CPI_f5 trae "0.0000  0": el AR(1) está FIJO, no es un valor inicial.
echo "   [f=5 + AR fijo] FR_CPI_f5: AR(1) fijado en 0 por el .pre"
OUT="$TMPDIR/fr5.txt"
$DRTRAN "$CASES/FR_CPI_f5.pre" "$CASES/WTI_ar1.pre" -0 -o "$OUT" > /dev/null 2>&1

grep -q "phi_1\[B\^1\].*fixed" "$OUT" && pass "el AR(1) respeta el flag del .pre (fijo)" \
                                      || fail "el AR(1) NO respeta el flag del .pre"
check "MA f=5 (c2)"   -0.954212 "$(val "$OUT" 'theta_1\[f=5\]')"  0.0005
check "Sigma[1,1]"     0.046484 "$(grep 'Sigma\[1,1\]' "$OUT" | awk '{print $NF}')" 0.0005

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

S11=$(grep "Sigma\[1,1\]" "$OUT" | awk '{print $NF}')
S22=$(grep "Sigma\[2,2\]" "$OUT" | awk '{print $NF}')
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
echo "── 3. PASS-THROUGH: con Y = X la verdad es omega_0 = 1 ──"
echo ""

OUT="$TMPDIR/passthru.txt"
$DRTRAN "$WORK/WTI_ar1.pre" "$WORK/WTI_ar1.pre" -r 0 -s 0 -b 0 -o "$OUT" > /dev/null 2>&1
check "omega_0 (Y=X)"  1.0 "$(val "$OUT" 'omega1\[0\]')"  0.01

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

SN=$(grep "Sigma\[1,1\]" "$OUT" | awk '{print $NF}')
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
