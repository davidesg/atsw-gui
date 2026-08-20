#!/usr/bin/env python3
"""An ORACLE for the VEC forecast: simulate the truth and ask it, not a formula.

Why this exists.  The recursion `drvec -f` runs is the standard VARMA one, but
it runs it on the TRANSFORMED system and then inverts the transformation back to
levels.  Nothing in `literature/` documents that step for this class: Ahn and
Reinsel (1990), Mauricio (2006) and Yap and Reinsel (1995) all mention
forecasting as a MOTIVATION and none of them writes the recursion down, and the
reference the last two point to for it -- JTSA 13, 353-375 -- is not in the
bank.  The other programs of the suite each have an oracle; `drvec` did not, and
its two certificates (FORECAST.md 3 and 4) are internal consistency checks that
say nothing about horizons beyond one.

What this does instead of a formula.  It simulates the data-generating process,
hands `drvec` the sample, and then continues the SAME process many times from
the true final state with fresh shocks.  The average of those continuations is
the conditional expectation and their spread is the forecast error dispersion --
by construction, with no algorithm of `drvec`'s involved.  If the level
reconstruction, the cumulation of the differenced block, or the accumulated
weights `C_m` were wrong, this would show it at every horizon at once.

Two caveats, stated because they bound what it proves:

  * `drvec` forecasts with ESTIMATED parameters and the oracle continues with the
    TRUE ones, so the gap contains estimation error.  It has to shrink with `n`,
    and the script reports it at two sample sizes so that can be seen.
  * with `q >= 1` the recursion needs the last shocks; `drvec` uses its fitted
    residuals and the oracle uses the true ones.  With `q = 0` that difference
    is absent, which is why the `q = 0` column is the clean one.

Usage:  python3 tools/sim/forecast_oracle.py [nrep] [H]
"""
import os
import re
import subprocess
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DRVEC = os.path.join(ROOT, "bin", "drvec")
TMP = "/tmp/drvec_fc_oracle"

B2 = -0.5
LAM = np.array([0.30, 0.10])          # orden interno [Y1 ; Y2]
F1 = np.diag([0.20, 0.20])


def simulate(n, theta, seed, burn=500):
    """The DGP of sim_vec.py, returning the path AND the shocks that made it."""
    rng = np.random.default_rng(seed)
    N = n + burn + 2
    A = rng.standard_normal((N, 2))
    Th1 = np.diag([theta, theta])
    Y = np.zeros((N, 2))
    dY = np.zeros((N, 2))
    for t in range(2, N):
        W = Y[t-1, 0] + B2 * Y[t-1, 1]
        dY[t] = -LAM * W + F1 @ dY[t-1] + A[t] - Th1 @ A[t-1]
        Y[t] = Y[t-1] + dY[t]
    k = burn + 2
    return Y[k:], dY[k:], A[k:], Th1


def write_inp(Y, path):
    n = len(Y)
    lines = ["* oracle sample", "1", f"2 {n} 1 1900", "Y2 Y1", "1.0 0 0"]
    lines += [f"{Y[t,1]:.10f} {Y[t,0]:.10f}" for t in range(n)]   # [Y2 ; Y1]
    open(path, "w").write("\n".join(lines) + "\n")


def oracle(Y, dY, A, Th1, H, nrep, seed):
    """Continue the true process nrep times from the true final state."""
    rng = np.random.default_rng(seed)
    out = np.zeros((nrep, H, 2))
    y0, dy0, a0 = Y[-1].copy(), dY[-1].copy(), A[-1].copy()
    for k in range(nrep):
        y, dy, aprev = y0.copy(), dy0.copy(), a0.copy()
        for h in range(H):
            a = rng.standard_normal(2)
            W = y[0] + B2 * y[1]
            dy = -LAM * W + F1 @ dy + a - Th1 @ aprev
            y = y + dy
            aprev = a
            out[k, h] = y
    #  columnas del .inp: [Y2 ; Y1]
    return np.stack([out[:, :, 1], out[:, :, 0]], axis=2)


def run_drvec(base, p, q, H, extra=()):
    subprocess.run([DRVEC, base, str(p), str(q), "1", "-case", "1", "-f", str(H),
                    *extra], capture_output=True, timeout=3600)
    t = open(base + ".out", encoding="latin-1", errors="replace").read()
    rows = re.findall(r"^ *(\d+) +([\-0-9.]+) +([\-0-9.]+) +([\-0-9.]+) +([\-0-9.]+)$",
                      t, re.M)
    f = np.array([[float(r[1]), float(r[3])] for r in rows[:H]])
    se = np.array([[float(r[2]), float(r[4])] for r in rows[:H]])
    return f, se


def main(nrep=20000, H=6):
    os.makedirs(TMP, exist_ok=True)
    print(__doc__.split("Usage:")[0].strip()[:0] or "", end="")
    print("ORACLE for the VEC forecast.  DGP: B2=-0.5, Lam=(0.30,0.10), F1=0.2I,")
    print(f"Th1 = theta*I, Sigma = I.  {nrep} continuations, H = {H}.\n")
    for q, theta, extra in ((0, 0.0, ()), (1, 0.5, ()), (1, 0.5, ('-mafree',)),
                            (1, -0.5, ()), (1, -0.5, ('-mafree',))):
        cls = "-mafree (the DGP IS in this class)" if extra else \
              "default -marow (the DGP is NOT: it has Theta22 = %.1f)" % theta
        print(f"--- q = {q}, theta = {theta}, {cls} " + "-" * 8)
        print(f"{'n':>7} {'h':>3} {'gap/sd':>8} {'se ratio':>9}"
              f" {'drvec mean':>12} {'oracle mean':>12}")
        for n in (2000, 20000):
            Y, dY, A, Th1 = simulate(n, theta, seed=11)
            base = os.path.join(TMP, f"s{n}q{q}{'f' if extra else ''}")
            write_inp(Y, base + ".inp")
            f, se = run_drvec(base, 2, q, H, extra)
            oc = oracle(Y, dY, A, Th1, H, nrep, seed=99)
            om, os_ = oc.mean(axis=0), oc.std(axis=0, ddof=1)
            if len(f) < H:
                print(f"{n:7d}   (drvec produced no forecast table)")
                continue
            for h in (1, min(3, H), H):
                i = h - 1
                gapn = np.max(np.abs(f[i] - om[i]) / os_[i])
                rat = np.max(se[i] / os_[i])
                print(f"{n:7d} {h:3d} {gapn:8.4f} {rat:9.4f}"
                      f" {f[i,0]:12.5f} {om[i,0]:12.5f}")
        print()


if __name__ == "__main__":
    if not os.access(DRVEC, os.X_OK):
        sys.exit(f"{DRVEC} not built; run make first")
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 20000,
         int(sys.argv[2]) if len(sys.argv) > 2 else 6)
