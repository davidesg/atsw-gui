#!/bin/sh
# LA BATERIA DE atsw-cli: cada orden sobre un proyecto de prueba hecho con
# los golden del motor fue (engines/fue/tests/golden), sin red ni ventana.
#
#   sh tests/run_tests.sh            (desde gui/atsw-cli, con atsw-cli hecho)
#
# Lo que se comprueba es lo que el asistente va a usar: que cada orden hace
# en el proyecto lo mismo que el gesto de la madre (son las mismas
# funciones), que el JSON se lee, y que los errores dicen por que y salen
# con su estado (1 no se pudo, 2 la orden esta mal escrita).

TOP=$(cd "$(dirname "$0")/.." && pwd)
R=$(cd "$TOP/../.." && pwd)
G="$R/engines/fue/tests/golden"
EXE=; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; esac
CLI="$TOP/atsw-cli$EXE"
FUE="$R/engines/fue/bin/fue$EXE"
PY=$(command -v python3 || command -v python)
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t atswcli)
trap 'rm -rf "$WORK"' EXIT INT TERM

[ -x "$CLI" ] || { echo "falta $CLI: make primero"; exit 1; }

fallos=0
ok()   { echo "  ok    $1"; }
mal()  { echo "  FALLA $1"; fallos=$((fallos + 1)); }
mira() { if eval "$2"; then ok "$1"; else mal "$1"; fi; }
# lee un campo del JSON de stdin (python, que esta en los tres sistemas de CI)
campo() { "$PY" -c "import json,sys; d=json.load(sys.stdin); print($1)"; }

# --- el proyecto de prueba ------------------------------------------------
P="$WORK/proy"
mkdir -p "$P/RIPC/work" "$P/en4/work"
cp "$G/RIPC.2/RIPC.2.pre" "$P/RIPC/work/RIPC_m00.inp"     # hace de datos
cp "$G/RIPC.2/RIPC.2.pre" "$P/RIPC/work/RIPC_m01.pre"
cp "$G/RIPC.2/RIPC.2.out" "$P/RIPC/work/RIPC_m01.out"
cp "$G/en4_ar18/en4_ar18.pre" "$P/en4/work/en4_m00.inp"
cp "$G/en4_ar18/en4_ar18.pre" "$P/en4/work/en4_m01.pre"
cp "$G/en4_ar18/en4_ar18.out" "$P/en4/work/en4_m01.out"
cat > "$P/proyecto.yaml" <<'YAML'
schema_version: 2
id: prueba
titulo: "la de atsw-cli"
creado: "2026-10-23"
analista: ""
herramienta: fue
raiz: .

series:
  RIPC:
  en4:

modelos:
  RIPC/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
  RIPC/m01:
    version: 1
    padre: m00
    razon: "el AR(2) del golden"
  en4/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
  en4/m01:
    version: 1
    padre: m00
    razon: "el AR(18) del golden"
YAML
Y="$P/proyecto.yaml"

echo
echo "atsw-cli: las órdenes"

mira "version" '"$CLI" version | grep -q "^atsw-cli "'
mira "version --json" '[ "$("$CLI" --json version | campo "d[\"ok\"]")" = True ]'
mira "proyecto ver --json: cuatro modelos" \
     '[ "$("$CLI" --json proyecto ver "$Y" | campo "len(d[\"modelos\"])")" = 4 ]'

# proyecto ruta: la de lib/proyecto, para el extra Python del asistente
# (sin comparar la cadena: en Windows el separador de lib/proyecto es otro)
RUTA=$("$CLI" proyecto ruta "$Y" RIPC m01 .out)
mira "proyecto ruta: el .out de RIPC/m01, el que hay" \
     '[ -f "$RUTA" ] && cmp -s "$RUTA" "$P/RIPC/work/RIPC_m01.out" && case "$RUTA" in *RIPC_m01.out) true ;; *) false ;; esac'
mira "proyecto ruta datos: el nodo de datos de la serie" \
     '[ "$("$CLI" --json proyecto ruta "$Y" RIPC datos .inp | campo "d[\"modelo\"]")" = m00 ]'
mira "proyecto ruta de un fichero que no está: estado 1" \
     '! "$CLI" proyecto ruta "$Y" RIPC m00 .out >/dev/null 2>&1'
mira "proyecto ruta con otra extensión: no" \
     '"$CLI" proyecto ruta "$Y" RIPC m01 .csv 2>&1 | grep -q "es .inp, .pre o .out"'

# dictamen: el mismo de la ventana (dx_dictamen sobre el .out)
J=$("$CLI" --json dictamen "$Y" RIPC m01)
mira "dictamen --json: se lee y trae sus líneas" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"ok\"] and len(d[\"lineas\"]) >= 5")" = True ]'
mira "dictamen: la estimación convergió" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"lineas\"][0][\"estado\"]")" = cuadra ]'

# ecuacion: el bloque de estimaciones del .out, tal cual, con sus t. Con printf
# y no echo: el JSON lleva \n escapados, y el echo de sh (dash, macOS) los
# convierte en saltos de linea de verdad.
J=$("$CLI" --json ecuacion "$Y" RIPC m01)
NEST=$(grep -cE '^ +-?[0-9.]+ +\([0-9.]+\) +\[ *[0-9]+\]' "$P/RIPC/work/RIPC_m01.out")
mira "ecuacion --json: un parámetro por cada línea estimada del .out ($NEST)" \
     '[ "$(printf "%s\n" "$J" | campo "len([q for q in d[\"parametros\"] if q[\"n\"]])")" = "$NEST" ]'
mira "ecuacion: la t es la estimación entre su error típico" \
     '[ "$(printf "%s\n" "$J" | campo "all(abs(q[\"t\"] - q[\"estimacion\"]/q[\"et\"]) < 1e-6 * (1 + abs(q[\"t\"])) for q in d[\"parametros\"] if q[\"n\"])")" = True ]'
mira "ecuacion: con su log-verosimilitud y su σ" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"logelf\"] is not None and d[\"sigma\"] != \"\"")" = True ]'
mira "ecuacion: en texto, la cabecera del modelo" \
     '"$CLI" ecuacion "$Y" RIPC m01 | grep -q "^MODELO ESTIMADO: RIPC/m01"'
J=$("$CLI" --json ecuacion "$Y" RIPC m00 2>/dev/null); rc=$?
mira "ecuacion de un modelo sin estimar: lo dice" '[ $rc -ne 0 ]'

# raices: los números de test_raices (art.roots)
J=$("$CLI" --json raices factores "$Y" en4 m01)
mira "raices factores: el AR(18) en nueve pares" \
     '[ "$(printf "%s\n" "$J" | campo "len(d[\"factores\"][0][\"partes\"])")" = 9 ]'
mira "raices factores: el par más cercano, d = 0.948, periodo 2.40" \
     '[ "$(printf "%s\n" "$J" | campo "round(d[\"factores\"][0][\"partes\"][0][\"d\"],3) == 0.948 and round(d[\"factores\"][0][\"partes\"][0][\"periodo\"],2) == 2.4")" = True ]'

# itera: el .pre del padre, byte a byte, al .inp del hijo, con su razón
J=$("$CLI" --json proyecto itera "$Y" RIPC m01 --razon "seguir desde el óptimo")
H=$(printf "%s\n" "$J" | campo "d[\"modelo\"]")
mira "itera: nace m02" '[ "$H" = m02 ]'
mira "itera: el .inp es el .pre del padre" 'cmp -s "$P/RIPC/work/RIPC_m01.pre" "$P/RIPC/work/RIPC_m02.inp"'
mira "itera: la ruta del JSON es absoluta y es la del .inp" \
     '[ -f "$(printf "%s\n" "$J" | campo "d[\"inp\"]")" ]'
V=$("$CLI" --json proyecto ver "$Y")
mira "itera: el linaje y la razón, en el manifiesto" \
     '[ "$(echo "$V" | campo "[(m[\"padre\"], m[\"razon\"]) for m in d[\"modelos\"] if m[\"serie\"]==\"RIPC\" and m[\"id\"]==\"m02\"][0]")" = "('"'"'m01'"'"', '"'"'seguir desde el óptimo'"'"')" ]'
mira "itera: de los datos no se itera (estado 1)" \
     '! "$CLI" proyecto itera "$Y" RIPC m00 2>/dev/null'

# nuevo: copia del .inp de los datos, colgado de m00
mira "nuevo: nace de los datos" '"$CLI" proyecto nuevo "$Y" RIPC >/dev/null && cmp -s "$P/RIPC/work/RIPC_m00.inp" "$P/RIPC/work/RIPC_m03.inp"'

# razon y elige
"$CLI" proyecto razon "$Y" RIPC m03 "empezar de cero" >/dev/null
"$CLI" proyecto elige "$Y" RIPC m01 "el del golden" >/dev/null
V=$("$CLI" --json proyecto ver "$Y")
mira "razon: apuntada" \
     '[ "$(echo "$V" | campo "[m[\"razon\"] for m in d[\"modelos\"] if m[\"serie\"]==\"RIPC\" and m[\"id\"]==\"m03\"][0]")" = "empezar de cero" ]'
mira "elige: uno solo elegido en la serie" \
     '[ "$(echo "$V" | campo "[m[\"id\"] for m in d[\"modelos\"] if m[\"serie\"]==\"RIPC\" and m[\"elegido\"]]")" = "['"'"'m01'"'"']" ]'

