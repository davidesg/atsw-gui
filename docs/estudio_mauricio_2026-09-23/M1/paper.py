"""Independent implementation of Mauricio (2006), Section 3, in numpy.

Conventions (the paper's):
  Y_t = (Y_t1' (P=r comps), Y_t2' (M-P=s comps))'
  nabla Y_t = -Lam B' Y_{t-1} + sum_{i=1}^{p-1} F_i nabla Y_{t-i} + Theta(L) A_t     (5)
  Theta(L) = I - sum Theta_j L^j,  B = [I_r ; B2],  B2 is s x r              (6)
  Ybar_t = (nabla Y_t2', W_t')',  W_t = Y_t1 + B2' Y_t2                       (17)
Nothing here is taken from drvec.
"""
import numpy as np


def blocks(B2, Lam):
    s, r = B2.shape
    M = r + s
    Cbar = np.zeros((M, M))
    Cbar[:s, r:] = np.eye(s)
    Cbar[s:, :r] = np.eye(r)
    Cbar[s:, r:] = B2.T                       # (11)
    Cinv = np.zeros((M, M))
    Cinv[:r, :s] = -B2.T
    Cinv[:r, s:] = np.eye(r)
    Cinv[r:, :s] = np.eye(s)                  # (14)
    Hbar = np.zeros((M, M))
    Hbar[s:, s:] = np.eye(r)                  # (13)
    LamBar = np.zeros((M, M))
    LamBar[:, s:] = Lam                       # (10)
    return Cbar, Cinv, Hbar, LamBar


def transform(B2, Lam, F, Th, Sig, p):
    """Eqs. (16) and (18).  F: list of p-1 MxM; Th: list of q MxM."""
    Cbar, Cinv, Hbar, LamBar = blocks(B2, Lam)
    M = Cbar.shape[0]
    PhB = [np.zeros((M, M)) for _ in range(p + 1)]
    PhB[0] = Cinv
    PhB[1] = Cinv @ Hbar - LamBar + (F[0] @ Cinv if p >= 2 else 0)
    for i in range(2, p):
        PhB[i] = F[i - 1] @ Cinv - F[i - 2] @ Cinv @ Hbar
    if p >= 2:
        PhB[p] = -F[p - 2] @ Cinv @ Hbar
    PhS = [None] + [Cbar @ PhB[i] for i in range(1, p + 1)]
    ThS = [None] + [Cbar @ T @ Cinv for T in Th]
    SgS = Cbar @ Sig @ Cbar.T
    return dict(Cbar=Cbar, Cinv=Cinv, Hbar=Hbar, LamBar=LamBar,
                PhB=PhB, PhS=PhS, ThS=ThS, SgS=SgS)


def polymul(A, B):
    """A, B lists of coefficient matrices (index = power of z)."""
    out = [np.zeros((A[0].shape[0], B[0].shape[1])) for _ in range(len(A) + len(B) - 1)]
    for i, a in enumerate(A):
        for j, b in enumerate(B):
            out[i + j] = out[i + j] + a @ b
    return out


def Phi_poly(B2, Lam, F, p):
    """Phi(z) = (I - sum F_i z^i)(1-z) + Lam B' z  -- the VARMA (1) behind (5)."""
    s, r = B2.shape
    M = r + s
    B = np.vstack([np.eye(r), B2])
    Fz = [np.eye(M)] + [-Fi for Fi in F]
    one_minus_z = [np.eye(M), -np.eye(M)]
    P = polymul(Fz, one_minus_z)
    while len(P) < p + 1:
        P.append(np.zeros((M, M)))
    P[1] = P[1] + Lam @ B.T
    return P


def G_poly(B2):
    """Ybar_t = G(L) Y_t,  G(z) = [[0, (1-z) I_s], [I_r, B2']]."""
    s, r = B2.shape
    M = r + s
    G0 = np.zeros((M, M)); G1 = np.zeros((M, M))
    G0[:s, r:] = np.eye(s); G1[:s, r:] = -np.eye(s)
    G0[s:, :r] = np.eye(r); G0[s:, r:] = B2.T
    return [G0, G1]


