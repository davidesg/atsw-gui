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
OMEGA   = [0.800, -0.400]  # omega_0, omega_1  (BJR: nu = 0.8 - (-0.4)B = 0.8 + 0.4·resp)
DELTA   = []               # (vacío: r=0)

SEED    = 20260712


def impulse_response(omega, s, delta, r, b, length):
    """nu[t], t=1..length (misma recursión que compute_irf en tran_shootx.c).

    Numerador en convención Box-Jenkins: omega(B) = omega_0 - omega_1 B - ...
    (el líder suma, los demás restan), como el calcnu de fue.
    """
    nu = [0.0] * (length + 1)
    for t in range(1, length + 1):
        lag = t - 1 - b
        if lag == 0:
            acc = omega[0]
        elif 0 < lag <= s:
            acc = -omega[lag]
        else:
            acc = 0.0
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
    b2, r2, s2, om2 = 0, 0, 1, [0.500, -0.300]  # X2: contemporaneo, dos omegas (BJR)

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


def build_chain(outdir, seed):
    """LA RED: una CADENA  X -> M -> Y.

    M es a la vez SALIDA (de X) y ENTRADA (de Y). Es la topologia de m6 en
    miniatura (EC -> EU -> EI) y es justo lo que el modelo en estrella NO puede
    representar: en la estrella toda transferencia acaba en la serie 1.

        w_M[t] = nu_XM(B) w_X[t] + N_M[t]      b=1, s=0, omega = 0.700
        w_Y[t] = nu_MY(B) w_M[t] + N_Y[t]      b=2, s=0, omega = 0.500

    Ojo: X influye en Y INDIRECTAMENTE, a traves de M. La verdad NO tiene un
    enlace X -> Y, y drtran debe poder decirlo.
    """
    rnd = random.Random(seed)
    burn = 200
    n = N_OBS + burn

    om_xm, b_xm = [0.700], 1
    om_my, b_my = [0.500], 2

    wx = [0.0] * n
    nm = [0.0] * n
    ny = [0.0] * n
    for t in range(1, n):
        wx[t] = 0.500 * wx[t - 1] + rnd.gauss(0.0, 1.0)
        nm[t] = 0.300 * nm[t - 1] + rnd.gauss(0.0, 0.5)
        ny[t] = 0.400 * ny[t - 1] + rnd.gauss(0.0, 0.4)

    nu_xm = impulse_response(om_xm, 0, [], 0, b_xm, 60)
    nu_my = impulse_response(om_my, 0, [], 0, b_my, 60)

    wm = [0.0] * n
    for t in range(n):
        acc = 0.0
        for j in range(1, 61):
            idx = t - (j - 1)
            if idx < 0:
                break
            acc += nu_xm[j] * wx[idx]
        wm[t] = acc + nm[t]

    wy = [0.0] * n
    for t in range(n):
        acc = 0.0
        for j in range(1, 61):
            idx = t - (j - 1)
            if idx < 0:
                break
            acc += nu_my[j] * wm[idx]
        wy[t] = acc + ny[t]

    wx, wm, wy = wx[burn:], wm[burn:], wy[burn:]

    def to_levels(v, base):
        z, acc = [], 0.0
        for u in v:
            acc += u / 100.0
            z.append(base * math.exp(acc))
        return z

    write_pre(os.path.join(outdir, "SYNC_Y.pre"), "SYNC_Y",
              to_levels(wy, 100.0), 0.400, mu_free=False)
    write_pre(os.path.join(outdir, "SYNC_M.pre"), "SYNC_M",
              to_levels(wm, 70.0), 0.300, mu_free=False)
    write_pre(os.path.join(outdir, "SYNC_X.pre"), "SYNC_X",
              to_levels(wx, 50.0), 0.500, mu_free=False)

    with open(os.path.join(outdir, "SYNC.net"), "w") as f:
        f.write("# LA RED:  X -> M -> Y   (M es salida Y entrada)\n")
        f.write("# SALIDA <- ENTRADA   b r s\n")
        f.write("SYNC_Y <- SYNC_M   2 0 0\n")
        f.write("SYNC_M <- SYNC_X   1 0 0\n")

    print("SYNC -> VERDAD (cadena): Y<-M b=2 omega=%s | M<-X b=1 omega=%s"
          % (om_my, om_xm))


