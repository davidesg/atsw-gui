#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# examples/m6/run.sh
#
# La ESCALERA METODOLOGICA de la escuela de Treadway (Munoz Polo 2001), de punta
# a punta con drtran, sobre el caso del empleo espanol por sectores (Relloso
# 1997, ICAE 9720, Tabla 4). Seis series: EP EI EU EC EA P.
#
#   Paso 1  univariantes .pre (Tabla 4)          <- ya construidos (build_m6.py)
#   Paso 2  modelo DIAGONAL con covarianzas       -> drtran -0
#   Paso 3  IDENTIFICACION de la red (ccf resid.) -> drtran -i
#   Paso 4  RED de transferencias                 -> drtran -n
#             (a) numeradores libres
#             (b) con los PRODUCTOS del legacy (MA compartida)
#             (c) con la ESTRUCTURA COMPLETA (+ el factor fijo (1-B))
#
# Uso:  ./run.sh            (o  DRTRAN=/ruta/a/drtran ./run.sh)
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")"

DT=${DRTRAN:-../../bin/drtran}
DAT=../../tests/data/m6
M6="$DAT/M6_EP.pre $DAT/M6_EI.pre $DAT/M6_EU.pre $DAT/M6_EC.pre $DAT/M6_EA.pre $DAT/M6_P.pre"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

[ -x "$DT" ] || { echo "No encuentro drtran en '$DT'. Compila (make) o pon DRTRAN=..."; exit 1; }

ll() { grep -E "Log-likelihood" "$1" | grep -oE "\-[0-9.]+"; }
hd() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }

# --- Paso 1: los univariantes ------------------------------------------------
hd "Paso 1  Univariantes de la Tabla 4 (.pre)"
echo "Los seis .pre son los ajustes UTI/MEG de la Tabla 4 (Relloso 1997),"
echo "construidos por tests/build_m6.py. Homologan con fue: drtran -0 los reproduce."
echo "  Ejemplo, EA (estacional generalizada, la unica estocastica en estacionalidad):"
$DT $DAT/M6_EA.pre $DAT/M6_P.pre -0 -o "$OUT/homolog.out" >/dev/null 2>&1
grep -E "^theta_1\[|^theta_1\[B\^1\]|^theta" "$OUT/homolog.out" | head -3 | sed 's/^/    /'

# --- Paso 2: el diagonal -----------------------------------------------------
hd "Paso 2  Modelo DIAGONAL con covarianzas libres (drtran -0)"
echo "Se estiman los seis univariantes JUNTOS, con Sigma = sigma2*Q y las 3"
echo "covarianzas contemporaneas que libera el legacy (EA*EI, EA*EC, EI*EU)."
$DT $M6 -0 -c $DAT/m6.cns -o "$OUT/diag.out" 2>&1 | grep -iE "CONV|STOP" | sed 's/^/  /'
echo "  log-likelihood diagonal:  $(ll "$OUT/diag.out")"

# --- Paso 3: identificar la red ----------------------------------------------
hd "Paso 3  IDENTIFICACION de la red (drtran -i)"
echo "Tras el diagonal, drtran lee las CCF de los residuos y PROPONE la red:"
$DT $M6 -0 -c $DAT/m6.cns -i -o "$OUT/netid.out" >/dev/null 2>&1
awk '/NETWORK IDENTIFICATION/{f=1} /WHAT TO REFORMULATE/{f=0} f' "$OUT/netid.out" \
    | grep -vE "^ *$" | sed 's/^/  /'
echo
echo "El DRIVER guiado (-g NAME) hace lo mismo pero ESCRIBE NAME.dag y NAME.cns"
echo "(listos para -n/-c) y emite el plan con el siguiente comando:"
$DT $M6 -g "$OUT/m6cand" -c $DAT/m6.cns -o "$OUT/guide.out" >/dev/null 2>&1
awk '/GUIDED MODE/{f=1} /^====/&&f&&n++{exit} f' "$OUT/guide.out" | sed 's/^/  /'

# --- Paso 4: la red de transferencias ----------------------------------------
hd "Paso 4  RED de transferencias (drtran -n)  EC->EU->EI->EP + EC->EP"
echo "(a) numeradores LIBRES:"
$DT $M6 -n $DAT/m6_net.dag -c $DAT/m6_net.cns -o "$OUT/net_free.out" >/dev/null 2>&1
echo "      log-likelihood:  $(ll "$OUT/net_free.out")   (el diagonal era $(ll "$OUT/diag.out"))"
echo "      -> los transfers ganan ~12 sobre el diagonal."
echo
echo "Los NUMERADORES FACTORIZADOS del legacy comparten parametros. Para aislarlos de"
echo "las intervenciones compuestas (debilmente identificadas, saltan de modo y dominan"
echo "la l) se FIJAN los deterministas en sus valores Relloso con -D -E:"
echo
$DT $M6 -n $DAT/m6_net.dag -c $DAT/m6_net.cns      -D -E -o "$OUT/net_dde.out"  >/dev/null 2>&1
echo "      transfer LIBRE con -D -E:  $(ll "$OUT/net_dde.out")   (la referencia)"
echo "(b) con los PRODUCTOS (MA compartida: EP<-EI usa la MA de EI; EU<-EC la de EU):"
$DT $M6 -n $DAT/m6_net.dag -c $DAT/m6_net_prod.cns -D -E -o "$OUT/net_prod.out" >/dev/null 2>&1
echo "      log-likelihood:  $(ll "$OUT/net_prod.out")   (+2 restricciones, Dl~0.4)"
echo "(c) con la ESTRUCTURA COMPLETA (+ el factor FIJO (1-B) de EI<-EU):"
$DT $M6 -n $DAT/m6_net.dag -c $DAT/m6_net_full.cns -D -E -o "$OUT/net_full.out" >/dev/null 2>&1
echo "      log-likelihood:  $(ll "$OUT/net_full.out")"
grep -E "^omega[0-9]\[" "$OUT/net_full.out" | sed 's/^/      /'

hd "Fin"
echo "La estructura del legacy expresable con la tabla de slots (2 productos + el (1-B))"
echo "no es rechazada: Dl=2.5 sobre 3 g.l., chi2(3).95=7.81, p~0.17."
echo "Detalle y teoria: docs/M6_TABLA4_BASELINE.md (secciones 1-11)."
