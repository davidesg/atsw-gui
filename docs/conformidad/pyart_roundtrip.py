#!/usr/bin/env python3
"""pyart_roundtrip.py -- el mismo viaje de ida y vuelta que gtkfue_roundtrip,
pero por el escritor de Python.

    pyart_roundtrip.py <entrada.inp> <directorio> <nombre>

deja <directorio>/<nombre>.inp, escrito por art/pipeline.py::_write_inp(), que
dice en su docstring replicar gtk_fue file_io.c:write_inp_file(). Aqui se
comprueba si de verdad lo hace.

El modelo se lee con fue.load(), que es el lector unico del formato en Python.
Asi que esto mide al ESCRITOR: lo que el lector entendio, ?lo vuelve a dejar
escrito de forma que el mismo lector vea lo mismo?

Sale 4 -- como el de C -- cuando el fichero no es de este dialecto: es la
puerta de inpcheck, aqui en forma de excepcion del lector.
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
    from art.pipeline import _write_inp

    try:
        ts, model = fue.load(entrada)
    except Exception as e:
        print("RECHAZADO el lector de Python no lo acepta: %s" % e)
        return 4

    salida = os.path.join(directorio, nombre + ".inp")
    # el refactor que el fichero declara, no el de la convencion: aqui se
    # copia un fichero, no se estima nada
    _write_inp(ts, model, salida, refactor=getattr(model, "refactor", None))
    return 0 if os.path.exists(salida) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
