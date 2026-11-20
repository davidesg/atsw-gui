#!/bin/sh
# rutas.sh -- la bateria de las rutas U y V: los VAR monetarios.
#
#   sh tests/rutas/rutas.sh             estima y compara  (desde engines/drvarma)
#   sh tests/rutas/rutas.sh --generar   reescribe ref/hechos.tsv
#   RUTAS_FILTRO=GS1 sh ...             solo los casos cuya clave casa
#   RUTAS_JOBS=4                        corridas a la vez (por defecto, los nucleos)
#
# Son estimaciones largas (40 a 80 parametros, hasta 200 iteraciones): la
# bateria entera tarda minutos, y por eso NO esta en `make check`. Se corre
# antes y despues de todo cambio en el motor.
#
# Comprueba dos cosas distintas:
#
#  1. HECHOS: de cada estimacion, el codigo de salida, el numero de
#     parametros y la log-verosimilitud (tolerancia RUTAS_TOL, 1e-4), contra
#     ref/hechos.tsv. El estado del optimizador (CONVERGED, STOPPED, WALL) y
#     las iteraciones se anotan y se avisan si cambian, sin fallar: dependen
#     de la plataforma.
#
#  2. PROPIEDADES, que no dependen de ninguna referencia:
#     anida      un VAR(p) no queda por debajo del VAR(p-1) (mismo sistema,
#                misma ruta, mismo arranque);
#     arranques  -start zero y -start ols llegan al mismo optimo, y ninguno
#                falla donde el otro estima.
#     Las que hoy no se cumplen estan en abiertos.txt, cada una con su parte.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
TESTS=$(cd "$AQUI/.." && pwd)
RAIZ=$(cd "$AQUI/../.." && pwd)
BIN=${DRVARMA:-$RAIZ/bin/drvarma}
MODO=${1:-comparar}
JOBS=${RUTAS_JOBS:-$( (nproc || sysctl -n hw.ncpu) 2>/dev/null || echo 2)}
[ -x "$BIN" ] || { echo "no encuentro $BIN (make)"; exit 2; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# una corrida: clave y orden de drvarma, a un fichero de hechos
cat > "$TMP/una.sh" <<'UNA'
#!/bin/sh
# $1 bin  $2 dir  $3 clave  $4.. argumentos de drvarma
bin=$1; d=$2; k=$3; shift 3
mkdir -p "$d/$k"; cd "$d/$k" || exit 2
"$bin" "$@" -o r > r.log 2>&1; rc=$?
o=r.out; [ -f "$o" ] || o=/dev/null
npar=$(awk '/^Number of parameters/ {print $NF; exit}' "$o")
logl=$(awk '/^Exact log-likelihood/ {print $NF; exit}' "$o")
its=$(sed -n 's/.*OPTIMIZER .* after \([0-9]*\) iterations.*/\1/p' "$o" | head -1)
est=FAILED
grep -aq 'OPTIMIZER CONVERGED' "$o" && est=CONVERGED
grep -aq 'OPTIMIZER STOPPED' "$o" && est=STOPPED
grep -aq 'OPTIMIZER STOPPED at the MA invertibility boundary' "$o" && est=WALL
arr=zero
grep -aq '^Start of the cross terms: ols' "$o" && arr=ols
grep -aq '^Start of the cross terms: zero (the ols start was not admissible)' "$o" && arr=zero-caida
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$k" "$rc" "${npar:--}" "${logl:--}" "$est" "${its:--}" "$arr" > "$d/$k.hecho"
UNA

grep -v '^#' "$AQUI/casos.txt" | grep -v '^[[:space:]]*$' |
while IFS='|' read -r sis ruta pq fich opc; do
    sis=$(echo $sis); ruta=$(echo $ruta); set -- $pq
    abs=""; for f in $fich; do abs="$abs $TESTS/$f"; done
    for st in zero ols; do
        echo "$sis-$ruta-p$1q$2-$st $abs $1 $2 -start $st $opc"
    done
done | grep -- "${RUTAS_FILTRO:-}" > "$TMP/ordenes"
n=$(wc -l < "$TMP/ordenes" | tr -d ' ')
echo "rutas: $n estimaciones, $JOBS a la vez"
tr '\n' '\0' < "$TMP/ordenes" | xargs -0 -n 1 -P "$JOBS" sh -c 'set -- $0; sh "'"$TMP"'/una.sh" "'"$BIN"'" "'"$TMP"'" "$@"'
cat "$TMP"/*.hecho | sort > "$TMP/hechos.tsv"

if [ "$MODO" = --generar ]; then
    { printf '# clave\trc\tnpar\tlogL\testado\tits\tarranque\n'; cat "$TMP/hechos.tsv"; } > "$AQUI/ref/hechos.tsv"
    echo "ref/hechos.tsv reescrito ($n estimaciones)"
fi

python3 - "$TMP/hechos.tsv" "$AQUI/ref/hechos.tsv" "$AQUI/abiertos.txt" "${RUTAS_TOL:-1e-4}" "${RUTAS_FILTRO:-}" <<'PY'
import sys, re
nuevo, ref, abiertos, tol, filtro = sys.argv[1:6]
tol = float(tol)
def lee(p):
    d = {}
    for l in open(p):
        if l.startswith('#') or not l.strip(): continue
        k, rc, npar, logl, est, its, arr = l.rstrip('\n').split('\t')
        d[k] = dict(rc=int(rc), npar=npar, logl=None if logl == '-' else float(logl), est=est, its=its, arr=arr)
    return d
N, R = lee(nuevo), lee(ref)
ab = {l.strip() for l in open(abiertos) if l.strip() and not l.startswith('#')}
ok = fallos = avisos = 0
def bien(t):
    global ok; ok += 1
def falla(t):
    global fallos; fallos += 1; print('  FALLA', t)
# 1. hechos
for k in sorted(N):
    n, r = N[k], R.get(k)
    if r is None: falla(f'{k}: no esta en ref/hechos.tsv (--generar)'); continue
    if n['rc'] != r['rc']: falla(f"{k}: codigo de salida {n['rc']}, referencia {r['rc']}"); continue
    if n['npar'] != r['npar']: falla(f"{k}: {n['npar']} parametros, referencia {r['npar']}"); continue
    if (n['logl'] is None) != (r['logl'] is None) or (n['logl'] is not None and abs(n['logl'] - r['logl']) > tol):
        falla(f"{k}: logL {n['logl']}, referencia {r['logl']}"); continue
    bien(k)
    if (n['est'], n['arr']) != (r['est'], r['arr']):
        avisos += 1; print(f"  aviso {k}: {n['est']} ({n['its']} it., arranque {n['arr']}); referencia {r['est']} ({r['its']} it., {r['arr']})")
if not filtro:
    for k in sorted(set(R) - set(N)): falla(f'{k}: esta en la referencia y no se ha corrido')
# 2. propiedades
vistos = set()
def propiedad(clave, cumple, texto):
    vistos.add(clave)
    if cumple:
        if clave in ab: falla(f'{clave}: YA SE CUMPLE; quitarla de abiertos.txt')
        else: bien(clave)
    elif clave in ab: print(f'  ABIERTO {clave}: {texto}')
    else: falla(f'{clave}: {texto}')
pat = re.compile(r'^(.*)-p(\d+)q(\d+)-(zero|ols)$')
M = {}
for k, v in N.items():
    s, p, q, st = pat.match(k).groups(); M[(s, int(p), int(q), st)] = v
for (s, p, q, st), v in sorted(M.items()):
    if q == 0 and p > 1 and (s, p - 1, 0, st) in M:
        a, b = v['logl'], M[(s, p - 1, 0, st)]['logl']
        if a is not None and b is not None:
            propiedad(f'anida:{s}-{st}-p{p}', a >= b - tol, f'VAR({p}) {a:.6f} por debajo del VAR({p-1}) {b:.6f}')
    if st == 'zero' and (s, p, q, 'ols') in M:
        o = M[(s, p, q, 'ols')]; clave = f'arranques:{s}-p{p}q{q}'
        if v['rc'] != o['rc'] or (v['logl'] is None) != (o['logl'] is None):
            propiedad(clave, False, f"zero sale con {v['rc']} ({v['logl']}) y ols con {o['rc']} ({o['logl']})")
        elif v['logl'] is not None:
            propiedad(clave, abs(v['logl'] - o['logl']) <= tol, f"zero {v['logl']:.6f} ({v['est']}, {v['its']} it.), ols {o['logl']:.6f} ({o['est']}, {o['its']} it.)")
if not filtro:
    for c in sorted(ab - vistos): falla(f'{c}: esta en abiertos.txt y no corresponde a ningun caso')
ab_n = len(ab & vistos)
print(f'rutas: {ok} ok, {fallos} fallos, {ab_n} abiertos, {avisos} avisos')
sys.exit(1 if fallos else 0)
PY
