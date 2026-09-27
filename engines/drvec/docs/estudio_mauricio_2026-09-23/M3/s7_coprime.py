"""s7_coprime.py -- Theorem 3's rank condition IS left-coprimeness of (Phi, Theta)
at z = 1:

   rank(Lp' Theta(1)) = s   <=>   rank [ Phi(1)  Theta(1) ] = M,   Phi(1) = Lambda B'.

Proof: w'[Lambda B', Theta(1)] = 0  <=>  w'Lambda = 0 (B' has full row rank) and
w'Theta(1) = 0  <=>  w = Lp c  with  c' Lp' Theta(1) = 0.

So the 'fatal' points of Theorems 3-5 are exactly the points where the levels
VARMA loses left-coprimeness at z = 1 (a common factor (1-L) in direction w)
-- which Yap-Reinsel (p.253) and Mauricio (2006, p.3646) ASSUME.  The claim of
DEMOSTRACIONES s7 item 2 (the rank condition is 'stronger than coprimeness'; a
point can be 'a perfectly identified VARMA and inadmissible') is false.

Random draws: generic points, and points forced onto the fatal surface.
"""
import numpy as np
rng = np.random.default_rng(21)
def perp(A):
    u, s, vt = np.linalg.svd(A, full_matrices=True); return u[:, A.shape[1]:]
def sv_min(A): return np.linalg.svd(A, compute_uv=False)[-1] if A.shape[0] <= A.shape[1] else np.linalg.svd(A, compute_uv=False).min()
agree = tot = 0
for _ in range(20000):
    M = rng.integers(2, 6); r = rng.integers(1, M); s = M - r
    Lam = rng.normal(size=(M, r)); B = np.vstack([np.eye(r), rng.normal(size=(s, r))])
    Th1 = rng.normal(size=(M, M))
    if rng.random() < 0.5:                      # force the fatal surface
        w = perp(Lam) @ rng.normal(size=(s, 1)); w /= np.linalg.norm(w)
        Th1 = Th1 - w @ (w.T @ Th1)
    a = sv_min(perp(Lam).T @ Th1) < 1e-9
    b = np.linalg.svd(np.hstack([Lam @ B.T, Th1]), compute_uv=False)[M - 1] < 1e-9
    agree += (a == b); tot += 1
print(f"rank condition fails  <=>  [Phi(1) Theta(1)] rank-deficient : agreement {agree}/{tot}")
