#!/bin/sh
# acuerdo.sh -- ¿coinciden los dos LECTORES en que es legible?
#
#     sh acuerdo.sh [corpus]
#
# La bateria mide a los ESCRITORES: si lo que sale dice lo mismo que lo que
# entro. Esto mide otra cosa, y es una promesa distinta: que las dos puertas
# del formato --inpcheck.c en C, fue.load() en Python-- estén de acuerdo en
# QUE FICHEROS SON LEGIBLES Y DE QUE DIALECTO.
#
# No es una sutileza. Un fichero que una acepta y la otra no es un fichero que
# funciona en media suite. Y lo peor no es el que una rechaza: es el que las
# dos aceptan LEYENDO COSAS DISTINTAS -- un .inp de fuf que el C manda a fuf y
# Python lee como si fuera de fue, quedandose el horizonte en un atributo que
# ningun escritor devuelve.
#
# Cuatro veredictos por fichero:
#
#     ok        las dos dicen lo mismo
#     C-no      el C lo rechaza y Python lo acepta
#     py-no     Python lo rechaza y el C lo acepta
#     dialecto  las dos lo aceptan, pero como dialectos distintos
#     cabecera  las dos lo aceptan y del mismo dialecto, pero LEEN COSAS
#               DISTINTAS en la linea de fecha
#
# La ultima es la mas dificil de ver, y la razon de que esto exista aparte de
# la bateria: cuando el escritor es FIEL, un fichero mal leido se reescribe
# igual de mal y la bateria dice "iguales" -- esta comparando la lectura
# equivocada consigo misma. Aqui se comparan las DOS lecturas.

TOP=$(cd "$(dirname "$0")" && pwd)
CORPUS=${1:-$TOP/corpus}
ACUERDO="$TOP/acuerdo"

[ -x "$ACUERDO" ] || { echo "falta $ACUERDO (compilar.sh)"; exit 1; }

RT="$TOP/gtkfue_roundtrip"

ok=0; cno=0; pyno=0; dial=0; cab=0
: > "$TOP/acuerdo.log"

for f in "$CORPUS"/*.inp "$CORPUS"/*.pre; do
    [ -f "$f" ] || continue
    b=$(basename "$f")

    enc=$("$ACUERDO" "$f" 2>/dev/null)
    dice_c=${enc%% *}

    # que dice Python, y de que dialecto lo toma
    dice_py=$(python3 - "$f" 2>/dev/null <<'EOF'
import sys, warnings
warnings.filterwarnings("ignore")
import fue
try:
    ts, m = fue.load(sys.argv[1])
except Exception:
    print("no")
else:
    print("fuf" if hasattr(m, "_fuf_horizon") else "fue")
EOF
)
    [ -z "$dice_py" ] && dice_py=no

    if [ "$dice_c" = "$dice_py" ]; then
        # De acuerdo en el dialecto. ¿Y en lo que dice la linea de fecha?
        if [ "$dice_c" = fue ] && [ -x "$RT" ]; then
            cab_c=$(timeout 20 "$RT" --cabecera "$f" 2>/dev/null)
            cab_py=$(python3 - "$f" 2>/dev/null <<'EOF'
import sys, warnings
warnings.filterwarnings("ignore")
import fue
ts, m = fue.load(sys.argv[1])
a = ts.start if hasattr(ts.start, "__iter__") else (ts.start, 1)
print("nobs=%d freq=%d year=%d period=%d name=%s"
      % (ts.nobs, ts.freq, a[0], a[1] if ts.freq > 1 else 1, ts.name or ""))
EOF
)
            if [ -n "$cab_c" ] && [ -n "$cab_py" ] && [ "$cab_c" != "$cab_py" ]; then
                cab=$((cab + 1))
                echo "cabecera  $b  los dos lo aceptan y leen cosas distintas" >> "$TOP/acuerdo.log"
                echo "            C:      $cab_c" >> "$TOP/acuerdo.log"
                echo "            Python: $cab_py" >> "$TOP/acuerdo.log"
                continue
            fi
        fi
        ok=$((ok + 1))
        continue
    fi
    if [ "$dice_c" = no ]; then
        cno=$((cno + 1))
        echo "C-no      $b  el C lo rechaza, Python lo lee como $dice_py" >> "$TOP/acuerdo.log"
        echo "          ${enc#no }" >> "$TOP/acuerdo.log"
    elif [ "$dice_py" = no ]; then
        pyno=$((pyno + 1))
        echo "py-no     $b  Python lo rechaza, el C lo toma por $dice_c" >> "$TOP/acuerdo.log"
    else
        dial=$((dial + 1))
        echo "dialecto  $b  el C dice $dice_c y Python dice $dice_py" >> "$TOP/acuerdo.log"
    fi
done

echo
echo "$ok de acuerdo; $cno los rechaza solo el C, $pyno solo Python,"
echo "$dial de dialecto distinto, $cab leidos distinto"
echo "(el detalle, en acuerdo.log)"
[ $((cno + pyno + dial + cab)) = 0 ]