def det_poly_roots(coefs):
    """Roots of det(sum_k coefs[k] z^k) via companion of the monic reversed..."""
    # evaluate det on a grid is fragile; use numpy polynomial fit through
    # enough points (degree <= M*deg) -- exact up to rounding.
    M = coefs[0].shape[0]
    deg = M * (len(coefs) - 1)
    zs = np.exp(2j * np.pi * np.arange(deg + 1) / (deg + 1)) * 1.3
    vals = [np.linalg.det(sum(c * z ** k for k, c in enumerate(coefs))) for z in zs]
    c = np.polyfit(zs, vals, deg)
    return np.roots(c)


def ma_inf(PhS, ThS, M, L):
    """Psi weights of Ybar = Phi*(L)^-1 Theta*(L) A*;  Phi*(L)=I-sum Phi*_i L^i."""
    p = len(PhS) - 1
    q = len(ThS) - 1
    Psi = [np.eye(M)]
    for l in range(1, L + 1):
        acc = np.zeros((M, M))
        for i in range(1, min(l, p) + 1):
            acc += PhS[i] @ Psi[l - i]
        if l <= q:
            acc -= ThS[l]
        Psi.append(acc)
    return Psi


def autocov(PhS, ThS, SgS, M, nlag, L=4000):
    Psi = ma_inf(PhS, ThS, M, L + nlag)
    G = []
    for h in range(nlag + 1):
        acc = np.zeros((M, M))
        for k in range(L):
            acc += Psi[k + h] @ SgS @ Psi[k].T
        G.append(acc)          # Gamma(h) = E[y_{t+h} y_t']
    return G


def exact_loglik(Ybar, mu, PhS, ThS, SgS):
    """Exact Gaussian log-likelihood of the stacked Ybar_1..Ybar_n, both the
    full one at Sigma* = SgS and the one concentrated in a scalar scale."""
    n, M = Ybar.shape
    G = autocov(PhS, ThS, SgS, M, n - 1)
    Om = np.zeros((n * M, n * M))
    for i in range(n):
        for j in range(n):
            h = i - j
            Om[i*M:(i+1)*M, j*M:(j+1)*M] = G[h] if h >= 0 else G[-h].T
    y = (Ybar - mu).reshape(-1)
    Lc = np.linalg.cholesky(Om)
    z = np.linalg.solve(Lc, y)
    S = z @ z
    logdet = 2 * np.sum(np.log(np.diag(Lc)))
    N = n * M
    full = -0.5 * (N * np.log(2 * np.pi) + logdet + S)
    s2 = S / N
    conc = -0.5 * (N * (np.log(2 * np.pi) + np.log(s2) + 1) + logdet)
    return full, conc, s2


def simulate_vec(B2, Lam, F, Th, Sig, n, mu_vec=None, burn=300, rng=None):
    """Simulate levels Y (paper order) from (5) plus an intercept mu_vec."""
    s, r = B2.shape
    M = r + s
    B = np.vstack([np.eye(r), B2])
    rng = np.random.default_rng(rng)
    Ch = np.linalg.cholesky(Sig)
    N = n + burn
    A = rng.standard_normal((N, M)) @ Ch.T
    Y = np.zeros((N, M)); dY = np.zeros((N, M))
    mu_vec = np.zeros(M) if mu_vec is None else mu_vec
    for t in range(1, N):
        v = mu_vec - Lam @ (B.T @ Y[t - 1]) + A[t]
        for i, Fi in enumerate(F, start=1):
            if t - i >= 0:
                v += Fi @ dY[t - i]
        for j, Tj in enumerate(Th, start=1):
            if t - j >= 0:
                v -= Tj @ A[t - j]
        dY[t] = v
        Y[t] = Y[t - 1] + v
    return Y[burn:]
