#!/bin/sh
# Regression tests of FUE (make check).
#
#   sh tests/run_tests.sh [path/to/fue]            run the tests
#   sh tests/run_tests.sh --update [path/to/fue]   rewrite tests/golden/ and the
#                                                  statuses of tests/runs.tsv
#
# tests/runs.tsv lists the runs: an input file of tests/corpus/, the arguments
# of fue (- for none), the format of the file (fue, or fuf/fug: files of the other programs)
# and the exit status expected. A run that ends with status 0 must write the
# same .out, .pre and LaTeX files as tests/golden/<id>/ (the results of FUE
# 1.13.1, the base line). Files of fuf and fug are not compared: fue does not
# read them, and what it writes with them has no meaning.
#
# gnuplot and pdflatex are replaced by programs that do nothing: fue 1.14
# uses neither (it draws its graph and its report itself), but fue 1.13.1 can
# then run the same tests, and -latex does not compile anything here.

UPDATE=0
if [ "$1" = "--update" ]; then UPDATE=1; shift; fi
FUE=${1:-bin/fue}
case "$FUE" in /*) ;; *) FUE="$(pwd)/$FUE" ;; esac
TOP=$(cd "$(dirname "$0")/.." && pwd)
TESTS="$TOP/tests"
WORK="$TESTS/work"
# Byte a byte en Linux; fuera, cifras con tolerancia y los casos fragiles
# apuntados (ver conformidad/referencia.sh).
REFERENCIA_SH="$TOP/../../conformidad/referencia.sh"
. "$REFERENCIA_SH"
FRAGILES="$TESTS/fragiles.txt"
TIMEOUT=${TIMEOUT:-120}

[ -x "$FUE" ] || { echo "fue not found: $FUE (run make first)"; exit 1; }

rm -rf "$WORK"
mkdir -p "$WORK/bin" || exit 1
printf '#!/bin/sh\ncat > /dev/null\n' > "$WORK/bin/gnuplot"
printf '#!/bin/sh\nexit 0\n' > "$WORK/bin/pdflatex"
chmod +x "$WORK/bin/gnuplot" "$WORK/bin/pdflatex"
ulimit -c 0 2>/dev/null

# the files a run writes and the golden copies (LaTeX: .tex, _res.tex, _dist.tex;
# with -f, the input file of fuf forecast_<input>.inp instead of the .pre; the
# graph of the residuals A<input>.eps, drawn by fugdraw since fue 1.14)
results() { echo "$1.out $1.pre forecast_$1.inp $1.tex $1_res.tex $1_dist.tex A$1.eps"; }

NEWRUNS="$WORK/runs.tsv"
grep '^#' "$TESTS/runs.tsv" > "$NEWRUNS"

grep -v '^#' "$TESTS/runs.tsv" | while IFS='	' read -r id input args format status; do
    [ -n "$id" ] || continue
    [ "$args" = "-" ] && args=""
    dir="$WORK/$id"
    mkdir -p "$dir"
    cp "$TESTS/corpus/$input.inp" "$dir/"
    # (the status goes through a file so that a crash is not reported here)
    ( cd "$dir" && PATH="$WORK/bin:$PATH" timeout "$TIMEOUT" "$FUE" "$input" $args \
          > console.txt 2>&1; echo $? > "$dir/status" ) 2>/dev/null
    rc=$(cat "$dir/status")

    if [ $UPDATE = 1 ]; then
        printf '%s\t%s\t%s\t%s\t%s\n' "$id" "$input" "${args:--}" "$format" "$rc" >> "$NEWRUNS"
        rm -rf "$TESTS/golden/$id"
        if [ $rc = 0 ] && [ "$format" = fue ]; then
            mkdir -p "$TESTS/golden/$id"
            for f in $(results "$input"); do
                [ -f "$dir/$f" ] && cp "$dir/$f" "$TESTS/golden/$id/"
            done
        fi
        continue
    fi

    if [ "$rc" != "$status" ]; then
        echo "FAIL: $id: exit status $rc (expected $status)"
        echo "x" >> "$WORK/failed"
        continue
    fi
    if [ $rc = 0 ] && [ "$format" = fue ]; then
        for f in $(results "$input"); do
            if [ -f "$TESTS/golden/$id/$f" ]; then
                referencia "$TESTS/golden/$id/$f" "$dir/$f" "$id" "$FRAGILES"
                case $? in
                    0) ;;
                    2) echo "FRAGIL: $id: $f differs from tests/golden/$id/$f (see tests/fragiles.txt)" ;;
                    *) echo "FAIL: $id: $f differs from tests/golden/$id/$f"
                       echo "x" >> "$WORK/failed" ;;
                esac
            elif [ -f "$dir/$f" ]; then
                echo "FAIL: $id: $f is new (not in tests/golden/$id)"
                echo "x" >> "$WORK/failed"
            fi
        done
    fi
    echo "x" >> "$WORK/runs"
done

# A model without AR or MA operators is the same model as with one AR(1)
# factor fixed at 0 (the way .inp files avoided the crash of fue <= 1.13.1):
# for every such run, the variant must give the same .out (but for the lines
# of that factor) and the same LaTeX files.

# exit status 0 if the .inp declares its six sections of operators, all empty
# (el sub del \r: un corpus escrito en Windows trae \r\n, y para awk una
#  linea con solo un \r no esta en blanco)
no_arma() {
    awk '{ sub(/\r$/, "") }
         want && NF { if ($1 != 0) bad = 1; want = 0; next }
         /^\*\*/ && tolower($0) ~ /operators/ { n++; want = 1 }
         END { exit (bad || n != 6) }' "$1"
}
# the .inp with one regular AR(1) factor fixed at 0
ar0_variant() {
    awk '{ sub(/\r$/, "") }
         rep && NF { print "1 1"; print "**"; print "0.000000  0"; rep = 0; next }
         { print }
         /^\*\*/ && tolower($0) ~ /regular ar operators/ { rep = 1 }' "$1" > "$2"
}
# the .out without the lines of that factor
no_ar0_lines() {
    awk '/^Coefficients for regular AR factor 1:$/ { skip = 1; next }
         skip { skip = 0; next }
         /^ *phi\[ *1\] *= *0\.0+$/ { next }
         { print }' "$1"
}

