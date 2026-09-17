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

# --- la CCF preblanqueada, contra la que imprime el propio motor -------------
# El oraculo es  drtran -p ES_CPI_airline.pre WTI_ar1.pre ; los numeros que se
# exigen aqui estan copiados de su .out.
echo
$CC -O2 -w -I"$TOP/include" -I"$E/include" -I"$L/prewhiten" -I"$L/dates" \
    "$TOP/tests/test_prewhiten.c" "$E/src/fue_pre_reader.c" \
    "$E/src/nlatools.c" "$E/src/diagnose.c" \
    "$L/prewhiten/prewhiten.c" "$L/dates/dates.c" \
    -o "$W/test_prewhiten" -lgsl -lgslcblas -lm || exit 1

C="$E/tests/cases"
if [ -f "$C/ES_CPI_airline.pre" ] && [ -f "$C/WTI_ar1.pre" ]; then
    "$W/test_prewhiten" "$C/ES_CPI_airline.pre" "$C/WTI_ar1.pre"
else
    echo "no encuentro los .pre de la prueba en $C"
fi

# --- el .dag: la red -------------------------------------------------------
# Que el GUI y el motor LEAN igual lo garantiza compartir lib/netfile. Que lo
# que el GUI ESCRIBE lo lea el motor hay que preguntarselo al motor.
echo
$CC -O2 -Wall -Wextra -I"$L/netfile" \
    "$L/netfile/test_netfile.c" "$L/netfile/netfile.c" \
    -o "$W/test_netfile" || exit 1

DAG="$E/tests/data/m6/m6_net.dag"
if [ -f "$DAG" ]; then
    "$W/test_netfile" "$DAG" "$W/reescrita.dag" || exit 1

    M6D="$E/tests/data/m6"
    if [ -x "$E/bin/drtran" ] && [ -f "$M6D/M6_EP.pre" ]; then
        for f in "$DAG" "$W/reescrita.dag"; do
            ( cd "$W" && "$E/bin/drtran" "$M6D/M6_EP.pre" "$M6D/M6_EI.pre" \
                 "$M6D/M6_EC.pre" "$M6D/M6_EU.pre" -n "$f" \
                 -o "$W/$(basename $f).out" >/dev/null 2>&1 )
        done
        a=$(sed -n '/Transfer network/,/^$/p' "$W/$(basename $DAG).out" 2>/dev/null)
        b=$(sed -n '/Transfer network/,/^$/p' "$W/reescrita.dag.out" 2>/dev/null)
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            echo "  el motor lee la red reescrita y da la MISMA               ok"
        else
            echo "  el motor lee la red reescrita y da la MISMA               FALLA"
            echo "--- original ---"; echo "$a"
            echo "--- reescrita ---"; echo "$b"
            exit 1
        fi
    else
        echo "  (sin bin/drtran o sin los .pre del m6: no compruebo el motor)"
    fi
fi

# --- el .cns: la tabla de slots --------------------------------------------
# El oraculo es el recuento que imprime el motor:
#   Structural parameters: 67   (free: 52, fixed/shared: 15)
echo
$CC -O2 -w -I"$L/slots" -I"$L/netfile" -I"$L/dates" -I"$E/include" \
    "$L/slots/test_slots.c" "$L/slots/slots.c" "$L/netfile/netfile.c" \
    "$E/src/fue_pre_reader.c" "$E/src/nlatools.c" "$L/dates/dates.c" \
    -o "$W/test_slots" -lgsl -lgslcblas -lm || exit 1

M6D="$E/tests/data/m6"
if [ -f "$M6D/M6_EP.pre" ]; then
    "$W/test_slots" "$M6D" || exit 1

    # Y lo que mtram ESCRIBE, que lo lea el motor y de el mismo recuento.
    if [ -x "$E/bin/drtran" ]; then
        SER="$M6D/M6_EP.pre $M6D/M6_EI.pre $M6D/M6_EU.pre $M6D/M6_EC.pre \
             $M6D/M6_EA.pre $M6D/M6_P.pre"
        for f in "$M6D/m6_net_full.cns" "$W/reescrito.cns"; do
            [ "$f" = "$W/reescrito.cns" ] && \
                "$W/test_slots" "$M6D" --write "$W/reescrito.cns" >/dev/null 2>&1
            ( cd "$W" && "$E/bin/drtran" $SER -n "$M6D/m6_net.dag" -c "$f" \
                 -o "$W/$(basename $f).out" >/dev/null 2>&1 )
        done
        a=$(grep "Structural parameters" "$W/m6_net_full.cns.out" 2>/dev/null)
        b=$(grep "Structural parameters" "$W/reescrito.cns.out" 2>/dev/null)
        if [ -n "$a" ] && [ "$a" = "$b" ]; then
            echo "  el motor lee el .cns reescrito y cuenta lo MISMO         ok"
            echo "     $a"
        else
            echo "  el motor lee el .cns reescrito y cuenta lo MISMO         FALLA"
            echo "--- original  --- $a"
            echo "--- reescrito --- $b"
            exit 1
        fi
    fi
fi

# --- la estimacion: como acabo el optimizador -------------------------------
# Primero las frases del motor, con sus cinco desenlaces. Y luego de verdad: se
# LANZA drtran sobre el m6 y se lee su salida con el mismo codigo que usa la
# pantalla. Si un dia el motor cambiara como lo dice, esto se entera.
echo
$CC -O2 -Wall -Wextra -I"$L/verdict" \
    "$L/verdict/test_verdict.c" "$L/verdict/verdict.c" \
    -o "$W/test_verdict" -lm || exit 1
"$W/test_verdict" || exit 1

M6D="$E/tests/data/m6"
if [ -x "$E/bin/drtran" ] && [ -f "$M6D/M6_EP.pre" ]; then
    echo
    echo "  y ahora de verdad: se lanza drtran sobre el m6"
    ( cd "$W" && "$E/bin/drtran" \
        "$M6D/M6_EP.pre" "$M6D/M6_EI.pre" "$M6D/M6_EU.pre" \
        "$M6D/M6_EC.pre" "$M6D/M6_EA.pre" "$M6D/M6_P.pre" \
        -n "$M6D/m6_net.dag" -c "$M6D/m6_net_full.cns" \
        -o "$W/est.out" > "$W/est.log" 2>&1 )

    "$W/test_verdict" --file "$W/est.log" | sed 's/^/     /'

    # Lo que el motor dijo, contra lo que lib/verdict leyo.
    motor_it=$(sed -n 's/.*AFTER \([0-9]*\) ITERATIONS.*/\1/p' "$W/est.log" | head -1)
    leido_it=$("$W/test_verdict" --file "$W/est.log" | sed -n 's/^iters: //p')
    leido_ok=$("$W/test_verdict" --file "$W/est.log" | sed -n 's/^ok: //p')

    if [ "$motor_it" = "$leido_it" ] && [ "$leido_ok" = "1" ]; then
        echo "  las iteraciones leidas son las que el motor escribio     ok"
    else
        echo "  las iteraciones leidas son las que el motor escribio     FALLA"
        echo "    motor: $motor_it   leido: $leido_it   ok: $leido_ok"
        exit 1
    fi
fi
