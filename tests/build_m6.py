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

LO QUE ESTE SCRIPT GRABA: LA TABLA 4 (representacion MEG mas estocastica).

Decision (jul-2026): NO re-identificar cada serie, sino GRABAR la especificacion
publicada en la Tabla 4 de Relloso (pp. 23-24 del 9720.pdf) como valores de arranque
de los seis .pre de la diagonal. drtran -0 los re-estima conjuntamente. Detalle y
verificacion: docs/M6_TABLA4_BASELINE.md (la linea de contexto de este trabajo).

  - Se toma la representacion MAS ESTOCASTICA: ∇∇₄ + testigos MEG (λ₁ en π/2, λ₂ en
    Nyquist π) para EA/EP/EI/EC; ∇² con MA(1) para P/EU (sin estacional).
  - Las INTERVENCIONES son las de la TABLA 4 (univariante baseline), NO las del m1.inp
    legacy (que son las del multivariante FINAL, paso 4, y traen mas incidentes).

CODIFICACION .pre DEL MEG ∇∇₄ (verificada vs EA_meg2.pre de ART y vs fue_pre_reader.c):
  - ∇∇₄ = (1-B)²(1+B)(1+B²)  =>  d=2, D=0, ifadf=[0,1,1]  (¡NO d=1!). El (1-B) interior
    de ∇₄ suma a la diferencia regular: son DOS raices a f0.
  - θ (f0):  MA regular, factor (1 - θB).
  - λ₂ (Nyquist π): 2º factor de la MA regular, lineal, (1 - λ₂B).
  - λ₁ (π/2): MA(2) de frecuencia fija a la frecuencia indice 1, (1 - λ₁B²).
  - SIGNO: se graba el valor de Tabla 4 VERBATIM (θ>0; λ₁,λ₂<0).
  - ifadf para s=4 son 3 valores [f0, π/2, π] (drtran reserva freq/2+1).