# caso-nuevo: con el sha256 de cada .pre
J=$("$CLI" --json proyecto caso-nuevo "$Y" RIPC/m01 en4/m01 --titulo "prueba")
mira "caso-nuevo: C1" '[ "$(printf "%s\n" "$J" | campo "d[\"caso\"]")" = C1 ]'
SHA=$("$PY" -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$P/RIPC/work/RIPC_m01.pre")
mira "caso-nuevo: el sha del .pre en el manifiesto" 'grep -q "$SHA" "$Y"'
mira "caso-nuevo: un modelo sin estimar no entra" \
     '! "$CLI" proyecto caso-nuevo "$Y" RIPC/m03 en4/m01 2>/dev/null'
J=$("$CLI" --json proyecto caso-nuevo "$Y" RIPC/m01 en4/m01 --motor drvarma --titulo "sistema")
mira "caso-nuevo --motor drvarma: un caso de SIM" '[ "$(printf "%s\n" "$J" | campo "d[\"motor\"]")" = drvarma ]'
mira "caso-nuevo --motor: solo drtran o drvarma" \
     '! "$CLI" proyecto caso-nuevo "$Y" RIPC/m01 en4/m01 --motor fue 2>/dev/null'

# raices factorizado: los nueve AR(2), y fue los estima con la misma
# verosimilitud que el AR(18)
J=$("$CLI" --json raices factorizado "$Y" en4 m01)
F=$(printf "%s\n" "$J" | campo "d[\"inp\"]")
mira "raices factorizado: el hijo, con nueve factores AR de orden 2" \
     'grep -A1 "regular AR operators" "$F" | grep -q "^9 2 2 2 2 2 2 2 2 2"'
if [ -x "$FUE" ]; then
    D=$(dirname "$F"); B=$(basename "$F" .inp)
    ( cd "$D" && "$FUE" "$B" >/dev/null 2>&1 </dev/null )
    L1=$(grep "^logelf" "$D/$B.out" | awk '{print $2}')
    L0=$(grep "^logelf" "$G/en4_ar18/en4_ar18.out" | awk '{print $2}')
    mira "raices factorizado: fue da la misma verosimilitud ($L1 y $L0)" \
         '"$PY" -c "import sys; sys.exit(0 if abs($L1-($L0)) < 1e-4 else 1)"'
fi
mira "raices factorizado: el ya factorizado no se vuelve a partir (estado 1)" \
     '! "$CLI" raices factorizado "$Y" en4 "$(printf "%s\n" "$J" | campo "d[\"modelo\"]")" 2>/dev/null'

# prever: fue -f y fuf desde el .pre, sin tocar el modelo (fue -f reestima y
# reescribia el .out: la C3 de AUDITORIA-procesos). Una serie mensual (RIPC) y
# una anual (MINK, de docs/ejemplos: la tabla anual es el BUG-0003 de fuf).
FUF="$R/engines/fuf/bin/fuf$EXE"
if [ -x "$FUE" ] && [ -x "$FUF" ]; then
    PV="$WORK/proy_prever"; cp -R "$P" "$PV"; YV="$PV/proyecto.yaml"; rm -f "$PV/costes.csv"
    H=$(cksum < "$PV/RIPC/work/RIPC_m01.out")
    J=$("$CLI" --json prever "$YV" RIPC m01 --por asistente)
    mira "prever: la tabla de fuf, 24 meses desde 6/2008" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"origen\"], len(d[\"prevision\"]), d[\"prevision\"][0][\"fecha\"])")" = "('"'"'6/2008'"'"', 24, '"'"'7/2008'"'"')" ]'
    mira "prever no toca el modelo: su .out queda igual" '[ "$H" = "$(cksum < "$PV/RIPC/work/RIPC_m01.out")" ]'
    mira "prever deja forecast_RIPC_m01.inp y .out junto al modelo" \
         '[ -f "$PV/RIPC/work/forecast_RIPC_m01.inp" ] && [ -f "$PV/RIPC/work/forecast_RIPC_m01.out" ]'
    mira "prever: las dos corridas, en costes.csv como «prever»" \
         '[ "$(grep -c ",atsw-cli,asistente,RIPC/m01,prever," "$PV/costes.csv")" = 2 ]'
    PA="$WORK/proy_anual"; cp -R "$R/docs/ejemplos/jenkins_alavi" "$PA"
    J=$("$CLI" --json prever "$PA/proyecto.yaml" MINK m01)
    mira "prever de una serie anual: la fecha es el año, sin columnas anuales" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"origen\"], d[\"prevision\"][0][\"fecha\"], d[\"prevision\"][0][\"var_anual\"])")" = "('"'"'1911'"'"', '"'"'1912'"'"', None)" ]'
fi
mira "prever de un modelo sin estimar: estado 1" '! "$CLI" prever "$Y" RIPC m00 2>/dev/null'

# identifica: lo que art propone, con las mismas funciones que la ventana de
# identificacion (lib/artcand/artrun.c). Propone y no escribe nada.
ART="$R/engines/art/bin/art$EXE"
if [ -x "$ART" ]; then
    H=$(cksum < "$Y")
    J=$("$CLI" --json identifica "$Y" RIPC --transformacion 1,0,0)
    mira "identifica: art propone, y su propuesta va la primera" \
         '[ "$(printf "%s\n" "$J" | campo "len(d[\"candidatos\"]) > 0 and d[\"candidatos\"][0][\"propuesto\"]")" = True ]'
    mira "identifica: sobre los datos, con la transformación pedida y s = 12" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"sobre\"], d[\"lambda\"], d[\"d\"], d[\"D\"], d[\"s\"])")" = "('"'"'datos'"'"', 1, 0, 0, 12)" ]'
    J=$("$CLI" --json identifica "$Y" RIPC --residuos m01)
    mira "identifica --residuos: lo que les falta a los residuos de m01" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"sobre\"], d[\"modelo\"], len(d[\"candidatos\"]) > 0)")" = "('"'"'residuos'"'"', '"'"'m01'"'"', True)" ]'
    mira "identifica no escribe nada en el proyecto" '[ "$H" = "$(cksum < "$Y")" ]'
fi
mira "identifica con una λ que art no admite: estado 1, y dice cuáles sí" \
     '"$CLI" identifica "$Y" RIPC --transformacion 0.5,0,0 2>&1 | grep -q "logaritmos"'
mira "identifica de una serie que no está: estado 1" '! "$CLI" identifica "$Y" OTRA 2>/dev/null'
mira "identifica --residuos de los datos: no están estimados (estado 1)" \
     '! "$CLI" identifica "$Y" RIPC --residuos m00 2>/dev/null'

# especifica y estima: lo que el asistente hace con un modelo nuevo. Sobre una
# copia (estima deja costes.csv). El editor es lib/inpdet, el de la madre, y
# el juez del .inp, fue (inp_check_fue).
PE="$WORK/proy_especifica"; cp -R "$P" "$PE"; YE="$PE/proyecto.yaml"; rm -f "$PE/costes.csv"
N=$("$CLI" --json proyecto nuevo "$YE" RIPC --por asistente --razon "un AR(2)" | campo "d[\"modelo\"]")
J=$("$CLI" --json especifica "$YE" RIPC "$N" --transformacion 1,0,0 --arma 2,0,0,0 --media libre)
mira "especifica: el ARMA queda como se pidió, (2,0)(0,0)" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"arma\"]")" = "[2, 0, 0, 0]" ]'
F=$(printf "%s\n" "$J" | campo "d[\"inp\"]")
mira "especifica: fue acepta lo que salió" \
     '[ -n "$F" ] && [ -f "$F" ] && ! ls "$(dirname "$F")" | grep -q "esp[12].tmp"'
H=$(cksum < "$F")
mira "especifica: un cambio que no vale deja el .inp como estaba (estado 1)" \
     '! "$CLI" especifica "$YE" RIPC "$N" --intervencion "step" 2>/dev/null && [ "$H" = "$(cksum < "$F")" ]'
mira "especifica sin cambios: estado 1" '! "$CLI" especifica "$YE" RIPC "$N" 2>/dev/null'
mira "especifica de los datos: no se tocan (estado 1)" \
     '"$CLI" especifica "$YE" RIPC m00 --arma 1,0,0,0 2>&1 | grep -q "son los datos"'
# los armónicos y Semana Santa: deterministas que fue lee por su nombre. Los
# datos de RIPC ya traen el paquete entero (cos/sin 1..5 y alter).
N2=$("$CLI" --json proyecto nuevo "$YE" RIPC --por asistente --razon "Semana Santa" | campo "d[\"modelo\"]")
F2="$PE/RIPC/work/RIPC_$N2.inp"; H2=$(cksum < "$F2")
mira "especifica --armonicos todos: los que ya trae no se repiten (estado 1)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --armonicos todos 2>&1 | grep -q "ya trae esos armónicos" && [ "$H2" = "$(cksum < "$F2")" ]'
mira "especifica --armonicos 6 con s = 12: son de 1 a 5 pares (estado 1)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --armonicos 6 2>&1 | grep -q "de 1 a 5"'
mira "especifica --armonicos con D = 1: la estacionalidad ya va en la diferencia (estado 1)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --transformacion 1,0,1 --armonicos 1 2>&1 | grep -q "D = 1" && [ "$H2" = "$(cksum < "$F2")" ]'
J=$("$CLI" --json especifica "$YE" RIPC "$N2" --pascua)
mira "especifica --pascua: easter entra, y fue lo acepta" \
     '[ "$(printf "%s\n" "$J" | campo "\"easter\" in d[\"cambios\"]")" = True ] && grep -qx "easter" "$F2"'