def build_corr(outdir, seed):
    """Innovaciones CORRELACIONADAS contemporaneamente (rho = 0.600), sin
    transferencia.

    Es la forma REDUCIDA de una dependencia contemporanea que no se modela como
    transferencia. m6-1 la usa: tiene covarianzas fuera de la diagonal SIN tener
    ninguna estructura contemporanea, lo que prueba que no son sustitutos.

        w_X[t] = 0.5 w_X[t-1] + e[t]
        N[t]   = 0.3 N[t-1]   + a[t]      corr(a_t, e_t) = 0.600
        w_Y[t] = N[t]
    """
    rnd = random.Random(seed)
    burn = 200
    n = N_OBS + burn
    rho = 0.600
    sd_e, sd_a = 1.0, 0.5

    wx = [0.0] * n
    nz = [0.0] * n
    for t in range(1, n):
        z1 = rnd.gauss(0.0, 1.0)
        z2 = rnd.gauss(0.0, 1.0)
        e = sd_e * z1
        a = sd_a * (rho * z1 + math.sqrt(1.0 - rho * rho) * z2)
        wx[t] = 0.500 * wx[t - 1] + e
        nz[t] = 0.300 * nz[t - 1] + a

    wx, wy = wx[burn:], nz[burn:]

    def to_levels(v, base):
        z, acc = [], 0.0
        for u in v:
            acc += u / 100.0
            z.append(base * math.exp(acc))
        return z

    write_pre(os.path.join(outdir, "SYNQ_Y.pre"), "SYNQ_Y",
              to_levels(wy, 100.0), 0.300, mu_free=False)
    write_pre(os.path.join(outdir, "SYNQ_X.pre"), "SYNQ_X",
              to_levels(wx, 50.0), 0.500, mu_free=False)

    with open(os.path.join(outdir, "SYNQ.cns"), "w") as f:
        f.write("# liberar la covarianza de las innovaciones (nace fija en cero)\n")
        f.write("q[2,1] = free\n")

    print("SYNQ -> VERDAD: rho(a_N, a_X) = %.3f, sin transferencia" % rho)


def build_ident(outdir, seed):
    """LA NO IDENTIFICACION, con verdad construida.

    Una transferencia CONTEMPORANEA (b=0) y una covarianza de innovaciones
    explican LO MISMO en el retardo 0. Solo las separa la cola de la ccf
    preblanqueada, proporcional a (phi_N - phi_X). Luego:

        si phi_N == phi_X, los dos modelos son EXACTAMENTE indistinguibles.

    Se generan DOS pares. En los dos la verdad es una transferencia
    contemporanea Y = 0.5*X + N, con innovaciones INDEPENDIENTES:

      SYNI  : phi_N = phi_X = 0.500   -> no identificado. Los dos modelos deben
                                         dar la MISMA verosimilitud.
      SYNJ  : phi_N = 0.200, phi_X = 0.700 -> bien separados. Deben diferir.
    """
    for tag, pn, px in (("SYNI", 0.500, 0.500), ("SYNJ", 0.200, 0.700)):
        rnd = random.Random(seed + hash(tag) % 1000)
        burn = 300
        n = N_OBS + burn
        w0 = 0.500

        wx = [0.0] * n
        nz = [0.0] * n
        for t in range(1, n):
            wx[t] = px * wx[t - 1] + rnd.gauss(0.0, 1.0)
            nz[t] = pn * nz[t - 1] + rnd.gauss(0.0, 0.5)

        wy = [w0 * wx[t] + nz[t] for t in range(n)]
        wx, wy = wx[burn:], wy[burn:]

        def to_levels(v, base):
            z, acc = [], 0.0
            for u in v:
                acc += u / 100.0
                z.append(base * math.exp(acc))
            return z

        write_pre(os.path.join(outdir, "%s_X.pre" % tag), "%s_X" % tag,
                  to_levels(wx, 50.0), px, mu_free=False)
        write_pre(os.path.join(outdir, "%s_Y.pre" % tag), "%s_Y" % tag,
                  to_levels(wy, 100.0), pn, mu_free=False)

        print("%s -> VERDAD: Y = %.3f*X + N,  phi_N=%.3f  phi_X=%.3f  %s"
              % (tag, w0, pn, px,
                 "(NO IDENTIFICADO)" if abs(pn - px) < 1e-9 else "(separables)"))


