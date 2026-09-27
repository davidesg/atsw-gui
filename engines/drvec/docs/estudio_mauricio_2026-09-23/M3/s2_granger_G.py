"""s2_granger_G.py -- Theorem 3 vs what granger_smin() computes.

Theorem 3 (DEMOSTRACIONES): rank of cointegration = r  <=>  rank(Lp' Theta(1)) = s
   (Lp' Theta(1) is s x M).
Code (drvec.c:390-470): sigma_min(G),  G = Lp' Theta(1) Bp   (s x s), both
   orthonormalised; reported as "Rank condition (Granger)", imposed by
   -rankadm, used for the ADMISSIBLE column of -specs, and a WARNING
   "This fit DENIES THE RANK" when sigma_min(G) < 0.2.

(1) sigma_min(G) > 0  => rank(Lp'Theta(1)) = s     (sufficient)          -- true
(2) the converse fails: explicit point in the DEFAULT class (-marow),
    MA invertible, det Theta(1) != 0, (b),(c) hold, rank exactly r, G = 0.
(3) in -mawarma, Theta(1) Bp = Bp, so G = Lp'Bp for ANY Theta: the reported
    'rank condition' does not depend on the MA at all.
(4) with q = 0 (pure VAR), G = Lp'Bp too -- but the code only calls it for q>0.
(5) the true long-run matrix C Theta(1) has rank s at the point of (2):
    checked analytically and by simulation (Var of b'Y grows only for b not in col(B)).
"""
import numpy as np
rng = np.random.default_rng(7)

def orth(A):
    q, _ = np.linalg.qr(A)
    return q

def perp(A):
    u, s, vt = np.linalg.svd(A, full_matrices=True)
    return u[:, A.shape[1]:]

def smin(A):
    return np.linalg.svd(A, compute_uv=False).min()

def G_code(Lam, B2, Theta1):
    M, r = Lam.shape; s = M - r
    Lp = perp(Lam)
    Bp = orth(np.vstack([-B2.T, np.eye(s)]))
    return smin(Lp.T @ Theta1 @ Bp)

def rank_cond(Lam, Theta1):
    Lp = perp(Lam)
    return smin(Lp.T @ Theta1)   # s x M: its s-th singular value

# (1) random draws: G>0 => theorem condition; count disagreements
dis = 0; N = 20000
for _ in range(N):
    M = rng.integers(2, 5); r = rng.integers(1, M); s = M - r
    Lam = rng.normal(size=(M, r)); B2 = rng.normal(size=(s, r))
    T1 = rng.normal(size=(M, M))
    g, t = G_code(Lam, B2, T1), rank_cond(Lam, T1)
    if t < 1e-10 and g > 1e-10: dis += 1
print("(1) draws with theorem-condition FAILING but G > 0 :", dis, "of", N)

# (2) the counterexample, M=2, r=1, p=1 (F = I), q=1, class -marow
lam = np.array([[0.5], [-0.2]]); b2 = -1.0
B2 = np.array([[b2]]); B = np.array([[1.0], [b2]])
t11, t12 = 0.5, 3.0                      # Theta_1 = [[t11, t12],[0,0]]  (-marow)
Th1 = np.array([[t11, t12], [0.0, 0.0]])
Theta1 = np.eye(2) - Th1                  # Theta(1)
print("\n(2) point: Lambda =", lam.ravel(), " B2 =", b2, " Theta_1 =", Th1.tolist())
print("    MA companion eigenvalues (moduli):", np.abs(np.linalg.eigvals(Th1)))
print("    det Theta(1)              =", np.linalg.det(Theta1))
print("    sigma_min(Lp' Theta(1))   =", rank_cond(lam, Theta1), " (theorem 3: rank OK iff > 0)")
print("    sigma_min(G) [code]       =", G_code(lam, B2, Theta1), " (code: 'DENIES THE RANK' if < 0.2)")
Lp = perp(lam); Bp = np.array([[-b2], [1.0]])
K = Lp.T @ np.eye(2) @ Bp
print("    (c) Lp'F(1)Bp             =", K.ravel(), "(non-zero)")
Pi = lam @ B.T
Phi1 = np.eye(2) - Pi                     # Y_t = (I - Lam B') Y_{t-1} + ...
# levels VAR(1): Phi(x) = I - (I - Pi) x ; roots
ev = np.linalg.eigvals(Phi1)
print("    eigenvalues of I - Lambda B' :", ev, " (one unit root, other |.|<1 => (b) holds)")
C = Bp @ np.linalg.solve(K, Lp.T)
print("    rank C Theta(1) =", np.linalg.matrix_rank(C @ Theta1, tol=1e-10), " (= s = 1: rank of cointegration exactly r=1)")

# simulate
n = 40000
A = rng.normal(size=(n + 1, 2))
E = A[1:] - A[:-1] @ Th1.T                # e_t = A_t - Theta_1 A_{t-1}
Y = np.zeros((n, 2))
for t in range(1, n):
    Y[t] = Y[t - 1] - Pi @ Y[t - 1] + E[t]
W = Y @ B
other = Y @ np.array([1.0, 0.0])
# k-step variance ratio  Var(b'Y_{t+k} - b'Y_t)/k : -> long-run variance (>0 iff b'Y is I(1))
for k in (10, 100, 1000):
    vr_B = np.var(W[k:] - W[:-k]) / k
    vr_1 = np.var(other[k:] - other[:-k]) / k
    print(f"    k={k:5d}  VR(B'Y)={vr_B:8.4f}   VR(Y1)={vr_1:8.4f}")
LR = C @ Theta1 @ Theta1.T @ C.T
print("    analytic long-run var of dY (C Th(1) Sigma Th(1)' C'), Sigma=I:", LR.ravel(), " rank", np.linalg.matrix_rank(LR, tol=1e-10))
np.savetxt('dgp_G0.txt', Y[:2001])

# (3) -mawarma: Theta(1) Bp == Bp for any Theta_w
for _ in range(3):
    M, r = 4, 2; s = M - r
    B2 = rng.normal(size=(s, r)); Tw = rng.normal(size=(r, r))
    Th = np.zeros((M, M)); Th[:r, :r] = Tw; Th[:r, r:] = Tw @ B2.T
    Bp = np.vstack([-B2.T, np.eye(s)])
    Lam = rng.normal(size=(M, r))
    print("(3) mawarma: |Theta(1)Bp - Bp| =", np.abs((np.eye(M) - Th) @ Bp - Bp).max(),
          "  G(code) =", round(G_code(Lam, B2, np.eye(M) - Th), 6),
          "  sigma_min(Lp'Bp) =", round(smin(perp(Lam).T @ orth(Bp)), 6))

# (4) pure VAR analogue (Theta=I): G = Lp'Bp can vanish while the VAR(2) is I(1) rank r
lam = np.array([[1.0], [1.0]]); B2 = np.array([[-1.0]])   # Lambda'B = 0
Bp = np.array([[1.0], [1.0]])
F1 = np.array([[0.5, 0.0], [0.0, -0.3]])                    # VAR(2) in VEC form
Lp = perp(lam)
print("\n(4) q=0 analogue: sigma_min(Lp'Bp) =", smin(Lp.T @ orth(Bp)),
      "  Lp'F(1)Bp =", (Lp.T @ (np.eye(2) - F1) @ Bp).ravel(), "(nonzero: valid I(1), rank 1)")
