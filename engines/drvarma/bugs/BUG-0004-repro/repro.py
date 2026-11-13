#!/usr/bin/env python3
"""BUG-0004 (drvarma): la IRF de la escalera se mide con Q, no con Sigma.

La escalera (src/escalera.c) llama a impulse_response(&F.vm, ...) con el
modelo que deja shootx_esc, que pone armax->sigma2 = 1.0 (la sigma2 esta
concentrada y vive en F.sigma2). impulse_response toma Sigma = sigma2 * Q,
asi que factoriza Q: el «choque de una desviacion tipica» es de sqrt(Q_jj)
y no de sqrt(Sigma_jj), y toda la IRF ortogonalizada (y su acumulada, y la
ganancia) sale dividida por sigma. La FEVD no cambia: es un cociente.

El caso: Jenkins y Alavi, MUSKRAT y MINK, el VARMA(1,1) con Sigma llena
(2 1). La tabla «Structural shock standard deviations» tiene que decir
sqrt(Sigma_11) para la primera serie; la respuesta a h = 0 de la primera
serie a su propio choque es esa misma cifra.

Sale con 1 mientras el fallo siga abierto, con 0 cuando este arreglado y
con 2 si el caso no vale. No depende del estado de git.
"""
import math, pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("MUSKRAT_m03.pre", "MINK_m02.pre"):
        shutil.copy(AQUI / f, d / f)
    subprocess.run([str(BIN), "MUSKRAT_m03.pre", "MINK_m02.pre", "2", "1", "-o", "x"],
                   cwd=d, capture_output=True, timeout=600)
    out = (d / "x.out").read_text(errors="replace") if (d / "x.out").exists() else ""

m = re.search(r"Innovation covariance Sigma = sigma2 \* Q.*\n\s*\S+\s+(\S+)\s+(\S+)\n\s*\S+\s+(\S+)\s+(\S+)", out)
s = re.search(r"Structural shock standard deviations.*\n\s*\S+\s*:\s*(\S+)", out)
if not m or not s:
    print("el .out no trae Sigma o la tabla de los choques")
    sys.exit(2)
s11, s12 = float(m.group(1)), float(m.group(2))
if abs(s12) < 1e-9 * s11:
    print("Sigma salio diagonal: el caso no vale")
    sys.exit(2)
want, got = math.sqrt(s11), float(s.group(1))
print(f"Sigma_11 = {s11:g}: el choque de una d.t. de la primera serie es {want:.4f}; la tabla dice {got:.4f}")
abierto = abs(got - want) > 1e-3 * want
print("ABIERTO" if abierto else "cerrado")
sys.exit(1 if abierto else 0)
