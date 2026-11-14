#!/usr/bin/env python3
"""BUG-0003 (gui/atsw, lib/proyecto): pr_escribir pisa en silencio un
manifiesto que no puede leer.

Visto en la auditoria de procesos (docs/AUDITORIA-procesos.md, C6), al
repasar lo bajo despues de las siete fases.

Que pasa: pr_escribir, transaccional desde la fase 1, lee el manifiesto del
disco para fusionar lo que otro proceso cambio. Si esa lectura FALLA --una
clave que este programa no conoce, escrita por una version mas nueva; un
schema_version mayor; un fichero estropeado a mano--, la trata como si el
fichero no existiera y escribe su copia encima. La clave nueva y cualquier
cambio de otro proceso se pierden, y devuelve 0: nadie se entera.

Lo que hace este guion: compila pisa.c contra lib/proyecto, escribe un
manifiesto, le anade una clave desconocida (como lo haria un programa mas
nuevo) y guarda encima un cambio con la copia que habia en memoria. Sale con
1 mientras pr_escribir devuelva 0 y la clave haya desaparecido; con 0 cuando
se niegue a pisarlo (o la conserve).
"""
import os, pathlib, shutil, subprocess, sys, tempfile

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol
AQUI = pathlib.Path(__file__).resolve().parent


def main():
    cc = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not cc:
        print("no hay compilador de C")
        return 2
    with tempfile.TemporaryDirectory() as d:
        exe = pathlib.Path(d, "pisa.exe" if os.name == "nt" else "pisa")
        r = subprocess.run([cc, "-I", str(R / "lib/proyecto"), str(AQUI / "pisa.c"),
                            str(R / "lib/proyecto/proyecto.c"), "-o", str(exe)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr)
            return 2
        r = subprocess.run([str(exe), d], capture_output=True, text=True)
        print(r.stdout.strip())
        return r.returncode


if __name__ == "__main__":
    sys.exit(main())
