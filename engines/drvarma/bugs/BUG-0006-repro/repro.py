#!/usr/bin/env python3
"""BUG-0006 (drvarma): con -start ols, si el arranque de minimos cuadrados no
es admisible, la escalera dice que vuelve a cero y no vuelve.

start_ols() (src/escalera.c) evalua el punto de MCO con fit_run(&chk, 0); si
elf no lo acepta pone a cero los AR cruzados y las covarianzas de Q y LUEGO
llama a fit_free(&chk). fit_free cierra el ajuste con
shootx_esc(chk.x, ..., lastx = 1), que empieza por esc_unpack(chk.x): vuelve
a escribir en el estado global el punto de MCO que se acababa de borrar. La
estimacion arranca del punto no admisible y falla («AR nonstationary»,
codigo 3), con el .out diciendo «zero (the ols start was not admissible)».
El porte (drvarma-python, Ladder._take_start) si vuelve a cero.

El caso: el horno de gas de Box y Jenkins (serie J) por la ruta V de Tiao y
Box: dos especificaciones con solo su transformacion y un AR(6) propio libre
(GAS_v.inp, CO2_v.inp), VAR(6). El MCO del VAR(6) junto con los AR propios
del ajuste diagonal no es estacionario. Control: -start zero, que llega a
logL -2638.762693 (el mismo que sima).

Sale con 1 mientras -start ols no llegue al mismo optimo que -start zero,
con 0 cuando llegue y con 2 si falta el motor o el control no vale.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")
OPTIMO = -2638.762693


def corre(d, start):
    r = subprocess.run([str(BIN), "GAS_v.inp", "CO2_v.inp", "6", "0", "-start", start,
                        "-o", start], cwd=d, capture_output=True, text=True, timeout=900)
    m = re.search(r"^Exact log-likelihood\s*:?\s*(-?[\d.]+)", r.stdout, re.M)
    usado = re.search(r"^Start of the cross terms: (.*)$", r.stdout, re.M)
    return r.returncode, (float(m.group(1)) if m else None), (usado.group(1) if usado else "")


with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("GAS_v.inp", "CO2_v.inp"):
        shutil.copy(AQUI / f, d / f)
    rc0, l0, _ = corre(d, "zero")
    rc1, l1, usado = corre(d, "ols")

print(f"-start zero: rc {rc0}, logL {l0}")
print(f"-start ols : rc {rc1}, logL {l1}, arranque: {usado}")
if rc0 != 0 or l0 is None or abs(l0 - OPTIMO) > 1e-5:
    print("el control no vale")
    sys.exit(2)
if rc1 != 0 or l1 is None or abs(l1 - l0) > 1e-5:
    print("la vuelta a cero no vuelve: arranca del punto de MCO")
    sys.exit(1)
print("vuelve a cero y llega al optimo")
sys.exit(0)
