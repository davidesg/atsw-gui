#!/bin/bash
# Second exact-ML queue (prioritised; rao7 raw replaced by rao7_sc; rao6 dropped: drvec cannot fit it).
cd "$(dirname "$0")"
printf "%s\n" "rao1 1" "rao1 2" "rao5 0" "rao5 1" "rao5 2" "rao5 3" "rao7_sc 0" "rao7_sc 1" "rao7_sc 2" "rao7_sc 3" \
  "rao3_3v 0" "rao3_3v 1" "rao3_3v 2" "rao4 0" "rao4 1" "rao4 2" "rao4 3" "rao1 3" "rao1 4" "rao5 4" "rao4 4" "rao4 5" \
  "rao3 0" "rao3 1" "rao3 2" "rao3 3" "rao3 4" "rao3 5" |
  xargs -P ${NP:-3} -L 1 sh -c 'OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 exact_ml.py $0 $1 > exact_$0_r$1.log 2>&1'
