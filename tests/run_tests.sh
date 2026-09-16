#!/bin/sh
# Regression tests of FUF (make check).
#
#   sh tests/run_tests.sh [path/to/fuf]            run the tests
#   sh tests/run_tests.sh --update [path/to/fuf]   rewrite tests/golden/ and the
#                                                  statuses of tests/runs.tsv
#
# tests/runs.tsv lists the runs: an input file of tests/corpus/, the arguments
# of fuf (- for none) and the exit status expected. A run that ends with
# status 0 must write the same files as tests/golden/<id>/, byte for byte:
# the .out, the LaTeX files and the EPS graph. The PDF is not compared (what
# determines it is compared), and neither is anything pdflatex leaves behind.
#
# gnuplot and pdflatex are replaced by programs that do nothing: what is
# compared does not depend on them, and the tests run in seconds.

UPDATE=0
if [ "$1" = "--update" ]; then UPDATE=1; shift; fi
FUF=${1:-bin/fuf}
case "$FUF" in /*) ;; *) FUF="$(pwd)/$FUF" ;; esac
TOP=$(cd "$(dirname "$0")/.." && pwd)
TESTS="$TOP/tests"
WORK="$TESTS/work"
TIMEOUT=${TIMEOUT:-120}

[ -x "$FUF" ] || { echo "fuf not found: $FUF (run make first)"; exit 1; }

rm -rf "$WORK"
mkdir -p "$WORK/bin" || exit 1
printf '#!/bin/sh\ncat > /dev/null\n' > "$WORK/bin/gnuplot"
printf '#!/bin/sh\nexit 0\n' > "$WORK/bin/pdflatex"
chmod +x "$WORK/bin/gnuplot" "$WORK/bin/pdflatex"
ulimit -c 0 2>/dev/null

# the files of a run that are compared: everything but the input, the PDF and
# what the tests themselves write
results() {
    ( cd "$1" && ls | grep -v -e '\.inp$' -e '\.pdf$' -e '^console\.txt$' -e '^status$' )
}

NEWRUNS="$WORK/runs.tsv"
grep '^#' "$TESTS/runs.tsv" > "$NEWRUNS"

grep -v '^#' "$TESTS/runs.tsv" | while IFS='	' read -r id input args status; do
    [ -n "$id" ] || continue
    [ "$args" = "-" ] && args=""
    dir="$WORK/$id"
    mkdir -p "$dir"
    cp "$TESTS/corpus/$input.inp" "$dir/"
    # (the status goes through a file so that a crash is not reported here)
    ( cd "$dir" && PATH="$WORK/bin:$PATH" timeout "$TIMEOUT" "$FUF" "$input" $args \
          > console.txt 2>&1; echo $? > "$dir/status" ) 2>/dev/null
    rc=$(cat "$dir/status")

    if [ $UPDATE = 1 ]; then
        printf '%s\t%s\t%s\t%s\n' "$id" "$input" "${args:--}" "$rc" >> "$NEWRUNS"
        rm -rf "$TESTS/golden/$id"
        if [ $rc = 0 ]; then
            mkdir -p "$TESTS/golden/$id"
            for f in $(results "$dir"); do cp "$dir/$f" "$TESTS/golden/$id/"; done
        fi
        continue
    fi

    if [ "$rc" != "$status" ]; then
        echo "FAIL: $id: exit status $rc (expected $status)"
        echo "x" >> "$WORK/failed"
        continue
    fi
    if [ $rc = 0 ]; then
        for f in $(results "$dir"); do
            if [ ! -f "$TESTS/golden/$id/$f" ]; then
                echo "FAIL: $id: $f is new (not in tests/golden/$id)"
                echo "x" >> "$WORK/failed"
            elif ! cmp -s "$TESTS/golden/$id/$f" "$dir/$f"; then
                echo "FAIL: $id: $f differs from tests/golden/$id/$f"
                echo "x" >> "$WORK/failed"
            fi
        done
        for f in $( cd "$TESTS/golden/$id" && ls ); do
            if [ ! -f "$dir/$f" ]; then
                echo "FAIL: $id: $f was not written"
                echo "x" >> "$WORK/failed"
            fi
        done
    fi
    echo "x" >> "$WORK/runs"
done

if [ $UPDATE = 1 ]; then
    cp "$NEWRUNS" "$TESTS/runs.tsv"
    echo "tests/golden and tests/runs.tsv updated with $FUF"
    exit 0
fi

runs=0; failed=0
[ -f "$WORK/runs" ] && runs=$(wc -l < "$WORK/runs")
[ -f "$WORK/failed" ] && failed=$(wc -l < "$WORK/failed")
total=$(grep -vc '^#' "$TESTS/runs.tsv")
echo
echo "$total runs, $failed failures"
[ "$failed" = 0 ] && [ "$runs" -gt 0 ]