if [ $UPDATE = 0 ]; then
    grep -v '^#' "$TESTS/runs.tsv" | while IFS='	' read -r id input args format status; do
        [ "$format" = fue ] && [ "$status" = 0 ] || continue
        no_arma "$TESTS/corpus/$input.inp" || continue
        [ "$args" = "-" ] && args=""
        dir="$WORK/$id.ar0"
        mkdir -p "$dir"
        ar0_variant "$TESTS/corpus/$input.inp" "$dir/$input.inp"
        ( cd "$dir" && PATH="$WORK/bin:$PATH" timeout "$TIMEOUT" "$FUE" "$input" $args \
              > console.txt 2>&1; echo $? > "$dir/status" ) 2>/dev/null
        rc=$(cat "$dir/status")
        same=1
        if [ "$rc" != 0 ]; then
            same=0
        else
            no_ar0_lines "$WORK/$id/$input.out" > "$dir/a.out"
            no_ar0_lines "$dir/$input.out" > "$dir/b.out"
            cmp -s "$dir/a.out" "$dir/b.out" || same=0
            for f in "$input.tex" "${input}_res.tex" "${input}_dist.tex"; do
                [ -f "$WORK/$id/$f" ] && ! cmp -s "$WORK/$id/$f" "$dir/$f" && same=0
            done
        fi
        if [ $same = 1 ]; then
            echo "x" >> "$WORK/equivalent"
        else
            echo "FAIL: $id: differs from the same model with an AR(1) factor fixed at 0 ($dir)"
            echo "x" >> "$WORK/failed"
        fi
    done
fi

