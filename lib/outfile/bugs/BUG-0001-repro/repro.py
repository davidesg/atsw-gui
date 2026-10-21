#!/usr/bin/env python3
"""BUG-0001 (lib/outfile): el AR entra en la ganancia del último escalón.

IPC, muestra completa, m04: dos escalones (step 3 2022, con 4 omegas, y
step 1 2021, con 2) y un AR(1) detrás. El lector del .out contaba como omega
del último determinista toda línea «valor (et) [n]» hasta «Box-Cox lambda»,
así que el AR [18] salía como tercer omega de step 1 2021 y la ventana de
Ganancia lo restaba. Los «Deltas for deterministic variable» caían igual.

Compila un lector mínimo contra lib/outfile y mira cuántos omegas tiene el
determinista 13. Sale con 1 mientras el fallo siga abierto, con 0 cuando
esté arreglado.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
LIB = AQUI.parents[1]                      # lib/outfile
C = r'''
#include <stdio.h>
#include "outfile.h"
int main( int c, char **v )
{
    FueOut o;
    if ( !fueout_read( v[1], &o ) ) return 2;
    printf( "%d %d\n", o.det_nom[12], o.det_i0[12] );
    return 0;
}
'''

def flags():
    r = subprocess.run(["pkg-config", "--cflags", "--libs", "glib-2.0"],
                       capture_output=True, text=True, check=True)
    return r.stdout.split()

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    (d / "r.c").write_text(C)
    subprocess.run(["cc", "-o", str(d / "r"), str(d / "r.c"),
                    str(LIB / "outfile.c"), f"-I{LIB}", *flags(), "-lm"],
                   check=True)
    out = subprocess.run([str(d / "r"), str(AQUI / "ipc_m04.out")],
                         capture_output=True, text=True).stdout.split()
    if len(out) != 2:
        sys.exit("no pude leer ipc_m04.out")
    nom, i0 = map(int, out)
    print(f"step 1 2021: {nom} omegas desde el parámetro {i0} (deben ser 2 desde el 16)")
    if (nom, i0) != (2, 16):
        print("ABIERTO: el AR (o un delta) cuenta como omega")
        sys.exit(1)
    print("cerrado")
