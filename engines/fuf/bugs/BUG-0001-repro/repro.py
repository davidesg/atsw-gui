#!/usr/bin/env python3
"""BUG-0001 (fuf): el informe de previsión da las variaciones y ERR mal escaladas.

1. Con lambda = 1 (niveles) la variación mensual sale multiplicada por 100:
   CPI_USA pasa de 256.974 a 257.971 (+0.997) y el informe dice 99.7.
2. La columna ERR de las observaciones no está en la escala de la sigma de la
   entrada: con logaritmos, sigma = sqrt(0.0627) = 0.25 y ERR vale 100.4.

Sale con 1 mientras el fallo siga abierto, con 0 cuando esté arreglado.
"""
import math, pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
FUF = AQUI.parents[2] / "fuf" / "bin" / "fuf"
FILA = re.compile(r"^\s*(\d+/\d+)\s+([-\d.]+)\s+\S+\s+([-\d.]+)\s+\S+\s+([-\d.]+)\s+\S+\s+([-\d.]+)\s*$")

def corre(d, nombre):
    shutil.copy(AQUI / f"{nombre}.inp", d / "x.inp")
    subprocess.run([str(FUF), "x"], cwd=d, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
    txt = (d / "x.out").read_text(errors="replace")
    filas = [m.groups() for m in map(FILA.match, txt.split("\n")) if m]
    inp = (d / "x.inp").read_text().split("\n")
    i = next(k for k, l in enumerate(inp) if "innovation variance" in l)
    sigma = math.sqrt(float(inp[i + 1].split()[1]))
    return filas, sigma

abierto = False
with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    filas, _ = corre(d, "CPI_USA_model_AR.fuf")
    n0, n1 = float(filas[0][1]), float(filas[1][1])
    var = float(filas[1][2])
    print(f"niveles: {filas[1][0]} sube {n1 - n0:+.3f} y el informe dice {var:+.3f}")
    if abs(var - (n1 - n0)) > 0.01:
        abierto = True
    filas, sigma = corre(d, "IPC_ES_m01_ar1.fuf")
    err = max(abs(float(f[4])) for f in filas[:12])
    print(f"logaritmos: sigma {sigma:.3f}, |ERR| máximo {err:.3f}")
    if err > 10 * sigma:
        abierto = True
print("ABIERTO" if abierto else "cerrado")
sys.exit(1 if abierto else 0)
