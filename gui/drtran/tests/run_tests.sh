#!/bin/sh
# El banco de drtran_gui. Contrasta lo que el GUI dice con lo que dice drtran sobre
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

    # Y lo que drtran_gui ESCRIBE, que lo lea el motor y de el mismo recuento.
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

# --- la diagnosis: leer el .out que el motor acaba de escribir --------------
# Es un lector de TEXTO FORMATEADO, fragil por naturaleza: el motor no emite
# nada legible por maquina. Por eso se prueba contra el .out que la seccion
# anterior acaba de generar, no contra uno guardado.
echo
$CC -O2 -Wall -Wextra -I"$L/outdiag" \
    "$L/outdiag/test_outdiag.c" "$L/outdiag/outdiag.c" \
    -o "$W/test_outdiag" -lm || exit 1

if [ -f "$W/est.out" ]; then
    "$W/test_outdiag" "$W/est.out" || exit 1
else
    echo "  (sin est.out: no compruebo la diagnosis)"
fi

# --- la prevision y la evaluacion fuera de muestra --------------------------
# Otra vez contra .out DE VERDAD: uno con -f sobre el m6 (la prevision de una
# RED, que es lo que distingue a drtran) y otro con -estwin/-C sobre un par,
# que es la evaluacion recursiva.
echo
$CC -O2 -Wall -Wextra -I"$L/outfcst" \
    "$L/outfcst/test_outfcst.c" "$L/outfcst/outfcst.c" \
    -o "$W/test_outfcst" -lm || exit 1

M6D="$E/tests/data/m6"
C="$E/tests/cases"
if [ -x "$E/bin/drtran" ] && [ -f "$M6D/M6_EP.pre" ]; then
    ( cd "$W" && "$E/bin/drtran" \
        "$M6D/M6_EP.pre" "$M6D/M6_EI.pre" "$M6D/M6_EU.pre" \
        "$M6D/M6_EC.pre" "$M6D/M6_EA.pre" "$M6D/M6_P.pre" \
        -n "$M6D/m6_net.dag" -c "$M6D/m6_net_full.cns" -f 8 \
        -o "$W/fcst.out" >/dev/null 2>&1 )
    ( cd "$W" && "$E/bin/drtran" \
        "$C/ES_CPI_airline.pre" "$C/WTI_ar1.pre" \
        -estwin 180 -f 6 -C "$W/evaluacion.csv" \
        -o "$W/eval.out" >/dev/null 2>&1 )

    "$W/test_outfcst" "$W/fcst.out" "$W/eval.out" || exit 1

    # El CSV por origen: la evaluacion tiene que dejarlo, y con una fila por
    # (origen, horizonte). 31 origenes x 6 horizontes + cabecera.
    if [ -f "$W/evaluacion.csv" ]; then
        n=$(wc -l < "$W/evaluacion.csv")
        if [ "$n" -eq 187 ]; then
            echo "  el CSV por origen: 31 x 6 filas mas la cabecera          ok"
        else
            echo "  el CSV por origen: 31 x 6 filas mas la cabecera          FALLA ($n)"
            exit 1
        fi
    fi
fi

# --- el operador no estacionario en forma canonica --------------------------
# Las columnas d/D/f NO se leen de nrdiff/nadiff: no son canonicos. En el m6
# las SEIS series traen nrdiff=2 y una de ellas es otro operador.
echo
$CC -O2 -Wall -Wextra -I"$L/nsop" \
    "$L/nsop/test_nsop.c" "$L/nsop/nsop.c" -o "$W/test_nsop" -lm || exit 1
"$W/test_nsop" || exit 1

# --- el R² de Brajin --------------------------------------------------------
# La bondad del ajuste que la escuela usa: sobre la serie ESTACIONARIA, con un
# denominador que es propiedad de los DATOS. Que ese denominador no se mueva
# entre las dos corridas es lo unico que hace comparables los dos R².
echo
$CC -O2 -w -I"$TOP/include" -I"$E/include" -I"$L/outdiag" -I"$L/gof" -I"$L/dates" \
    "$TOP/tests/test_gof.c" "$L/gof/gof.c" "$L/outdiag/outdiag.c" \
    "$E/src/fue_pre_reader.c" "$E/src/nlatools.c" "$L/dates/dates.c" \
    -o "$W/test_gof" -lgsl -lgslcblas -lm || exit 1

DR="$E/bin/drtran"
if [ -x "$DR" ] && [ -d "$M6" ]; then
    S="$M6/M6_EP.pre $M6/M6_EI.pre $M6/M6_EU.pre $M6/M6_EC.pre $M6/M6_EA.pre $M6/M6_P.pre"
    $DR $S -n "$M6/m6_net.dag" -c "$M6/m6_net.cns" \
        -e "$W/gof_t.txt" -o "$W/gof_t.out" >/dev/null 2>&1
    $DR $S -0 -e "$W/gof_d.txt" -o "$W/gof_d.out" >/dev/null 2>&1
    "$W/test_gof" "$M6" "$W/gof_t.txt" "$W/gof_d.txt" || exit 1
