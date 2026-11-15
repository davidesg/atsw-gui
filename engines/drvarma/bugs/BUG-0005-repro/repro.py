#!/usr/bin/env python3
"""BUG-0005 (drvarma): la tabla de prevision de la escalera multiplica las
variaciones por 100/refactor sea cual sea lambda.

`escalera.c` escribe en el .forecast (y en el bloque de prevision del .out)
mon%/ann% y sus desviaciones tipicas como 100 * delta / refactor. En
logaritmos es un tanto por ciento. En niveles (lambda = 1) es el cambio del
nivel por cien, bajo una columna que se llama «mon%». La tabla de exactitud de
la misma escalera, fuf, drtran y drvec usan (lambda = 0 ? 100 : 1) / refactor.
`drvarma.c` (-forecast, la ruta del .inp) hace lo mismo con su -scale.

El caso: dos series sinteticas mensuales en torno a 100, en niveles, cada una
un AR(1) con media estimado por fue con lambda = 1 y factor 100 (NIVY.pre,
NIVX.pre); la escalera VARMA(0,0) y su prevision a 3 periodos.

Sale con 1 mientras el mon% de la primera prevision sea cien veces el cambio
del nivel, con 0 cuando sea el cambio y con 2 si el caso no vale. No depende
del estado de git.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

ULTIMA = 103.8379                    # la ultima observacion de NIVY

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("NIVY.pre", "NIVX.pre"):
        shutil.copy(AQUI / f, d / f)
    subprocess.run([str(BIN), "NIVY.pre", "NIVX.pre", "0", "0", "-forecast", "3", "-o", "x"],
                   cwd=d, capture_output=True, timeout=600)
    fc = (d / "x.forecast").read_text(errors="replace") if (d / "x.forecast").exists() else ""

m = re.search(r"Series 1 \(NIVY\):\n.*\n\s*\S+\s+(\S+)\s+\S+\s+\S+\s+(\S+)", fc)
if not m:
    print("no hay tabla de prevision")
    sys.exit(2)
nivel, mon = float(m.group(1)), float(m.group(2))
cambio = nivel - ULTIMA
print(f"NIVY: ultima observacion {ULTIMA}, primera prevision {nivel}: cambio {cambio:+.4f}")
print(f"mon% de la tabla: {mon:+.4f}")
if abs(mon) > 10 * abs(cambio) + 1e-3:
    print("x100 en niveles")
    sys.exit(1)
print("el cambio del nivel")
sys.exit(0)
