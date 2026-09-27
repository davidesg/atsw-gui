#!/usr/bin/env python3
"""INDEPENDENT exact ML for drvec's q=0 model (case 2), via a Kalman filter.

drvec maximises the exact Gaussian likelihood of Ybar_t = (nabla Y2_t, W_t),
t = 2..n, W = beta'Y, beta = [B2 ; I_r] ([Y2;Y1] order), under stationarity.
With one lagged difference (drvec p=2) the state z_t = (dY_t, W_t - mu) is a
VAR(1):  z_t = A z_{t-1} + D e_t,  A = [[G, a],[b'G, I + b'a]],  D = [I ; b'],
and Ybar_t = S z_t selects dY2 and W.  The stationary initial covariance
solves P = A P A' + D Sigma D'.  This code shares nothing with drvec.

  python3 exact_ml.py CASE R            evaluate at drvec's reported point
                                         (both Gamma/Sigma orderings) and
                                         maximise from Johansen's and drvec's
                                         points; writes exact_<CASE>_r<R>.json
"""
import json, re, sys, time, numpy as np
from scipy.linalg import solve_discrete_lyapunov
from scipy.optimize import minimize
from statsmodels.tsa.vector_ar.vecm import VECM

def read_inp(path):
    L = [l for l in open(path) if l.strip() and not l.startswith("*")]
    M, n = map(int, L[1].split()[:2])
    return np.array([list(map(float, l.split())) for l in L[4:4 + n]])

def loglik(y, r, alpha, B2, G, mu, Sig):
    n, M = y.shape; s = M - r
    dy = np.diff(y, axis=0)                              # t = 2..n
    if r == 0:
        A = G; Q = Sig; Z = dy; k = M
    else:
        beta = np.vstack([B2, np.eye(r)])
        W = (y @ beta)[1:] - mu
        A = np.block([[G, alpha], [beta.T @ G, np.eye(r) + beta.T @ alpha]])
        D = np.vstack([np.eye(M), beta.T]); Q = D @ Sig @ D.T
        Z = np.hstack([dy[:, :s], W]); k = M + r
    if np.max(np.abs(np.linalg.eigvals(A))) >= 0.9999: return -1e10
    try: P = solve_discrete_lyapunov(A, Q)
    except Exception: return -1e10
    S = np.zeros((M, k)); S[:s, :s] = np.eye(s)
    if r: S[s:, M:] = np.eye(r)
    else: S = np.eye(M)
    x = np.zeros(k); ll = 0.0
    for t in range(len(Z)):
        v = Z[t] - S @ x; F = S @ P @ S.T
        try: c = np.linalg.cholesky(F)
        except np.linalg.LinAlgError: return -1e10
        u = np.linalg.solve(c, v)
        ll += -0.5 * (M * np.log(2 * np.pi) + 2 * np.log(np.diag(c)).sum() + u @ u)
        K = P @ S.T @ np.linalg.inv(F)
        x = x + K @ v; P = P - K @ S @ P
        x = A @ x; P = A @ P @ A.T + Q
    return ll

def pack(r, alpha, B2, G, mu, Sig):
    L = np.linalg.cholesky(Sig); il = np.tril_indices(len(Sig))
    return np.concatenate([alpha.ravel(), B2.ravel(), G.ravel(), np.ravel(mu), L[il]])

def unpack(x, M, r):
    s = M - r; i = 0
    a = x[i:i + M * r].reshape(M, r); i += M * r
    b = x[i:i + s * r].reshape(s, r); i += s * r
    G = x[i:i + M * M].reshape(M, M); i += M * M
    mu = x[i:i + r]; i += r
    L = np.zeros((M, M)); L[np.tril_indices(M)] = x[i:]
    return a, b, G, mu, L @ L.T

def johansen_start(y, r):
    n, M = y.shape
    if r == 0:
        dy = np.diff(y, axis=0); X, Yv = dy[:-1], dy[1:]
        G = np.linalg.lstsq(X, Yv, rcond=None)[0].T; E = Yv - X @ G.T
        return np.zeros((M, 0)), np.zeros((M, 0)), G, np.zeros(0), E.T @ E / len(E)
    v = VECM(y, k_ar_diff=1, coint_rank=r, deterministic="ci").fit()
    b = np.asarray(v.beta); g = b[M - r:, :]; bn = b @ np.linalg.inv(g)
    a = np.asarray(v.alpha) @ g.T
    c = np.asarray(v.det_coef_coint).reshape(1, r) @ np.linalg.inv(g)
    return a, bn[:M - r], np.asarray(v.gamma), -np.asarray(c).ravel(), np.asarray(v.sigma_u)