else
    echo "  sin drtran compilado en $DR, me salto el R²"
fi

# --- las rutas: el prefijo va en el NOMBRE, no delante de la ruta -----------
# Los tres primeros casos son las tres invocaciones que fallaban de verdad,
# reproducidas en INVENTARIO-madre.md §2.
echo
$CC -O2 -Wall -Wextra -I"$L/rutas" \
    "$L/rutas/test_rutas.c" "$L/rutas/rutas.c" -o "$W/test_rutas" || exit 1
"$W/test_rutas" || exit 1

# --- la tabla publicable: el modulo que TASTE declaro y nunca escribio ------
echo
$CC -O2 -Wall -Wextra -I"$L/tabla" \
    "$L/tabla/test_tabla.c" "$L/tabla/tabla.c" -o "$W/test_tabla" || exit 1
"$W/test_tabla" "$W" || exit 1

# --- la puerta de los datos: una, no cuatro --------------------------------
# El primer caso es el fichero de dos columnas con el que fue y fug daban DOS
# SERIES DISTINTAS. Aqui se fija cual es la buena.
echo
$CC -O2 -Wall -Wextra -I"$L/datos" -I"$L/xlsx" \
    "$L/datos/test_datos.c" "$L/datos/datos.c" "$L/xlsx/xlsx.c" \
    -o "$W/test_datos" -lz || exit 1
# El .xlsx de prueba lo fabrica Python si hay openpyxl: asi la prueba no
# depende de que haya un libro del analista a mano.
XLSX=""
if python3 -c "import openpyxl" 2>/dev/null; then
    python3 - "$W/prueba.xlsx" <<'XL' && XLSX="$W/prueba.xlsx"
import sys, datetime, openpyxl
wb = openpyxl.Workbook(); ws = wb.active
ws.append(["Fecha", "UEM", "ES"])
for i in range(24):
    a, m = 1996 + i // 12, i % 12 + 1
    # fecha de FIN DE MES: el salto en DIAS no dice nada (31, 29, 31) y el
    # de MESES si. Es el caso real del fichero del analista.
    fin = datetime.date(a + (m == 12), m % 12 + 1, 1) - datetime.timedelta(days=1)
    ws.append([fin, 55.0 + i, 50.0 + 2 * i])
for c in ws["A"][1:]:
    c.number_format = "mmm-yy"
wb.save(sys.argv[1])
XL
fi
"$W/test_datos" "$W" $XLSX || exit 1

# --- el proyecto: la cadena de iteracion -----------------------------------
# El nombre del fichero es CORTESIA: la identidad esta en el manifiesto.
echo
$CC -O2 -Wall -Wextra -I"$L/proyecto" \
    "$L/proyecto/test_proyecto.c" "$L/proyecto/proyecto.c" \
    -o "$W/test_proyecto" || exit 1
"$W/test_proyecto" "$W" || exit 1

# Y lo que hace convivir a las dos encarnaciones del taller: que el manifiesto
# que escribimos lo lea un yaml.safe_load de Python, con sus acentos.
if python3 -c "import yaml" 2>/dev/null; then
    python3 - "$W/proyecto.yaml" <<'PY' || exit 1
import sys, yaml
d = yaml.safe_load(open(sys.argv[1]))
assert d["id"] == "SF_MEG", d["id"]
assert d["titulo"] == "Inflación del área euro", d["titulo"]
assert d["series"]["IPC_ES"]["elegido"] == "m02"
assert d["modelos"]["IPC_ES/m01"]["padre"] == "m00"
assert d["modelos"]["IPC_ES/m01"]["version"] == 1
sin = [k for k, v in d["modelos"].items() if not v.get("razon")]
assert sin == ["IPC_ES/m00", "IPC_ES/m02", "SUELTA/m01"], sin
# m00 son LOS DATOS, y lo dice el manifiesto: no se deduce del numero.
assert d["modelos"]["IPC_ES/m00"]["rol"] == "datos"
assert "rol" not in d["modelos"]["IPC_ES/m01"]
# Los metadatos de la serie: van solo si estan, y se leen con sus acentos.
assert d["series"]["IPC_ES"]["descripcion"].startswith("Índice de precios")
assert d["series"]["IPC_ES"]["bajada"] == "2026-09-20"
assert "descripcion" not in d["series"]["SUELTA"], d["series"]["SUELTA"]
# Las muestras: la COMPLETA no se declara, y el modelo dice en cual nacio.
assert d["muestras"]["pre-covid"]["hasta"] == "12/2019"
assert d["modelos"]["IPC_ES/m01"]["muestra"] == "pre-covid"
assert "muestra" not in d["modelos"]["IPC_ES/m02"], "la completa no se escribe"
print("  ok    yaml.safe_load de Python lee el manifiesto entero")
PY
else
    echo "  (sin PyYAML: me salto la lectura desde Python)"
