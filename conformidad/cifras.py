#!/usr/bin/env python3
r"""cifras.py -- dos salidas de un motor, iguales salvo en las ultimas cifras.

    cifras.py REFERENCIA NUEVO [--rtol R] [--atol A] [--ulps K] [--max N]

Sale con 0 si son iguales en la tolerancia, con 1 si no (y dice donde), con
2 si no puede leerlos.

POR QUE EXISTE. Las referencias de las baterias se escribieron en Linux y se
comparan byte a byte. En macOS y en Windows los mismos programas, sobre los
mismos datos, dan los mismos coeficientes en ~9 cifras, pero los errores
estandar cambian en la cuarta (0.048690 / 0.048682 / 0.048686): salen de un
hessiano numerico, que amplifica el redondeo de la libm de cada sistema. No
es un fallo del programa; exigir esos bytes fuera de Linux solo mide la
libm. En Linux se sigue exigiendo el byte (las baterias llaman a esto solo
cuando cmp falla y la plataforma no es Linux).

LA REGLA. El TEXTO tiene que ser identico (salvo los \\r de Windows y la
cantidad de espacios, que cambia con el ancho de un numero). Cada NUMERO
puede diferir en

    max( rtol * max(|a|,|b|),  atol,  ulps * una unidad de la ultima cifra )

La cota absoluta es para lo que deberia ser cero y sale de un calculo
numerico: las covarianzas entre estimaciones casi independientes (~1e-6,
del mismo hessiano) cambian en su primera cifra, y relativamente eso es
mucho, pero en la escala de la matriz no es nada. La ultima es para los
numeros impresos con pocas cifras: 0.0203 y 0.0204 son el mismo p-valor
redondeado de dos formas.

LA SALIDA DE UNA ESTIMACION NO ES UNA LISTA DE NUMEROS IGUALES. Lo que se
mide en CI (macOS, Windows) es que las ESTIMACIONES coinciden en 7-9 cifras
y lo que sale del hessiano numerico (fdhess) no: en un modelo mal escalado
los errores estandar cambian hasta un 4 %. Asi que cada numero se mide con
su vara:

  estimacion (error)   la estimacion, con la cota de arriba o el 2 % de SU
                       error estandar si es mayor (una diferencia del 2 % de
                       la incertidumbre no cambia ninguna conclusion); el
                       error, con un 5 %. Vale para "0.37 (0.16)" del .out y
                       para "\est{0.37}{(0.16)}" del .tex.
  covarianzas          en la escala de la correlacion: 0.05*sqrt(Cii*Cjj).
  correlaciones        0.05 absoluto.
  .eps                 2 puntos absolutos: un rotulo se corre medio
                       milimetro si el numero de delante tiene otra anchura.

Las tolerancias son las de una REPRODUCCION en otra plataforma, no las de
una comparacion de modelos. Estan aqui, juntas y con su razon, para que se
puedan discutir.

LO QUE NO ARREGLA: un optimo distinto. Si el optimizador acaba en otro sitio
(pasa en los modelos mal definidos, con verosimilitud plana), las cifras
difieren mucho mas que esto, y eso tiene que seguir viendose.
"""

import re
import sys

