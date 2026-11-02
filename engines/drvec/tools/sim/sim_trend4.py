"""Writes datasets/synthetic/trend4.inp: a VEC with a trend RESTRICTED to the
cointegrating space (case 4, Johansen 1995 ch. 6), the fixture of the rank
test with -trend.

    python3 tools/sim/sim_trend4.py [n] [seed] [out]

M = 3, r = 1, p = 1, Sigma = I. Internal order Y = [Y1 ; Y2], B = [1 ; B2]:

    W_t   = Y1_t + B2' Y2_t,                   B2 = (-1, -0.5)
    z_t   = W_t - mu_W - d (t - tc)            the equilibrium error
    dY_t  = -Lam z_{t-1} + c + A_t,            Lam = (0.30, 0.10, 0)
    c     = (d - B2'g, g)                      g = (0.2, 0.1), Y2's drifts

so E[dY2] = g and E[dW] = d = 0.05: W drifts, and no drift of Y2 explains it.
drvec's -case 3 -trend fits this model; -case 3 alone has no d and must
carry the trend in W as a stochastic one. M = 3 so that the rank test
compares r = 1 with r = 2: its H0 has W, and the bootstrap simulates the
slope (at r = 0 there is no W and no trend).
"""
import sys

import numpy as np

n = int(sys.argv[1]) if len(sys.argv) > 1 else 150
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 4
out = sys.argv[3] if len(sys.argv) > 3 else "datasets/synthetic/trend4.inp"
B2, g, d, muw = np.array([-1.0, -0.5]), np.array([0.2, 0.1]), 0.05, 1.0
Lam = np.array([0.30, 0.10, 0.0])
c = np.concatenate(([d - B2 @ g], g))
rng = np.random.default_rng(seed)
burn = 200
N = n + burn + 1
tc = 0.5 * (N + 1)
A = rng.standard_normal((N, 3))
Y = np.zeros((N, 3))
Y[0, 0] = muw - d * (1 - tc)                           # z_0 = 0
for t in range(1, N):
    z = Y[t - 1, 0] + B2 @ Y[t - 1, 1:] - muw - d * (t - 1 - tc)
    Y[t] = Y[t - 1] - Lam * z + c + A[t]
Y = Y[burn:]                                           # n + 1 rows of levels
lines = ["* synthetic: case 4 BY CONSTRUCTION.  A VEC with M = 3, r = 1 whose",
         "* equilibrium error drifts: W = Y1 - Y2a - 0.5 Y2b has slope d = 0.05,",
         "* Y2 drifts (0.2, 0.1), Lam = (0.30, 0.10, 0), p = 1, Sigma = I.",
         "* n = %d, seed %d" % (n, seed),
         "* (tools/sim/sim_trend4.py).  The fixture of -trend -lrtest -bootstrap.",
         "1", "3 %d 1 1900" % len(Y), "Y2a Y2b Y1", "1.0 0 0"]
lines += ["%.10f %.10f %.10f" % (Y[t, 1], Y[t, 2], Y[t, 0]) for t in range(len(Y))]
with open(out, "w", newline="\n") as fh:
    fh.write("\n".join(lines) + "\n")
print("written", out, "rows", len(Y))
