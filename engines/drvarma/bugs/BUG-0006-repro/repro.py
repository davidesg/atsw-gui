#!/usr/bin/env python3
"""BUG-0006 (drvarma): cuando el arranque -start ols no es admisible, la
escalera dice que cae a cero y la estimacion falla; con -start zero, estima.

El caso: el sistema monetario de EE. UU. (IPI, IPC, WTI, FF; FRED,
1990:01-2007:12) como especificaciones (los armonicos y la media libres a
cero, un AR(1)/MA(1) propio libre a cero), VARMA(1,1) con -redet.

  -start zero  -> estima (acaba en el muro del MA, logL -1756.75)
  -start ols   -> "Start of the cross terms: zero (the ols start was not
                  admissible)" y despues "AR nonstationary (code 3)"

Si la caida a cero dejara el estado como lo deja -start zero, las dos
corridas serian la misma. El porte (drvarma-python, Ladder(start="ols",
redet=True)) en el mismo caso cae a cero y estima.

Sale con 1 mientras -start ols falle donde -start zero estima, con 0 cuando
las dos estimen y con 2 si el caso no vale (cero tampoco estima, o el
arranque ols resulta admisible). Tarda unos minutos. No depende del estado
de git.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")
FICH = [f"{s}_v_p1q1.inp" for s in ("IPI", "IPC", "WTI", "FF")]

def corre(d, arranque):
    r = subprocess.run([str(BIN), *FICH, "1", "1", "-start", arranque, "-redet", "-o", arranque],
                       cwd=d, capture_output=True, text=True, errors="replace", timeout=3600)
    return r.returncode, r.stdout

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in FICH:
        shutil.copy(AQUI / f, d / f)
    rc0, _ = corre(d, "zero")
    rc1, sal = corre(d, "ols")

print(f"-start zero: codigo {rc0}")
print(f"-start ols : codigo {rc1}")
for l in sal.splitlines():
    if l.startswith(("Start of the cross terms", "Estimation error")):
        print("   ", l)
if rc0 != 0 or "ols start was not admissible" not in sal:
    print("el caso no vale: cero no estima, o el arranque ols es admisible")
    sys.exit(2)
if rc1 != 0:
    print("ABIERTO: la caida de ols a cero falla donde -start zero estima")
    sys.exit(1)
print("cerrado: las dos corridas estiman")
sys.exit(0)
