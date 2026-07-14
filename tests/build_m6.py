#!/usr/bin/env python3
# drtran -- Box-Jenkins transfer function models.
# Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.  Distributed WITHOUT ANY WARRANTY; see COPYING for details.
"""Reconstruye los .pre de m6 a partir del legacy.

FUENTE. Los datos y la especificacion salen de drv-source/m6-1 (drv.c + m1.inp).
La TEORIA y la logica de estos modelos estan en:

    Relloso Pereda, S. (1997). "Un modelo multivariante para el empleo por
    sectores, activos y parados en Espana". Documento de trabajo ICAE 9720,
    Universidad Complutense. (Tesis dirigida por A. B. Treadway.)

Ese documento dice, y conviene tenerlo presente antes de tocar nada:

  - Datos: EPA trimestral, 1976:III - 1993:II, 69 observaciones.
  - "Todos los modelos presentados estan estimados por Maxima Verosimilitud
     Exacta (MVE) [...] no habria sido factible de no disponer de los algoritmos
     [...] desarrollados por Mauricio (1995, 1996 y 1997)."
    O sea: la escuela usaba elf, NO la aproximacion condicional de Box-Jenkins.
  - El PRIMER modelo multivariante es "un modelo pentavariante con DINAMICA
    DIAGONAL", motivado por "fuertes correlaciones contemporaneas entre algunos
    de los sectores", y "en la diagonal principal de la matriz MA se recogen los
    modelos UTI de las variables".

    Ese modelo es, exactamente, el caso -0 de drtran con covarianzas liberadas.
    Es el punto de entrada, y es el que este script prepara.

LAS SERIES (m1.inp, 6 columnas):
    1  P    poblacion en edad de trabajar     d=2
    2  EA   agricultura                       d=1, D=1 (anual, s=4)
    3  EP   servicios privados                d=2
    4  EI   industria                         d=2
    5  EU   servicios publicos                d=2
    6  EC   construccion                      d=2

Sin Box-Cox (niveles; lambda = 1) y sin reescalado.

LOS MA DIAGONALES del legacy, factorizados (convencion de fue: 1 - theta B):

    Theta_11 = (1 - x1 B)                                         MA(1)
    Theta_22 = (1 - x2 B)(1 + x4 B)(1 + x3 B^2)                   MA(1) x MA(1) x FF(f=1)
    Theta_33 = (1 - x5 B)... etc.   <-- se leen del propio drv.c

Los OFF-DIAGONAL del legacy son transferencias racionales cuyos numeradores estan
FACTORIZADOS -- p.ej. theta_36(B) = B (1 - x14 B)(x12 + x13 B) --, y eso drtran
todavia NO lo expresa: su tabla de slots hace LIBRE / FIJO / ALIAS, pero no
PRODUCTOS. Es la carencia que este ejercicio destapa.
"""
import os
import sys

# ── Los datos del legacy ────────────────────────────────────────────────────
LEGACY = "/home/david/Dropbox/SRC/drv-source/m6-1/m1.inp"

# nombre, d, D, deterministas (por nombre; ver GenDet en drv.c)
SERIES = [
    ("P",  2, 0, []),
    ("EA", 1, 1, ["s185", "s287", "s189", "s489", "s192"]),
    ("EP", 2, 0, ["s280", "s380", "s287", "s387", "s388", "s488",
                  "cos1", "sin1", "alt"]),
    ("EI", 2, 0, ["i280", "ic282", "s287", "s188", "s492", "s193",
                  "cos1", "sin1", "alt"]),
    ("EU", 2, 0, ["s187", "s287", "i388", "s192", "s492", "s193"]),
    ("EC", 2, 0, ["s283", "s289", "s389", "s192", "cos1", "sin1", "alt"]),
]

# Las 23 deterministas de drv.c (GenDet: primer trimestre 1976:III, s=4).
# tipo fue -> (tipo, periodo, subperiodo)
DET = {
    "s280":  ("step", 2, 1980),  "s380":  ("step", 3, 1980),
    "s283":  ("step", 2, 1983),  "s185":  ("step", 1, 1985),
    "s187":  ("step", 1, 1987),  "s287":  ("step", 2, 1987),
    "s387":  ("step", 3, 1987),  "s188":  ("step", 1, 1988),
    "s388":  ("step", 3, 1988),  "s488":  ("step", 4, 1988),
    "s189":  ("step", 1, 1989),  "s289":  ("step", 2, 1989),
    "s389":  ("step", 3, 1989),  "s489":  ("step", 4, 1989),
    "s192":  ("step", 1, 1992),  "s492":  ("step", 4, 1992),
    "s193":  ("step", 1, 1993),
    "i280":  ("impulse", 2, 1980), "i388": ("impulse", 3, 1988),
    "ic282": ("compimp", 2, 1982),
    "cos1":  ("cos", 1, 0), "sin1": ("sin", 1, 0), "alt": ("alter", 0, 0),
}