mira "especifica --pascua otra vez: ya la trae (estado 1)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --pascua 2>&1 | grep -q "ya trae Semana Santa"'
# las intervenciones se AÑADEN; una igual a otra, no (BUG-0001): los datos
# de RIPC traen «step 2 2002», y el hijo la hereda.
H2=$(cksum < "$F2")
mira "especifica --intervencion que ya trae: no se repite (estado 1, BUG-0001)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --intervencion "step 2 2002" 2>&1 | grep -q "ya trae «step 2 2002»" && [ "$H2" = "$(cksum < "$F2")" ]'
mira "  y con otros espacios es la misma" \
     '! "$CLI" especifica "$YE" RIPC "$N2" --intervencion "step  2   2002" 2>/dev/null && [ "$H2" = "$(cksum < "$F2")" ]'
mira "  y dos veces en la orden, tampoco" \
     '"$CLI" especifica "$YE" RIPC "$N2" --intervencion "impulse 5 2003" --intervencion "impulse 5 2003" 2>&1 | grep -q "va dos veces" && [ "$H2" = "$(cksum < "$F2")" ]'
mira "especifica: otra forma en la misma fecha entra, con el aviso de art (BUG-0162 de art)" \
     '"$CLI" especifica "$YE" RIPC "$N2" --intervencion "impulse 2 2002" | grep -q "Ya había una intervención en esa fecha" && grep -qx "impulse 2 2002" "$F2"'
if [ -x "$FUE" ]; then
    J=$("$CLI" --json estima "$YE" RIPC "$N" --por asistente)
    mira "estima: fue termina y trae la log-verosimilitud y el dictamen" \
         '[ "$(printf "%s\n" "$J" | campo "d[\"estado\"] == 0 and d[\"logelf\"] is not None and d[\"dictamen\"] != \"\"")" = True ]'
    mira "estima: su línea en costes.csv, por el asistente" \
         'grep -q ",atsw-cli,asistente,RIPC/$N,estimar,fue" "$PE/costes.csv"'
    mira "estima de lo ya estimado: no se reestima (estado 1)" \
         '"$CLI" estima "$YE" RIPC "$N" 2>&1 | grep -q "ya está estimado"'
    mira "especifica de lo ya estimado: no se reescribe (estado 1)" \
         '! "$CLI" especifica "$YE" RIPC "$N" --arma 1,0,0,0 2>/dev/null'
fi

# anomalos: las mismas funciones que la ventana de la madre (an_residuos,
# an_calibra). SIN OMITIR NADA, la ACF y la PACF de lib/anomalos son las que
# fue imprime en el .out, retardo a retardo, y su Jarque-Bera tambien
# (lib/anomalos/anomalos.h): el cotejo es contra el motor. RIPC.2 tiene un
# episodio, 1/2003, y sin el la ACF y la PACF del retardo 13 cambian de lado.
O="$G/RIPC.2/RIPC.2.out"
# LA MISMA Q, LA MISMA p (BUG-0006 de la madre): RIPC.2 es un AR(2), y la Q
# de 39 retardos tiene 37 g.l. en anomalos como en el dictamen.
mira "anomalos: la Q descuenta los ARMA, como el dictamen (BUG-0006)" \
     '"$CLI" anomalos "$Y" RIPC m01 | grep -q "Q(39), 37 g.l.  con 43.4 (p = 0.217)" && "$CLI" dictamen "$Y" RIPC m01 | grep -q "Q(39)=43.4 (37 g.l.) p=0.217"'
mira "dictamen: Q(retardo), no Q(g.l.) (BUG-0006)" \
     '"$CLI" dictamen "$Y" RIPC m01 | grep -q "Q(12)=8.6 (10 g.l.)"'
J=$("$CLI" --json anomalos "$Y" RIPC m01 --episodios 1)
mira "anomalos: un episodio, en 1/2003" \
     '[ "$(printf "%s\n" "$J" | campo "len(d[\"episodios\"]), d[\"episodios\"][0][\"desde\"]")" = "1 1/2003" ]'
cola() { "$PY" - "$O" "$1" <<'PY'
import json, re, sys
t = open(sys.argv[1], errors="replace").read()
def tabla(titulo):
    b = t[t.rindex(titulo):].split("\n")[4:]
    v = {}
    for l in b:
        m = re.match(r"\s*(\d+)\s+(-?\d+\.\d+)\s+[|+]", l)   # cada 12, con +
        if not m: break
        v[int(m.group(1))] = float(m.group(2))
    return v
acf, pacf = tabla("Autocorrelation function"), tabla("Partial autocorrelation function")
jb = float(re.search(r"Jarque-Bera:\s+(\S+)", t).group(1))
d = json.loads(sys.argv[2])
peor = max(max(abs(x["acf_con"] - acf[x["lag"]]), abs(x["pacf_con"] - pacf[x["lag"]]))
           for x in d["calibracion"] if x["lag"] in acf and x["lag"] in pacf)
n = len([x for x in d["calibracion"] if x["lag"] in acf])
print("%d %.5f %.6f %.6f" % (n, peor, d["jb"]["con"], jb))
PY
}
R4=$(cola "$J")
NL=$(echo "$R4" | cut -d' ' -f1); PEOR=$(echo "$R4" | cut -d' ' -f2)
JBC=$(echo "$R4" | cut -d' ' -f3); JBO=$(echo "$R4" | cut -d' ' -f4)
mira "anomalos: la ACF y la PACF de los $NL retardos son las del .out de fue (peor $PEOR)" \
     '[ "$NL" -ge 30 ] && "$PY" -c "import sys; sys.exit(0 if $PEOR <= 0.0006 else 1)"'
mira "anomalos: el Jarque-Bera es el del .out ($JBC y $JBO)" \
     '"$PY" -c "import sys; sys.exit(0 if abs($JBC - $JBO) < 1e-3 * abs($JBO) else 1)"'
mira "anomalos: sin el episodio, el retardo 13 cambia de lado en la ACF y en la PACF" \
     '[ "$(printf "%s\n" "$J" | campo "[(x[\"acf\"], x[\"pacf\"]) for x in d[\"calibracion\"] if x[\"lag\"] == 13][0]")" = "('"'"'enmascarada'"'"', '"'"'enmascarada'"'"')" ]'
mira "anomalos: tantos residuos como el .out, y uno omitido" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"residuos\"], d[\"omitidas\"]")" = "78 1" ]'
mira "anomalos: sin --episodios se omiten todos (lo mismo aquí, que hay uno)" \
     '[ "$("$CLI" --json anomalos "$Y" RIPC m01 | campo "d[\"omitidas\"]")" = 1 ]'
mira "anomalos --episodios fuera de rango: estado 1, y dice cuáles hay" \
     '"$CLI" anomalos "$Y" RIPC m01 --episodios 2 2>&1 | grep -q "de 1 a 1"'
mira "anomalos de un modelo sin estimar: estado 1" \
     '! "$CLI" anomalos "$Y" RIPC m00 2>/dev/null'

# prewhiten: la CCF preblanqueada, con las mismas funciones que la pantalla de
# identificacion de drtran_gui (prewhiten_ccf, hosking_test, prewhiten_lee).
# El oraculo es el del motor, el que fija gui/drtran/tests/test_prewhiten.c
# sobre lo que imprime «drtran -p» con estos dos .pre: 203 observaciones, la
# banda 0.14037, r(0) 0.3542, r(1) 0.2167, r(12) -0.3282, r(13) -0.1797,
# nu(0) 0.0151 y ningun retardo negativo fuera de banda.
PW="$WORK/proy_pw"; DC="$R/engines/drtran/tests/cases"
mkdir -p "$PW/ES/work" "$PW/WTI/work"
cp "$DC/ES_CPI_airline.pre" "$PW/ES/work/ES_m00.inp"; cp "$DC/ES_CPI_airline.pre" "$PW/ES/work/ES_m01.pre"
cp "$DC/WTI_ar1.pre" "$PW/WTI/work/WTI_m00.inp";      cp "$DC/WTI_ar1.pre" "$PW/WTI/work/WTI_m01.pre"
cat > "$PW/proyecto.yaml" <<'YAML'
schema_version: 2
id: pw
titulo: "el preblanqueo"
creado: "2026-11-14"
analista: ""
herramienta: fue
raiz: .

series:
  ES:
  WTI:

modelos:
  ES/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
  ES/m01:
    version: 1
    padre: m00
    razon: "el airline"
  WTI/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
  WTI/m01:
    version: 1
    padre: m00
    razon: "el AR(1)"
