#!/bin/sh
# tests/repro/repro.sh -- minimal reproductions of the defects registered on
# 2026-09-23 (docs/BUGS.md, BUG-23 .. BUG-45), from the review recorded in
# docs/REVIEW_2026-09-23.md.
#
#   sh tests/repro/repro.sh            all of them
#   sh tests/repro/repro.sh 23 31      only those
#
# Each block prints what the register says it prints.  It is NOT part of
# `make test`: several of these crash on purpose (exit 139) and one runs a
# Monte Carlo.  When a defect is fixed its block should stop showing it, and
# that is the moment to turn it into a check in tests/run_tests.sh.
#
# Works in a temporary directory: drvec writes its .out next to its input.

HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/../.." && pwd)
B="$TOP/bin/drvec"
F="$HERE/fixtures"
DS="$TOP/datasets"
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
cd "$W" || exit 1

want() { [ $# -eq 0 ] && return 0; for n in $SEL; do [ "$n" = "$1" ] && return 0; done; return 1; }
SEL="$*"
hdr() { printf '\n=== BUG-%s  %s\n' "$1" "$2"; }
rc()  { "$@" >/dev/null 2>&1; echo $?; }
lr()  { grep -a 'LR = 2(free' "$1.out" | awk '{print $NF}'; }

if want 23; then hdr 23 "Gamma/Theta in internal order, labelled in .inp order"
  cp "$F/sim2.inp" s.inp; "$B" s 2 0 1 -case 1 >/dev/null 2>&1
  echo "  truth: D.x <- 0.4 D.y(-1)   (x = column 1 = xY2, y = column 2 = yY1)"
  grep -a 'D.yY1 <- D.xY2(-1)\|D.xY2 <- D.yY1(-1)' s.out
  grep -a 'drives the others\|is driven by the others' s.out | grep -a REJECT
fi

if want 24; then hdr 24 "case-1 critical values are the unrestricted-constant table"
  echo "  Monte Carlo under H0 (independent random walks, no constant), 200 reps:"
  python3 "$HERE/mc_case1.py" "$B" 200
fi

if want 25; then hdr 25 "-lrtest: one cold start per rank; UKconsumption LRs swapped"
  awk -F, 'NR>1 && $1+0>0 {n++; a[n]=log($1); b[n]=log($2); c[n]=log($3)}
    END{ printf "4\n3 %d 1 1955\nlcons linc lprice\n1.0 0 0\n", n;
         for(i=1;i<=n;i++) printf "%.10f %.10f %.10f\n", a[i], b[i], c[i] }' \
    "$DS/urca_UKconsumption.csv" > uk.inp
  "$B" uk 2 0 1 -case 2 -lrtest >/dev/null 2>&1
  sed -n '/M-r        LR/,/^$/p' uk.out
  echo "  (2026-08-17, benchmark/README.md: r=0 70.1280, r=1 25.5141)"
  "$B" uk 2 0 1 -case 2 -lrtest -multistart 20 >/dev/null 2>&1
  echo "  with -multistart 20 (ignored):"; sed -n '/M-r        LR/,/^$/p' uk.out
fi

if want 26; then hdr 26 "a negative rank LR is printed as proof of non-convergence"
  cp "$F/neg.inp" n.inp; "$B" n 1 0 1 -case 1 -lrtest >/dev/null 2>&1
  grep -a -A1 'NOT INTERPRETABLE' n.out
  "$B" n 1 0 1 -case 1 -multistart 20 >/dev/null 2>&1
  echo "  r=1 with -multistart 20: $(grep -a -m1 'logelf' n.out)"
  "$B" n 1 0 0 -case 1 >/dev/null 2>&1 && echo "  r=0: $(grep -a -m1 'logelf' n.out)"
fi

if want 27; then hdr 27 "-lrtest with q>=1 compares non-nested MA classes"
  cp "$F/lq.inp" l.inp
  "$B" l 2 1 1 -case 2 -lrtest >/dev/null 2>&1;         echo "  default: $(grep -a -A2 'M-r        LR' l.out | tail -1)"
  "$B" l 2 1 1 -case 2 -lrtest -mafree >/dev/null 2>&1; echo "  -mafree: $(grep -a -A2 'M-r        LR' l.out | tail -1)"
fi

if want 28; then hdr 28 "-alpha rows read in internal order; the run_tests.sh check is vacuous"
  printf '2 1\n1\n0\n' > A10.txt
  for o in "-alpha A10.txt" "-weakex 1" "-weakex 2"; do
    cp "$DS/mauricio/mink_muskrat.inp" c.inp; "$B" c 2 1 1 -case 2 $o >/dev/null 2>&1
    printf '  %-16s LR = %s\n' "$o" "$(lr c)"; done
  echo "  (A = [1;0] declares equation 2 not to adjust: it must equal -weakex 2)"
  echo "  run_tests.sh lr_of prints field 5: [$(grep -a 'LR = 2(free' c.out | awk '{print $5}')]"
fi

if want 29; then hdr 29 "-lrtest -weakex restricts a different series at each rank (M>=3)"
  cp "$DS/synthetic/rank2.inp" g.inp
  "$B" g 2 0 1 -case 2 -lrtest -weakex 1 >/dev/null 2>&1
  echo "  -lrtest -weakex 1, r=2 row: $(grep -a -E '^ +2 +[0-9]+ +-?[0-9]' g.out | head -1)"
  for k in 1 2 3; do "$B" g 2 0 2 -case 2 -weakex $k >/dev/null 2>&1
    echo "  standalone r=2 -weakex $k : $(grep -a -m1 'logelf' g.out)"; done
fi

if want 30; then hdr 30 "-warma with -alpha/-weakex reads past the parameter vector"
  cp "$DS/mauricio/mink_muskrat.inp" w.inp
  if command -v valgrind >/dev/null; then
    valgrind -q "$B" w 2 1 1 -case 2 -warma -weakex 2 >/dev/null 2>vg.log
    echo "  exit=$?  invalid reads: $(grep -c 'Invalid read' vg.log)"
  else echo "  (needs valgrind)"; fi
fi

if want 31; then hdr 31 "three crashes (expect exit 139)"
  cp "$F/mmd.inp" d.inp
  echo "  -differenced -matest 3          : exit $(rc "$B" d 2 1 1 -case 2 -differenced -matest 3)"
  cp "$DS/synthetic/rank2.inp" r.inp
  echo "  -lrtest -fixb2 -bootstrap (M=3) : exit $(rc "$B" r 1 0 1 -case 2 -lrtest -fixb2 -bootstrap 20)"
  cp "$DS/mauricio/mink_muskrat.inp" w.inp
  echo "  -warma -seedgate                : exit $(rc "$B" w 2 1 1 -case 2 -warma -seedgate)"
fi

if want 32; then hdr 32 "-seedgate -fixb2 v replaces the fixed B2 with 0"
  cp "$DS/mauricio/mink_muskrat.inp" m.inp
  "$B" m 2 1 1 -case 2 -mean -fixb2 1.0 >/dev/null 2>&1;           echo "  -fixb2 1.0          : B2 = $(grep -a -m1 -A1 'FIXED at' m.out | tail -1)"
  "$B" m 2 1 1 -case 2 -mean -seedgate -fixb2 1.0 >/dev/null 2>&1; echo "  -seedgate -fixb2 1.0: B2 = $(grep -a -m1 -A1 'FIXED at' m.out | tail -1)"
fi

if want 33; then hdr 33 "-fixb2 v starts from the static-OLS B2, not from v"
  awk -F, 'NR>1{printf "%.10f %.10f\n",$2,$1}' "$DS/urca_UKconinc.csv" > body
  n=$(wc -l < body); m=$(awk '{s+=$1}END{printf "%.12f", s/NR}' body)
  printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" "$n" > u.inp;  cat body >> u.inp
  printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" "$n" > ud.inp
  awk -v m="$m" '{printf "%.10f %.10f\n",$1-m,$2}' body >> ud.inp
  for s in u ud; do "$B" $s 2 0 1 -case 2 -fixb2 0 >/dev/null 2>&1
    echo "  $s -fixb2 0: $(grep -a -m1 'logelf' $s.out)  $(grep -a -m1 'Convergence criterion' $s.out)"; done
  echo "  (ud is u with incl demeaned: the case-2 likelihood is the same function)"
fi

if want 34; then hdr 34 "-fdhess publishes SEs built from penalty jumps"
  cp "$TOP/data/VILL.inp" v.inp; "$B" v 2 1 1 -case 2 -fdhess >/dev/null 2>&1
  grep -a 'A.Vienna(-1)' v.out
fi

if want 35; then hdr 35 "-artest skips the admissible-start ladder that -warma uses"
  cp "$DS/mauricio/mink_muskrat.inp" t.inp
  "$B" t 2 1 1 -case 2 -artest 20 >/dev/null 2>&1; grep -a -i 'failed' t.out | head -2
  "$B" t 2 1 1 -case 2 -warma >/dev/null 2>&1;     echo "  plain -warma: $(grep -a -m1 'logelf' t.out)"
fi

if want 36; then hdr 36 ".pre route forecasts are in w units (refactor, Box-Cox, deterministics)"
  cp "$F/mink100.pre" "$F/musk100.pre" .
  "$B" musk100.pre mink100.pre 2 0 1 -f 2 >/dev/null 2>&1
  f=$(ls *.forecast 2>/dev/null | head -1); [ -n "$f" ] && sed -n '1,8p' "$f"
  echo "  (the log series are ~11-14; refactor 100 is never undone)"
fi

if want 37; then hdr 37 "residuals dated one period early"
  cp "$TOP/data/synth.inp" y.inp; "$B" y 2 0 1 >/dev/null 2>&1
  echo "  data: 99 obs 2000-2098; after the difference 98 obs 2001-2098"
  grep -a 'observations from\|observations: from' y.out | head -2
fi

if want 38; then hdr 38 "the univariate evaluation of a .pre drops a FIXED mean"
  cp "$DS/mauricio/mink_muskrat.inp" m.inp; cp "$F"/free.?.pre "$F"/fixd.?.pre .
  for p in free fixd; do printf '  %s (mu flag %s): ' $p "$(sed -n '27p' $p.1.pre)"
    "$B" m 2 1 1 -case 2 -mafree -seed $p -eval 2>/dev/null | grep -a 'sum univariate'; done
  cp "$F"/anna.?.pre .
  printf '  annual AR(1) seed: '; "$B" m 2 1 1 -case 2 -mafree -seed anna -eval 2>&1 | grep -a -i 'sum univariate\|order' | head -2 | tr '\n' ' '; echo
fi

if want 39; then hdr 39 "the shared .pre reader accepts malformed files"
  cp "$F"/ifempty.pre "$F"/long100.pre "$F"/trunc.pre "$TOP/tests/fixtures/mmdiag.2.pre" .
  for f in ifempty long100 trunc; do "$B" $f.pre mmdiag.2.pre 2 0 1 >$f.log 2>&1
    echo "  $f: exit $?  $(grep -a -m1 -i 'error' $f.log)"; done
  echo "  (ifempty: SIGSEGV; long100: a lambda=1 file read as lambda=0; trunc: estimates with zeros)"
fi

if want 41; then hdr 41 "engine: the line search never returns on a NaN objective"
  gcc -O0 -I"$TOP/include" -o pn "$HERE/probes/probe_nan.c" "$TOP"/src/qnewtopt.c "$TOP"/src/nlatools.c -lgsl -lgslcblas -lm 2>/dev/null \
    && { timeout 10 ./pn >/dev/null 2>&1; echo "  exit $? (124 = still looping after 10 s)"; } \
    || echo "  (probe did not build here)"
fi

if want 42; then hdr 42 "diagnose.c: histogram heap overflow; chisq capped above 1000"
  gcc -g -fsanitize=address -I"$TOP/include" -o ph "$HERE/probes/probe_hist.c" "$TOP"/src/diagnose.c "$TOP"/src/nlatools.c -lgsl -lgslcblas -lm 2>/dev/null \
    && ./ph 700 6 2>&1 | grep -a -m2 'AddressSanitizer\|diagnose.c:8'
  gcc -I"$TOP/include" -o pc "$HERE/probes/probe_chi.c" "$TOP"/src/nlatools.c -lgsl -lgslcblas -lm 2>/dev/null \
    && ./pc 2>&1 | grep -a -m2 'df=1000'
fi

if want 43; then hdr 43 "no fallback from a non-stationary start; raw scale breaks the optimiser"
  cp "$F/rao6.inp" "$F/rao7.inp" "$F/rao7_sc.inp" .
  "$B" rao6 2 0 1 -case 3 -lrtest >/dev/null 2>&1
  echo "  rao6 -case 3 -lrtest (a stationary OLS start exists, max|eig| 0.966):"
  sed -n '/M-r        LR/,/^$/p' rao6.out | sed -n '3,5p'
  grep -a -m2 'estimation failed' rao6.out
  "$B" rao7 2 0 1 -case 2 -lrtest >/dev/null 2>&1;    echo "  rao7 raw   :"; sed -n '/M-r        LR/,/^$/p' rao7.out
  "$B" rao7_sc 2 0 1 -case 2 -lrtest >/dev/null 2>&1; echo "  rao7 scaled:"; sed -n '/M-r        LR/,/^$/p' rao7_sc.out
fi

if want 45; then hdr 45 "command-line options accepted and ignored"
  cp "$DS/mauricio/mink_muskrat.inp" c.inp
  "$B" c 1 0 1 -name zzz >/dev/null 2>&1; echo "  -name zzz on the .inp route: zzz.out exists? $( [ -f zzz.out ] && echo yes || echo no)"
  "$B" c 1 0 1 -case 1 -mean >/dev/null 2>&1; echo "  -case 1 -mean -> $(grep -a -m1 -i 'case' c.out)"
fi
echo