Los OFF-DIAGONAL del legacy (paso 4) siguen siendo transferencias con numerador
FACTORIZADO -- p.ej. theta_36(B) = B (1 - x14 B)(x12 + x13 B) --, que drtran todavia
NO expresa (su tabla de slots hace LIBRE / FIJO / ALIAS, pero no PRODUCTOS). Esa es la
carencia que el ejercicio destapa; es del paso 4, no de esta diagonal de arranque.
"""
import os
import sys

# ── Los datos del legacy ────────────────────────────────────────────────────
LEGACY = "/home/david/Dropbox/SRC/drv-source/m6-1/m1.inp"

# Series LISTAS para grabar (las seis).
READY = {"P", "EA", "EP", "EI", "EU", "EC"}

# INTERVENCIONES COMPUESTAS (dos parametros ω₀,ω₁, con ganancia g=ω₀-ω₁ en Tabla 4):
# Se graban los valores de Relloso DIRECTOS (ω₀, ω₁): drtran aplica el numerador en
# la convencion Box-Jenkins ω(B)=ω₀-ω₁B (como fue calcnu), asi que la ganancia
# permanente ω₀-ω₁ = g coincide sola.  (Antes se negaba ω₁ para compensar un bug de
# signo en drtran, ya corregido; ver la auditoria del signo en TODO.md.)

# Cada serie:  (name, kind, theta, lam1, lam2, dets)
#   kind : "reg2" = ∇²  (d=2, ifadf 0 0 0, MA regular de 1 factor)
#          "dd4"  = ∇∇₄ (d=2, ifadf 0 1 1, MA regular θ+λ₂, MA(2) freq fija λ₁)
#   theta: MA regular en f0 (Tabla 4, verbatim);  lam1: testigo π/2;  lam2: Nyquist.
#   dets : lista de (tipo_fue, per, year, [ω₀, ...]).  Los ω son los coeficientes de
#          arranque de la Tabla 4 (unidades reales, refactor=1). len==2 => compuesta.
# ORDEN = columnas de m1.inp: P, EA, EP, EI, EU, EC (no cambiar; indexa los datos).
SERIES = [
    ("P",  "reg2", 0.82, None,  None,  []),
    ("EA", "dd4",  0.43, -0.68, -0.72, [
        ("step", 1, 1985, [86.5]),
        ("step", 2, 1987, [63.3]),
        ("step", 1, 1989, [-84.0]),
        ("step", 4, 1989, [-70.9]),
        ("step", 1, 1992, [42.4]),
    ]),
    # EP: DISCREPANCIA univariante↔final, resuelta A FAVOR DEL LEGACY.
    #   - Tabla 4 (univariante) da DOS filas, ninguna ∇² all-determinista: ∇∇₄
    #     (λ₁=-.78, λ₂=-.87) y ∇²(1+B²) (π/2 estocástica λ₁=-.80, Nyquist det). Ambas
    #     dejan π/2 ESTOCÁSTICA.
    #   - El legacy m6-1 (drv.c, x[36-38]) la modela ∇² con estacionalidad DETERMINISTA
    #     en todas las frecuencias (cos/sin/alter) + MA(1). El paso 4 la simplificó.
    # Decisión (usuario): "como legacy" → ∇² determinista. Deterministas estacionales =
    # valores del legacy (Tabla 4 no da esta fila); MA(1) e intervención (II/87) de Tabla 4.
    ("EP", "reg2", 0.64, None,  None,  [
        ("step", 2, 1987, [179.2]),        # II/87 (Tabla 4, fila ∇²)
        ("cos", 1, 0, [1.2]),              # π/2 determinista (legacy x[36-37])
        ("sin", 1, 0, [36.2]),
        ("alter", 0, 0, [-3.4]),           # Nyquist determinista (legacy x[38] / T4 -3.6)
    ]),
    # EI: la representacion ∇∇₄ de Tabla 4 tiene λ₁=λ₂=-1.0 (testigos en la frontera =
    # estacionalidad DE FACTO DETERMINISTA). Grabar -1.0 seria degenerado (MA no
    # invertible que cancela la raiz AR). Se graba la fila ∇² DETERMINISTA de Tabla 4
    # (θ=.43 + armonicos cos/sin/alter), que es el modelo equivalente y estimable, y el
    # que uso el legacy. Ver docs/M6_TABLA4_BASELINE.md §6 punto 2.
    ("EI", "reg2", 0.43, None,  None,  [
        ("impulse", 2, 1980, [-49.1]),
        ("compimp", 2, 1982, [18.2]),
        ("step", 2, 1987, [63.2]),
        ("step", 1, 1988, [-53.2]),
        ("step", 4, 1992, [-60.9, 81.3]),      # compuesta (Relloso ω₀,ω₁ directos; g=-142.2)
        ("cos", 1, 0, [4.8]),
        ("sin", 1, 0, [2.3]),
        ("alter", 0, 0, [3.1]),
    ]),
    ("EU", "reg2", 0.88, None,  None,  [        # ∇², sin estacional
        ("step", 2, 1987, [-60.8, 118.7]),     # compuesta (Relloso ω₀,ω₁ directos; g=-179.5)
        ("impulse", 3, 1988, [-60.3]),
        ("step", 1, 1992, [-44.6]),
        ("step", 4, 1992, [-44.5, 39.2]),      # compuesta (Relloso ω₀,ω₁ directos; g=-83.7)
    ]),
    # EC: el legacy m6-1 (drv.c) la modela ∇² con estacionalidad DETERMINISTA
    # (cos/sin/alter) + MA(1), NO ∇∇₄. Coincide con la fila ∇² de la Tabla 4
    # (θ=.62, σ=18.0 < 19.0 de la ∇∇₄) y con lo que el diagonal reveló (su testigo
    # π/2 se iba a la frontera -1.0 = de facto determinista). Se graba esa fila.
    ("EC", "reg2", 0.62, None,  None,  [
        ("step", 1, 1992, [-30.2]),
        ("cos", 1, 0, [-2.4]),
        ("sin", 1, 0, [19.7]),
        ("alter", 0, 0, [2.3]),
    ]),
]

FREQ, BEG_SUB, BEG_YEAR = 4, 3, 1976


def read_legacy():
    with open(LEGACY) as f:
        toks = f.read().split()
    nser, nobs = int(toks[0]), int(toks[1])
    vals = [float(v) for v in toks[2:2 + nser * nobs]]
    cols = [[vals[t * nser + i] for t in range(nobs)] for i in range(nser)]
    return nobs, cols


def det_block(dets):
    """Bloque determinista, en el formato EXACTO del .pre de fue:

         N
         **
         <spec 1>            p.ej.  "step 2 1987",  "impulse 3 1988"
         ...
         **
         n0 n1 ... nk        un Nomega por variable (0 = solo ω₀; 1 = ω₀,ω₁ compuesta)
         **
         <ω₀>  <flag>        Nomega+1 lineas por variable, cada bloque tras su "**"
         [<ω₁>  <flag>]
         ...
         **
         0 0 ... 0           un Ndelta por variable (0 = sin denominador)

    Cada det es (tipo_fue, per, year, [ω₀, ...]); Nomega = len(ω)-1.
    """
    if not dets:
        return ["0"]
    out = ["%d" % len(dets), "**"]
    for typ, per, year, _w in dets:
        if typ in ("cos", "sin"):
            out.append("%s %d" % (typ, per))
        elif typ == "alter":
            out.append("alter")
        else:
            out.append("%s %d %d" % (typ, per, year))
    # Nomega de cada variable (len(ω)-1)
    out.append("**")
    out.append(" ".join(str(len(w) - 1) for _, _, _, w in dets) + " ")
    # los omegas: Nomega+1 valores de arranque (Tabla 4) por variable
    for _, _, _, w in dets:
        out.append("**")
        for om in w:
            out.append("%.6f  1" % om)
    # Ndelta de cada variable (0 = sin denominador). SEGUNDA linea de enteros:
    # el .pre lleva DOS -- Nomega y Ndelta -- y omitir la segunda desplaza todo
    # el parseo y hace que la ultima observacion se lea como cero.
    out.append("**")
    out.append(" ".join("0" for _ in dets) + " ")
    return out


def ma_block(kind, theta, lam1, lam2):
    """Las secciones ARMA + MEG del modelo, segun la representacion Tabla 4.

    kind "dd4": MA regular de 2 factores (θ en f0, λ₂ en Nyquist) + MA(2) de
                frecuencia fija (λ₁ en π/2) + ifadf [0,1,1], d=2.
    kind "reg2": MA regular de 1 factor (θ) + sin MA(2) freq fija + ifadf [0,0,0], d=2.
    """
    L = ["**Number and orders of regular AR operators:", "0",
         "** Number and orders of annual AR operators:", "0",
         "** Number and orders of regular MA operators:"]
    if kind == "dd4":                       # θ (f0) y λ₂ (Nyquist), ambos orden 1
        L += ["2 1 1", "**", "%.4f 1" % theta, "**", "%.4f 1" % lam2]
    else:                                   # solo θ
        L += ["1 1", "**", "%.4f 1" % theta]
    L += ["** Number and orders of anual MA operators:", "0",
          "** Number and frequencies of regular AR(2) operators with fixed frequency:", "0",
          "** Number and frequencies of regular MA(2) operators with fixed frequency:"]
    if kind == "dd4":                       # λ₁ (π/2): 1 factor a la frecuencia indice 1
        L += ["1 1.000000", "**", "%.4f 1" % lam1]
    else:
        L += ["0"]
    L += ["** Mean parameter (mu):", "0",
          "** Box-Cox lambda, regular differences and complete annual differences:",
          "1.00 2 0",                       # d=2, D=0 (∇∇₄ via ifadf; ∇² sin factores)
          "** Individual factors of the annual difference (from freq 0.0): ",
          (" 0 1 1" if kind == "dd4" else " 0 0 0")]
    return L


def write_pre(path, name, data, kind, theta, lam1, lam2, dets):
    L = []
    L.append("************************************************")
    L.append("*        Input file for program DRVUS          *")
    L.append("*   m6 (Relloso 1997, Tabla 4) -- baseline     *")
    L.append("************************************************")
    L.append("")
    L.append("** Frequency of time series: either 1(A), 4(Q) or 12(M):")
    L.append(" %d" % FREQ)
    L.append("** Number of observations and starting date of time series:")
    L.append(" %d  %d %d %s" % (len(data), BEG_SUB, BEG_YEAR, name))
    L.append("** Number of deterministic variables (including seasonal components):")
    L += det_block(dets)
    L += ma_block(kind, theta, lam1, lam2)
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
    print("m6 (Relloso 1997, Tabla 4): %d series, %d observaciones (EPA trimestral, "
          "1976:III-1993:II)\n" % (len(cols), nobs))
    for i, (name, kind, theta, lam1, lam2, dets) in enumerate(SERIES):
        if name not in READY:
            print("  %-3s  PENDIENTE (intervenciones compuestas) -- no se graba"
                  % name)
            continue
        p = os.path.join(outdir, "M6_%s.pre" % name)
        write_pre(p, name, cols[i], kind, theta, lam1, lam2, dets)
        op = {"dd4": "∇∇₄", "reg2": "∇²"}[kind]
        meg = (" θ=%.2f λ₁=%.2f λ₂=%.2f" % (theta, lam1, lam2)
               if kind == "dd4" else " θ=%.2f" % theta)
        print("  %-3s  %-4s%s  %d interv.  -> %s"
              % (name, op, meg, len(dets), p))
    print("\nValores de arranque = Tabla 4. Verificar cada .pre en solitario vs Tabla 4")
    print("antes de usarlo en la diagonal (docs/M6_TABLA4_BASELINE.md §4).")


if __name__ == "__main__":
    main()