# fue needs no other program: with an empty PATH it must write its report
if [ $UPDATE = 0 ]; then
    mkdir -p "$WORK/nopath"
    cp "$TESTS/corpus/DE.2.inp" "$WORK/nopath/"
    # En Windows, sin PATH tampoco se encuentran las DLL con que se enlazo fue
    # (GSL, de MSYS2): se deja solo esa carpeta. Lo que se prueba es que no
    # hace falta OTRO PROGRAMA, no que no hagan falta las DLL (ver fuf).
    NOPATH=/nonexistent
    case "$(uname -s)" in
        MINGW*|MSYS*) NOPATH=$(dirname "$(command -v gcc 2>/dev/null || echo /ucrt64/bin/gcc)") ;;
    esac
    ( cd "$WORK/nopath" && env PATH="$NOPATH" "$FUE" DE.2 > console.txt 2>&1
      echo $? > status ) 2>/dev/null
    if [ "$(cat "$WORK/nopath/status")" = 0 ] && [ -s "$WORK/nopath/DE.2.pdf" ] &&
       head -c 8 "$WORK/nopath/DE.2.pdf" | grep -q "PDF-1.4"; then
        echo "x" >> "$WORK/errors"
    else
        echo "FAIL: with an empty PATH fue does not write its report in PDF"
        echo "x" >> "$WORK/failed"
    fi
fi

# A missing input file: exit status 1
if [ $UPDATE = 0 ]; then
    mkdir -p "$WORK/nofile"
    ( cd "$WORK/nofile" && "$FUE" NOFILE > console.txt 2>&1; echo $? > status ) 2>/dev/null
    if [ "$(cat "$WORK/nofile/status")" = 1 ] &&
       grep -q "Error opening input file: NOFILE.inp" "$WORK/nofile/console.txt"; then
        echo "x" >> "$WORK/errors"
    else
        echo "FAIL: a missing input file is not reported with exit status 1"
        echo "x" >> "$WORK/failed"
    fi
fi

# Errors: tests/errors.tsv lists damaged .inp files (tests/corpus/bad_*) and
# models that can not be estimated. Each run must end with its exit status
# and say what is wrong; an invalid .inp (status 2) must not write anything.

if [ $UPDATE = 0 ]; then
    grep -v '^#' "$TESTS/errors.tsv" | while IFS='	' read -r id status message; do
        [ -n "$id" ] || continue
        dir="$WORK/bad_$id"
        mkdir -p "$dir"
        cp "$TESTS/corpus/bad_$id.inp" "$dir/"
        ( cd "$dir" && PATH="$WORK/bin:$PATH" timeout "$TIMEOUT" "$FUE" "bad_$id" \
              > console.txt 2>&1; echo $? > "$dir/status" ) 2>/dev/null
        rc=$(cat "$dir/status")
        if [ "$rc" != "$status" ]; then
            echo "FAIL: bad_$id: exit status $rc (expected $status)"
        elif ! grep -qF "$message" "$dir/console.txt"; then
            echo "FAIL: bad_$id: the message '$message' is missing"
        elif [ "$status" = 2 ] && [ -e "$dir/bad_$id.out" ]; then
            echo "FAIL: bad_$id: invalid input, but bad_$id.out was written"
        elif [ "$status" = 3 ] && [ ! -s "$dir/bad_$id.out" ]; then
            echo "FAIL: bad_$id: bad_$id.out was not written"
        else
            echo "x" >> "$WORK/errors"
            continue
        fi
        echo "x" >> "$WORK/failed"
    done
fi

if [ $UPDATE = 1 ]; then
    cp "$NEWRUNS" "$TESTS/runs.tsv"
    echo "tests/golden and tests/runs.tsv updated with $FUE"
    exit 0
fi

runs=0; failed=0
[ -f "$WORK/runs" ] && runs=$(wc -l < "$WORK/runs")
[ -f "$WORK/failed" ] && failed=$(wc -l < "$WORK/failed")
total=$(grep -vc '^#' "$TESTS/runs.tsv")
equivalent=0
[ -f "$WORK/equivalent" ] && equivalent=$(wc -l < "$WORK/equivalent")
echo
errors=0
[ -f "$WORK/errors" ] && errors=$(wc -l < "$WORK/errors")
echo "$total runs ($equivalent models without ARMA checked against an AR(1) fixed at 0),"
echo "$errors damaged inputs and failed estimations reported, $failed failures"
[ "$failed" = 0 ] && [ "$runs" -gt 0 ]
