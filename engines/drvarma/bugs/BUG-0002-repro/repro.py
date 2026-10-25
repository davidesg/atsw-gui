#!/usr/bin/env python3
"""BUG-0002 (drvarma): la vía .inp desborda el montón con un nombre largo.

drvarma.c:173-177 reserva 80 bytes para el nombre (inputf, outputf,
base_name) y copia en ellos argv[1] con strcpy; luego les añade «.inp» y
«.out» con strcat (drvarma.c:259-261). Un nombre de 76 caracteres o más
escribe fuera del bloque; con uno de ~100 glibc aborta
(«malloc(): corrupted top size») antes de escribir el .out.

Con un nombre corto (control) el mismo .inp se estima bien. Sale con 1
mientras el fallo siga abierto, con 0 cuando esté arreglado y con 2 si el
control falla. No depende del estado de git.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")

if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

def via_inp(raiz, carpeta):
    """La vía .inp (VAR(1) con -mean) sobre carpeta/IPC3: (rc, longitud, .out?)."""
    carpeta.mkdir(parents=True)
    shutil.copy(AQUI / "IPC3.inp", carpeta / "IPC3.inp")
    nombre = str(carpeta / "IPC3")
    p = subprocess.run([str(BIN), nombre, "1", "0", "-mean"], cwd=raiz,
                       capture_output=True, text=True, errors="replace")
    return p.returncode, len(nombre), (carpeta / "IPC3.out").exists()

with tempfile.TemporaryDirectory() as t:
    raiz = pathlib.Path(t)
    rc1, n1, out1 = via_inp(raiz, raiz / "c")
    rc2, n2, out2 = via_inp(raiz, raiz / ("d" * 100))
    print(f"nombre de {n1} caracteres: rc={rc1}, .out {'sí' if out1 else 'no'}")
    print(f"nombre de {n2} caracteres: rc={rc2}, .out {'sí' if out2 else 'no'}")
    if rc1 != 0 or not out1 or n1 >= 76:
        print("el control (nombre corto) no vale: el repro no prueba nada")
        sys.exit(2)
    if rc2 != 0 or not out2:
        print("ABIERTO: un nombre largo desborda los búferes de 80 bytes")
        sys.exit(1)
    print("cerrado")
