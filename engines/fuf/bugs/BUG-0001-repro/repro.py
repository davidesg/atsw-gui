#!/usr/bin/env python3
"""BUG-0001 (fuf): con lambda = 1 la tabla de previsión sale x100 salvo el nivel.

forecast_table_acii (src/usfo.c) multiplica por 100 las variaciones, sus
desviaciones típicas, la del nivel y ERR. En logaritmos eso las pasa a
porcentaje, que es lo que se quiere; en niveles (lambda = 1) las deja cien
veces más grandes que en las unidades de la serie. El nivel no lleva el 100
y sale bien.

Los dos .fuf.inp los escribe fue (fue <x> -f 24) desde los .inp de fue de
conformidad/corpus, con su factor de reescalado (100):

  CPI_USA_model_AR  niveles: el IPC pasa de 257.208 a 256.974 (-0.234) y
                    la tabla dice -23.4; la d.t. de la primera previsión
                    del nivel es sigma / 100 = 0.56 y la tabla dice 56.1.
  IPC_ES_m01_ar1    logaritmos: el control. Las variaciones son porcentajes
                    y ERR está en la escala de sigma (en %).

Sale con 1 mientras el fallo siga abierto, con 0 cuando esté arreglado.
"""
import math, pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
FUF = AQUI.parents[1] / "bin" / "fuf"
FECHA = re.compile(r"^\s*\d{1,2}/\d{4}")


def corre(d, nombre):
    """Las filas de la tabla (observadas y previstas) y sigma."""
    shutil.copy(AQUI / f"{nombre}.inp", d / "x.inp")
    subprocess.run([str(FUF), "x"], cwd=d, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
    txt = (d / "x.out").read_text(errors="replace")
    tabla = txt[txt.index("FORECAST REPORT"):].split("\n")
    obs, prev = [], []
    for l in tabla:
        if not FECHA.match(l):
            continue
        # desde el arreglo del BUG-0002 los campos van separados: se parten
        # por espacios. Observada: fecha valor - var - anual - ERR; prevista:
        # fecha valor dt var dt anual dt -
        f = l.split()
        (obs if f[2] == "-" else prev).append(f)
    inp = (d / "x.inp").read_text().split("\n")
    i = next(k for k, l in enumerate(inp) if "innovation variance" in l)
    sigma = math.sqrt(float(inp[i + 1].split()[1]))
    i = next(k for k, l in enumerate(inp) if "reescaling" in l)
    refactor = float(inp[i + 1].split()[1])
    return obs, prev, sigma / refactor


if not FUF.exists():
    sys.exit(f"falta {FUF}: compila fuf (make en engines/fuf)")

abierto = False
with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)

    obs, prev, sigma = corre(d, "CPI_USA_model_AR.fuf")
    n0, n1, var = float(obs[-2][1]), float(obs[-1][1]), float(obs[-1][3])
    dt = float(prev[0][2])
    print(f"niveles: {obs[-1][0].strip()} cambia {n1 - n0:+.3f} y la tabla dice {var:+.3f}")
    print(f"         d.t. de la primera previsión: sigma = {sigma:.3f}, la tabla dice {dt:.3f}")
    if abs(var - (n1 - n0)) > 0.01 or abs(dt - sigma) > 0.01 * sigma:
        abierto = True

    obs, prev, sigma = corre(d, "IPC_ES_m01_ar1.fuf")
    dt = float(prev[0][2])
    err = max(abs(float(f[7])) for f in obs[-24:])
    print(f"logaritmos (control): sigma {100 * sigma:.3f} %, d.t. {dt:.3f}, |ERR| máximo {err:.3f}")
    if abs(dt - 100 * sigma) > 0.01 or err > 10 * 100 * sigma:
        sys.exit("el control falla: en logaritmos la tabla debería salir bien")

print("ABIERTO" if abierto else "cerrado")
sys.exit(1 if abierto else 0)
