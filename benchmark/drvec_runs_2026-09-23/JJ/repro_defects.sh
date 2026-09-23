#!/bin/sh
# Minimal repros of the suspected drvec defects found by the JJ-block validation.
# Self-contained: builds its inputs from the repo's datasets/ into ./repro/.
D=/home/david/Dropbox/SRC/drvec/bin/drvec
DS=/home/david/Dropbox/SRC/drvec/datasets
cd "$(dirname "$0")"; mkdir -p repro; cd repro
ll() { grep -m1 -E '^(eval )?logelf' "$1.out" | awk '{print $NF}'; }
crit() { grep -m1 'Convergence criterion' "$1.out" | sed 's/.*criterion: //'; }

# UK log consumption/income, [incl ; conl]; and the SAME data with incl demeaned.
awk -F, 'NR>1{printf "%.10f %.10f\n",$2,$1}' $DS/urca_UKconinc.csv > body
n=$(wc -l < body)
m=$(awk '{s+=$1}END{printf "%.12f", s/NR}' body)
printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" $n > ukc.inp;  cat body >> ukc.inp
printf "4\n2 %d 1 1955\nincl conl\n1.0 0 0\n" $n > ukcd.inp
awk -v m=$m '{printf "%.10f %.10f\n",$1-m,$2}' body >> ukcd.inp

echo "D1  -fixb2 v seeds E[W]/Lambda/F/Sigma from the static-OLS B2, not from v"
echo "    (case 2: shifting a Y2 column by a constant is likelihood-invariant)"
for s in ukc ukcd; do
  $D $s 2 0 1 -case 2 -fixb2 0 >/dev/null 2>&1; a=$(ll $s); c=$(crit $s)
  $D $s 2 0 1 -case 2 -fixb2 0 -eval >/dev/null 2>&1; e=$(ll $s)
  $D $s 2 0 1 -case 2 >/dev/null 2>&1; f=$(ll $s)
  echo "    $s: free logL=$f   -fixb2 0: start=$e  end=$a  ($c)"
done

# UK consumption, logs, the fixture of tests/run_tests.sh
awk -F, 'NR>1 && $1+0>0 {n++; a[n]=log($1); b[n]=log($2); c[n]=log($3)}
  END{ printf "4\n3 %d 1 1955\nlcons linc lprice\n1.0 0 0\n", n;
       for(i=1;i<=n;i++) printf "%.10f %.10f %.10f\n", a[i], b[i], c[i] }' $DS/urca_UKconsumption.csv > uk.inp
echo "D2  -lrtest / cold start stops at local optima (tc3); other routes find more"
$D uk 2 0 1 -case 2 >/dev/null 2>&1;            echo "    uk r=1 cold          : $(ll uk)  ($(crit uk))"
$D uk 2 0 1 -case 2 -seedjoh >/dev/null 2>&1;   echo "    uk r=1 -seedjoh      : $(ll uk)"
$D uk 2 0 2 -case 2 >/dev/null 2>&1;            echo "    uk r=2 cold          : $(ll uk)  ($(crit uk))"
$D uk 2 0 2 -case 2 -multistart 30 >/dev/null 2>&1; echo "    uk r=2 -multistart 30: $(ll uk)"
$D uk 2 0 1 -case 2 -lrtest >/dev/null 2>&1
echo "    uk -lrtest table:"; sed -n '/r    npar/,/^$/p' uk.out | sed 's/^/      /'
echo "D3  -seedjoh hard-fails when the canonical seed is non-stationary (no fallback)"
$D uk 2 0 2 -case 2 -seedjoh >/dev/null 2>&1; echo "    exit=$?  $(grep -A1 'ESTIMATION FAILED' uk.out | tr '\n' ' ')"
echo "    seed printed: $(grep -A1 'seeded from the canonical' uk.out | tail -1)"
echo "    (ca.jo ecdet=const, same data, gives (-1.365, -0.624); ecdet=none on n-1 obs gives (-1.408,-0.319))"