YAML
YP="$PW/proyecto.yaml"
J=$("$CLI" --json prewhiten "$YP" ES/m01 WTI/m01 --retardos 24)
pw() { echo "$J" | "$PY" -c "
import json, sys
e = json.load(sys.stdin)['entradas'][0]
r = {c['k']: c for c in e['ccf']}
want = [('n', e['n'], 203, 0.5), ('banda', e['banda'], 0.14037, 5e-5),
        ('r(0)', r[0]['r'], 0.3542, 5e-4), ('r(1)', r[1]['r'], 0.2167, 5e-4),
        ('r(12)', r[12]['r'], -0.3282, 5e-4), ('r(13)', r[13]['r'], -0.1797, 5e-4),
        ('nu(0)', r[0]['nu'], 0.0151, 5e-5), ('nu(12)', r[12]['nu'], -0.0140, 5e-5),
        ('r(-6)', r[-6]['r'], -0.107, 5e-4), ('negativos', e['negativos_fuera'], 0, 0.5)]
mal = [w[0] for w in want if abs(w[1] - w[2]) > w[3]]
print(' '.join(mal) if mal else 'bien')"; }
mira "prewhiten: los números del motor (drtran -p): n, banda, r(k), nu(k) y los negativos" \
     '[ "$(pw)" = bien ]'
mira "prewhiten: b = 0, el pico en k = 0, y la entrada puede tratarse como exógena" \
     '[ "$(printf "%s\n" "$J" | campo "(d[\"entradas\"][0][\"b\"], d[\"entradas\"][0][\"kmax\"], d[\"entradas\"][0][\"negativos_fuera\"])")" = "(0, 0, 0)" ]'
mira "prewhiten: el Hosking sobre los preblanqueados, con 4 x 24 grados de libertad" \
     '[ "$(printf "%s\n" "$J" | campo "d[\"entradas\"][0][\"gl\"]")" = 96 ]'
mira "prewhiten sin entrada: estado 1" '! "$CLI" prewhiten "$YP" ES/m01 2>/dev/null'
mira "prewhiten de un modelo sin .pre: estado 1, y dice por qué" \
     '"$CLI" prewhiten "$YP" ES/m00 WTI/m01 2>&1 | grep -q "no tiene .pre"'
mira "prewhiten con una entrada que no se puede leer: esa fila lo dice, las demás siguen" \
     '[ "$("$CLI" --json prewhiten "$YP" ES/m01 WTI/m09 WTI/m01 | campo "[e[\"vale\"] for e in d[\"entradas\"]]")" = "[False, True]" ]'

# formales: los contrastes del modelo estimado, con las mismas funciones que
# las dos ventanas de la madre (lib/formales). Los numeros son los de art que
# fija lib/formales/test_formales.c para RIPC.2: la simplificacion estacional
# 15.77767809, Shin-Fuller Phi = 1.69946526 (critico 1.75) y la
# sobrediferenciacion LR = 0. Su MEG esta mal condicionado: solo se coteja
# el veredicto, ninguna frecuencia estocastica. SOBRE UNA COPIA del
# proyecto: sus reestimaciones dejan costes.csv, y las pruebas de coste de mas
# abajo empiezan sin el.
PF="$WORK/proy_formales"; cp -R "$P" "$PF"; YF="$PF/proyecto.yaml"; rm -f "$PF/costes.csv"
if [ -x "$FUE" ]; then
    J=$("$CLI" --json formales "$YF" RIPC m01)
    # al fallar dice que vio: el estadistico, si es calculable y por que no
    cerca() { "$PY" -c "
import json, sys
d = json.load(sys.stdin)
c = [x for x in d['contrastes'] if x['clave'] == '$1'][0]
bien = c['estadistico'] is not None and abs(c['estadistico'] - ($2)) < $3
if not bien:
    sys.stderr.write('      se vio: estadistico %s, L %s, L0 %s, calculable %s, dudoso %s, motivo %r\\n'
                     % (c['estadistico'], c['l_libre'], c['l_restringido'], c['calculable'],
                        c['dudoso'], c['motivo']))
sys.exit(0 if bien else 1)"; }
    mira "formales: la simplificación estacional, LR 15.7777 (art)" \
         'echo "$J" | cerca estacional 15.77767809 1e-5'
    mira "formales: Shin-Fuller, Phi 1.6995 (art)" 'echo "$J" | cerca sf 1.69946526 1e-5'
    mira "formales: la sobrediferenciación, LR 0 (art)" \
         'echo "$J" | cerca sobre 0 1e-6'
    mira "formales: el MEG, ninguna frecuencia estocástica (art)" \
         '[ "$(printf "%s\n" "$J" | campo "(sum(1 for x in d[\"contrastes\"] if x[\"clave\"] == \"meg\"), sum(1 for x in d[\"contrastes\"] if x[\"clave\"] == \"meg\" and x[\"calculable\"] and x[\"rechaza\"]))")" = "(6, 0)" ]'
    mira "formales: lo que no aplica, dicho (sin MA regular, no hay DCD)" \
         'printf "%s\n" "$J" | campo "d[\"no_aplica\"]" | grep -q "DCD:"'
    mira "formales: cada reestimación, en costes.csv por atsw-cli" \
         '[ "$(grep -c ",atsw-cli,analista,RIPC/m01,contraste," "$PF/costes.csv")" -ge 2 ] &&
          grep -q ",atsw-cli,analista,RIPC/m01,meg," "$PF/costes.csv"'
    J=$("$CLI" --json formales "$YF" RIPC m01 --familia integracion)
    mira "formales --familia integracion: solo los suyos" \
         '[ "$(printf "%s\n" "$J" | campo "all(x[\"familia\"] == \"integracion\" for x in d[\"contrastes\"]) and len(d[\"contrastes\"]) == 2")" = True ]'
    mira "formales --sub con d = 0: no hay diferencia que cuestionar, y lo dice" \
         '"$CLI" formales "$YF" RIPC m01 --familia integracion --sub 2>&1 >/dev/null | grep -q "d = 0"'
fi
mira "formales --familia estacionalidad de una serie sin estacionalidad: estado 1" \
     '! "$CLI" formales "$YF" en4 m01 --familia estacionalidad 2>/dev/null'
mira "formales --familia mal escrita: estado 1, y dice por qué" \
     '"$CLI" formales "$YF" RIPC m01 --familia otra 2>&1 | grep -q "integracion o estacionalidad"'

# caso-estima: lo que hace «Estimar» en drtran_gui, sin ventana. Sobre el
# mismo proyecto: el caso ES <- WTI, y sus corridas.
DRT="$R/engines/drtran/bin/drtran$EXE"
"$CLI" proyecto caso-nuevo "$YP" ES/m01 WTI/m01 --motor drtran --razon "ES <- WTI" > /dev/null
"$CLI" proyecto caso-nuevo "$YP" ES/m01 WTI/m01 --motor drvarma --razon "el sistema" > /dev/null
mira "caso-estima de un caso de SIM sin --p --q: un VARMA los lleva (estado 1)" \
     '"$CLI" caso-estima "$YP" C2 2>&1 | grep -q -- "--p N --q N"'
mira "caso-estima con un enlace mal escrito: estado 1, y dice cómo es" \
     '"$CLI" caso-estima "$YP" C1 --enlace "WTI-ES:0,0,1" 2>&1 | grep -q "ENTRADA->SALIDA"'
if [ -x "$DRT" ]; then
    J=$("$CLI" --json caso-estima "$YP" C1 --razon "que drtran identifique" --por asistente)
    mira "caso-estima: la primera corrida, c00, con su .out" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"red\"])")" = "('"'"'c00'"'"', False)" ] && [ -f "$(printf "%s\n" "$J" | campo "d[\"out\"]")" ]'
    mira "caso-estima: su línea en costes.csv, por el asistente" \
         'grep -q ",atsw-cli,asistente,C1/c00,estimar,drtran" "$PW/costes.csv"'
    J=$("$CLI" --json caso-estima "$YP" C1 --enlace "WTI->ES:0,0,1" --desde c00 --razon "un omega más")
    mira "caso-estima --enlace --desde: c01, hija de c00, con su red escrita" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"padre\"], d[\"red\"], d[\"enlaces\"])")" = "('"'"'c01'"'"', '"'"'c00'"'"', True, 1)" ] &&
          grep -q "<- *WTI *0 0 1" "$PW/_casos/C1/work/C1_c01.dag"'
    J=$("$CLI" --json caso-estima "$YP" C1 --diagonal --razon "el diagonal")
    mira "caso-estima --diagonal: sin red" '[ "$(printf "%s\n" "$J" | campo "(d[\"diagonal\"], d[\"red\"])")" = "(True, False)" ]'
    H=$(cksum < "$YP")
    mira "caso-estima con una red que drtran rechaza: no queda la corrida" \
         '! "$CLI" caso-estima "$YP" C1 --enlace "WTI->ES:0,0,1" --enlace "ES->WTI:0,0,1" 2>/dev/null &&
          [ "$H" = "$(cksum < "$YP")" ] && ! ls "$PW/_casos/C1/work" | grep -q "c03"'
    # la prevision y la evaluacion, en la misma corrida; y frente al diagonal
    # con la misma ventana (lib/fcomp), como la pestaña Prevision de drtran_gui
    J=$("$CLI" --json caso-estima "$YP" C1 --desde c01 --prever 6 --evaluar 180 --razon "con prevision")
    mira "caso-estima --prever: la previsión de las dos series, a 6" \
         '[ "$(printf "%s\n" "$J" | campo "sorted((x[\"serie\"], x[\"horizonte\"]) for x in d[\"prevision\"])")" = "[('"'"'ES_CPI'"'"', 6), ('"'"'WTI'"'"', 6)]" ]'
    mira "caso-estima --evaluar: la evaluación de la salida, h = 1..6" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"evaluacion\"][\"salida\"], len(d[\"evaluacion\"][\"horizontes\"]))")" = "('"'"'ES_CPI'"'"', 6)" ]'
    mira "caso-estima --evaluar: frente al diagonal, el cociente de RMSE y DM por horizonte" \
         '[ "$(printf "%s\n" "$J" | campo "len(d[\"frente_al_diagonal\"][\"horizontes\"]) == 6 and d[\"frente_al_diagonal\"][\"horizontes\"][0][\"cociente\"] > 0")" = True ]'
    mira "caso-estima --evaluar: la corrida del diagonal, en costes.csv como «evaluar»" \
         'grep -q ",atsw-cli,analista,C1/c03,evaluar,drtran" "$PW/costes.csv"'
    # las restricciones: detras de las heredadas, en el .cns de la corrida, y
    # el juez es el motor; la tabla de parametros del .out, tal cual
    J=$("$CLI" --json caso-estima "$YP" C1 --desde c01 --restriccion "omega1[1] = 0" --razon "sin el segundo omega")
    mira "caso-estima --restriccion: c04, con su .cns y el slot fijo en la tabla" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"restricciones\"])")" = "('"'"'c04'"'"', True)" ] &&
          grep -qx "omega1\[1\] = 0" "$PW/_casos/C1/work/C1_c04.cns" &&
          printf "%s\n" "$J" | campo "d[\"tabla\"]" | grep -q "omega1\[1\] .*(fixed)"'
    mira "caso-estima: la tabla trae la log-verosimilitud y cada slot por su nombre" \
         '"$CLI" caso-estima "$YP" C1 --desde c04 --razon "otra" | grep -q "Log-likelihood ="'
    H=$(cksum < "$YP")
    mira "caso-estima con un slot que no existe: lo dice el motor, y no queda la corrida" \
         '"$CLI" caso-estima "$YP" C1 --desde c04 --restriccion "omega9[1] = 0" 2>&1 | grep -q "unknown parameter .omega9\[1\]." &&
          [ "$H" = "$(cksum < "$YP")" ]'
    mira "caso-estima --restriccion sin =: estado 1, y dice cómo es" \
         '"$CLI" caso-estima "$YP" C1 --desde c04 --restriccion "omega1" 2>&1 | grep -q "NOMBRE = VALOR"'
