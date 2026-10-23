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
