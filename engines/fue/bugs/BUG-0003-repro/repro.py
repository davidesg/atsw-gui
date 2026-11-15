#!/usr/bin/env python3
"""BUG-0003 (fue): el .pre escribe el factor de reescalado con dos decimales;
un factor menor que 0.005 sale «0.00» y se relee como 1.

`fue.c` escribe la linea de bandas y factor con " %.2f" los dos campos. Un
factor 0.001 (una serie en niveles de varios miles con cambios de cientos lo
pide) se escribe 0.00, y los cinco lectores de la suite hacen
`if (refactor == 0) refactor = 1`. El .pre ya no es el modelo que se estimo:
mu y los omegas siguen en las unidades del factor 0.001 y el fichero dice 1.

El caso: NIV_factor.inp, una serie sintetica mensual en torno a 100, AR(1)
con media, lambda = 1 y factor 0.001. Se estima y se lee la linea del factor
del .pre.

Sale con 1 mientras el .pre no devuelva el factor del .inp, con 0 cuando lo
devuelva y con 2 si el caso no vale. No depende del estado de git.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("fue.exe" if sys.platform == "win32" else "fue")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/fue")


def factor(path):
    lineas = path.read_text(errors="replace").splitlines()
    i = next(k for k, l in enumerate(lineas) if "escaling factor" in l)
    return lineas[i + 1].split()[1]


with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    shutil.copy(AQUI / "NIV_factor.inp", d / "NIV_factor.inp")
    subprocess.run([str(BIN), "NIV_factor"], cwd=d, capture_output=True, timeout=600)
    if not (d / "NIV_factor.pre").exists():
        print("fue no escribio el .pre")
        sys.exit(2)
    antes, despues = factor(d / "NIV_factor.inp"), factor(d / "NIV_factor.pre")

print(f"factor del .inp: {antes}   factor del .pre: {despues}")
if float(despues) != float(antes):
    print("el .pre no devuelve el factor (0 se relee como 1)")
    sys.exit(1)
print("el .pre devuelve el factor")
sys.exit(0)
