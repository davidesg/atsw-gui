#!/usr/bin/env bash
# benchmark_identification.sh — Monte Carlo del identificador Box-Jenkins de ART.
#
# Mide "true ∈ shortlist" (recall del shortlist BJ del MLP) y la precisión exacta,
# para AR/MA/ARMA puros y mixtos. El shortlist es lo que importa en la atsw-suite,
# porque atsw-MCP estima y elige el modelo final entre los candidatos.
#
# Uso:  tests/benchmark_identification.sh [REPS] [N] [SEED]
set -euo pipefail
cd "$(dirname "$0")/.."

REPS="${1:-200}"; N="${2:-300}"; SEED="${3:-123}"
CLI=bin/art_cli
[ -x "$CLI" ] || { echo "Compila primero: make cli"; exit 1; }

run() {
  local label="$1"; shift
  local out
  out=$("$CLI" -s -n "$N" --reps "$REPS" --seed "$SEED" --pmax 5 --qmax 5 --mlp-direct "$@" 2>/dev/null)
  local exact short
  exact=$(echo "$out" | grep "Precisión" | grep -oE '[0-9.]+%' | head -1)
  short=$(echo "$out" | grep "en shortlist" | grep -oE '[0-9.]+%' | head -1)
  printf "%-22s exact %-8s  true∈shortlist %s\n" "$label" "$exact" "$short"
}

echo "=== ART identification benchmark (reps=$REPS, n=$N, seed=$SEED, pmax=qmax=5) ==="
run "AR(1) .6"          -p 1 --phi 0.6
run "AR(2) .5,-.3"      -p 2 --phi 0.5,-0.3
run "AR(3) .5,-.3,.2"   -p 3 --phi 0.5,-0.3,0.2
run "MA(1) .6"          -q 1 --theta 0.6
run "MA(2) .5,.3"       -q 2 --theta 0.5,0.3
run "ARMA(1,1) .5/.4"   -p 1 -q 1 --phi 0.5 --theta 0.4
run "ARMA(2,1)"         -p 2 -q 1 --phi 0.5,-0.3 --theta 0.4
