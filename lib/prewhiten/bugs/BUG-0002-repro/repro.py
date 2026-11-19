#!/usr/bin/env python3
"""BUG-0002 (lib/prewhiten): con lambda = 1 la serie se transforma como
refactor*(y - 1), y fue la transforma como refactor*y.

fue (engines/fue/src/fue.c, BoxCox) hace con lambda = 1 DataOutput =
refactor*y: en niveles no resta nada. apply_univariate_model y fuepre_bc
(lib/fuepre/fuepre_forecast.c) aplican la Box-Cox de libro, (y^lambda -
1)/lambda, tambien con lambda = 1: refactor*(y - 1). La diferencia es una
constante, refactor (100). La verosimilitud no cambia si la media se estima
(se la come la media), pero la media de la escalera sale refactor por debajo
de la del .pre, y la puerta de drvarma marca «<- not an optimum, moved 100»
en un .pre que es un optimo de fue. drvec (boxcox_w) y los portes de Python
(fue.cast_us) hacen lo mismo que fue.

El caso: los .pre de BUG-0001 (la serie J de Box y Jenkins), que escribio
fue: el gas y el CO2 en niveles, cada uno ruido blanco con su media. Son
optimos de fue, asi que en la puerta tienen que moverse lo que redondea el
.pre (~1e-5), no 100.

Sale con 1 mientras alguno se mueva 1 o mas, con 0 cuando no, y con 2 si
falta el motor o la tabla.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

AQUI = pathlib.Path(__file__).resolve().parent
RAIZ = AQUI.parents[3]
BIN = RAIZ / "engines" / "drvarma" / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    print(f"falta {BIN}: make -C engines/drvarma")
    sys.exit(2)

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("GAS.pre", "CO2.pre"):
        shutil.copy(AQUI / f, d / f)
    subprocess.run([str(BIN), "GAS.pre", "CO2.pre", "0", "0", "-diagcov", "-o", "puerta"],
                   cwd=d, capture_output=True, timeout=600)
    out = (d / "puerta.out").read_text(errors="replace") if (d / "puerta.out").exists() else ""

filas = re.findall(r"^\s+(GAS|CO2)\s+(-?[\d.]+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)", out, re.M)
if len(filas) != 2:
    print("no encuentro la tabla de la puerta")
    sys.exit(2)
mal = 0
for serie, logl, s2, move in filas:
    print(f"{serie}: logL {logl}, max|move| {move}")
    if float(move) >= 1.0:
        mal = 1
print("la media se mueve refactor: el cast no es el de fue" if mal else "los .pre son puntos fijos")
sys.exit(mal)
