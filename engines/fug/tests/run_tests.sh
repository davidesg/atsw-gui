#!/bin/sh
# Regression tests of FUG 1.15.   Usage: sh tests/run_tests.sh [path/to/fug]
# fug needs no other program: the tests do not use gnuplot or pdflatex.

FUG=${1:-./fug}
case "$FUG" in /*) ;; *) FUG="$(pwd)/$FUG" ;; esac
TOP=$(cd "$(dirname "$0")/.." && pwd)
WORK="$TOP/tests/work"
PASS=0
FAIL=0

rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 1
cp "$TOP"/examples/*.inp "$TOP"/tests/data/*.inp .


ok()   { PASS=$((PASS+1)); }
bad()  { FAIL=$((FAIL+1)); echo "FAIL: $*"; }

# run <name> <expected rc> fug-args...: stdout/stderr in <name>.log/<name>.err
run() {
    name=$1; want=$2; shift 2
    "$FUG" "$@" > "$name.log" 2> "$name.err"
    rc=$?
    if [ "$rc" != "$want" ]; then bad "$name: exit status $rc (expected $want)"; sed 's/^/    /' "$name.err"; return 1; fi
    ok
}
# no messages from fug on stderr
quiet() {
    if [ -s "$1.err" ]; then bad "$1: unexpected messages:"; sed 's/^/    /' "$1.err" | head -5; else ok; fi
}
exists() {
    for f in "$@"; do
        if [ -s "$f" ]; then ok; else bad "missing file $f"; fi
    done
}
# statistics block of the first series of a .out file
stats() { grep -E "Mean:|Variance:|Skewness:|Kurtosis:" "$1" | head -4; }

echo "== plots of every kind"
for s in IPCM IPCQ IPCA PU; do
    for opt in -a -b "-b -l 40" -c "-c -l 40" -d "-e -m 6"; do
        run "$s$opt" 0 $s $opt && quiet "$s$opt"
    done
done
exists d1D1lnIPCM.eps acf_d1D1lnIPCM.eps hist_d1D1lnIPCM.eps m_dt_d0lnIPCM.eps
exists d1lnIPCQ.eps acf_d1lnIPCQ.eps hist_d1lnIPCQ.eps m_dt_d0lnIPCQ.eps
exists d1lnIPCA.eps d0lnPU.eps
# annual data: vertical lines and labels every 5 years (2002-2019)
years=$(grep -ao '(20[0-9][0-9])' d1lnIPCA.eps | tr -d '()' | tr '\n' ' ')
[ "$years" = "2005 2010 2015 " ] && ok || bad "annual year labels: '$years' (expected 2005 2010 2015)"
exists IPCM_fug.pdf IPCQ_fug.pdf IPCA_fug.pdf PU_fug.pdf

echo "== identification set"
run set 0 IPCM set 2 1 -c -h -e -m 12 && quiet set
exists d0IPCM.eps m_dt_d0IPCM.eps m_dt_d0lnIPCM.eps
for d in 0 1 2; do exists d${d}lnIPCM.eps d${d}D1lnIPCM.eps; done
# PDF written directly by fugdraw: 7 -c graphs, 2 per landscape A4 page
exists IPCM_fug.pdf
# 9 figures: the original series and its mean-std. graph, 3 x D0, the mean-
# std. graph of ln, 3 x D1 -> 5 landscape A4 pages (compressed streams)
n=$(grep -ac "/Type /Page " IPCM_fug.pdf)
[ "$n" = 5 ] && ok || bad "IPCM_fug.pdf has $n pages (expected 5)"
grep -aq "/Filter /FlateDecode" IPCM_fug.pdf && ok || bad "IPCM_fug.pdf is not compressed"
grep -aq "/BaseFont /Helvetica" IPCM_fug.pdf && ok || bad "IPCM_fug.pdf without fonts"
run setq 0 IPCQ set 1 1 -c && quiet setq
run seta 0 IPCA set 1 0 -c && quiet seta
exists d0lnIPCA.eps d1lnIPCA.eps

echo "== formats of the Box-Cox line (3, 4 and 5 values)"
sed 's/^ 0.00 0.00  1  1$/ 0.00 1  1/'      IPCM.inp > F3.inp
sed 's/^ 0.00 0.00  1  1$/ 0.00 0.00 0 1 1/' IPCM.inp > F5.inp
for f in IPCM F3 F5; do run "fmt$f" 0 $f -l 0 -m 12; done
stats IPCM_fug.out > s4; stats F3_fug.out > s3; stats F5_fug.out > s5
cmp -s s4 s3 && ok || bad "3-value format gives different results"
cmp -s s4 s5 && ok || bad "5-value format gives different results"
tr -d '\r' < IPCQ.inp | sed 's/$/\r/' > CRLF.inp
run crlf 0 CRLF -l 0 && stats CRLF_fug.out > sc && stats IPCQ_fug.out > sq
run ipcq 0 IPCQ -l 0 && stats IPCQ_fug.out > sq && cmp -s sq sc && ok || bad "CRLF input gives different results"

echo "== individual factors of the annual difference"
# (the Box-Cox and factor lines of IPCM.inp, whatever their spacing and decimals)
BC='^ *0\.0* *0\.0*  *1  *1 *$'
FA='^ *0 0 0 0 0 0 0 *$'
# (1-B) as factor 0 must equal one regular difference
sed "s/$BC/ 0.00 0.00  0  0/; s/$FA/   1 0 0 0 0 0 0/" IPCM.inp > I0.inp
sed "s/$BC/ 0.00 0.00  1  0/" IPCM.inp > R1.inp
# the seven factors must equal one complete annual difference
sed "s/$BC/ 0.00 0.00  0  0/; s/$FA/   1 1 1 1 1 1 1/" IPCM.inp > IA.inp
sed "s/$BC/ 0.00 0.00  0  1/" IPCM.inp > A1.inp
grep -q '^   1 0 0 0 0 0 0$' I0.inp && grep -q '^ 0.00 0.00  0  1$' A1.inp && ok || bad "IPCM.inp: unexpected Box-Cox or factor line"
for f in I0 R1 IA A1; do run "if$f" 0 $f -l 0 -m 12 -a && quiet "if$f"; done
stats I0_fug.out > si0; stats R1_fug.out > sr1; stats IA_fug.out > sia; stats A1_fug.out > sa1
cmp -s si0 sr1 && ok || bad "factor (1-B) differs from a regular difference"
awk '{print $1, sprintf("%.4f", $NF)}' sia > sia4; awk '{print $1, sprintf("%.4f", $NF)}' sa1 > sa14
cmp -s sia4 sa14 && ok || bad "the seven factors differ from an annual difference"

echo "== the .inp of fue (shared by fug and fue)"
# FULL.inp: IPCM with a model of every kind (deterministic variables, one of
# them non-standard with its own data column, AR, MA, fixed-frequency
# operators and mean); ART.inp: the .inp without model written by ART.
# fug must take the same series: same results as R1 (fug layout, d = 1)
run full 0 FULL -B 0 0 1 0 -l 0 -m 12 -a && quiet full
run art 0 ART -B 0 0 1 0 -l 0 -m 12 -a && quiet art
cp IPCM.inp BOPT.inp
run bopt 0 BOPT -B 0 0 1 0 -l 0 -m 12 -a && quiet bopt
for f in FULL ART BOPT; do
    stats ${f}_fug.out > s$f
    cmp -s s$f sr1 && ok || bad "$f.inp: different series than R1.inp"
done
grep -q "Series Name *: IPCM" FULL_fug.out && ok || bad "FULL.inp: series name"
# fug does not write the .out and .pdf of fue
[ ! -e FULL.out ] && [ ! -e FULL.pdf ] && ok || bad "fug wrote FULL.out or FULL.pdf"
# the scale of the acf/pacf of the fue file (1.0) is used when there is no -f
run fullb 0 FULL -B 0 0 1 0 -b && quiet fullb
grep -aq "(-1) Tj" acf_d1lnFULL.eps && ok || bad "FULL.inp: acf/pacf scale of the file not used"
run fullf 0 FULL -B 0 0 1 0 -b -f 0.6 && grep -aq "(-0.6) Tj" acf_d1lnFULL.eps && ok || bad "-f does not win over the file"
# without -B, the Box-Cox line of the file (FULL: lambda 0, d 1, D 1)
run fulld 0 FULL -a && exists d1D1lnFULL.eps
# a damaged fue file is reported
head -40 FULL.inp > CUT.inp
run cut 1 CUT -a && grep -q "Error reading input file CUT.inp" cut.err && ok || bad "damaged fue file"
run bopt4 1 BOPT -B 0 0 1 && grep -q "requires four values" bopt4.err && ok || bad "-B with three values"

echo "== without a display, with spaces and extension in the path"
mkdir -p "dir with space"
cp IPCQ.inp "dir with space/"
env -u DISPLAY -u WAYLAND_DISPLAY "$FUG" "dir with space/IPCQ.inp" -ce > nodisp.log 2> nodisp.err
[ $? = 0 ] && ok || bad "run without DISPLAY / path with spaces"
quiet nodisp
exists "dir with space/d1lnIPCQ.eps" "dir with space/m_dt_d0lnIPCQ.eps" "dir with space/IPCQ_fug.out"

echo "== no external programs (empty PATH)"
mkdir -p nopath && cp IPCQ.inp nopath/
env PATH=/nonexistent "$FUG" nopath/IPCQ set 1 1 -c -h -e -a -b -d > nopath.log 2> nopath.err
[ $? = 0 ] && ok || bad "fug needs something from the PATH"
quiet nopath
exists nopath/IPCQ_fug.pdf nopath/d1D1lnIPCQ.eps nopath/acf_d1D1lnIPCQ.eps nopath/hist_d1D1lnIPCQ.eps \
       nopath/m_dt_d0lnIPCQ.eps nopath/m_dt_d0IPCQ.eps

echo "== Box-Cox lambda estimation"
run lam 0 IPCM -x -2 2 0.1 && quiet lam
grep -q "Estimated Box-Cox lambda" lam.log && ok || bad "-x does not report the lambda"
grep -q "OPTIMAL LAMBDA" IPCM_fug.out && ok || bad "-x does not write the search to the .out file"

echo "== errors are reported"
sed '15s/.*/-1.0/' IPCQ.inp > NEG.inp
run neg 1 NEG -a && grep -q "requires data + m > 0" neg.err && ok || bad "negative data with lambda 0"
sed 's/^ 0.00 0.00  1  0$/ 1.00 0.00  1  0/' NEG.inp > NEG1.inp
run neg1 0 NEG1 -a && quiet neg1
run few 1 IPCQ set 2 30 -c
run unknown 1 IPCQ -z
run missing 1 NOFILE -a
run badnum 1 IPCQ -l abc
sed 's/^ 0.00 0.00  1  0$/ 0.00 0.00/' IPCQ.inp > BAD.inp
run badinp 1 BAD -a

echo "== constant series (warning, no crash)"
awk 'NR<=14 {print} NR>14 {print "5.0"}' IPCQ.inp | sed 's/^ 0.00 0.00  1  0$/ 1.00 0.00  0  0/' > CONST.inp
run const 0 CONST -c -d -e
grep -q "constant" const.err && ok || bad "no warning for a constant series"

echo
echo "$PASS checks passed, $FAIL failed"
[ $FAIL = 0 ]
