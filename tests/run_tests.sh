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
# gnuplot and pdflatex are replaced by programs that do nothing: the results
# compared do not depend on them, and the tests run in seconds.

UPDATE=0
if [ "$1" = "--update" ]; then UPDATE=1; shift; fi
FUE=${1:-bin/fue}
case "$FUE" in /*) ;; *) FUE="$(pwd)/$FUE" ;; esac
TOP=$(cd "$(dirname "$0")/.." && pwd)
TESTS="$TOP/tests"
WORK="$TESTS/work"
TIMEOUT=${TIMEOUT:-120}

[ -x "$FUE" ] || { echo "fue not found: $FUE (run make first)"; exit 1; }

rm -rf "$WORK"
mkdir -p "$WORK/bin" || exit 1
printf '#!/bin/sh\ncat > /dev/null\n' > "$WORK/bin/gnuplot"
printf '#!/bin/sh\nexit 0\n' > "$WORK/bin/pdflatex"
chmod +x "$WORK/bin/gnuplot" "$WORK/bin/pdflatex"
ulimit -c 0 2>/dev/null

# the files a run writes and the golden copies (LaTeX: .tex, _res.tex, _dist.tex)
results() { echo "$1.out $1.pre $1.tex $1_res.tex $1_dist.tex"; }

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
                if ! cmp -s "$TESTS/golden/$id/$f" "$dir/$f"; then
                    echo "FAIL: $id: $f differs from tests/golden/$id/$f"
                    echo "x" >> "$WORK/failed"
                fi
            elif [ -f "$dir/$f" ]; then
                echo "FAIL: $id: $f is new (not in tests/golden/$id)"
                echo "x" >> "$WORK/failed"
            fi
        done
    fi
    echo "x" >> "$WORK/runs"
done

if [ $UPDATE = 1 ]; then
    cp "$NEWRUNS" "$TESTS/runs.tsv"
    echo "tests/golden and tests/runs.tsv updated with $FUE"
    exit 0
fi

runs=0; failed=0
[ -f "$WORK/runs" ] && runs=$(wc -l < "$WORK/runs")
[ -f "$WORK/failed" ] && failed=$(wc -l < "$WORK/failed")
total=$(grep -vc '^#' "$TESTS/runs.tsv")
echo
echo "$total runs, $failed failures"
[ "$failed" = 0 ] && [ "$runs" -gt 0 ]
