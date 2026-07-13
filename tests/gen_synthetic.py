#!/usr/bin/env python3
# drtran -- Box-Jenkins transfer function models.
# Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.  Distributed WITHOUT ANY WARRANTY; see COPYING for details.
"""Genera un caso sintético de función de transferencia con verdad conocida.

Construye dos series en el espacio ESTACIONARIO que drtran reconstruirá:

    w_X[t] = phi_X * w_X[t-1] + e[t]                      (entrada, AR(1))
    N[t]   = phi_N * N[t-1]   + a[t]                      (ruido,   AR(1))
    w_Y[t] = sum_j nu_j * w_X[t-j] + N[t]                 (salida)

con nu la respuesta impulso de omega(B)/delta(B) * B^b.

Luego las integra a niveles: z = exp(cumsum(w)/100), de modo que la
transformación de fue (log, 1a diferencia, factor de reescalado 100) devuelve
exactamente las w simuladas. Escribe un .pre por serie.

Uso: gen_synthetic.py <dir_salida>
"""
import sys
import os
import math
import random

# ─── Verdad ──────────────────────────────────────────────────────────────
N_OBS   = 400
FREQ    = 12
YEAR0   = 2000

PHI_X   = 0.500      # AR(1) de la entrada
PHI_N   = 0.300      # AR(1) del ruido
SD_E    = 1.000      # innovación de la entrada
SD_A    = 0.500      # innovación del ruido

B_DELAY = 2          # retardo puro
R_ORD   = 0          # sin denominador
S_ORD   = 1          # numerador de orden 1
OMEGA   = [0.800, 0.400]   # omega_0, omega_1
DELTA   = []               # (vacío: r=0)

SEED    = 20260712


def impulse_response(omega, s, delta, r, b, length):
    """nu[t], t=1..length (misma recursión que compute_irf en tran_shootx.c)."""
    nu = [0.0] * (length + 1)
    for t in range(1, length + 1):
        lag = t - 1 - b
        acc = omega[lag] if 0 <= lag <= s else 0.0
        for j in range(1, r + 1):
            if t > j:
                acc += delta[j - 1] * nu[t - j]
        nu[t] = acc
    return nu


def write_pre(path, name, data, phi, mu_free):
    """Escribe un .pre de fue: AR(1), log, 1a diferencia, reescalado 100."""
    L = []
    L.append("************************************************")
    L.append("*        Input file for program DRVUS          *")
    L.append("*   Caso sintetico generado por gen_synthetic  *")
    L.append("************************************************")
    L.append("")
    L.append("** Frequency of time series: either 1(A), 4(Q) or 12(M):")
    L.append(" %d" % FREQ)
    L.append("** Number of observations and starting date of time series:")
    L.append(" %d  1 %d %s" % (len(data), YEAR0, name))
    L.append("** Number of deterministic variables (including seasonal components):")
    L.append("0")
    L.append("**Number and orders of regular AR operators:")
    L.append("1 1")
    L.append("**")
    L.append("%.4f 1" % phi)
    L.append("** Number and orders of annual AR operators:")
    L.append("0")
    L.append("** Number and orders of regular MA operators:")
    L.append("0")
    L.append("** Number and orders of anual MA operators:")
    L.append("0")
    L.append("** Number and frequencies of regular AR(2) operators with fixed frequency:")
    L.append("0")
    L.append("** Number and frequencies of regular MA(2) operators with fixed frequency:")
    L.append("0")
    L.append("** Mean parameter (mu):")
    L.append("0.000000  1" if mu_free else "0")
    L.append("** Box-Cox lambda, regular differences and complete annual differences:")
    L.append("0.00 1 0")
    L.append("** Individual factors of the annual difference (from freq 0.0): ")
    L.append(" 0 0 0 0 0 0 0")
    L.append("** ACF/PACF bands (0 Automatic) and reescaling factor: ")
    L.append(" 0.00 100.00")
    L.append("** Time series (stochastic and non-standard deterministic variables): ")
    for v in data:
        L.append("%.10f " % v)
    open(path, "w").write("\n".join(L) + "\n")


def build_case(outdir, tag, b, r, s, omega, delta, seed):
    """Genera un par (Y, X) con la transferencia indicada y devuelve la verdad."""
    rnd = random.Random(seed)
    burn = 200
    n = N_OBS + burn

    wx = [0.0] * n
    for t in range(1, n):
        wx[t] = PHI_X * wx[t - 1] + rnd.gauss(0.0, SD_E)

    nz = [0.0] * n
    for t in range(1, n):
        nz[t] = PHI_N * nz[t - 1] + rnd.gauss(0.0, SD_A)

    nu = impulse_response(omega, s, delta, r, b, 60)
    wy = [0.0] * n
    for t in range(n):
        acc = 0.0
        for j in range(1, 61):
            idx = t - (j - 1)
            if idx < 0:
                break
            acc += nu[j] * wx[idx]
        wy[t] = acc + nz[t]

    wx, wy = wx[burn:], wy[burn:]

    def to_levels(w, base):
        z, acc = [], 0.0
        for v in w:
            acc += v / 100.0
            z.append(base * math.exp(acc))
        return z

    write_pre(os.path.join(outdir, "%s_X.pre" % tag), tag + "_X",
              to_levels(wx, 50.0), PHI_X, mu_free=False)
    write_pre(os.path.join(outdir, "%s_Y.pre" % tag), tag + "_Y",
              to_levels(wy, 100.0), PHI_N, mu_free=False)

    print("%s -> VERDAD: b=%d r=%d s=%d omega=%s delta=%s"
          % (tag, b, r, s, omega, delta))


