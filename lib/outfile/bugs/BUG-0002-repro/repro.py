#!/usr/bin/env python3
"""BUG-0002 (lib/outfile): todo modelo «estima la media», aunque mu esté fija.

fue escribe SIEMPRE la cabecera «Mean parameter (mu):», también cuando mu
está fija en 0 (entonces la línea siguiente es «0.000000», sin error típico
ni índice). El lector pone tiene_mu = TRUE al ver la cabecera, así que:

  - la Diagnosis dice en «Media» «El modelo estima la media, así que ésta es
    cero por construcción» y no contrasta la media de los residuos de un
    modelo que NO la estima (un D = 1 sin mu, o el D = 0 que deriva la
    identificación, que la deja fija en 0);
  - dx_nombre_par llama «μ» al último parámetro estimado (en el D = 0 con
    AR(1)+MA(1) x SAR(1) que deriva la madre: «[12] y μ van juntos», cuando
    el par es el AR [12] y el MA [14]).

IPC subyacente (docs/manual/datos/es_core.csv), 2002-2019:
  sin_mu.out  ARIMA(3,1,0)(0,1,1)12 sin media (4 parámetros)
  con_mu.out  armónicos + AR(3) x SAR(1) + 3 escalones + mu libre [19]

Sale con 1 mientras el fallo siga abierto, con 0 cuando esté arreglado.
"""
import pathlib, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
LIB = AQUI.parents[1]                      # lib/outfile
C = r'''
#include <stdio.h>
#include "outfile.h"
int main( int c, char **v )
{
    FueOut o;
    if ( !fueout_read( v[1], &o ) ) return 2;
    printf( "%d\n", o.tiene_mu ? 1 : 0 );
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
    def mu(f):
        out = subprocess.run([str(d / "r"), str(AQUI / f)],
                             capture_output=True, text=True).stdout.split()
        if len(out) != 1:
            sys.exit(f"no pude leer {f}")
        return int(out[0])
    sin, con = mu("sin_mu.out"), mu("con_mu.out")
    print(f"tiene_mu: sin_mu.out = {sin} (debe ser 0), con_mu.out = {con} (debe ser 1)")
    if (sin, con) != (0, 1):
        print("ABIERTO: la cabecera «Mean parameter» basta para creer que mu se estima")
        sys.exit(1)
    print("cerrado")
