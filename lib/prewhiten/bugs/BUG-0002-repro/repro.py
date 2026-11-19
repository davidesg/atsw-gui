#!/usr/bin/env python3
"""BUG-0002 (lib/prewhiten): con lambda = 1 la Box-Cox de la libreria es
refactor*(y - 1); la de fue, el dueno del formato, es refactor*y.

fue (C, `BoxCox` de fue.c; y su porte, fue.cast_us) y drvec (`boxcox_w`)
transforman con lambda = 1 sin restar 1. `apply_univariate_model`
(prewhiten.c) y `fuepre_bc` (fuepre_forecast.c) restan 1. Con d = 0 la
diferencia, refactor, cae entera en la media: un .pre de fue en niveles con
media no es un optimo para la escalera, que tiene que mover mu en refactor.

El caso: los ficheros del BUG-0001 (la serie J de Box y Jenkins, el horno de
gas, que escribio fue en niveles y con media, factor 100). La puerta de la
escalera de drvarma (p = q = 0, -diagcov) los reestima uno a uno: la columna
max|move| tiene que ser ~0 (el .pre es el optimo de fue). Hoy es 100 en los
dos: mu se mueve exactamente el factor.

Sale con 1 mientras algun max|move| sea >= 1, con 0 cuando no, y con 2 si
falta el motor.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

AQUI = pathlib.Path(__file__).resolve().parent
RAIZ = AQUI.parents[3]
FICH = AQUI.parent / "BUG-0001-repro"
EXE = ".exe" if sys.platform == "win32" else ""
DRVARMA = RAIZ / "engines" / "drvarma" / "bin" / ("drvarma" + EXE)
if not DRVARMA.exists():
    print(f"falta {DRVARMA}: make -C engines/drvarma")
    sys.exit(2)

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("GAS.pre", "CO2.pre"):
        shutil.copy(FICH / f, d / f)
    r = subprocess.run([str(DRVARMA), "GAS.pre", "CO2.pre", "0", "0", "-diagcov",
                        "-o", "puerta"], cwd=d, capture_output=True, text=True,
                       errors="replace", timeout=600)
    out = (d / "puerta.out").read_text(errors="replace") if (d / "puerta.out").exists() else ""
    filas = re.findall(r"^\s+(GAS|CO2)\s+(-?[\d.]+)\s+([\d.]+)\s+([\d.eE+-]+)", out, re.M)
    if len(filas) != 2:
        print("no encuentro la tabla de la puerta:\n" + (r.stdout + r.stderr)[-800:])
        sys.exit(2)
    mov = {s: float(m) for s, _l, _s2, m in filas}
    print("max|move| en la puerta (el .pre de fue reestimado por la escalera):", mov)
    mal = any(v >= 1.0 for v in mov.values())
    print("ABIERTO: la escalera mueve mu el factor: su Box-Cox con lambda = 1 no es la de fue"
          if mal else "ARREGLADO: el .pre de fue es un optimo para la escalera")
    sys.exit(1 if mal else 0)
