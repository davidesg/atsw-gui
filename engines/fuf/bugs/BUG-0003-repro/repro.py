#!/usr/bin/env python3
"""BUG-0003 (fuf): con una serie anual no hay tabla de previsión.

forecast_table_acii (src/usfo.c) solo escribe la tabla con freq > 1, o con
una Box-Cox negativa (su tabla propia). Una serie anual en niveles o en
logaritmos se queda sin FORECAST REPORT en el .out: los números solo salen en
la tabla LaTeX y en el PDF, que no son para leer a máquina. El porte de Python
(fue-python, report._fuf_section_forecast_table) sí la escribe.

Se ve con el visón de Jenkins y Alavi (docs/ejemplos/jenkins_alavi, MINK/m01):
fue -f escribe el .inp de fuf desde el .pre, y fuf no deja tabla.

Sale con 1 mientras el .out no traiga la tabla con sus filas, con 0 cuando sí.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
RAIZ = AQUI.parents[3]
FUE = RAIZ / "engines/fue/bin/fue"
FUF = AQUI.parents[1] / "bin" / "fuf"
PRE = RAIZ / "docs/ejemplos/jenkins_alavi/MINK/work/MINK_m01.pre"


def main():
    with tempfile.TemporaryDirectory() as d:
        d = pathlib.Path(d)
        shutil.copy(PRE, d / "MINK_m01.inp")
        subprocess.run([str(FUE), "MINK_m01", "-f"], cwd=d, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False)
        subprocess.run([str(FUF), "forecast_MINK_m01"], cwd=d, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False)
        out = d / "forecast_MINK_m01.out"
        txt = out.read_text(errors="replace") if out.exists() else ""
        if "FORECAST REPORT" not in txt:
            print("fuf no escribe la tabla de previsión de una serie anual")
            return 1
        filas = [l for l in txt[txt.index("FORECAST REPORT"):].split("\n")
                 if re.match(r"^\s+\d{4}\s+\d", l)]
        print(f"la tabla de la serie anual, con {len(filas)} filas")
        return 0 if len(filas) > 1 else 1


if __name__ == "__main__":
    sys.exit(main())
