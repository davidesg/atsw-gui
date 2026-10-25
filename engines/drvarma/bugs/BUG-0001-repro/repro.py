#!/usr/bin/env python3
"""BUG-0001 (drvarma): los .inp de -split no se leen si su cabecera es larga.

`drvarma -split` escribe en la línea 4 de cada .inp univariante
«* Split from <ruta del .inp multivariante> by drvarma <versión (git ...)>»
(split.c:87), de longitud libre. fue y su validador (que es el que usa la
escalera para reconocer el fichero) leen esa cabecera con fgets de 90 bytes:
con 89 caracteres o más la línea cuenta como dos y el fichero se rechaza
(fue BUG-0002). Con la versión de hoy («5.0.0 (git v0.2.0-4-g3f56422f48)»)
y la ruta de la batería, la línea mide 84; con «-dirty», 90. Una ruta larga
lo dispara igual sin -dirty, y así lo hace este repro: no depende del estado
de git.

Segunda parte: build/split.o no depende de build/git_hash.h (Makefile:141),
así que tras un commit un make incremental deja en -split la versión vieja
(«-dirty» incluido) aunque -version ya diga la nueva.

Sale con 1 mientras alguna de las dos siga abierta, con 0 cuando estén
arregladas, con 2 si el control (ruta corta) falla.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
DRV = AQUI.parents[1]
BIN = DRV / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")

if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

def split_y_escalera(raiz, carpeta, relativa):
    """-split de carpeta/IPC3 (o de IPC3, relativa, desde carpeta) y la
    escalera VAR(1) sobre lo que escribe."""
    carpeta.mkdir(parents=True)
    shutil.copy(AQUI / "IPC3.inp", carpeta / "IPC3.inp")
    sal = raiz / ("s_" + str(len(str(carpeta))))
    sal.mkdir()
    fuente = "IPC3" if relativa else str(carpeta / "IPC3")
    subprocess.run([str(BIN), "-split", fuente, "-mean", "-ar", "1",
                    "-dir", str(sal)], cwd=carpeta, capture_output=True, check=True)
    cab = (sal / "IPC_ES.inp").read_text().splitlines()[3]
    p = subprocess.run([str(BIN)] + [str(sal / f"IPC_{s}.inp") for s in ("ES", "FR", "DE")]
                       + ["1", "0", "-o", str(sal / "lad")],
                       capture_output=True, text=True, errors="replace")
    ok = p.returncode == 0 and (sal / "lad.out").exists()
    err = next((l for l in p.stdout.splitlines() if l.startswith("ERROR")), "")
    return len(cab), ok, err

abiertos = []
with tempfile.TemporaryDirectory() as t:
    raiz = pathlib.Path(t)
    n1, ok1, _ = split_y_escalera(raiz, raiz / "c", True)
    n2, ok2, err2 = split_y_escalera(raiz, raiz / ("d" * 90), False)
    print(f"ruta corta: cabecera de {n1} caracteres, escalera {'bien' if ok1 else 'FALLA'}")
    print(f"ruta larga: cabecera de {n2} caracteres, escalera {'bien' if ok2 else 'FALLA'}")
    if err2:
        print("   " + err2[:160])
    if not ok1 or n1 >= 89:
        print("el control (ruta corta) no vale: el repro no prueba nada")
        sys.exit(2)
    if not ok2:
        abiertos.append("la cabecera larga de -split hace ilegible el .inp")

# El Makefile: ¿se recompila split.o cuando cambia el hash?
p = subprocess.run(["make", "-C", str(DRV), "-pqn"], capture_output=True,
                   text=True, errors="replace")
reglas = [l for l in p.stdout.splitlines() if re.match(r"^build/split\.o\s*:", l)]
dep = any("git_hash.h" in l for l in reglas)
print(f"Makefile: build/split.o depende de build/git_hash.h: {'sí' if dep else 'NO'}")
if not dep:
    abiertos.append("split.o no se recompila al cambiar el hash")

if abiertos:
    print("ABIERTO: " + "; ".join(abiertos))
    sys.exit(1)
print("cerrado")
