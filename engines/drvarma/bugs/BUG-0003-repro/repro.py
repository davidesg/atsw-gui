#!/usr/bin/env python3
"""BUG-0003 (drvarma): la vía .inp sale con 0 cuando la estimación falla.

drvarma.c:457-473 guarda el fallo de est() en est_fault, lo escribe en
pantalla y en el .out («**** ESTIMATION FAILED: ...») y se salta los
resultados, pero main acaba en «return 0» (drvarma.c:725) sin mirarlo.
Quien llama al motor por el código de salida (el GUI viejo, un guion)
toma un fallo por un éxito. La escalera no tiene el defecto: sale con 3.

El caso: IPC3 (tres IPC mensuales, 2002-2019) en niveles con log, sin
diferenciar, y el VARMA(1,1) por defecto, que es lo que hace el GUI viejo
sin tocar nada. La estimación falla («AR nonstationary»).
El control: el mismo .inp con d = 1 y un VAR(1) con -mean, que estima bien
y sale con 0.

Sale con 1 mientras el fallo siga abierto (el .out dice ESTIMATION FAILED
y el código es 0), con 0 cuando esté arreglado y con 2 si el caso o el
control no valen. No depende del estado de git.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")

if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

ORIGINAL = (AQUI / "IPC3.inp").read_text()
TRANSF = "** Box-Cox lambda, regular differences, annual differences:\n 0.0 1 0\n"
assert TRANSF in ORIGINAL

def corre(carpeta, d, args):
    """drvarma sobre carpeta/IPC3 con d diferencias: (rc, texto del .out)."""
    carpeta.mkdir()
    texto = ORIGINAL.replace(TRANSF, TRANSF.replace(" 0.0 1 0", f" 0.0 {d} 0"))
    (carpeta / "IPC3.inp").write_text(texto)
    p = subprocess.run([str(BIN), "IPC3", *args], cwd=carpeta,
                       capture_output=True, text=True, errors="replace", timeout=600)
    out = carpeta / "IPC3.out"
    return p.returncode, out.read_text(errors="replace") if out.exists() else ""

with tempfile.TemporaryDirectory() as t:
    raiz = pathlib.Path(t)
    rc_c, out_c = corre(raiz / "control", 1, ["1", "0", "-mean"])
    rc_f, out_f = corre(raiz / "falla", 0, ["1", "1"])
    falla_c = "ESTIMATION FAILED" in out_c
    falla_f = "ESTIMATION FAILED" in out_f
    print(f"control (d=1, VAR(1) -mean): rc={rc_c}, ESTIMATION FAILED {'sí' if falla_c else 'no'}")
    print(f"niveles (d=0, VARMA(1,1)):  rc={rc_f}, ESTIMATION FAILED {'sí' if falla_f else 'no'}")
    if rc_c != 0 or falla_c or not out_c:
        print("el control no estima bien: el repro no prueba nada")
        sys.exit(2)
    if not falla_f:
        print("el caso ya no falla en la estimación: el repro no prueba nada")
        sys.exit(2)
    if rc_f == 0:
        print("ABIERTO: la estimación falla y el motor sale con 0")
        sys.exit(1)
    print(f"cerrado: la estimación fallida sale con {rc_f}")
