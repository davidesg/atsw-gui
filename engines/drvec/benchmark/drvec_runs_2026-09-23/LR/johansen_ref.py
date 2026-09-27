#!/usr/bin/env python3
"""Johansen reference for each <case>.inp: an independent numpy reduced-rank
regression with the constant RESTRICTED to the cointegrating space (ca.jo
ecdet='const'; Johansen case H1*), K=2 in levels = 1 lagged difference, plus
statsmodels VECM(deterministic='ci') as a cross-check of beta/alpha/llf, and
statsmodels coint_johansen(det_order=0) -- which is the UNRESTRICTED constant
(ca.jo ecdet='none'), reported only to document that difference.
Writes johansen_<case>.json."""
import json, sys, numpy as np
from statsmodels.tsa.vector_ar.vecm import VECM, coint_johansen

def read_inp(path):
    L = [l for l in open(path) if l.strip() and not l.startswith("*")]
    M, n = map(int, L[1].split()[:2]); names = L[2].split()
    y = np.array([list(map(float, l.split())) for l in L[4:4 + n]])
    return y, names

def rrr_const(y, k=1):
    n, M = y.shape
    dy = np.diff(y, axis=0)
    Z0 = dy[k:]                                   # dY_t
    Z1 = np.column_stack([y[k:-1], np.ones(n - 1 - k)])   # [Y_{t-1}, 1]
    Z2 = np.column_stack([dy[k - j - 1:len(dy) - j - 1] for j in range(k)])
    T = len(Z0)
    P = lambda A: A - Z2 @ np.linalg.lstsq(Z2, A, rcond=None)[0]
    R0, R1 = P(Z0), P(Z1)
    S00, S11, S01 = R0.T @ R0 / T, R1.T @ R1 / T, R0.T @ R1 / T
    A = S01.T @ np.linalg.solve(S00, S01)
    Lc = np.linalg.cholesky(S11); Li = np.linalg.inv(Lc)
    lam, U = np.linalg.eigh(Li @ A @ Li.T)
    o = np.argsort(lam)[::-1]; lam, U = lam[o], U[:, o]
    V = Li.T @ U                                   # V'S11V = I
    lam = lam[:M]; V = V[:, :M]
    lmax = -T * np.log(1 - lam)
    trace = np.cumsum(lmax[::-1])[::-1]
    ll0 = -T / 2 * (M * np.log(2 * np.pi) + M + np.log(np.linalg.det(S00)))
    llf = [ll0 - T / 2 * np.sum(np.log(1 - lam[:r])) for r in range(M + 1)]
    return dict(T=T, lam=lam, lmax=lmax, trace=trace, V=V, S01=S01, llf=llf)

def normalise(V, r, M):
    """beta (M+1 x r incl. constant) normalised so the Y1 block (last r of the
    M variables, i.e. rows M-r..M-1) is I_r; alpha adjusted accordingly."""
    B = V[:, :r]; G = B[M - r:M, :]
    return B @ np.linalg.inv(G), G

def main(case, r):
    y, names = read_inp(case + ".inp"); n, M = y.shape
    J = rrr_const(y, 1)
    Bn, G = normalise(J["V"], r, M)
    alpha = J["S01"] @ J["V"][:, :r] @ G.T         # alpha*G' so Pi unchanged
    Pi = alpha @ Bn[:M].T
    out = dict(case=case, names=names, M=M, n=n, T=J["T"], r=r,
               lam=J["lam"].tolist(), lmax=J["lmax"].tolist(), trace=J["trace"].tolist(),
               llf=J["llf"], beta=Bn[:M].tolist(), beta_const=Bn[M].tolist(),
               alpha=alpha.tolist(), Pi=Pi.tolist())
    try:
        v = VECM(y, k_ar_diff=1, coint_rank=r, deterministic="ci").fit()
        b = np.asarray(v.beta); bn = b @ np.linalg.inv(b[M - r:M, :])
        out["sm_beta"] = bn.tolist(); out["sm_llf"] = float(v.llf)
        out["sm_Pi"] = (np.asarray(v.alpha) @ np.asarray(v.beta).T).tolist()
        out["sm_maxdiff_Pi"] = float(np.abs(np.asarray(out["sm_Pi"]) - Pi).max())
        out["sm_maxdiff_beta"] = float(np.abs(bn - Bn[:M]).max())
    except Exception as e:
        out["sm_error"] = repr(e)
    try:
        cj = coint_johansen(y, det_order=0, k_ar_diff=1)
        out["sm_cj_det0_trace"] = cj.lr1.tolist()
        out["sm_cj_det0_lmax"] = cj.lr2.tolist()
    except Exception as e:
        out["sm_cj_error"] = repr(e)
    json.dump(out, open("johansen_%s.json" % case, "w"), indent=1)
    np.set_printoptions(precision=4, suppress=True, linewidth=150)
    print(case, "T=%d" % J["T"], "trace", np.round(J["trace"], 3), "lmax", np.round(J["lmax"], 3))
    print("  beta (Y1-normalised)\n", Bn[:M].T, "\n  const", Bn[M], "\n  alpha\n", alpha.T)
    print("  llf by rank", np.round(J["llf"], 3), " sm llf", out.get("sm_llf"),
          " sm |dPi|", out.get("sm_maxdiff_Pi"), " sm |dbeta|", out.get("sm_maxdiff_beta"))
    print("  statsmodels coint_johansen det_order=0 (UNRESTRICTED const) trace",
          np.round(out.get("sm_cj_det0_trace", []), 3))

if __name__ == "__main__":
    RANK = dict(e1=1, e3=1, rao1=2, rao2=3, rao3=3, rao4=2, rao5=2, rao6=5, rao7=2, rao7_sc=2, rao3_3v=1, rao6_sc=5)
    for c in (sys.argv[1:] or RANK):
        main(c, RANK[c])