fi

# --- el .out de fue, leido para la rejilla -----------------------------------
# Contra los GOLDEN del motor, que estan en control de versiones: si el formato
# de la salida cambia, cambia el golden y esto se entera el mismo dia.
echo
$CC -O2 -Wall -Wextra -I"$L/outfile" $(pkg-config --cflags glib-2.0) \
    "$L/outfile/test_outfile.c" "$L/outfile/outfile.c" \
    -o "$W/test_outfile" $(pkg-config --libs glib-2.0) -lm || exit 1
"$W/test_outfile" "$TOP/../../engines/fue/tests/golden" || exit 1

# --- el dato y lo que se deriva de el ---------------------------------------
# El .csv es el dato y los .inp se generan de el: si el ida y vuelta perdiera
# algo, el .csv no seria el dato.
echo
$CC -O2 -Wall -Wextra -I"$TOP/../atsw/include" -I"$L/datos" -I"$L/xlsx" \
    -I"$L/proyecto" -I"$L/outdiag" -I"$L/outfile" -I"$L/dates" \
    -I"$L/tabla" -I"$L/preview" -I"$L/fugdraw" -I"$L/engine" -I"$L/inpcheck" \
    $(pkg-config --cflags gtk+-3.0) \
    "$TOP/../atsw/tests/test_genera.c" "$TOP/../atsw/src/genera.c" \
    "$L/datos/datos.c" "$L/xlsx/xlsx.c" "$L/proyecto/proyecto.c" \
    "$L/outdiag/outdiag.c" "$L/dates/dates.c" \
    "$TOP/../../engines/fug/src/inpfile.c" \
    -I"$TOP/../../engines/fug/src" \
    -o "$W/test_genera" $(pkg-config --libs gtk+-3.0) -lz -lm || exit 1
"$W/test_genera" "$W" || exit 1

# --- LA regla del editor del .inp -------------------------------------------
# Un guardado fallido no puede costar trabajo. Se valida con el comprobador
# DEL MOTOR -- se compila engines/fue/src/inpcheck.c, no una copia.
echo
$CC -O2 -Wall -Wextra -I"$TOP/../atsw/include" -I"$L/proyecto" -I"$L/inpcheck" \
    -I"$L/engine" -I"$L/outfile" -I"$TOP/../../engines/fue/include" \
    -Dinp_check=inp_check_fue -c "$TOP/../../engines/fue/src/inpcheck.c" \
    $(pkg-config --cflags glib-2.0) -o "$W/inpcheck_fue.o" || exit 1
$CC -O2 -Wall -Wextra -I"$TOP/../atsw/include" -I"$L/proyecto" -I"$L/inpcheck" \
    -I"$L/engine" -I"$L/outfile" -I"$L/outdiag" -I"$L/tabla" -I"$L/dates" \
    -I"$L/preview" -I"$L/rutas" -I"$L/fugdraw" -I"$L/utils" \
    $(pkg-config --cflags gtk+-3.0) \
    "$TOP/../atsw/tests/test_editor.c" "$TOP/../atsw/src/editor.c" \
    "$L/proyecto/proyecto.c" "$L/outfile/outfile.c" "$L/engine/engine.c" \
    "$L/outdiag/outdiag.c" "$L/dates/dates.c" "$L/rutas/rutas.c" \
    "$L/preview/preview.c" "$L/fugdraw/fugdraw.c" "$L/utils/ext.c" \
    "$W/inpcheck_fue.o" \
    -o "$W/test_editor" $(pkg-config --libs gtk+-3.0) -lm || exit 1
"$W/test_editor" "$W" "$TOP/../../engines/fue/tests/corpus/CPI_USA_model.inp" \
    || exit 1

# --- las reglas de la interfaz madre, sin widgets ---------------------------
# A que apuntan los botones cuando el analista no marca nada. Es una regla del
# metodo, no de la interfaz, y por eso se puede probar.
echo
$CC -O2 -Wall -Wextra -I"$TOP/../atsw/include" -I"$L/proyecto" -I"$L/outdiag" \
    -I"$L/outfile" -I"$L/tabla" -I"$L/dates" $(pkg-config --cflags gtk+-3.0) \
    -I"$L/rutas" -I"$L/preview" -I"$L/fugdraw" -I"$L/utils" -I"$L/datos" \
    -I"$L/xlsx" -I"$L/engine" -I"$L/inpcheck" \
    "$TOP/../atsw/tests/test_atsw.c" "$TOP/../atsw/src/ventana.c" \
    "$L/proyecto/proyecto.c" "$L/outdiag/outdiag.c" "$L/outfile/outfile.c" \
    "$L/dates/dates.c" \
    -o "$W/test_atsw" $(pkg-config --libs gtk+-3.0) -lm || exit 1
"$W/test_atsw" || exit 1