FREQ, BEG_SUB, BEG_YEAR = 4, 3, 1976


def read_legacy():
    with open(LEGACY) as f:
        toks = f.read().split()
    nser, nobs = int(toks[0]), int(toks[1])
    vals = [float(v) for v in toks[2:2 + nser * nobs]]
    cols = [[vals[t * nser + i] for t in range(nobs)] for i in range(nser)]
    return nobs, cols


def det_block(names):
    """Bloque determinista, en el formato EXACTO del .pre de fue:

         N
         **
         <spec 1>            p.ej.  "step 2 1980",  "cos 1",  "alter"
         ...
         **
         0 0 ... 0           un Nomega por variable (0 = solo omega_0)
         **
         <omega>  <flag>     uno por variable, cada uno tras su "**"
         ...
         **
         0 0 ... 0           un Ndelta por variable (0 = sin denominador)
    """
    if not names:
        return ["0"]
    out = ["%d" % len(names), "**"]
    for nm in names:
        typ, per, sub = DET[nm]
        if typ in ("cos", "sin"):
            out.append("%s %d" % (typ, per))
        elif typ == "alter":
            out.append("alter")
        else:
            out.append("%s %d %d" % (typ, per, sub))
    # Nomega de cada variable (0 = solo omega_0)
    out.append("**")
    out.append(" ".join("0" for _ in names) + " ")
    # los omegas, uno por variable
    for _ in names:
        out.append("**")
        out.append("0.000000  1")
    # Ndelta de cada variable (0 = sin denominador). SEGUNDA linea de enteros:
    # el .pre lleva DOS -- Nomega y Ndelta -- y omitir la segunda desplaza todo
    # el parseo y hace que la ultima observacion se lea como cero.
    out.append("**")
    out.append(" ".join("0" for _ in names) + " ")
    return out


def write_pre(path, name, data, d, D, dets):
    L = []
    L.append("************************************************")
    L.append("*        Input file for program DRVUS          *")
    L.append("*   m6 (Relloso 1997) reconstruido del legacy  *")
    L.append("************************************************")
    L.append("")
    L.append("** Frequency of time series: either 1(A), 4(Q) or 12(M):")
    L.append(" %d" % FREQ)
    L.append("** Number of observations and starting date of time series:")
    L.append(" %d  %d %d %s" % (len(data), BEG_SUB, BEG_YEAR, name))
    L.append("** Number of deterministic variables (including seasonal components):")
    L += det_block(dets)
    L.append("**Number and orders of regular AR operators:")
    L.append("0")
    L.append("** Number and orders of annual AR operators:")
    L.append("0")
    L.append("** Number and orders of regular MA operators:")
    L.append("1 1")
    L.append("**")
    L.append("0.3000 1")
    L.append("** Number and orders of anual MA operators:")
    L.append("0")
    L.append("** Number and frequencies of regular AR(2) operators with fixed frequency:")
    L.append("0")
    L.append("** Number and frequencies of regular MA(2) operators with fixed frequency:")
    L.append("0")
    L.append("** Mean parameter (mu):")
    L.append("0")
    L.append("** Box-Cox lambda, regular differences and complete annual differences:")
    L.append("1.00 %d %d" % (d, D))
    L.append("** Individual factors of the annual difference (from freq 0.0): ")
    L.append(" 0 0 0 0 0 0 0")
    L.append("** ACF/PACF bands (0 Automatic) and reescaling factor: ")
    L.append(" 0.00 1.00")
    L.append("** Time series (stochastic and non-standard deterministic variables): ")
    for v in data:
        L.append("%.10f " % v)
    open(path, "w").write("\n".join(L) + "\n")


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "tests/data/m6"
    os.makedirs(outdir, exist_ok=True)
    nobs, cols = read_legacy()
    print("m6 (Relloso 1997): %d series, %d observaciones (EPA trimestral, "
          "1976:III-1993:II)\n" % (len(cols), nobs))
    for i, (name, d, D, dets) in enumerate(SERIES):
        p = os.path.join(outdir, "M6_%s.pre" % name)
        write_pre(p, name, cols[i], d, D, dets)
        print("  %-3s  d=%d D=%d  %2d deterministas  -> %s"
              % (name, d, D, len(dets), p))
    print("\nCada serie con su MA(1) regular como arranque. Los factores MEG y los")
    print("compartidos se anaden despues, siguiendo el documento.")


if __name__ == "__main__":
    main()
