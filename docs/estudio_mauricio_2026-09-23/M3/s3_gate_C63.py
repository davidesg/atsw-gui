"""s3_gate_C63.py -- Corollary 6.3 ("the engine's gate IS the admissibility
condition"), Theorems 4-5 ("invisible to the engine's checks").

chekma (elfvarma.c:500-530) rejects iff some eigenvalue of the MA companion has
modulus >= 1.00005.  Admissibility (Theorem 3) is rank(Lp' Theta(1)) = s.

(A) C6.3 items (1)-(2) algebra: det Theta(1) = det(I_r - sum T_k); companion
    spectrum = r-block companion spectrum + s*q zeros.     [random draws]
(B) gate-admitted but INADMISSIBLE point inside -marow (the default):
    T-block eigenvalue exactly 1 (|.| = 1 < 1.00005) aligned with Lp.
(C) gate-REJECTED but ADMISSIBLE point inside -marow: |t11| = 3.
(D) free class: every inadmissible point has an MA companion eigenvalue
    exactly 1 -- the same thing chekma "sees" in -marow.  So Theorem 5's
    "invisible" and C6.3's "visible" describe the same situation.
(E) Theorem 4: finite-sample covariance of a VMA(1) with det Theta(1) = 0 is
    non-singular (min eigenvalue > 0), for a free Theta with Lp-aligned null.
"""
import numpy as np
rng = np.random.default_rng(11)
TOL = 1.00005

def companion(Ths):
    q = len(Ths); M = Ths[0].shape[0]
    A = np.zeros((M * q, M * q))
    for k, T in enumerate(Ths): A[:M, k*M:(k+1)*M] = T
    for k in range(1, q): A[k*M:(k+1)*M, (k-1)*M:k*M] = np.eye(M)
    return A

def chekma_passes(Ths):
    return np.abs(np.linalg.eigvals(companion(Ths))).max() < TOL

def perp(A):
    u, s, vt = np.linalg.svd(A, full_matrices=True); return u[:, A.shape[1]:]

def admissible(Lam, Ths, tol=1e-9):
    Th1 = np.eye(Lam.shape[0]) - sum(Ths)
    return np.linalg.svd(perp(Lam).T @ Th1, compute_uv=False).min() > tol

# (A)
e1 = e2 = 0
for (r, s, q) in [(1,1,1),(2,3,1),(2,1,2),(3,2,3),(1,4,2),(2,2,4)]:
    for _ in range(200):
        M = r + s
        Ths = []
        for _ in range(q):
            T = np.zeros((M, M)); T[:r, :] = rng.normal(size=(r, M)); Ths.append(T)
        d1 = np.linalg.det(np.eye(M) - sum(Ths))
        d2 = np.linalg.det(np.eye(r) - sum(T[:r, :r] for T in Ths))
        e1 = max(e1, abs(d1 - d2) / max(1, abs(d2)))
        ev = np.sort_complex(np.linalg.eigvals(companion(Ths)))
        evr = np.linalg.eigvals(companion([T[:r, :r] for T in Ths]))
        evr = np.sort_complex(np.concatenate([evr, np.zeros(s * q)]))
        e2 = max(e2, np.abs(np.sort(np.abs(ev)) - np.sort(np.abs(evr))).max())
print("(A) C6.3(1) max rel err:", e1, "   C6.3(2) max |spectrum diff|:", e2)

# (B) M=2, r=1, q=1, -marow: Theta_1 = [[1, t12],[0,0]], Lambda with lam1 = -lam2*t12
t12 = 0.7; lam = np.array([[-0.8 * t12], [0.8]])
Ths = [np.array([[1.0, t12], [0.0, 0.0]])]
print("(B) -marow point Theta_1 =", Ths[0].tolist(), " Lambda =", lam.ravel())
print("    chekma passes:", chekma_passes(Ths), "  admissible (Thm 3):", admissible(lam, Ths))
# same with |t11| = 1.00004 (slightly non-invertible, still admitted by the gate)
Ths2 = [np.array([[1.00004, t12], [0.0, 0.0]])]
print("    t11 = 1.00004: chekma passes:", chekma_passes(Ths2), " (non-invertible, admitted)")

# (C) admissible but rejected
Ths = [np.array([[3.0, 0.4], [0.0, 0.0]])]; lam = np.array([[0.5], [-0.2]])
print("(C) Theta_1 =", Ths[0].tolist(), " chekma passes:", chekma_passes(Ths),
      " admissible (Thm 3):", admissible(lam, Ths))

# (D) free class: inadmissible => eigenvalue 1 of the companion
worst = 0
for _ in range(2000):
    M = rng.integers(2, 5); r = rng.integers(1, M); s = M - r; q = rng.integers(1, 3)
    Lam = rng.normal(size=(M, r)); Lp = perp(Lam)
    Ths = [rng.normal(size=(M, M)) * 0.4 for _ in range(q)]
    # force Lp c to be a left null vector of Theta(1): adjust Theta_1
    c = rng.normal(size=(s, 1)); w = Lp @ c; w /= np.linalg.norm(w)
    Th1 = np.eye(M) - sum(Ths)
    Ths[0] = Ths[0] + w @ (w.T @ Th1)           # now w' Theta(1) = 0
    assert not admissible(Lam, Ths)
    ev = np.abs(np.linalg.eigvals(companion(Ths)))
    worst = max(worst, np.abs(ev - 1).min())
print("(D) free class, inadmissible points: max over draws of min_i |(|lambda_i| - 1)| =", worst)

# (E) Theorem 4: covariance nonsingular at the boundary
M, n = 2, 150
Lam = np.array([[0.5], [-0.2]]); w = perp(Lam)[:, 0]
T = rng.normal(size=(M, M)) * 0.5
Th1 = np.eye(M) - T
T = T + np.outer(w, w @ Th1)                    # w'Theta(1) = 0, w in col(Lp)
Sigma = np.array([[1.0, 0.3], [0.3, 0.8]])
# Cov of (y_1..y_n), y_t = a_t - T a_{t-1}
G0 = Sigma + T @ Sigma @ T.T; G1 = -T @ Sigma  # Cov(y_t, y_{t-1}) = -T Sigma
V = np.zeros((M * n, M * n))
for t in range(n):
    V[t*M:(t+1)*M, t*M:(t+1)*M] = G0
    if t > 0:
        V[t*M:(t+1)*M, (t-1)*M:t*M] = G1
        V[(t-1)*M:t*M, t*M:(t+1)*M] = G1.T
print("(E) det Theta(1) =", np.linalg.det(np.eye(M) - T), " min eig of Cov(y_1..y_150) =",
      np.linalg.eigvalsh(V).min(), " (> 0: likelihood finite)")
# scalar check of the doc's formula for (1-B): eigenvalues 2 - 2cos(k pi/(n+1))
n = 50; Vs = 2*np.eye(n) - np.eye(n, k=1) - np.eye(n, k=-1)
ev = np.sort(np.linalg.eigvalsh(Vs)); th = np.sort(2 - 2*np.cos(np.arange(1, n+1)*np.pi/(n+1)))
print("    scalar (1-B): max|eig - (2-2cos)| =", np.abs(ev - th).max(), "; min eig =", ev.min(),
      " ~ pi^2/n^2 =", np.pi**2/(n+1)**2)
