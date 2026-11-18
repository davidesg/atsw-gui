#!/usr/bin/env python3
"""BUG-0001 (lib/ccfplot): con datos mensuales y 12 retardos el eje marca cada
4 retardos; GraphMaker marca cada 12.

El oráculo es GraphMaker (`SRC/GraphMaker/Projects/graphmakertri2/
ccfgrafico.cpp`, `TccfForm::FormCreate`): el incremento del eje lo decide la
FRECUENCIA — anual 2, trimestral 4 (el de la ficha), mensual 12, y mensual con
8 retardos 2 —. `paso_eje` lo decide por el NÚMERO de retardos (<= 8: 2;
<= 20: 4; si no, 12), así que el mensual de 12 retardos —el que
`ccf_lags_por_defecto` da por defecto— sale cada 4, y cualquier mensual entre
9 y 20 retardos también. El comentario de `paso_eje` dice lo correcto («12 al
mes»); el código no. Y `ccf_draw` no recibe la frecuencia, así que no puede
distinguir un mensual de un trimestral con los mismos retardos.

Compila un dibujo mínimo contra lib/ccfplot y lib/fugdraw, escribe el panel
mensual por defecto (12 retardos) en EPS y lee los rótulos del eje x. Sale con
1 mientras marque algo distinto de -12, 0, 12; con 0 cuando no.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

AQUI = pathlib.Path(__file__).resolve().parent
LIB = AQUI.parents[2]                      # lib/
C = r'''
#include "ccfplot.h"
int main( int c, char **v )
{
    double corr[25];
    int k;
    for ( k = 0; k < 25; k++ ) corr[k] = 0.0;
    corr[12 + 3] = 0.3;
    return ccf_write_eps( v[1], corr, 12, 216, "X", "Y", 10.0, 40 );
}
'''


def main():
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        print("no hay compilador de C")
        return 2
    d = pathlib.Path(tempfile.mkdtemp(prefix="ccfplot-"))
    (d / "r.c").write_text(C)
    exe, eps = d / "r", d / "r.eps"
    subprocess.run([cc, "-I", str(LIB / "ccfplot"), "-I", str(LIB / "fugdraw"),
                    str(d / "r.c"), str(LIB / "ccfplot" / "ccfplot.c"),
                    str(LIB / "fugdraw" / "fugdraw.c"), "-lm", "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe), str(eps)], check=True)
    textos = re.findall(r"\(([^()]*)\)\s*Tj", eps.read_text(errors="replace"))
    enteros = sorted({int(t) for t in textos if re.fullmatch(r"-?\d+", t)})
    print("rótulos enteros del eje x:", enteros)
    ok = enteros == [-12, 0, 12]
    print("OK: cada 12, como GraphMaker" if ok else
          "FALLA: con datos mensuales y 12 retardos GraphMaker marca -12, 0, 12")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
