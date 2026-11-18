#!/usr/bin/env python3
"""BUG-0006 (gui/atsw): la misma Q de Ljung-Box con dos p, y Q(g.l.) leido
como Q(retardo).

Visto en el estudio de lo que art-python ya aprendio
(docs/ESTUDIO-art-python-asistente.md §3.6), que lo encontro igual que art
lo tuvo (sus BUG-0195 y BUG-0213):

- la ventana de anomalos (y atsw-cli anomalos) contrastaba la Q de m
  retardos con m grados de libertad, sin descontar los parametros ARMA. El
  dictamen (la diagnosis de la madre, la ficha) si los descuenta: la misma
  Q salia con dos p distintas;
- el dictamen rotulaba cada peldaño con los grados de libertad --«Q(10)»
  para la Q de 12 retardos de un AR(2)-- y decia «donde peor es en el
  retardo 10». Un fallo en el 12 se leia como del 10: cerca o lejos, y lo
  estacional, se confunden.

Lo que hace este guion: un proyecto con el golden RIPC.2 de fue (un AR(2))
y atsw-cli dictamen y anomalos sobre el. Sale con 1 si la Q de 39 retardos
no tiene la misma p en los dos, o si el dictamen no rotula Q(12); con 0 si
todo casa. Con 2 si falta algo (el arbol sin construir).

ARREGLADO (18-nov-2026): lib/outfile lee el retardo de cada peldaño
(lb_lag_), fueout_gl_q da los g.l. (m - p - q - P - Q); el dictamen escribe
«Q(12)=8.6 (10 g.l.)» y el retardo que es; anomalos, «Q(39), 37 g.l.».

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
  RIPC/m01:
    version: 1
    padre: m00
    razon: "el AR(2) del golden"
"""

def corre(*a):
    return subprocess.run([str(CLI), *a], capture_output=True, text=True).stdout

def main():
    if not CLI.exists() or not (G / "RIPC.2.out").exists():
        print("falta atsw-cli o el golden de fue: make primero", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as t:
        p = pathlib.Path(t)
        (p / "RIPC/work").mkdir(parents=True)
        shutil.copy(G / "RIPC.2.pre", p / "RIPC/work/RIPC_m00.inp")
        shutil.copy(G / "RIPC.2.pre", p / "RIPC/work/RIPC_m01.pre")
        shutil.copy(G / "RIPC.2.out", p / "RIPC/work/RIPC_m01.out")
        (p / "proyecto.yaml").write_text(YAML)
        y = str(p / "proyecto.yaml")
        dx = corre("dictamen", y, "RIPC", "m01")
        an = corre("anomalos", y, "RIPC", "m01")
        print("dictamen:", next((l.strip() for l in dx.splitlines() if "Autocorr" in l), ""))
        print("anomalos:", next((l.strip() for l in an.splitlines() if "Q(" in l), ""))
        misma_p = "p=0.217" in dx and "(p = 0.217)" in an
        rotulo = "Q(12)=" in dx
        if not misma_p:
            print("BUG-0006 ABIERTO: la misma Q tiene dos p")
        if not rotulo:
            print("BUG-0006 ABIERTO: el dictamen rotula la Q con los g.l., no con el retardo")
        if misma_p and rotulo:
            print("BUG-0006 arreglado")
            return 0
        return 1

if __name__ == "__main__":
    sys.exit(main())
