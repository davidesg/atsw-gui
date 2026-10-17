#!/usr/bin/env python3
"""BUG-0001 (fue): el .pre no es el óptimo exacto, va redondeado.

fue escribe el .pre con los AR/MA y los delta a 4 decimales y los omega a 6
(fue.c, la escritura del .pre). Estimar otra vez sobre ese .pre tendría que
dejar los números quietos en 0 o 1 iteraciones; hoy cuesta 10.

Sale con 1 mientras el fallo siga abierto, con 0 cuando esté arreglado.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
FUE = AQUI.parents[2] / "fue" / "bin" / "fue"

def iteraciones(d, base):
    subprocess.run([str(FUE), base], cwd=d, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
    txt = (d / f"{base}.out").read_text(errors="replace")
    m = re.search(r"CONVERGENCE OBTAINED AFTER\s+(\d+)\s+ITERATIONS", txt)
    if not m:
        sys.exit(f"no veo la convergencia en {base}.out")
    return int(m.group(1))

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    shutil.copy(AQUI / "IPC_ES_m01_ar1.inp", d / "a.inp")
    n1 = iteraciones(d, "a")
    shutil.copy(d / "a.pre", d / "b.inp")
    n2 = iteraciones(d, "b")
    print(f"desde el .inp: {n1} iteraciones; desde su propio .pre: {n2}")
    if n2 > 1:
        print("ABIERTO: el .pre no es el óptimo (va redondeado)")
        sys.exit(1)
    print("cerrado")