def drvec_point(out, M, r):
    t = open(out, errors="ignore").read()
    def mat(head, rows):
        i = t.find(head); L = t[i:].split("\n")[1:1 + rows]
        return np.array([[float(v) for v in l.split()] for l in L])
    a = mat("alpha matrix (M x r)", M) if r else np.zeros((M, 0))
    beta = mat("beta matrix (M x r)", M) if r else np.zeros((M, 0))
    G = mat("Gamma(1) matrix:", M)
    # Sigma = sigma2 * Q (Q and sigma2 carry more significant digits than the printed Sigma)
    Sl = [list(map(float, l.split())) for l in t[t.find("Q matrix (lower triangle"):].split("\n")[1:1 + M]]
    s2 = float(re.search(r"sigma2\s*:\s*([-\d.eE+]+)", t).group(1))
    Sl = [[v * s2 for v in row] for row in Sl]
    S = np.zeros((M, M))
    for i, row in enumerate(Sl): S[i, :len(row)] = row
    S = S + np.tril(S, -1).T
    mu = np.array([float(v) for v in re.search(r"E\[W\] vector:\n((?:\s+[-\d.]+\n)+)", t).group(1).split()]) if r else np.zeros(0)
    ll = float(re.search(r"logelf\s*:\s*([-\d.]+)", t).group(1))
    return a, beta[:M - r], G, mu, S, ll

def fit(y, r, x0, maxit=3000):
    n, M = y.shape
    f = lambda x: -loglik(y, r, *unpack(x, M, r))
    res = minimize(f, x0, method="BFGS", options=dict(maxiter=maxit, gtol=1e-5))
    return -res.fun, res

if __name__ == "__main__":
    case, r = sys.argv[1], int(sys.argv[2]); stem = sys.argv[3] if len(sys.argv) > 3 else "%s_q0r%d" % (case, r)
    y = read_inp(case + ".inp"); n, M = y.shape; out = dict(case=case, r=r, stem=stem)
    np.set_printoptions(precision=5, suppress=True, linewidth=160)
    # 1. identity: evaluate at drvec's reported point, both orderings of Gamma/Sigma
    try:
        a, b2, G, mu, S, lld = drvec_point(stem + ".out", M, r)
        perm = list(range(M - r, M)) + list(range(M - r))     # internal [Y1;Y2] -> positions
        Pm = np.eye(M)[perm]                                   # rows: internal order
        G_inp = Pm.T @ G @ Pm; S_inp = Pm.T @ S @ Pm          # reinterpret as internal order
        out.update(drvec_logelf=lld,
                   eval_as_printed=loglik(y, r, a, b2, G, mu, S),
                   eval_internal_order=loglik(y, r, a, b2, G_inp, mu, S_inp))
        print("%s r=%d drvec logelf %.4f | exact loglik at drvec point: labels as printed %.4f, read as internal [Y1;Y2] %.4f"
              % (case, r, lld, out["eval_as_printed"], out["eval_internal_order"]))
        xd = pack(r, a, b2, G_inp, mu, S_inp)
    except Exception as e:
        print("no drvec point:", e); xd = None
    # 2. independent maximisation
    js = johansen_start(y, r); xj = pack(r, *js)
    out["eval_at_johansen"] = loglik(y, r, *js); out["johansen_start_shrunk"] = 1.0
    if out["eval_at_johansen"] <= -1e9:      # conditional ML point not stationary: shrink G, alpha
        for c in (0.98, 0.95, 0.9, 0.8, 0.7, 0.5, 0.3, 0.1):
            js2 = (js[0] * c, js[1], js[2] * c, js[3], js[4])
            if loglik(y, r, *js2) > -1e9:
                xj = pack(r, *js2); out["johansen_start_shrunk"] = c
                print("  Johansen point non-stationary for the exact model; start shrunk by %.2f" % c); break
        else:
            xj = None; print("  Johansen point non-stationary and not recoverable by shrinking")
    t0 = time.time(); best = None
    for lab, x0 in (("johansen", xj), ("drvec", xd)):
        if x0 is None: continue
        if -loglik(y, r, *unpack(x0, M, r)) >= 1e9: print("  %s start infeasible" % lab); continue
        ll, res = fit(y, r, x0)
        out["max_from_" + lab] = ll; out["nit_" + lab] = int(res.nit); out["ok_" + lab] = bool(res.success)
        if best is None or ll > best[0]: best = (ll, res.x)
        print("  maximised from %s start: %.4f (nit %d, success %s)" % (lab, ll, res.nit, res.success))
    if best is None:
        json.dump(out, open("exact_%s_r%d.json" % (case, r), "w"), indent=1); sys.exit(0)
    a, b2, G, mu, S = unpack(best[1], M, r)
    beta = np.vstack([b2, np.eye(r)]) if r else None
    out.update(best=best[0], beta=None if beta is None else beta.tolist(), alpha=a.tolist(), mu=mu.tolist(),
               Pi=(a @ beta.T).tolist() if r else None, secs=time.time() - t0)
    print("  exact loglik at Johansen's point %.4f ; independent exact max %.4f ; secs %.0f" % (out["eval_at_johansen"], best[0], out["secs"]))
    if r: print("  beta'\n", beta.T, "\n  alpha'\n", a.T, "\n  E[W]", mu)
    json.dump(out, open("exact_%s_r%d.json" % (case, r), "w"), indent=1)
