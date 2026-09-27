#!/bin/sh
# Re-run the whole JJ-block validation of drvec (drvec 0.10, statsmodels 0.14.6, R urca 1.3.4).
# ~30-40 min on 4 cores (the -multistart 30 audits of the M=5 cases dominate).
set -e
cd "$(dirname "$0")"
CASES="denmark denmark5 finland finland_const UKconinc UKconinc_seas UKconsumption UKconsumption_lev Canada UKpppuip"
# 1. inputs, Johansen references (ca.jo published spec + same data; statsmodels), drvec -lrtest / fits
for c in $CASES; do echo $c; done | xargs -P 4 -I{} python3 bench.py {}
# 2. rank-by-rank optimum audit (cold, -seedjoh, -multistart 30, profile at Johansen's beta)
for c in $CASES; do echo $c; done | xargs -P 4 -I{} python3 scan.py {}
# 3. tables
python3 report.py   > /dev/null   # RESULTS_tables.md  (bench.py view, all routes as run)
python3 analyze.py  > /dev/null   # RESULTS_rank.md    (rank audit)
python3 final.py    > /dev/null   # RESULTS_summary.md (compact per-case tables)
cat RESULTS_head.md RESULTS_summary.md RESULTS_rank.md > RESULTS.md
# minimal repros of the defects: see RESULTS.md, section "Suspected drvec defects"
