#!/bin/bash
# Extra drvec runs: rescaled inputs, case 3 for the trending rao6, rao3 without dummies,
# and -lrtest -seedjoh on every case (-multistart is ignored by -lrtest).
cd "$(dirname "$0")"; BIN=/home/david/Dropbox/SRC/drvec/bin/drvec
run() { local stem=$1 src=$2; shift 2; cp $src.inp $stem.inp; timeout 1800 $BIN $stem "$@" > $stem.log 2>&1; echo "$stem exit=$? $*" | tee -a runs.log; }
for c in e1 e3 rao1 rao2 rao3 rao4 rao5 rao7_sc rao3_3v; do run ${c}_lrsj $c 2 0 1 -case 2 -lrtest -seedjoh; done
run rao7_sc_lr     rao7_sc 2 0 1 -case 2 -lrtest
run rao7_sc_q0r2   rao7_sc 2 0 2 -case 2
run rao7_sc_q0r2_ms rao7_sc 2 0 2 -case 2 -multistart 20
run rao7_sc_q0r2_sj rao7_sc 2 0 2 -case 2 -seedjoh
run rao7_sc_q1r2   rao7_sc 2 1 2 -case 2
run rao3_3v_lr     rao3_3v 2 0 1 -case 2 -lrtest
run rao3_3v_q0r1   rao3_3v 2 0 1 -case 2
run rao6_sc_lr     rao6_sc 2 0 1 -case 2 -lrtest
run rao6_c3_lr     rao6    2 0 1 -case 3 -lrtest
run rao6_sc_c3_lr  rao6_sc 2 0 1 -case 3 -lrtest
run rao6_c3_q0r5   rao6    2 0 5 -case 3
run rao2_q0r3_sj_ms rao2   2 0 3 -case 2 -seedjoh -multistart 20
run rao1_q0r2_sj_ms rao1   2 0 2 -case 2 -seedjoh -multistart 20
run rao4_q0r2_sj_ms rao4   2 0 2 -case 2 -seedjoh -multistart 20
