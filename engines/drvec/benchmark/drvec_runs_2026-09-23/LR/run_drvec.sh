#!/bin/bash
# Re-runnable: builds inputs then runs every drvec fit of the LR validation.
# Usage: ./run_drvec.sh [case ...]   (default: all)
set -u
D=$(cd "$(dirname "$0")" && pwd); cd "$D"
BIN=/home/david/Dropbox/SRC/drvec/bin/drvec
TO=${TO:-1800}
python3 build_inputs.py >/dev/null
declare -A RANK=( [e1]=1 [e3]=1 [rao1]=2 [rao2]=3 [rao3]=3 [rao4]=2 [rao5]=2 [rao6]=5 [rao7]=2 )
CASES=${*:-"e1 e3 rao7 rao2 rao1 rao5 rao4 rao3 rao6"}
run() { # stem extra-args...
  local stem=$1; shift; local src=$1; shift
  cp "$src.inp" "$stem.inp"
  local t0=$(date +%s.%N)
  timeout $TO $BIN "$stem" "$@" > "$stem.log" 2>&1; local ec=$?
  local t1=$(date +%s.%N)
  printf "%-22s exit=%-3s %6.1fs  %s\n" "$stem" "$ec" "$(echo "$t1-$t0"|bc)" "$*" | tee -a runs.log
}
for c in $CASES; do
  R=${RANK[$c]}
  run ${c}_lr        $c 2 0 1  -case 2 -lrtest
  run ${c}_q0r$R     $c 2 0 $R -case 2
  run ${c}_q0r${R}_ms $c 2 0 $R -case 2 -multistart 20
  run ${c}_q0r${R}_sj $c 2 0 $R -case 2 -seedjoh
  run ${c}_q1r$R     $c 2 1 $R -case 2
done