def build_two_inputs(outdir, seed):
    """Caso de DOS entradas: Y = nu1(B) X1 + nu2(B) X2 + N, con verdad conocida.

    Es la prueba del cast multivariante: si drtran recupera las DOS
    transferencias a la vez, m > 2 funciona.
    """
    rnd = random.Random(seed)
    burn = 200
    n = N_OBS + burn

    # verdad
    b1, r1, s1, om1 = 1, 0, 0, [0.700]          # X1: retardo 1, un solo omega
    b2, r2, s2, om2 = 0, 0, 1, [0.500, 0.300]   # X2: contemporaneo, dos omegas

    x1 = [0.0] * n
    x2 = [0.0] * n
    nz = [0.0] * n
    for t in range(1, n):
        x1[t] = 0.500 * x1[t - 1] + rnd.gauss(0.0, 1.0)
        x2[t] = 0.200 * x2[t - 1] + rnd.gauss(0.0, 1.5)   # entrada distinta
        nz[t] = PHI_N * nz[t - 1] + rnd.gauss(0.0, SD_A)

    nu1 = impulse_response(om1, s1, [], r1, b1, 60)
    nu2 = impulse_response(om2, s2, [], r2, b2, 60)

    wy = [0.0] * n
    for t in range(n):
        acc = 0.0
        for j in range(1, 61):
            idx = t - (j - 1)
            if idx < 0:
                break
            acc += nu1[j] * x1[idx] + nu2[j] * x2[idx]
        wy[t] = acc + nz[t]

    x1, x2, wy = x1[burn:], x2[burn:], wy[burn:]

    def to_levels(v, base):
        z, acc = [], 0.0
        for u in v:
            acc += u / 100.0
            z.append(base * math.exp(acc))
        return z

    write_pre(os.path.join(outdir, "SYN2_X1.pre"), "SYN2_X1",
              to_levels(x1, 50.0), 0.500, mu_free=False)
    write_pre(os.path.join(outdir, "SYN2_X2.pre"), "SYN2_X2",
              to_levels(x2, 30.0), 0.200, mu_free=False)
    write_pre(os.path.join(outdir, "SYN2_Y.pre"), "SYN2_Y",
              to_levels(wy, 100.0), PHI_N, mu_free=False)

    print("SYN2 -> VERDAD: X1 b=1 r=0 s=0 omega=%s | X2 b=0 r=0 s=1 omega=%s"
          % (om1, om2))


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    build_two_inputs(outdir, SEED + 2)

    # Caso 2: transferencia RACIONAL. nu decae geométricamente con razón delta,
    # que es lo que debe hacer aflorar la propuesta [B] (denominador r=1) en la
    # identificación, en lugar de una ristra de omegas libres.
    build_case(outdir, "SYNR", b=1, r=1, s=0,
               omega=[0.600], delta=[0.600], seed=SEED + 1)

    rnd = random.Random(SEED)

    burn = 200
    n = N_OBS + burn

    # entrada AR(1)
    wx = [0.0] * n
    for t in range(1, n):
        wx[t] = PHI_X * wx[t - 1] + rnd.gauss(0.0, SD_E)

    # ruido AR(1)
    nz = [0.0] * n
    for t in range(1, n):
        nz[t] = PHI_N * nz[t - 1] + rnd.gauss(0.0, SD_A)

    # transferencia
    nu = impulse_response(OMEGA, S_ORD, DELTA, R_ORD, B_DELAY, 60)
    # transfer[t] = sum_{j>=1} nu[j] * w_X[t-(j-1)]   (mismo índice que shootx)
    wy = [0.0] * n
    for t in range(n):
        acc = 0.0
        for j in range(1, 61):
            idx = t - (j - 1)
            if idx < 0:
                break
            acc += nu[j] * wx[idx]
        wy[t] = acc + nz[t]

    wx = wx[burn:]
    wy = wy[burn:]

    # integrar a niveles: z = exp(cumsum(w)/100) * base
    def to_levels(w, base):
        z, acc = [], 0.0
        for v in w:
            acc += v / 100.0
            z.append(base * math.exp(acc))
        return z

    zx = to_levels(wx, 50.0)
    zy = to_levels(wy, 100.0)

    write_pre(os.path.join(outdir, "SYN_X.pre"), "SYN_X", zx, PHI_X, mu_free=False)
    write_pre(os.path.join(outdir, "SYN_Y.pre"), "SYN_Y", zy, PHI_N, mu_free=False)

    print("VERDAD: b=%d r=%d s=%d omega=%s phi_X=%.3f phi_N=%.3f"
          % (B_DELAY, R_ORD, S_ORD, OMEGA, PHI_X, PHI_N))


if __name__ == "__main__":
    main()
