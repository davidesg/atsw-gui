#!/bin/sh
# ADF/KPSS de ART C contra art-python (art/_raiz_unitaria.py, que es statsmodels
# 0.14 a 1e-10), sobre ocho series reales: la serie G y el IPC de España
# 2002-2019, en log, ∇, ∇² y ∇12. Uso: sh tests/unit_root/run.sh (desde la raíz)
set -e
D=$(dirname "$0"); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
gcc -O2 -Iinclude $(pkg-config --cflags gsl) -o "$T/h" "$D/harness.c" src/unit_root_tests.c \
    $(pkg-config --libs gsl) -lm 2>/dev/null
"$T/h" "$D/series.txt" > "$T/c.txt"
grep -v '^#' "$D/referencia.txt" | paste -d' ' - "$T/c.txt" | awk '
function ad(a,b){ d=a-b; return d<0?-d:d }
{ nm=$1; ok = ($10==0) && ad($2,$11)<1e-8 && ad($3,$12)<1e-10 && $4==$13 && $5==$14 \
          && ad($6,$15)<1e-12 && ad($7,$16)<1e-12 && ad($8,$17)<1e-12 && $9==$18
  printf "%s %s\n", ok?"PASS":"FAIL", nm; if(!ok) bad++ }
END { printf "%d FAIL\n", bad+0; exit bad>0 }'
