"""s1_algebra.py -- Theorems 1, 2, 2' of DEMOSTRACIONES.md, numerically.

Checks, over random draws (M, r, p, q):
  (T1)  the Phi_bar_k of the statement satisfy  Phi_bar(L) D(L) Cbar = Phi(L)
        with D(L) = diag((1-L) I_s, I_r)  -- i.e. Ybar = D(L) Cbar Y exactly.
  (T2') det Phi_bar(x) (1-x)^s det Cbar = det Phi(x)   for all x
        => roots of |Phi*| are the roots of |Phi| minus s unit roots: no
           identification/coprimeness argument is needed.
  (c)   Phi_bar(1) = [F(1) B_perp , Lambda]  and
        det Phi_bar(1) != 0  <=>  det(Lambda_perp' F(1) B_perp) != 0,
        so the engine's AR-stationarity gate on Phi* enforces (b)+(c).
  (T2)  the residue lim_{x->1} (1-x) Phi(x)^{-1} equals
        C = B_perp (Lambda_perp' F(1) B_perp)^{-1} Lambda_perp'.
"""
import numpy as np
rng = np.random.default_rng(1)

def null_left(A):
    # orthonormal basis of the orthogonal complement of col(A)
    u, s, vt = np.linalg.svd(A, full_matrices=True)
    return u[:, A.shape[1]:]

def polyval_mat(coefs, x):
    return sum(c * x**k for k, c in enumerate(coefs))

def draw(M, r, p):
    s = M - r
    Lam = rng.normal(size=(M, r))
    B2 = rng.normal(size=(s, r))
    B = np.vstack([np.eye(r), B2])
    F = [0.3 * rng.normal(size=(M, M)) for _ in range(p - 1)]
    return Lam, B2, B, F

def phi_levels(Lam, B, F, M):
    # Phi(L) = F(L)(1-L) + Lam B' L,  F(L) = I - sum F_i L^i
    p = len(F) + 1
    Fc = [np.eye(M)] + [-Fi for Fi in F]          # F(L) coefficients
    P = [np.zeros((M, M)) for _ in range(p + 1)]
    for k, c in enumerate(Fc):
        P[k] += c
        P[k + 1] -= c
    P[1] += Lam @ B.T
    return P, Fc

def phibar(Lam, B2, F, M, r):
    s = M - r
    Cbar = np.block([[np.zeros((s, r)), np.eye(s)], [np.eye(r), B2.T]])
    Cinv = np.block([[-B2.T, np.eye(r)], [np.eye(s), np.zeros((s, r))]])
    Hbar = np.zeros((M, M)); Hbar[s:, s:] = np.eye(r)
    LamBar = np.hstack([np.zeros((M, s)), Lam])
    p = len(F) + 1
    Pb = [None] * (p + 1)
    Pb[0] = Cinv.copy()
    Pb[1] = Cinv @ Hbar - LamBar + (F[0] @ Cinv if p >= 2 else 0)
    for i in range(2, p):
        Pb[i] = F[i - 1] @ Cinv - F[i - 2] @ Cinv @ Hbar
    Pb[p] = -F[p - 2] @ Cinv @ Hbar if p >= 2 else np.zeros((M, M))
    if p == 1:
        Pb[1] = Cinv @ Hbar - LamBar
    # doc convention:  Phi_bar_0 Ybar_t = sum Phi_bar_k Ybar_{t-k} + ...
    # as an operator:  Phi_bar(L) = Phi_bar_0 - sum_k Phi_bar_k L^k
    op = [Pb[0]] + [-Pb[k] for k in range(1, p + 1)]
    return op, Cbar, Cinv

worst = dict(T1=0, T2p=0, c=0, T2=0)
for trial in range(400):
    M = rng.integers(2, 6); r = rng.integers(1, M); p = rng.integers(1, 4)
    s = M - r
    Lam, B2, B, F = draw(M, r, p)
    P, Fc = phi_levels(Lam, B, F, M)
    op, Cbar, Cinv = phibar(Lam, B2, F, M, r)
    for x in rng.normal(size=3) + 1j * rng.normal(size=3):
        D = np.diag([1 - x] * s + [1] * r)
        lhs = polyval_mat(op, x) @ D @ Cbar
        rhs = polyval_mat(P, x)
        worst['T1'] = max(worst['T1'], np.abs(lhs - rhs).max())
        d1 = np.linalg.det(polyval_mat(op, x)) * (1 - x)**s * np.linalg.det(Cbar)
        d2 = np.linalg.det(rhs)
        worst['T2p'] = max(worst['T2p'], abs(d1 - d2) / max(1, abs(d2)))
    # (c)
    Bp = np.vstack([-B2.T, np.eye(s)])
    F1 = polyval_mat(Fc, 1.0)
    PB1 = polyval_mat(op, 1.0)
    worst['c'] = max(worst['c'], np.abs(PB1 - np.hstack([F1 @ Bp, Lam])).max())
    # (T2) residue
    Lp = null_left(Lam)
    K = Lp.T @ F1 @ Bp
    if abs(np.linalg.det(K)) > 1e-3:
        C = Bp @ np.linalg.solve(K, Lp.T)
        eps = 1e-7
        R = eps * np.linalg.inv(polyval_mat(P, 1 - eps))
        worst['T2'] = max(worst['T2'], np.abs(R - C).max() / max(1, np.abs(C).max()))

print("max |Phi_bar(x) D(x) Cbar - Phi(x)|           :", worst['T1'])
print("max rel |detPhibar (1-x)^s detC - detPhi|     :", worst['T2p'])
print("max |Phi_bar(1) - [F(1)B_perp, Lambda]|       :", worst['c'])
print("max rel |(1-x)Phi^-1 at x=1-1e-7  -  C|       :", worst['T2'], "(O(eps) expected)")
