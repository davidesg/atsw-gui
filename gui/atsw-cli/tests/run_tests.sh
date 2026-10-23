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

# dictamen: el mismo de la ventana (dx_dictamen sobre el .out)
J=$("$CLI" --json dictamen "$Y" RIPC m01)
mira "dictamen --json: se lee y trae sus líneas" \
     '[ "$(echo "$J" | campo "d[\"ok\"] and len(d[\"lineas\"]) >= 5")" = True ]'
mira "dictamen: la estimación convergió" \
     '[ "$(echo "$J" | campo "d[\"lineas\"][0][\"estado\"]")" = cuadra ]'

# raices: los números de test_raices (art.roots)
J=$("$CLI" --json raices factores "$Y" en4 m01)
mira "raices factores: el AR(18) en nueve pares" \
     '[ "$(echo "$J" | campo "len(d[\"factores\"][0][\"partes\"])")" = 9 ]'
mira "raices factores: el par más cercano, d = 0.948, periodo 2.40" \
     '[ "$(echo "$J" | campo "round(d[\"factores\"][0][\"partes\"][0][\"d\"],3) == 0.948 and round(d[\"factores\"][0][\"partes\"][0][\"periodo\"],2) == 2.4")" = True ]'

# itera: el .pre del padre, byte a byte, al .inp del hijo, con su razón
J=$("$CLI" --json proyecto itera "$Y" RIPC m01 --razon "seguir desde el óptimo")
H=$(echo "$J" | campo "d[\"modelo\"]")
mira "itera: nace m02" '[ "$H" = m02 ]'
mira "itera: el .inp es el .pre del padre" 'cmp -s "$P/RIPC/work/RIPC_m01.pre" "$P/RIPC/work/RIPC_m02.inp"'
mira "itera: la ruta del JSON es absoluta y es la del .inp" \
     '[ -f "$(echo "$J" | campo "d[\"inp\"]")" ]'
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
mira "caso-nuevo: C1" '[ "$(echo "$J" | campo "d[\"caso\"]")" = C1 ]'
SHA=$("$PY" -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$P/RIPC/work/RIPC_m01.pre")
mira "caso-nuevo: el sha del .pre en el manifiesto" 'grep -q "$SHA" "$Y"'
mira "caso-nuevo: un modelo sin estimar no entra" \
     '! "$CLI" proyecto caso-nuevo "$Y" RIPC/m03 en4/m01 2>/dev/null'

# raices factorizado: los nueve AR(2), y fue los estima con la misma
# verosimilitud que el AR(18)
J=$("$CLI" --json raices factorizado "$Y" en4 m01)
F=$(echo "$J" | campo "d[\"inp\"]")
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
     '! "$CLI" raices factorizado "$Y" en4 "$(echo "$J" | campo "d[\"modelo\"]")" 2>/dev/null'

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
     '[ "$(echo "$J" | campo "[(g[\"clave\"], g[\"corridas\"], g[\"cpu_s\"]) for g in d[\"grupos\"]]")" = "[('"'"'RIPC'"'"', 1, 1.5), ('"'"'en4'"'"', 1, 0.5)]" ]'
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

# los errores
J=$("$CLI" --json dictamen "$Y" RIPC m99 2>/dev/null); rc=$?
mira "un modelo que no está: estado 1 y {ok: false} con el porqué" \
     '[ $rc = 1 ] && [ "$(echo "$J" | campo "d[\"ok\"]")" = False ]'
"$CLI" proyecto bla >/dev/null 2>&1; rc=$?
mira "una orden mal escrita: estado 2" '[ $rc = 2 ]'
"$CLI" --nada version >/dev/null 2>&1; rc=$?
mira "una opción desconocida: estado 2" '[ $rc = 2 ]'

echo
if [ $fallos -eq 0 ]; then echo "atsw-cli: todo bien"; exit 0; fi
echo "atsw-cli: $fallos fallos"
exit 1
