#!/usr/bin/env python3
"""BUG-0007 (drvarma): `-split -mean` siembra la media a cero, y con las
series lejos de cero la escalera sobre esos ficheros no llega al optimo.

NIVEL100.inp: un VAR(1) bivariante simulado (n = 300) con las dos series en
torno a 100, en niveles (lambda = 1, d = 0). Con el factor 100, la serie
estacionaria esta en torno a 10000.

  el .inp original, `NIVEL100 1 0 -mean`        logL -3620.221
      (init_varma siembra la media muestral)
  la escalera sobre `-split -mean -ar 1`, 1 0    logL -3669.432
      (la media arranca en 0: el optimizador absorbe el nivel con los AR
       propios, 0.67 y 0.86, y deja medias de -385 y +400)

Es el mismo VAR(1) con media. Sale con 1 mientras la escalera quede por
debajo del .inp, con 0 cuando coincidan y con 2 si el caso no vale. No
depende del estado de git.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

def logl(texto):
    m = re.findall(r"Exact log-likelihood:\s*(-?[\d.]+)", texto)
    return float(m[-1]) if m else None

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    shutil.copy(AQUI / "NIVEL100.inp", d / "NIVEL100.inp")
    kw = dict(cwd=d, capture_output=True, text=True, errors="replace", timeout=1800)
    subprocess.run([str(BIN), "NIVEL100", "1", "0", "-mean"], **kw)
    original = logl((d / "NIVEL100.out").read_text(errors="replace")) if (d / "NIVEL100.out").exists() else None
    s = subprocess.run([str(BIN), "-split", "NIVEL100.inp", "-mean", "-ar", "1"], **kw)
    fich = [pathlib.Path(l.strip()).name for l in s.stdout.splitlines() if l.strip().endswith(".inp")]
    semillas = [(d / f).read_text().split("** Mean parameter (mu):\n")[1].splitlines()[0] for f in fich]
    subprocess.run([str(BIN), *fich, "1", "0", "-o", "esc"], **kw)
    escalera = logl((d / "esc.out").read_text(errors="replace")) if (d / "esc.out").exists() else None

print("semillas de la media en los ficheros de -split:", semillas)
print("logL del .inp original :", original)
print("logL de la escalera    :", escalera)
if original is None or len(fich) != 2:
    print("el caso no vale")
    sys.exit(2)
if escalera is None or escalera < original - 0.01:
    print("ABIERTO: la escalera sobre -split -mean queda por debajo del .inp")
    sys.exit(1)
print("cerrado: el mismo optimo")
sys.exit(0)
