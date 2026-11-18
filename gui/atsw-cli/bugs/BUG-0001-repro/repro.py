#!/usr/bin/env python3
"""BUG-0001 (atsw-cli): especifica --intervencion repite una intervencion que
el .inp ya trae.

Visto al grabar la parte de la terminal del video del asistente
(18-nov-2026): se le pide al asistente «un hijo de m02 sin el escalon de
octubre de 2007». Deriva el hijo (new_version: el .pre de m02, con sus dos
escalones) y llama a write_inp con interventions=["step 9 2012"], creyendo
que la lista SUSTITUYE a la que hay. especifica las AÑADE: el hijo queda con
step 9 2012, step 10 2007 y otra vez step 9 2012. fue lo acepta y lo estima:
dos columnas iguales, un modelo singular (el hessiano no es definido
positivo) con la misma verosimilitud que el padre.

Lo que hace este guion: un proyecto con el golden RIPC.2 de fue (sus datos
traen «step 2 2002»), un modelo nuevo de RIPC, y especifica con
--intervencion "step 2 2002". Sale con 1 si el .inp queda con la
intervencion dos veces; con 0 si especifica lo rechaza y el .inp queda como
estaba. Con 2 si falta algo (el arbol sin construir).

ARREGLADO (18-nov-2026): especifica rechaza una intervencion igual a una
que el .inp ya trae (o repetida en la orden), y dice que se AÑADEN y que
para un modelo sin una de ellas se parte del .pre de antes. Una sobre la
misma fecha y de otra forma se añade con un aviso, como en art-python (su
BUG-0162).

    python3 repro.py
"""
import pathlib, shutil, subprocess, sys, tempfile

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol
CLI = R / "gui/atsw-cli/atsw-cli"
G = R / "engines/fue/tests/golden/RIPC.2"

YAML = """schema_version: 2
id: repro
titulo: ""
creado: "2026-11-18"
analista: ""
herramienta: fue
raiz: .

series:
  RIPC:

modelos:
  RIPC/m00:
    version: 0
    padre: ""
    razon: ""
    rol: datos
"""

def main():
    if not CLI.exists() or not (G / "RIPC.2.pre").exists():
        print("falta atsw-cli o el golden de fue: make primero", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as t:
        p = pathlib.Path(t)
        (p / "RIPC/work").mkdir(parents=True)
        shutil.copy(G / "RIPC.2.pre", p / "RIPC/work/RIPC_m00.inp")
        (p / "proyecto.yaml").write_text(YAML)
        y = str(p / "proyecto.yaml")
        subprocess.run([str(CLI), "proyecto", "nuevo", y, "RIPC", "--razon", "repro"],
                       check=True, capture_output=True)
        inp = p / "RIPC/work/RIPC_m01.inp"
        antes = inp.read_text()
        r = subprocess.run([str(CLI), "especifica", y, "RIPC", "m01",
                            "--intervencion", "step 2 2002"], capture_output=True, text=True)
        veces = [l.strip() for l in inp.read_text().splitlines()].count("step 2 2002")
        print(f"estado {r.returncode}; «step 2 2002» en el .inp: {veces} vez/veces")
        print((r.stdout + r.stderr).strip())
        if veces > 1:
            print("BUG-0001 ABIERTO: la intervencion queda dos veces")
            return 1
        if r.returncode == 0 or inp.read_text() != antes:
            print("BUG-0001: no se repite, pero especifica no lo rechazo limpio")
            return 1
        print("BUG-0001 arreglado: se rechaza y el .inp queda como estaba")
        return 0

if __name__ == "__main__":
    sys.exit(main())