def build_intervention(outdir, seed):
    """Serie con una INTERVENCIÓN COMPUESTA (numerador de dos omegas), verdad conocida.

    Guarda el signo del omega DETERMINISTA -- el bug latente que m6 destapó y que la
    homologación (todo Nomega=0) nunca probaba. fue/drtran aplican el numerador en
    Box-Jenkins:  comp(t) = w0*step(t) - w1*step(t-1).  Se genera la serie con esa
    verdad y drtran debe RECUPERAR w1 con signo POSITIVO (con el bug +, saldría negativo).

        U(t) = comp(t) + random_walk(t)          (U = 100*log(z), sin diferenciar)
        z(t) = exp(U(t)/100)
        .pre: log, d=1, sin ARMA, step con Nomega=1 (omega libre)
    """
    rnd = random.Random(seed)
    n = 400
    t0 = 100                 # índice 0-based; obs 101 (1-based) = mayo 2008 (freq 12, ini 1/2000)
    W0, W1 = 10.0, 6.0       # impacto W0=10; efecto permanente W0-W1 = 4
    PHI = 0.500              # AR(1) del ruido (estacionario, d=0; ademas evita el
                             # caso "sin operadores", que el lector no maneja)
    comp = [W0 * (1.0 if t >= t0 else 0.0) - W1 * (1.0 if t >= t0 + 1 else 0.0)
            for t in range(n)]
    nz = [0.0] * n
    for t in range(1, n):
        nz[t] = PHI * nz[t - 1] + rnd.gauss(0.0, 0.3)
    U = [comp[t] + nz[t] for t in range(n)]   # U = 100*log(z), SIN diferenciar (d=0)
    z = [math.exp(u / 100.0) for u in U]       # base 1: 100*log(z) = U exacto

    per, yr = 5, 2008        # obs 101: 100 meses tras 1/2000 -> mes 1+100%12=5, año 2000+100//12=2008
    L = [        # OJO: el lector salta EXACTO 5 lineas de cabecera (banner de 4 + blanco)
        "************************************************",
        "*        Input file for program DRVUS          *",
        "*   Caso sintetico: intervencion compuesta     *",
        "************************************************", "",
        "** Frequency of time series: either 1(A), 4(Q) or 12(M):", " 12",
        "** Number of observations and starting date of time series:",
        " %d  1 2000 SYND" % n,
        "** Number of deterministic variables (including seasonal components):", "1",
        "**", "step %d %d" % (per, yr),
        "**", "1",                       # Nomega = 1 (dos coeficientes: w0, w1)
        "**", "1.000000 1", "1.000000 1",  # omega LIBRE (arranque neutro 1,1)
        "**", "0",                       # Ndelta = 0
        "**Number and orders of regular AR operators:", "1 1",
        "**", "%.4f 1" % PHI,
        "** Number and orders of annual AR operators:", "0",
        "** Number and orders of regular MA operators:", "0",
        "** Number and orders of anual MA operators:", "0",
        "** Number and frequencies of regular AR(2) operators with fixed frequency:", "0",
        "** Number and frequencies of regular MA(2) operators with fixed frequency:", "0",
        "** Mean parameter (mu):", "0",
        "** Box-Cox lambda, regular differences and complete annual differences:",
        "0.00 0 0",
        "** Individual factors of the annual difference (from freq 0.0): ", " 0 0 0 0 0 0 0",
        "** ACF/PACF bands (0 Automatic) and reescaling factor: ", " 0.00 100.00",
        "** Time series (stochastic and non-standard deterministic variables): ",
    ]
    L += ["%.10f " % v for v in z]
    open(os.path.join(outdir, "SYND.pre"), "w").write("\n".join(L) + "\n")
    print("SYND -> VERDAD: intervencion compuesta w0=%.1f w1=%.1f (BJR: w0 - w1 B), "
          "step obs %d, ruido AR(1) phi=%.1f" % (W0, W1, t0 + 1, PHI))


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    build_intervention(outdir, SEED + 6)

    build_ident(outdir, SEED + 5)

    build_corr(outdir, SEED + 4)

    build_chain(outdir, SEED + 3)

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