NUM = re.compile(r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?')
FILA = re.compile(r'^\s*x\[\s*(\d+)\]\s*->')

RTOL, ATOL, ULPS = 2e-3, 1e-6, 2.0
RTOL_SE, K_SE = 5e-2, 2e-2          # el error, y la estimacion frente a el
K_COV, ATOL_COR, ATOL_EPS = 5e-2, 5e-2, 2.0


def unidad(s):
    mant = re.split('[eE]', s)[0]
    dec = len(mant.split('.')[1]) if '.' in mant else 0
    exp = int(re.split('[eE]', s)[1]) if re.search('[eE]', s) else 0
    return 10.0 ** (exp - dec)


def fichas(texto, eps):
    """Lista de fichas ('t', texto, linea) y ('n', valor, cota, linea, s).

    La cota es la tolerancia de ese numero, decidida con el contexto de la
    REFERENCIA (las fichas del fichero nuevo se emparejan por posicion)."""
    out = []
    seccion = ''
    diag = {}
    for nl, linea in enumerate(texto.split('\n'), 1):
        # El informe de convergencia ("**** CONVERGENCE OBTAINED AFTER 123
        # ITERATIONS", "**** GRADIENT/PARAMETER STOPPING CRITERIUM...") es
        # el CAMINO, no el resultado: el mismo optimo se alcanza en 121
        # iteraciones o parando por otro criterio. Lo que importa son las
        # estimaciones, y esas se comparan.
        if linea.startswith('**** '):
            continue
        low = linea.lower()
        if 'covariance matrix' in low:
            seccion, diag = 'cov', {}
        elif 'correlation matrix' in low:
            seccion = 'cor'
        elif linea.strip() and not FILA.match(linea):
            if seccion in ('cov', 'cor') and not linea.startswith(' '):
                seccion = ''
        fila = FILA.match(linea)
        nums = list(NUM.finditer(linea))
        # Los numeros de la fila de una matriz: el primero es el indice.
        valores = [float(m.group(0)) for m in nums]
        if fila and seccion == 'cov' and len(valores) > 1:
            i = int(fila.group(1))
            diag[i] = abs(valores[-1])
        pos = 0
        for k, m in enumerate(nums):
            t = linea[pos:m.start()]
            if t.strip():
                out.append(('t', ' '.join(t.split()), nl))
            s_ = m.group(0)
            v = float(s_)
            cota = max(RTOL * abs(v), ATOL, ULPS * unidad(s_))
            if eps:
                cota = max(cota, ATOL_EPS)
            elif fila and k >= 1 and seccion == 'cov':
                i, j = int(fila.group(1)), k
                if i in diag and j in diag:
                    cota = max(cota, K_COV * (diag[i] * diag[j]) ** 0.5)
            elif fila and k >= 1 and seccion == 'cor':
                cota = max(cota, ATOL_COR)
            else:
                antes = linea[pos:m.start()]
                despues = linea[m.end():m.end() + 12]
                if '(' in antes and ')' in despues:
                    # es un error estandar; y la estimacion es la anterior
                    cota = max(cota, RTOL_SE * abs(v))
                    for f in reversed(out):
                        if f[0] == 'n' and f[3] == nl:
                            out[out.index(f)] = (f[0], f[1], max(f[2], K_SE * abs(v)), f[3], f[4])
                            break
            out.append(('n', v, cota, nl, s_))
            pos = m.end()
        t = linea[pos:]
        if t.strip():
            out.append(('t', ' '.join(t.split()), nl))
    return out


def lee(ruta):
    with open(ruta, 'rb') as f:
        return f.read().decode('latin-1').replace('\r', '')


def main(argv):
    global RTOL, ATOL, ULPS
    maxd = 8
    args = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in ('--rtol', '--atol', '--ulps', '--max') and i + 1 < len(argv):
            v = argv[i + 1]
            if a == '--rtol':
                RTOL = float(v)
            elif a == '--atol':
                ATOL = float(v)
            elif a == '--ulps':
                ULPS = float(v)
            else:
                maxd = int(v)
            i += 2
            continue
        args.append(a)
        i += 1
    if len(args) != 2:
        print('uso: cifras.py REFERENCIA NUEVO [--rtol R] [--atol A] [--ulps K] [--max N]',
              file=sys.stderr)
        return 2
    eps = args[0].lower().endswith(('.eps', '.ps'))
    try:
        a, b = fichas(lee(args[0]), eps), fichas(lee(args[1]), eps)
    except OSError as e:
        print('cifras: %s' % e, file=sys.stderr)
        return 2

    malas = 0
    for x, y in zip(a, b):
        if x[0] != y[0] or (x[0] == 't' and x[1] != y[1]):
            malas += 1
            if malas <= maxd:
                print('  linea %d: "%s" / "%s"' % (x[-1] if x[0] == 't' else x[3],
                      x[1] if x[0] == 't' else x[4], y[1] if y[0] == 't' else y[4]))
            continue
        if x[0] == 'n' and abs(x[1] - y[1]) > max(x[2], y[2]):
            malas += 1
            if malas <= maxd:
                print('  linea %d: %s / %s  (cota %.3g)' % (x[3], x[4], y[4], max(x[2], y[2])))
    if len(a) != len(b):
        malas += 1
        print('  longitudes distintas: %d / %d elementos' % (len(a), len(b)))
    if malas:
        print('  %d diferencias fuera de tolerancia' % malas)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
