#!/usr/bin/env python3
"""BUG-0002 (fue): una línea de la cabecera de 89 caracteres o más cuenta como dos.

fue lee las cinco primeras líneas del .inp («may contain anything») con
fgets( dumstrg, 90 ) (fue.c [3.0]), y su validador hace lo mismo
(inpcheck.c, LABEL_MAX 90). Una línea de 89 caracteres o más no cabe: fgets
la parte en dos, la cabecera se come una línea de más y todo lo que sigue va
desplazado. El validador lo rechaza con un mensaje que no dice la causa:
«line 6: the frequency must be 1, 4, 12 or 'number' (it is '**')».

El mismo fichero con la línea 3 de 88 caracteres (control) se lee bien; con
120 no. Sale con 1 mientras el fallo siga abierto, con 0 cuando esté
arreglado, y con 2 si el control falla (el repro no prueba nada).
No depende del estado de git: la línea larga se escribe aquí.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
FUE = AQUI.parents[1] / "bin" / ("fue.exe" if sys.platform == "win32" else "fue")
BASE = (AQUI / "IPC_ES_m01_ar1.inp").read_text().splitlines(keepends=True)

if not FUE.exists():
    sys.exit(f"falta {FUE}: make -C engines/fue")

def corre(d, n):
    """fue sobre el .inp con la línea 3 de n caracteres; (rc, mensaje)."""
    linea = "* " + "x" * (n - 2)
    assert len(linea) == n
    texto = BASE[:2] + [linea + "\n"] + BASE[3:]
    (d / f"h{n}.inp").write_text("".join(texto))
    p = subprocess.run([str(FUE), f"h{n}"], cwd=d, capture_output=True,
                       text=True, errors="replace")
    msg = [l for l in (p.stdout + p.stderr).splitlines() if "input file" in l]
    return p.returncode, (msg[0] if msg else "")

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    rc88, m88 = corre(d, 88)
    rc120, m120 = corre(d, 120)
    print(f"línea de 88 caracteres: rc={rc88} {m88}")
    print(f"línea de 120 caracteres: rc={rc120} {m120}")
    if rc88 != 0:
        print("el control (88) tampoco se lee: el repro no prueba nada")
        sys.exit(2)
    if rc120 != 0:
        print("ABIERTO: una línea larga de la cabecera desplaza la lectura")
        sys.exit(1)
    print("cerrado")
