#!/bin/bash
#  golden.sh — the whole output of a set of runs, hashed, so that a change that
#  is supposed to move NOTHING can be shown to move nothing.
#
#  WHY.  P8 is a refactor: it must not change a single number this program
#  prints.  The regression suite checks invariants and a handful of logL values,
#  which is the right net for a change that is MEANT to move something.  It is
#  not the right net for a change that is meant to move nothing: there the
#  claim is stronger, so the check has to be stronger too -- every byte of every
#  report, over the configurations that exercise each mode.
#
#  The version banner is stripped before hashing: it is the one line that is
#  allowed to move without the numbers moving.
#
#      tools/golden.sh capture [dir]   run and store the hashes
#      tools/golden.sh verify  [dir]   run again and compare, byte for byte
#
#  Default dir: tests/golden.  Storing only the hashes keeps the repository
#  small; on a mismatch the run is left on disk so it can be diffed.
set -u
cd "$(dirname "$0")/.." || exit 1

MODE=${1:-verify}
DIR=${2:-tests/golden}
DRVEC=bin/drvec
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MM=datasets/mauricio/mink_muskrat.inp
R2=datasets/synthetic/rank2.inp
R0=datasets/synthetic/rank0.inp
PRE1=tests/fixtures/mmpre.muskrat.pre
PRE2=tests/fixtures/mmpre.mink.pre

#  One line per case: a label, then the arguments.  They are chosen to touch
#  every mode that prints numbers, not to be exhaustive: a mode that no case
#  reaches is a mode this net does not protect, and that is worth knowing.
CASES=(
  "base2|$MM|2 1 1 -mean -case 2"
  "base3|$MM|2 1 1 -mean -case 3"
  "case1|$MM|2 1 1"
  "q0|$MM|2 0 1 -case 2"
  "mafree|$MM|2 1 1 -case 2 -mafree"
  "matri|$MM|2 1 1 -case 2 -matri"
  "mawarma|$MM|2 1 1 -case 2 -mawarma"
  "warma|$MM|2 1 1 -case 2 -warma"
  "r0|$MM|2 1 0 -case 2"
  "diag|$MM|2 1 0 -case 2 -diagar -diagma -diagcov"
  "fixb2|$MM|2 1 1 -case 2 -fixb2 0"
  "fdhess|$MM|2 1 1 -case 2 -fdhess"
  "weakex|$MM|2 1 1 -case 2 -weakex 2"
  "forecast|$MM|2 1 1 -case 2 -f 5"
  "estwin|$MM|2 1 1 -case 2 -f 3 -estwin 40"
  "lrtest|$MM|2 1 1 -lrtest"
  "rungs|$MM|2 1 1 -rungs"
  "specs|$MM|2 1 1 -case 2 -specs"
  "eval|$MM|2 1 1 -case 2 -eval"
  "multistart|$MM|2 1 1 -case 2 -multistart 4"
  "rank2|$R2|2 0 2 -case 2 -mean"
  "rank0|$R0|2 0 1 -case 2 -mean"
  "m3|$R2|2 1 1 -case 2 -mean"
)

hash_of() {
    #  Drop the version banner and any absolute path; everything else counts.
    #  The version line is the one line allowed to move without the numbers
    #  moving, so it is dropped before hashing -- otherwise every release
    #  invalidates the whole baseline and the net stops meaning anything.
    sed -e '/^DRVEC .* EML Estimation/d' -e '/^Program  *: DRVEC/d' \
        -e 's#/tmp/[^ ]*##g' "$1" \
        | cksum | awk '{print $1"-"$2}'
}

mkdir -p "$DIR"
out="$DIR/hashes.txt"
new="$TMP/hashes.txt"
: > "$new"

for c in "${CASES[@]}"; do
    label=${c%%|*}; rest=${c#*|}; src=${rest%%|*}; args=${rest#*|}
    cp "$src" "$TMP/case.inp"
    rm -f "$TMP/case.out"
    timeout 120 "$DRVEC" "$TMP/case" $args >/dev/null 2>&1
    if [ -f "$TMP/case.out" ]; then
        printf '%-12s %s\n' "$label" "$(hash_of "$TMP/case.out")" >> "$new"
        cp "$TMP/case.out" "$TMP/$label.out"
    else
        printf '%-12s %s\n' "$label" "NO-OUTPUT" >> "$new"
    fi
done

#  And the .pre route, which has its own entry point.
rm -f "$TMP/proute.out"
timeout 120 "$DRVEC" "$PRE1" "$PRE2" 2 1 1 -mean -case 2 -name "$TMP/proute" \
    >/dev/null 2>&1
if [ -f "$TMP/proute.out" ]; then
    printf '%-12s %s\n' "preroute" "$(hash_of "$TMP/proute.out")" >> "$new"
else
    printf '%-12s %s\n' "preroute" "NO-OUTPUT" >> "$new"
fi

if [ "$MODE" = "capture" ]; then
    cp "$new" "$out"
    echo "golden: captured $(wc -l < "$out") cases into $out"
    exit 0
fi

if [ ! -f "$out" ]; then
    echo "golden: no baseline in $out; run 'tools/golden.sh capture' first"
    exit 1
fi

if diff -u "$out" "$new" > "$TMP/diff.txt"; then
    echo "golden: all $(wc -l < "$out") cases byte-identical to the baseline"
    exit 0
fi

keep=$(mktemp -d /tmp/drvec-golden-XXXXXX)
cp "$TMP"/*.out "$keep" 2>/dev/null
echo "golden: MISMATCH"
sed -n '3,$p' "$TMP/diff.txt"
echo "the runs are in $keep"
exit 1
