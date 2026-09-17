#!/usr/bin/env python3
"""pypre_roundtrip.py -- el actor `pypre`: fue/report.py::write_pre.

    pypre_roundtrip.py <entrada.inp> <directorio> <nombre>

write_pre exige un modelo ESTIMADO -- es su razon de ser: un .pre es el .inp
con las estimaciones como valores iniciales. Asi que este actor estima y luego
escribe, que es el camino real, y la comparacion se hace en modo estructura:
los valores libres cambian por definicion, todo lo demas no.

Deja <directorio>/<nombre>.inp (con ese nombre, aunque sea un .pre, para que la
bateria lo encuentre donde los demas).

Sale 4 si el lector no acepta el fichero, 3 si la estimacion no sale.
"""

import sys
import os
import warnings

warnings.filterwarnings("ignore")


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2

    entrada, directorio, nombre = argv[1], argv[2], argv[3]

    import fue

    try:
        ts, model = fue.load(entrada)
    except Exception as e:
        print("RECHAZADO el lector de Python no lo acepta: %s" % e)
        return 4

    try:
        model.fit()
    except Exception as e:
        print("no estima: %s: %s" % (type(e).__name__, e))
        return 3

    salida = os.path.join(directorio, nombre + ".inp")
    from fue.report import write_pre
    write_pre(model, salida)
    return 0 if os.path.exists(salida) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