fi
mira "caso-estima --evaluar sin --prever: estado 1, y lo dice" \
     '"$CLI" caso-estima "$YP" C1 --evaluar 150 2>&1 | grep -q "prever H"'

# los casos de SIM (motor drvarma): la puerta y el VARMA, con lib/sistema --la
# orden de sim_gui-- y lib/outvarma. Sobre una copia de jenkins_alavi, cuyo C1
# es de SIM. Los numeros son los de drvarma sobre esos .pre: la puerta suma
# -579.345662, y el VARMA(1,0) completo da -560.335924, LR = 38.0211 con 3 g.l.
DRV="$R/engines/drvarma/bin/drvarma$EXE"
PS="$WORK/proy_sis"; cp -R "$R/docs/ejemplos/jenkins_alavi" "$PS"; YS="$PS/proyecto.yaml"; rm -f "$PS/costes.csv"
mira "caso-estima de SIM sin la puerta: antes, la puerta (estado 1)" \
     '"$CLI" caso-estima "$YS" C1 --p 1 --q 0 2>&1 | grep -q "caso-puerta"'
mira "caso-puerta de un caso de MT: no es de SIM (estado 1)" \
     '"$CLI" caso-puerta "$YP" C1 2>&1 | grep -q "caso-estima"'
if [ -x "$DRV" ]; then
    J=$("$CLI" --json caso-puerta "$YS" C1 --por asistente)
    mira "caso-puerta: pasa, y la suma es la de drvarma C (-579.345662)" \
         '[ "$(printf "%s\n" "$J" | campo "d[\"pasa\"] and abs(d[\"suma\"] + 579.345662) < 1e-5")" = True ]'
    mira "caso-puerta: las CCF del par, con los retardos fuera de banda" \
         '[ "$(printf "%s\n" "$J" | campo "len(d[\"ccf\"]) == 1 and d[\"ccf\"][0][\"k_pos\"] == \"1\"")" = True ]'
    mira "caso-puerta: un artefacto del caso, no una corrida; su línea en costes.csv" \
         '[ -f "$PS/_casos/C1/work/C1_puerta.out" ] && ! grep -q "C1/puerta:" "$YS" &&
          grep -q ",atsw-cli,asistente,C1/puerta,puerta,drvarma" "$PS/costes.csv"'
    J=$("$CLI" --json caso-estima "$YS" C1 --p 1 --q 0 --razon "el VARMA(1,0)")
    mira "caso-estima de SIM: c00, el VARMA(1,0), logL -560.335924 y LR 38.0211 (3 g.l.)" \
         '[ "$(printf "%s\n" "$J" | campo "d[\"corrida\"] == \"c00\" and abs(d[\"logl\"] + 560.335924) < 1e-5 and abs(d[\"lr\"] - 38.0211) < 1e-3 and d[\"lr_df\"] == 3")" = True ]'
    mira "caso-estima de SIM: la tabla de drvarma, con los cruzados por su nombre" \
         'printf "%s\n" "$J" | campo "d[\"tabla\"]" | grep -q "AR1\[MINK<-MUSKRAT\]"'
    J=$("$CLI" --json caso-estima "$YS" C1 --p 1 --q 0 --enlace "MINK<-MUSKRAT" --desde c00 --razon "un enlace")
    J2=$("$CLI" --json caso-estima "$YS" C1 --p 1 --q 0 --cero "AR1[MUSKRAT<-MINK]" --desde c00 --razon "un cero")
    mira "caso-estima de SIM: --enlace y --cero dan el mismo modelo (c01 y c02, hijas de c00)" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"padre\"], round(d[\"logl\"], 5))")" = "('"'"'c01'"'"', '"'"'c00'"'"', -564.75446)" ] &&
          [ "$(printf "%s\n" "$J2" | campo "(d[\"corrida\"], round(d[\"logl\"], 5))")" = "('"'"'c02'"'"', -564.75446)" ]'
    mira "caso-estima de SIM con un enlace mal escrito: estado 1, y dice cómo es" \
         '"$CLI" caso-estima "$YS" C1 --p 1 --q 0 --enlace "MINK-MUSKRAT" 2>&1 | grep -q "A<-B"'
    mira "caso-estima de SIM: el motor es el de C (drvarma 5), no el de Python" \
         'grep -q "^Program *: drvarma 5" "$PS/_casos/C1/work/C1_c00.out"'
    # simplificar y «¿es una red?» (Tiao y Box), sobre el VARMA(2,0): los
    # numeros de la bateria de sim_gui
    "$CLI" caso-estima "$YS" C1 --p 2 --q 0 --razon "el VARMA(2,0)" > /dev/null
    J=$("$CLI" --json caso-simplifica "$YS" C1 c03 --por asistente)
    mira "caso-simplifica: c04, hija de c03, LR = 0.4761 con 1 g.l. (los ceros se sostienen)" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"padre\"], round(d[\"lr_contra\"], 4), d[\"df_contra\"])")" = "('"'"'c04'"'"', '"'"'c03'"'"', 0.4761, 1)" ]'
    mira "caso-simplifica: los ceros quedan en el .out de la hija" \
         'grep -q "^Zero coefficients:" "$PS/_casos/C1/work/C1_c04.out"'
    mira "caso-simplifica sin nada que simplificar: estado 1, y lo dice" \
         '"$CLI" caso-simplifica "$YS" C1 c00 2>&1 | grep -q "no hay qué simplificar"'
    J=$("$CLI" --json caso-estructura "$YS" C1 c03)
    mira "caso-estructura: dos pares y dos órdenes, y es un sistema simultáneo" \
         '[ "$(printf "%s\n" "$J" | campo "(len(d[\"restricciones\"]), d[\"red\"])")" = "(4, False)" ]'
    J=$("$CLI" --json caso-estructura "$YS" C1 c01 --pasar-mt)
    mira "caso-estructura de un modelo triangular: es una red, y --pasar-mt da de alta el caso de MT" \
         '[ "$(printf "%s\n" "$J" | campo "d[\"red\"] and \"Pasado a MT: el caso C2, con MINK como salida\" in d[\"veredicto\"]")" = True ] &&
          grep -q "padre: C1" "$YS"'
fi
mira "caso-rango con una serie I(0): drvec no la admite (estado 1)" \
     '! "$CLI" caso-rango "$YS" C1 >/dev/null 2>&1'

# el rango, el VEC y la exogeneidad debil (drvec), sobre una copia de
# johansen_juselius (LRM LRY IBO IDE, todas I(1)). Los numeros son los de
# drvec C: el VEC r = 1 normalizado sobre LRM da -324.352224, y la
# exogeneidad debil la de Johansen y Juselius: IBO e IDE no corrigen.
DVC="$R/engines/drvec/bin/drvec$EXE"
PJ="$WORK/proy_vec"; cp -R "$R/docs/ejemplos/johansen_juselius" "$PJ"; YJ="$PJ/proyecto.yaml"; rm -f "$PJ/costes.csv"
if [ -x "$DVC" ] && [ -x "$DRV" ]; then
    J=$("$CLI" --json caso-rango "$YJ" C1 --p 1 --q 0)
    mira "caso-rango sin bootstrap: el ajuste de cada rango, y sin rango propuesto" \
         '[ "$(printf "%s\n" "$J" | campo "(len(d[\"rangos\"]), d[\"propuesto\"], d[\"bootstrap\"])")" = "(4, None, 0)" ]'
    mira "caso-rango sin bootstrap: dice que es orientación y recomienda el bootstrap" \
         '"$CLI" caso-rango "$YJ" C1 --p 1 --q 0 | grep -q "SIN BOOTSTRAP NO HAY RANGO.*RECOMENDADO"'
    mira "caso-rango --bootstrap sin --aparte: va aparte (estado 1)" \
         '"$CLI" caso-rango "$YJ" C1 --p 1 --bootstrap 19 2>&1 | grep -q -- "--aparte"'
    J=$("$CLI" --json caso-rango "$YJ" C1 --p 1 --bootstrap 19 --aparte --por asistente)
    mira "caso-rango --aparte: lanzado, 19 réplicas por cada uno de los 3 contrastes" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"lanzado\"], d[\"total\"])")" = "(True, 57)" ]'
    mira "caso-rango con uno en marcha: no lanza otro (estado 1)" \
         '"$CLI" caso-rango "$YJ" C1 --p 1 --bootstrap 19 --aparte 2>&1 | grep -q "en marcha"'
    mira "caso-rango --rehacer con uno en marcha: tampoco (estado 1)" \
         '"$CLI" caso-rango "$YJ" C1 --p 1 --bootstrap 19 --aparte --rehacer 2>&1 | grep -q "Ni --rehacer"'
    n=0
    until "$CLI" --json caso-rango "$YJ" C1 --estado | grep -q '"terminado": true' || [ $n -ge 120 ]; do sleep 2; n=$((n+2)); done
    J=$("$CLI" --json caso-rango "$YJ" C1 --estado)
    mira "caso-rango --estado: terminado, con el bootstrap y su rango propuesto" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"bootstrap\"], d[\"propuesto\"])")" = "(19, 0)" ]'
    mira "caso-rango: su coste en costes.csv, por el asistente, y sin el fichero del trabajo" \
         'grep -q ",atsw-cli,asistente,C1/rango,rango,DRVEC.*-bootstrap 19" "$PJ/costes.csv" &&
          [ ! -f "$PJ/_casos/C1/work/C1_rango.trabajo" ]'
    mira "caso-rango otra vez: el bootstrap ya hecho no se pisa" \
         '[ "$("$CLI" --json caso-rango "$YJ" C1 --p 1 --q 0 | campo "(d[\"ya_estaba\"], d[\"bootstrap\"])")" = "(True, 19)" ]'
    "$CLI" caso-puerta "$YJ" C1 > /dev/null
    J=$("$CLI" --json caso-estima "$YJ" C1 --r 1 --p 2 --q 0 --normaliza LRM --razon "el VEC r = 1")
    mira "caso-estima --r: c00, el VEC normalizado sobre LRM, logL -324.352224" \
         '[ "$(printf "%s\n" "$J" | campo "(d[\"corrida\"], d[\"clase\"], d[\"normalizacion\"], round(d[\"logl\"], 6))")" = "('"'"'c00'"'"', '"'"'VEC'"'"', '"'"'LRM'"'"', -324.352224)" ]'
    mira "caso-estima --r: la tabla de drvec entera, con alpha, beta y Gamma" \
         'printf "%s\n" "$J" | campo "d[\"tabla\"]" | grep -q "IDE in ec1" &&
          printf "%s\n" "$J" | campo "d[\"tabla\"]" | grep -q "D.LRM <- D.LRM(-1)"'
    mira "caso-estima --r con --enlace: un VEC no lleva enlaces (estado 1)" \
         '"$CLI" caso-estima "$YJ" C1 --r 1 --p 2 --q 0 --enlace "LRM<-IBO" 2>&1 | grep -q "no lleva --enlace"'
    mira "caso-estima --r con una normalización que no es del caso (estado 1)" \
         '"$CLI" caso-estima "$YJ" C1 --r 1 --p 2 --q 0 --normaliza OTRA 2>&1 | grep -q "no es una serie del caso"'
    J=$("$CLI" --json caso-exogeneidad "$YJ" C1 c00)
    mira "caso-simplifica de un VEC: es del VARMA (estado 1)" \
         '"$CLI" caso-simplifica "$YJ" C1 c00 2>&1 | grep -q "no es un VARMA"'
    mira "caso-exogeneidad: IBO e IDE no corrigen; LRY y LRM sí (Johansen y Juselius)" \
         '[ "$(printf "%s\n" "$J" | campo "sorted(x[\"serie\"] for x in d[\"series\"] if x[\"exogena\"])")" = "['"'"'IBO'"'"', '"'"'IDE'"'"']" ]'
