#!/bin/bash
# Independent exact ML (exact_ml.py) for every case and every rank r=0..M-1, in parallel.
cd "$(dirname "$0")"
declare -A MM=( [e1]=3 [e3]=4 [rao1]=5 [rao2]=4 [rao3]=6 [rao4]=6 [rao5]=5 [rao6]=8 [rao7]=4 )
jobs=()
for c in ${*:-e1 e3 rao7 rao2 rao1 rao5 rao4 rao3 rao6}; do
  for ((r=0; r<${MM[$c]}; r++)); do jobs+=("$c $r"); done
done
printf "%s\n" "${jobs[@]}" | xargs -P ${NP:-6} -L 1 sh -c 'OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 exact_ml.py $0 $1 > exact_$0_r$1.log 2>&1'
