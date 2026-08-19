#!/bin/bash
# tools/measure_seeding_bank.sh — la linea base de la SIEMBRA, sobre el banco.
#
# PARA QUE.  El paso 3 del plan de empotrado (docs/VEC_EMBEDDING_PLAN.md 8)
# cambia de donde arranca el ajuste con r >= 1.  Un cambio de arranque no se
# juzga por una corrida: se juzga contra una linea base tomada ANTES, sobre un
# banco fijado ANTES, con las mismas cantidades a los dos lados.  Este script es
# esa medida, y por eso sirve las dos veces -- hoy para (C), lo que se hace
# ahora, y despues para (B) sin mas que pasarle la opcion que la active.
#
#   tools/measure_seeding_bank.sh [opciones extra para drvec]
#
# Cinco cantidades por corrida, que son las que el plan 7 fija:
#
#   logL0   la verosimilitud EN EL PUNTO DE PARTIDA (-eval).  Es lo que separa
#           una semilla mala -- arranca peor -- de un optimizador que desde una
#           semilla mejor acaba peor, que es la superficie.  Sin ella, comparar
#           logL finales no distingue las dos cosas.
#   logL    la verosimilitud al parar
#   term    en que criterio paro: grad (gradiente, el bueno), step (el paso se
#           agoto), lower (la busqueda lineal no mejoro), its/len (se rindio)
#   best    con -multistart N, EN QUE ARRANQUE esta el mejor punto.  best = 1
#           dice que la siembra fria ya llegaba; best = 17 dice que no, y cuanto
#           costo.  Se anota tambien el rango logL peor..mejor: en una superficie
#           sana todos los arranques caen en el mismo sitio.
#   MAmin   el modulo de la raiz MA mas pequena en el optimo.  1.0000 es la
#           frontera de invertibilidad: ahi el optimo es RESTRINGIDO y los
#           errores estandar no valen en esa direccion.
#
# El banco esta escrito aqui dentro a proposito y no se pasa por argumento: un
# banco que cambia entre dos medidas no compara nada.
set -u
cd "$(dirname "$0")/.." || exit 1

DRVEC=${DRVEC:-bin/drvec}
MS=${MS:-20}                       # arranques del multiarranque
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
EXTRA=("$@")

[ -x "$DRVEC" ] || { echo "ERROR: $DRVEC no esta construido; corre make"; exit 1; }

# nombre|fichero.inp|argumentos de especificacion
BANK="
Milan|data/pairs/milan.inp|2 1 1 -case 2 -mean
Strasbourg|data/pairs/strasbourg.inp|2 1 1 -case 2 -mean
Utrecht|data/pairs/utrecht.inp|2 1 1 -case 2 -mean
Vienna|data/pairs/vienna.inp|2 1 1 -case 2 -mean
Aix|data/pairs/aix.inp|2 1 1 -case 2 -mean
Arevalo|data/pairs/arevalo.inp|2 1 1 -case 2 -mean
Angers|data/pairs/angers.inp|2 1 1 -case 2 -mean
Penn|data/pairs/penn.inp|2 1 1 -case 2 -mean
mink_muskrat c1|datasets/mauricio/mink_muskrat.inp|2 1 1 -case 1
mink_muskrat c2|datasets/mauricio/mink_muskrat.inp|2 1 1 -case 2 -mean
mink_muskrat c3|datasets/mauricio/mink_muskrat.inp|2 1 1 -case 3 -mean
rank2 r=2 (truth)|datasets/synthetic/rank2.inp|2 0 2 -case 2 -mean
rank2 r=1|datasets/synthetic/rank2.inp|2 0 1 -case 2 -mean
rank0 r=1 (no rank)|datasets/synthetic/rank0.inp|2 0 1 -case 2 -mean
badnorm r=1|datasets/synthetic/badnorm.inp|2 0 1 -case 2 -mean
"

printf '| case | spec | logL0 | logL | term | best/%d | logL spread | MAmin |\n' "$MS"
printf '|---|---|---|---|---|---|---|---|\n'

while IFS='|' read -r name file spec; do
    [ -z "$name" ] && continue
    cp "$file" "$TMP/c.inp"

    # 1. el punto de partida
    "$DRVEC" "$TMP/c" $spec "${EXTRA[@]+"${EXTRA[@]}"}" -eval >/dev/null 2>&1
    ll0=$(awk '/^eval logelf/{print $4}' "$TMP/c.out" 2>/dev/null)
    [ -z "$ll0" ] && ll0="--"

    # 2. el ajuste, su terminacion y la raiz MA
    "$DRVEC" "$TMP/c" $spec "${EXTRA[@]+"${EXTRA[@]}"}" >/dev/null 2>&1
    ll=$(awk '/^logelf/{print $3}' "$TMP/c.out" 2>/dev/null)
    [ -z "$ll" ] && ll="--"
    term=$(awk '/Convergence criterion:/{
                  if ($0 ~ /gradtol/)              t="grad";
                  else if ($0 ~ /steptol/)         t="step";
                  else if ($0 ~ /lower point/)     t="lower";
                  else if ($0 ~ /iteration limit/) t="its";
                  else if ($0 ~ /maximum length/)  t="len";
                  else                             t="?" } END{print (t=="")?"--":t}' \
           "$TMP/c.out" 2>/dev/null)
    mam=$(awk '/MA \(Theta\)/{m="";
                 for(i=3;i<=NF;i++) if ($i+0>0 && (m=="" || $i+0<m)) m=$i+0;
                 print (m=="")?"--":sprintf("%.4f", m)}' "$TMP/c.out" 2>/dev/null)
    [ -z "$mam" ] && mam="n/a"

    # 3. cuantos arranques hacen falta para llegar al mejor punto
    "$DRVEC" "$TMP/c" $spec "${EXTRA[@]+"${EXTRA[@]}"}" -multistart "$MS" \
        >/dev/null 2>&1
    ms=$(grep -a "^Multi-start:" "$TMP/c.out" 2>/dev/null | head -1)
    best=$(printf '%s' "$ms" | sed -n 's/.*best is start \([0-9]*\).*/\1/p')
    spread=$(printf '%s' "$ms" | sed -n 's/.*logL from \([-0-9.]*\) to \([-0-9.]*\).*/\1 .. \2/p')
    nok=$(printf '%s' "$ms" | sed -n 's/^Multi-start: \([0-9]*\) of.*/\1/p')
    [ -z "$best" ] && best="--"
    [ -z "$spread" ] && spread="--"
    [ -n "$nok" ] && [ "$nok" != "$MS" ] && best="$best ($nok ok)"

    printf '| %s | `%s` | %s | %s | %s | %s | %s | %s |\n' \
           "$name" "$spec" "$ll0" "$ll" "$term" "$best" "$spread" "$mam"
done <<< "$BANK"
