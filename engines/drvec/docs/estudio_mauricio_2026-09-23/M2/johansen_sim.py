"""Asymptotic null distributions of Johansen's lambda-max / trace statistics, simulated (T=1000),
for three deterministic specifications, dimensions g = 1, 2 (number of common trends under H0).
  none   : no deterministic term anywhere          (drvec case 1 / Mauricio case 1 / Johansen H2 / O-L Table 0)
  rconst : constant restricted to the coint. space (drvec case 2 / Mauricio case 2 / Johansen H1* / O-L Table 1*)
  uconst : unrestricted constant, DGP without drift (what urca's ecdet='none' tabulates for g=1: tau_mu^2)
"""
import numpy as np, sys
rng = np.random.default_rng(20260923)
T, R = 1000, 20000
def stats(g, case, T, R, rng):
    lmax = np.empty(R); tr = np.empty(R)
    for k in range(R):
        e = rng.standard_normal((T, g))
        Y = np.cumsum(e, axis=0)
        dY = e[1:]; Y1 = Y[:-1]
        if case == 'rconst':
            Y1 = np.hstack([Y1, np.ones((T - 1, 1))])
        if case == 'uconst':
            dY = dY - dY.mean(0); Y1 = Y1 - Y1.mean(0)
        S00 = dY.T @ dY; S11 = Y1.T @ Y1; S01 = dY.T @ Y1
        A = np.linalg.solve(S11, S01.T @ np.linalg.solve(S00, S01))
        lam = np.sort(np.linalg.eigvals(A).real)[::-1][:g]
        n = T - 1
        lmax[k] = -n * np.log(1 - lam[0]); tr[k] = -n * np.log(1 - lam).sum()
    return lmax, tr
out = {}
for case in ('none', 'rconst', 'uconst'):
    for g in (1, 2):
        lm, tr = stats(g, case, T, R, rng)
        out[(case, g)] = (lm, tr)
        q = lambda x: np.percentile(x, [90, 95, 99])
        print("%-6s g=%d  lambda-max 90/95/99 = %6.2f %6.2f %6.2f   trace = %6.2f %6.2f %6.2f" % ((case, g) + tuple(q(lm)) + tuple(q(tr))))
np.savez('johansen_sim.npz', **{'%s_%d_%s' % (c, g, s): v for (c, g), (lm, tr) in out.items() for s, v in (('max', lm), ('tr', tr))})
print("\nMauricio's p-values (g = M-P under H0):")
for lab, stat, g in (("AddOn A2 EML P=1 vs 2", 1.5534, 1), ("AddOn A2 CML P=1 vs 2", 1.9866, 1),
                     ("T3 EML P=1 vs 2 (pub 95.51%)", 0.9718, 1), ("T3 CML P=1 vs 2 (pub 45.50%)", 3.7228, 1),
                     ("T3 EML P=0 vs 1 (lambda-max)", 35.5308, 2), ("AddOn EML P=0 vs 1", 59.2195, 2)):
    s = "  %-32s stat %8.4f :" % (lab, stat)
    for case in ('none', 'rconst', 'uconst'):
        s += "  %s %.4f" % (case, np.mean(out[(case, g)][0] > stat))
    from scipy.stats import chi2
    s += "  chi2(1) %.4f" % chi2.sf(stat, 1)
    print(s)
