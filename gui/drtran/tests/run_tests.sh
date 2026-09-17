#!/bin/sh
# El banco de mtram. Contrasta lo que el GUI dice con lo que dice drtran sobre
# los mismos ficheros, no con lo que nos parezca.
TOP=$(cd "$(dirname "$0")/.." && pwd)
L="$TOP/../../lib"; E="$TOP/../../engines/drtran"
W="$TOP/tests/work"; mkdir -p "$W"
CC=${CC:-cc}
GTK=$(pkg-config --cflags --libs gtk+-3.0 2>/dev/null) || { echo "hace falta GTK3"; exit 0; }

$CC -O0 -g -w -I"$TOP/include" -I"$E/include" -I"$L/dates" \
    $(pkg-config --cflags gtk+-3.0) \
    "$TOP/tests/test_series.c" "$TOP/src/series.c" \
    "$E/src/fue_pre_reader.c" "$E/src/nlatools.c" "$L/dates/dates.c" \
    -o "$W/test_series" $(pkg-config --libs gtk+-3.0) -lgsl -lgslcblas -lm || exit 1

M6=${M6:-$TOP/../../engines/drtran/tests/data/m6}
if [ -d "$M6" ]; then
    "$W/test_series" "$M6"
else
    echo "no encuentro los .pre del m6 en $M6 (pon M6=...)"
fi
