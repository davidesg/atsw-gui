"""General-case check (beyond A1): M=3, r in {1,2}, p=3, q=2, case 3, random parameters.
drvec's transformed system and logelf vs the paper's formulas (10)-(18) + independent exact likelihood."""
import numpy as np, subprocess, os, sys
sys.path.insert(0, '.')
from exactlik import *
rng = np.random.default_rng(11)
M, T = 3, 80
EXTRA = sys.argv[1:]
Y = np.cumsum(rng.standard_normal((T, M)), axis=0) + 5
os.makedirs('run3', exist_ok=True)
with open('run3/g3.inp', 'w') as f:
    f.write("* sim\n1\n%d %d 1 1\na b c\n1.0 0 0\n" % (M, T))
    for row in Y: f.write(" ".join("%.10f" % v for v in row) + "\n")
for r in (1, 2):
    s = M - r
    p, q = 3, 2
    for attempt in range(200):
        Lam = rng.normal(0, 0.3, (M, r)); B2 = rng.normal(0, 0.7, (s, r))
        F = [rng.normal(0, 0.15, (M, M)) for _ in range(p - 1)]
        Th = [rng.normal(0, 0.2, (M, M)) for _ in range(q)]
        phis, thetas, Cb = mauricio_varma(Lam, B2, F, Th, M, r)
        Tm, _, _ = state_space(phis, thetas, M)
        Tq, _, _ = state_space([ -t for t in thetas], [], M)
        if np.abs(np.linalg.eigvals(Tm[:M*p, :M*p])).max() < 0.95 and np.abs(np.linalg.eigvals(Tq)).max() < 0.9: break
    A = rng.normal(0, 0.3, (M, M)); Q = A @ A.T + np.eye(M); Q = Q / Q[0, 0]
    mu = rng.normal(0, 1, M)                      # case 3: E[dY2] (s) and E[W] (r)
    # drvec x order: mean (case 3: s then r), Lam (row-major M x r), F_k row-major, Theta_k row-major,
    # Sigma: diag 2..M then lower triangle (i=2..M, j<i), B2 (loop j=1..r, i=1..s)
    x = list(mu) + list(Lam.ravel()) + [v for Fk in F for v in Fk.ravel()] + [v for Tk in Th for v in Tk.ravel()]
    x += [Q[i, i] for i in range(1, M)] + [Q[i, j] for i in range(1, M) for j in range(i)]
    x += [B2[i, j] for j in range(r) for i in range(s)]
    open('run3/x.txt', 'w').write(' '.join('%.15g' % v for v in x))
    out = subprocess.run(['../drvec_copy/bin/drvec', 'g3', str(p), str(q), str(r), '-case', '3', '-mean', '-mafree', '-eval'] + EXTRA,
                         cwd='run3', env=dict(os.environ, DRVEC_X='x.txt', DRVEC_DUMP='dump.txt'), capture_output=True, text=True).stdout
    line = [l for l in out.splitlines() if 'logelf' in l or 'inject' in l]
    d = [l.split() for l in open('run3/dump.txt').read().strip().split('\n')]
    dmu = np.array(d[1], float); i = 2
    dphi = [np.array(d[i + k*M:i + (k+1)*M], float) for k in range(p)]; i += p*M
    dth = [np.array(d[i + k*M:i + (k+1)*M], float) for k in range(q)]; i += q*M
    dqq = np.array(d[i:i+M], float); i += M
    dw = np.array(d[i:], float)
    # drvec's data columns are [Y2 block ; Y1 block] = file order; paper order is [Y1 ; Y2]
    Ypaper = np.hstack([Y[:, s:], Y[:, :s]])
    Yb = ybar_from_levels(Ypaper, B2, r)
    print("r=%d: max|Phi* diff| %.1e  max|Theta* diff| %.1e  max|Sigma* diff| %.1e  max|Ybar diff| %.1e  mu diff %.1e" % (r,
          max(np.abs(a - b).max() for a, b in zip(dphi, phis)), max(np.abs(a - b).max() for a, b in zip(dth, thetas)),
          np.abs(dqq - Cb @ Q @ Cb.T).max(), np.abs(dw - Yb).max(), np.abs(dmu - mu).max()))
    L, s2 = loglik_conc(Yb, mu, phis, thetas, Cb @ Q @ Cb.T)
    print("     independent logL = %.10f   drvec: %s" % (L, line[-1]))
