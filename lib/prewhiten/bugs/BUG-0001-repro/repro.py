#!/usr/bin/env python3
"""BUG-0001 (lib/prewhiten): una serie con datos <= 0 no entra aunque
lambda = 1.

`apply_univariate_model` rechaza cualquier dato <= 0 antes de la Box-Cox
(«Error: dato no positivo para Box-Cox»), sea cual sea lambda. Con lambda = 1
la transformacion es (y - 1)/1: no pide datos positivos, y fue estima el
mismo fichero sin problema. Como la libreria es la de la escalera de drvarma,
drtran y drvec, ninguna serie en niveles con negativos (un caudal centrado,
un saldo, una tasa) entra en un sistema ni en una transferencia. Los portes
en Python (fue.cast_us, drvarma-python, drtran-python) la aceptan, y
drvec-python solo rechaza los datos <= 0 con lambda != 1.

El caso: la serie J de Box y Jenkins, el horno de gas (296 pares, dominio
publico; openmv.net/info/gas-furnace). GAS.pre es el caudal de gas, con
negativos; CO2.pre, la salida. Los dos los escribio fue, en niveles y con
media. Se corren la puerta de la escalera (drvarma, p = q = 0, -diagcov) y la
transferencia CO2 <- GAS (drtran). Y, para que el arreglo no se pase, la
misma puerta con lambda = 0 (logaritmos) TIENE que rechazar el gas.

Sale con 1 mientras lambda = 1 se rechace (o lambda = 0 se acepte), con 0
cuando no, y con 2 si faltan los motores.
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile

AQUI = pathlib.Path(__file__).resolve().parent
RAIZ = AQUI.parents[3]
EXE = ".exe" if sys.platform == "win32" else ""
DRVARMA = RAIZ / "engines" / "drvarma" / "bin" / ("drvarma" + EXE)
DRTRAN = RAIZ / "engines" / "drtran" / "bin" / ("drtran" + EXE)
for b in (DRVARMA, DRTRAN):
    if not b.exists():
        print(f"falta {b}: make -C {b.parents[1].relative_to(RAIZ)}")
        sys.exit(2)

RECHAZO = "dato no positivo"


def corre(d, orden):
    r = subprocess.run([str(x) for x in orden], cwd=d, capture_output=True,
                       text=True, errors="replace", timeout=600)
    return r.returncode, r.stdout + r.stderr


with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("GAS.pre", "CO2.pre"):
        shutil.copy(AQUI / f, d / f)
    # El gas en logaritmos: la misma especificacion con lambda = 0.
    log = (d / "GAS.pre").read_text().replace(
        "complete annual differences:\n1.00 0 0", "complete annual differences:\n0.00 0 0")
    assert "\n0.00 0 0" in log, "no encuentro la lambda de GAS.pre"
    (d / "GASLOG.pre").write_text(log)

    rc1, s1 = corre(d, [DRVARMA, "GAS.pre", "CO2.pre", 0, 0, "-diagcov", "-o", "puerta"])
    rc2, s2 = corre(d, [DRTRAN, "CO2.pre", "GAS.pre"])
    rc3, s3 = corre(d, [DRVARMA, "GASLOG.pre", "CO2.pre", 0, 0, "-diagcov", "-o", "log"])

mal = 0
for que, rc, s in (("drvarma, la puerta (lambda = 1)", rc1, s1),
                   ("drtran, CO2 <- GAS (lambda = 1)", rc2, s2)):
    if RECHAZO in s:
        print(f"{que}: rechaza el gas («{RECHAZO}»), rc = {rc}")
        mal = 1
    else:
        print(f"{que}: entra, rc = {rc}")
if RECHAZO in s3:
    print("drvarma con lambda = 0: rechaza el gas, como debe")
else:
    print(f"drvarma con lambda = 0: NO rechaza el gas (rc = {rc3}); el log de un negativo")
    mal = 1
sys.exit(mal)