fi

# formales: los contrastes del modelo estimado, con las mismas funciones que
# las dos ventanas de la madre (lib/formales). Los numeros son los de art que
# fija lib/formales/test_formales.c para RIPC.2: la simplificacion estacional
# 15.77767809, Shin-Fuller Phi = 1.69946526 (critico 1.75) y la
# sobrediferenciacion LR = 0. Su MEG esta mal condicionado: solo se coteja
# el veredicto, ninguna frecuencia estocastica. SOBRE UNA COPIA del
# proyecto: sus reestimaciones dejan costes.csv, y las pruebas de coste de mas
# abajo empiezan sin el.
PF="$WORK/proy_formales"; cp -R "$P" "$PF"; YF="$PF/proyecto.yaml"; rm -f "$PF/costes.csv"
if [ -x "$FUE" ]; then
    J=$("$CLI" --json formales "$YF" RIPC m01)
    # al fallar dice que vio: el estadistico, si es calculable y por que no
    cerca() { "$PY" -c "
import json, sys
d = json.load(sys.stdin)
c = [x for x in d['contrastes'] if x['clave'] == '$1'][0]
bien = c['estadistico'] is not None and abs(c['estadistico'] - ($2)) < $3
if not bien:
    sys.stderr.write('      se vio: estadistico %s, L %s, L0 %s, calculable %s, dudoso %s, motivo %r\\n'
                     % (c['estadistico'], c['l_libre'], c['l_restringido'], c['calculable'],
                        c['dudoso'], c['motivo']))
sys.exit(0 if bien else 1)"; }
    mira "formales: la simplificación estacional, LR 15.7777 (art)" \
         'echo "$J" | cerca estacional 15.77767809 1e-5'
    mira "formales: Shin-Fuller, Phi 1.6995 (art)" 'echo "$J" | cerca sf 1.69946526 1e-5'
    mira "formales: la sobrediferenciación, LR 0 (art)" \
         'echo "$J" | cerca sobre 0 1e-6'
    mira "formales: el MEG, ninguna frecuencia estocástica (art)" \
         '[ "$(printf "%s\n" "$J" | campo "(sum(1 for x in d[\"contrastes\"] if x[\"clave\"] == \"meg\"), sum(1 for x in d[\"contrastes\"] if x[\"clave\"] == \"meg\" and x[\"calculable\"] and x[\"rechaza\"]))")" = "(6, 0)" ]'
    mira "formales: lo que no aplica, dicho (sin MA regular, no hay DCD)" \
         'printf "%s\n" "$J" | campo "d[\"no_aplica\"]" | grep -q "DCD:"'
    mira "formales: cada reestimación, en costes.csv por atsw-cli" \
         '[ "$(grep -c ",atsw-cli,analista,RIPC/m01,contraste," "$PF/costes.csv")" -ge 2 ] &&
          grep -q ",atsw-cli,analista,RIPC/m01,meg," "$PF/costes.csv"'
    J=$("$CLI" --json formales "$YF" RIPC m01 --familia integracion)
    mira "formales --familia integracion: solo los suyos" \
         '[ "$(printf "%s\n" "$J" | campo "all(x[\"familia\"] == \"integracion\" for x in d[\"contrastes\"]) and len(d[\"contrastes\"]) == 2")" = True ]'
    mira "formales --sub con d = 0: no hay diferencia que cuestionar, y lo dice" \
         '"$CLI" formales "$YF" RIPC m01 --familia integracion --sub 2>&1 >/dev/null | grep -q "d = 0"'
fi
mira "formales --familia estacionalidad de una serie sin estacionalidad: estado 1" \
     '! "$CLI" formales "$YF" en4 m01 --familia estacionalidad 2>/dev/null'
mira "formales --familia mal escrita: estado 1, y dice por qué" \
     '"$CLI" formales "$YF" RIPC m01 --familia otra 2>&1 | grep -q "integracion o estacionalidad"'

# coste: el MTM apunta lo que corre en Python, y el resumen lo suma. Hasta
# aqui nada ha corrido un motor con contexto: el registro no existe.
C="$P/costes.csv"
mira "coste: sin corridas no hay costes.csv" '[ ! -f "$C" ]'
mira "coste resumen: sin registro, cero grupos" \
     '[ "$("$CLI" --json coste resumen "$Y" | campo "len(d[\"grupos\"])")" = 0 ]'
"$CLI" coste apunta "$Y" --quien mtm --objeto RIPC/m01 --paso estimar \
       --motor "art 0.9" --via python --cpu 1.5 --pared 2 >/dev/null
"$CLI" coste apunta "$Y" --objeto en4/m01 --paso meg --motor "fue 1.14" \
       --via C --cpu 0.5 --pared 0.75 --energia 3 --metodo "medido: RAPL" >/dev/null
mira "coste apunta: la cabecera y dos líneas" \
     '[ "$(head -1 "$C" | cut -d, -f1-3)" = "fecha,quien,por" ] && [ $(wc -l < "$C") = 3 ]'
mira "coste apunta: sin energía, se estima y se dice cómo" \
     'sed -n 2p "$C" | grep -q "cpu×TDP/núcleos"'
mira "coste apunta: quien atsw-cli y por analista, si no se dice" \
     'sed -n 3p "$C" | grep -q ",atsw-cli,analista,en4/m01,meg,"'
J=$("$CLI" --json coste resumen "$Y" --por-que serie)
mira "coste resumen por serie: RIPC y en4, con sus tiempos" \
     '[ "$(printf "%s\n" "$J" | campo "[(g[\"clave\"], g[\"corridas\"], g[\"cpu_s\"]) for g in d[\"grupos\"]]")" = "[('"'"'RIPC'"'"', 1, 1.5), ('"'"'en4'"'"', 1, 0.5)]" ]'
mira "coste resumen total: dos corridas, 2 s de CPU" \
     '[ "$("$CLI" --json coste resumen "$Y" | campo "d[\"grupos\"][0][\"corridas\"] == 2 and d[\"grupos\"][0][\"cpu_s\"] == 2")" = True ]'
mira "coste apunta: una energía sin su método no se apunta (estado 1)" \
     '! "$CLI" coste apunta "$Y" --paso x --motor y --cpu 1 --pared 1 --energia 2 2>/dev/null'
mira "coste apunta: un tiempo que no es un número (estado 1)" \
     '! "$CLI" coste apunta "$Y" --paso x --motor y --cpu 1,5 --pared 1 2>/dev/null'
mira "coste resumen: un agrupamiento que no existe (estado 1)" \
     '! "$CLI" coste resumen "$Y" --por-que color 2>/dev/null'
mira "coste: lo rechazado no se escribe" '[ $(wc -l < "$C") = 3 ]'

# --por: quien dio el paso (ESTUDIO-terminal-mcp §7.2), y la ficha de
# confianza (§7.8), sobre un linaje de prueba en otro proyecto: m01 del
# analista, m02 del asistente, m03 del analista y sin razon, m04 del
# asistente. «Estimar» es copiar el .pre del golden: la ficha no lo distingue.
L="$WORK/lin"
mkdir -p "$L/RIPC/work"
cp "$G/RIPC.2/RIPC.2.pre" "$L/RIPC/work/RIPC_m00.inp"
cat > "$L/proyecto.yaml" <<'YAML'
schema_version: 2
id: linaje
titulo: "la ficha"
raiz: .

series:
  RIPC:

modelos:
  RIPC/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
YAML
YL="$L/proyecto.yaml"
W="$L/RIPC/work"
"$CLI" proyecto nuevo "$YL" RIPC --razon "airline de partida" >/dev/null
cp "$G/RIPC.2/RIPC.2.pre" "$W/RIPC_m01.pre"
"$CLI" proyecto itera "$YL" RIPC m01 --por asistente --razon "el residuo pide el 12" >/dev/null
cp "$G/RIPC.2/RIPC.2.pre" "$W/RIPC_m02.pre"
"$CLI" proyecto itera "$YL" RIPC m02 --por analista >/dev/null
cp "$G/RIPC.2/RIPC.2.pre" "$W/RIPC_m03.pre"
"$CLI" proyecto itera "$YL" RIPC m03 --por asistente --razon "el MA(12) sobra" >/dev/null
cp "$G/RIPC.2/RIPC.2.pre" "$W/RIPC_m04.pre"
cp "$G/RIPC.2/RIPC.2.out" "$W/RIPC_m04.out"
"$CLI" proyecto elige "$YL" RIPC m04 "el mejor" >/dev/null

mira "--por: dos pasos del asistente en el manifiesto, y el analista sin marca" \
     '[ $(grep -c "^    por: asistente" "$YL") = 2 ] && ! grep -q "por.*analista" "$YL"'
V=$("$CLI" --json proyecto ver "$YL")
mira "--por: proyecto ver --json lo dice, paso a paso" \
     '[ "$(echo "$V" | campo "[m[\"por\"] for m in d[\"modelos\"] if not m[\"datos\"]]")" = "['"'"'analista'"'"', '"'"'asistente'"'"', '"'"'analista'"'"', '"'"'asistente'"'"']" ]'
mira "--por: y en texto, con su marca" \
     '[ $("$CLI" proyecto ver "$YL" | grep -c "✦ asistente") = 2 ]'
mira "proyecto ver: la razón de la elección, en texto (guion_map la lee)" \
     '"$CLI" proyecto ver "$YL" | grep -qx "      elegido: el mejor"'
mira "proyecto ver --json: razon_elegido solo en el elegido" \
     '[ "$(echo "$V" | campo "[m[\"razon_elegido\"] for m in d[\"modelos\"] if not m[\"datos\"]]")" = "['"'"''"'"', '"'"''"'"', '"'"''"'"', '"'"'el mejor'"'"']" ]'
mira "--por: otro valor no se apunta (estado 1)" \
     '! "$CLI" proyecto nuevo "$YL" RIPC --por nadie 2>/dev/null && ! grep -q "nadie" "$YL"'

M=$("$CLI" ficha "$YL" RIPC m04)
mira "ficha: la parte del asistente, bien contada" \
     'echo "$M" | grep -q "^\*\*Recorrido\*\* (4 pasos; 2 del asistente, 50 %; elegido por el analista)"'
mira "ficha: el recorrido, de los datos al modelo, con quién y por qué" \
     'echo "$M" | grep -q "^m00 datos → m01 analista «airline de partida» → m02 asistente «el residuo pide el 12» → m03 analista (sin razón) → m04 asistente «el MA(12) sobra»$"'
mira "ficha: la adecuación sale del dictamen" \
     'echo "$M" | grep -q "^- ✓ Estimación: convergió"'
mira "ficha: lo no hecho dice «no consta»" \
     'echo "$M" | grep -q "^- — estabilidad: no consta" && echo "$M" | grep -q "^\*\*Coste\*\*  no consta$"'
mira "ficha: sin comprobar, la reproducibilidad no consta" \
     'echo "$M" | grep -q "^\*\*Reproducibilidad\*\*  no consta: pídela con atsw-cli ficha --comprobar$"'
mira "ficha: un modelo sin .out no está estimado, y lo dice" \
     '"$CLI" ficha "$YL" RIPC m03 | grep -q "no consta: el modelo no está estimado" && "$CLI" ficha "$YL" RIPC m03 | grep -q "no es el elegido"'
J=$("$CLI" --json ficha "$YL" RIPC m04)
# en bytes y en UTF-8: el python de Windows lee stdin en cp1252
printf '%s\n' "$M" > "$WORK/md"
mira "ficha --json: el mismo Markdown que el texto, byte a byte" \
     '[ "$(printf "%s\n" "$J" | "$PY" -c "import json,sys; d=json.loads(sys.stdin.buffer.read().decode(\"utf-8\")); sys.stdout.write(str(d[\"markdown\"] == open(sys.argv[1], encoding=\"utf-8\").read()))" "$WORK/md")" = True ]'
mira "ficha --json: 4 pasos, 2 del asistente, sin comprobación" \
     '[ "$(printf "%s\n" "$J" | campo "(d[\"pasos\"], d[\"del_asistente\"], d[\"reproducibilidad\"][\"se_reproduce\"])")" = "(4, 2, None)" ]'
"$CLI" proyecto elige "$YL" RIPC m04 "el mejor" --por asistente >/dev/null
mira "elige --por asistente: elegido por el asistente" \
     '"$CLI" ficha "$YL" RIPC m04 | grep -q "elegido por el asistente)" && grep -q "^    por_elegido: asistente" "$YL"'
"$CLI" proyecto elige "$YL" RIPC m04 "el mejor" >/dev/null
mira "elige sin --por: vuelve a ser del analista, sin marca" \
     '! grep -q "por_elegido" "$YL"'

if [ -x "$FUE" ]; then
    SHA4=$("$PY" -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$W/RIPC_m04.pre")
    M=$("$CLI" ficha "$YL" RIPC m04 --comprobar)
    mira "ficha --comprobar: un .pre intacto se reproduce" \
         'echo "$M" | grep -q "^\*\*Reproducibilidad\*\*  ✓ se reproduce con fue [0-9.]* (C), comprobado el [0-9]*-[a-z]*-20[0-9][0-9]$"'
    mira "ficha --comprobar: en costes.csv, reproducir:si con el sha del .pre" \
         'tail -1 "$L/costes.csv" | grep -q ",atsw-cli,analista,RIPC/m04,reproducir:si,fue [0-9.]*,C,.*,$SHA4\$"'
    mira "ficha --comprobar: con su CPU, que no es negativa, y su energía con método" \
         '[ "$(tail -1 "$L/costes.csv" | "$PY" -c "import csv,sys; r=next(csv.reader(sys.stdin)); print(float(r[7]) >= 0 and float(r[9]) >= 0 and \"TDP\" in r[10])")" = True ]'
    mira "ficha: y el coste del recorrido ya consta" \
         'echo "$M" | grep -q "el recorrido, [0-9]*,[0-9][0-9] s de CPU en 1 corrida"'
    # un .pre retocado a mano: un parametro de 0.955890 a 0.5
    sed '50s/^0\.955890/0.500000/' "$G/RIPC.2/RIPC.2.pre" > "$W/RIPC_m03.pre"
    mira "el retoque está hecho" '! cmp -s "$W/RIPC_m03.pre" "$G/RIPC.2/RIPC.2.pre"'
    SHA3=$("$PY" -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$W/RIPC_m03.pre")
    J=$("$CLI" --json ficha "$YL" RIPC m03 --comprobar --por asistente)
    mira "ficha --comprobar: un .pre retocado a mano no se reproduce" \
         '[ "$(printf "%s\n" "$J" | campo "d[\"comprobado\"][\"se_reproduce\"]")" = False ] && printf "%s\n" "$J" | campo "d[\"markdown\"]" | grep -q "✗ no se reproduce con fue"'
    mira "ficha --comprobar: y dice qué token se movió" \
         'printf "%s\n" "$J" | campo "d[\"comprobado\"][\"diferencia\"]" | grep -q "^línea 50: 0.500000 → "'
    mira "ficha --comprobar: reproducir:no, por el asistente, con el sha del retocado" \
         'tail -1 "$L/costes.csv" | grep -q ",asistente,RIPC/m03,reproducir:no,.*,$SHA3\$"'
    # OTRO .pre ES OTRO HASH: la comprobación de antes no vale para él
    printf ' \n' >> "$W/RIPC_m04.pre"
    mira "ficha: tocar el .pre deja la comprobación sin valor (no consta)" \
         '"$CLI" ficha "$YL" RIPC m04 | grep -q "^\*\*Reproducibilidad\*\*  no consta"'
fi
"$CLI" coste apunta "$YL" --por asistente --quien mtm --objeto RIPC/m02 --paso estimar \
       --motor "art 0.9" --via python --cpu 2 --pared 3 \
       --sha-pre "$("$PY" -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$W/RIPC_m02.pre")" >/dev/null
M=$("$CLI" ficha "$YL" RIPC m02)
mira "ficha: la estimación, con su motor y su vía" \
     'echo "$M" | grep -q "^\*\*Coste\*\*  la estimación, 2,00 s de CPU (art 0.9, python), ~"'
mira "ficha: el .pre que vino del extra Python lo dice" \
     'echo "$M" | grep -q "el .pre salió del extra Python (art 0.9)"'

# --- el proyecto de ejemplo de Ayuda -> Ejemplos (docs/ejemplos) ------------
# Lo que la madre copia tiene que estar entero: el manifiesto se lee, el caso
# es de SIM con el sha256 de cada .pre, cada .pre es un optimo (fue lo
# reproduce) y los datos de m00 son los del datos.csv.
for X in jenkins_alavi:MUSKRAT,MINK:62 johansen_juselius:LRM,LRY,IBO,IDE:55; do
EJN=${X%%:*}; SER=$(echo "$X" | cut -d: -f2 | tr , ' '); NOBS=${X##*:}
EJ="$WORK/$EJN"
cp -R "$R/docs/ejemplos/$EJN" "$EJ"
V=$("$CLI" --json proyecto ver "$EJ/proyecto.yaml")
mira "ejemplo $EJN: el caso C1 es de drvarma (SIM)" \
     '[ "$(echo "$V" | campo "[c[\"motor\"] for c in d[\"casos\"] if c[\"id\"]==\"C1\"][0]")" = drvarma ]'
mira "ejemplo $EJN: los m01 de $SER, elegidos" 'echo "$V" | "$PY" -c "
import json, sys
d = json.load(sys.stdin)
sys.exit(0 if sorted(m[\"serie\"] for m in d[\"modelos\"] if m[\"elegido\"] and m[\"id\"] == \"m01\") == sorted(sys.argv[1:]) else 1)
" $SER'
mira "ejemplo $EJN: el sha256 de cada entrada es el de su .pre" '"$PY" - "$EJ" <<"PYEOF"
import hashlib, re, sys, os
d = sys.argv[1]
y = open(os.path.join(d, "proyecto.yaml"), encoding="utf-8").read()
ent = re.search(r"entradas: (.*)", y).group(1).split()
for e in ent:
    sm, sha = e.split("@"); s, m = sm.split("/")
    f = os.path.join(d, s, "work", "%s_%s.pre" % (s, m))
    if hashlib.sha256(open(f, "rb").read()).hexdigest() != sha: sys.exit(1)
PYEOF'
for s in $SER; do
    mkdir -p "$WORK/ej_$s"
    cp "$EJ/$s/work/${s}_m01.pre" "$WORK/ej_$s/X.inp"
    ( cd "$WORK/ej_$s" && "$FUE" X > /dev/null 2>&1 )
    mira "ejemplo $EJN: fue reproduce el .pre de $s/m01" \
         '[ -f "$WORK/ej_$s/X.pre" ] && "$PY" - "$WORK/ej_$s/X.pre" "$EJ/$s/work/${s}_m01.pre" <<"PYEOF"
# numero a numero, con tolerancia: fuera de Linux el ultimo digito baila, y
# en un optimo plano (LRM de johansen_juselius) reestimar mueve una unidad la
# sexta decimal, que es la ultima que escribe el .pre
import re, sys
num = lambda f: [float(x) for x in re.findall(r"-?\d+\.?\d*(?:[eE][-+]?\d+)?", open(f).read())]
a, b = num(sys.argv[1]), num(sys.argv[2])
sys.exit(0 if len(a) == len(b) and all(abs(x - y) <= 2e-6 * max(1.0, abs(y)) for x, y in zip(a, b)) else 1)
PYEOF'
    mira "ejemplo $EJN: m00 de $s son los datos de su datos.csv" '"$PY" - "$EJ" "$s" "$NOBS" <<"PYEOF"
import sys, os
d, s, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
csv = [float(l.split(",")[1]) for l in open(os.path.join(d, s, "datos.csv"))
       if l.strip() and not l.startswith("#") and l[0].isdigit()]
inp = open(os.path.join(d, s, "work", s + "_m00.inp")).read().split("Time series")[1]
inp = [float(x) for x in inp.split("\n", 1)[1].split()]
sys.exit(0 if len(csv) == len(inp) == n and all(abs(a - b) <= 1e-9 * max(1.0, abs(b)) for a, b in zip(csv, inp)) else 1)
PYEOF'
    rm -rf "$WORK/ej_$s"
done
done

# los errores
J=$("$CLI" --json dictamen "$Y" RIPC m99 2>/dev/null); rc=$?
mira "un modelo que no está: estado 1 y {ok: false} con el porqué" \
     '[ $rc = 1 ] && [ "$(printf "%s\n" "$J" | campo "d[\"ok\"]")" = False ]'
"$CLI" proyecto bla >/dev/null 2>&1; rc=$?
mira "una orden mal escrita: estado 2" '[ $rc = 2 ]'
"$CLI" --nada version >/dev/null 2>&1; rc=$?
mira "una opción desconocida: estado 2" '[ $rc = 2 ]'

# EL MANIFIESTO ENTRE PROCESOS (docs/AUDITORIA-procesos.md, fase 1): 30
# atsw-cli A LA VEZ, cada uno con la razon de un modelo distinto. Cada uno lee,
# cambia y escribe; antes de la escritura transaccional, alguno escribia su
# copia encima de la de otro y se perdian razones (medido: en 4 de 10 vueltas).
# Ahora tienen que estar las 30, sin temporal que se quede (el .cerrojo se
# queda: es el fichero sobre el que el sistema bloquea).
C="$WORK/conc"; mkdir -p "$C"
{
  printf 'schema_version: 1\nid: conc\ntitulo: concurrencia\ncreado: "2026-11-14"\n'
  printf 'analista: ""\nherramienta: fue\nraiz: .\n\nseries:\n  IPC:\n\nmodelos:\n'
  printf '  IPC/m00:\n    version: 0\n    padre: ""\n    razon: ""\n    rol: datos\n'
  for i in $(seq 1 30); do
    printf '  IPC/m%02d:\n    version: %d\n    padre: m00\n    razon: ""\n' "$i" "$i"
  done
} > "$C/p.yaml"
for i in $(seq 1 30); do
  ( "$CLI" proyecto razon "$C/p.yaml" IPC "$(printf m%02d "$i")" "razon $i" \
        > /dev/null 2> "$C/err.$i"; echo $? > "$C/rc.$i" ) &
done
wait
n=$(grep -c 'razon: "*razon [0-9]' "$C/p.yaml")
# LA REGLA ES QUE NADA SE PIERDA EN SILENCIO: un proceso puede no escribir (el
# cerrojo no llega a tiempo, por ejemplo), pero entonces lo dice y sale con
# error. Lo que no puede pasar es que diga que si y su razon no este.
callados=""; avisan=""
for i in $(seq 1 30); do
  if [ "$(cat "$C/rc.$i")" = 0 ]; then
    grep -q "razon: \"*razon $i\"*\$" "$C/p.yaml" || callados="$callados m$i"
  else
    avisan="$avisan m$i($(tr '\n' ' ' < "$C/err.$i"))"
  fi
done
[ -n "$avisan" ] && echo "  los que avisaron:$avisan"
mira "30 atsw-cli a la vez: ninguno dice que escribio y se pierde${callados:+ (se perdieron:$callados)}" \
     '[ -z "$callados" ]'
mira "30 atsw-cli a la vez sobre el mismo manifiesto: estan las 30 razones ($n de 30)" \
     '[ "$n" = 30 ]'
rm -f "$C"/rc.* "$C"/err.*
mira "y no queda ningún temporal" \
     '[ -z "$(ls "$C" | grep -E "\.tmp$")" ]'

echo
if [ $fallos -eq 0 ]; then echo "atsw-cli: todo bien"; exit 0; fi
echo "atsw-cli: $fallos fallos"
exit 1
