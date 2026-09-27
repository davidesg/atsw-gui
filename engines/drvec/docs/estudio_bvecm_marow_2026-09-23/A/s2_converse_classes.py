"""s2 -- converses, and where each model class sits in Mauricio's VEC coordinates.

(a) VEC(2), M=2, r=1, rank Gamma_1 = 2: Gamma_1 alpha_perp != 0 -> not in the Theorem-1 image.
(b) MA converse of Cor. 2 ("every VEC with MA error structure can be transformed into the
    WARMA form"): VEC(1) + Theta_1 with non-zero Y2 row. The projection of dz2_t on the past has
    non-zero weight on lagged dz2 -> no Definition-3 representation.
(c) 'only if' of Theorem 6 without identification: left-multiply a triangular VEC-MA by
    (I - G L). Same process, invertible MA, stationary AR, but (5) and (6) violated.
(d) The article's illustrative full VAR(1) on (w, dz2) -> VEC: Gamma_1 = [[0,g12],[0,g22]].
(e) Legacy software's MA class Theta* = diag(theta on dz2, 0 on w) in VEC coordinates.
"""
import numpy as np, sympy as sp
rng = np.random.default_rng(7)

print("=== (a) VEC(2) with rank Gamma_1 = 2")
beta = 1.0; alpha = np.array([1.0, -beta]); aperp = np.array([beta, 1.0])
A = np.array([-0.3, 0.2]); G1 = np.array([[0.3, 0.2], [-0.1, 0.25]])
print("rank Gamma1 =", np.linalg.matrix_rank(G1), " Gamma1 @ alpha_perp =", G1 @ aperp,
      "(Theorem-1 image requires 0)")
# levels VAR(2) roots: must be 1 unit root and the rest outside
Pi = np.outer(A, alpha)
P1 = np.eye(2) + Pi + G1; P2 = -G1
comp = np.block([[P1, P2], [np.eye(2), np.zeros((2, 2))]])
ev = np.sort(np.abs(np.linalg.eigvals(comp)))[::-1]
print("companion |eig| (one = 1, rest < 1 => I(1), rank 1):", np.round(ev, 4))

print("\n=== (b) VEC(1) + MA with a non-zero dz2 row: no WARMA (Def. 3) form")
T = 200000
A = np.array([-0.4, 0.1]); Th = np.array([[0.0, 0.0], [0.0, 0.6]])   # dz2 row non-zero
Pi = np.outer(A, alpha)
e = rng.multivariate_normal([0, 0], [[1, .3], [.3, 1]], size=T)
dz = np.zeros((T, 2)); z = np.zeros((T, 2))
for t in range(1, T):
    dz[t] = Pi @ z[t - 1] + e[t] - Th @ e[t - 1]
    z[t] = z[t - 1] + dz[t]
w = z @ alpha; dz2 = dz[:, 1]
K = 8
X = np.column_stack([np.roll(dz2, j) for j in range(1, K + 1)] + [np.roll(w, j) for j in range(1, K + 1)])[K + 1:]
y = dz2[K + 1:]
b, *_ = np.linalg.lstsq(X, y, rcond=None)
res = y - X @ b; s2 = res @ res / (len(y) - X.shape[1])
se = np.sqrt(np.diag(s2 * np.linalg.inv(X.T @ X)))
print("OLS of dz2_t on 8 lags of dz2 and 8 lags of w; t-stats of the dz2 lags:")
print(np.round(b[:K] / se[:K], 1))
print("(Definition 3 implies these are all 0: dz2 depends on lagged w only, with white-noise eta)")

print("\n=== (c) same triangular process, second VEC-MA representation violating (5),(6)")
# triangular WARMA m=2 r=1: w_t = phi w_{t-1} + a_t - th a_{t-1}; dz2 = g w_{t-1} + eta
phi, th, g, beta = 0.6, 0.5, 0.2, 1.0
alpha = np.array([[1.0], [-beta]])
M1 = np.array([[beta * g + phi - 1], [g]]); A = M1
Tt = np.array([[th, -th * beta], [0, 0]])          # (6)
# levels VAR(1) poly: Phi(L) = I - (I + A alpha')L ; MA: I - Tt L
P1 = np.eye(2) + A @ alpha.T
G = np.array([[0.2, 0.1], [0.3, 0.4]])              # common left factor (I - G L), eig < 1
# new: (I - G L)(I - P1 L) = I - (P1 + G)L + G P1 L^2 ; MA: (I - G L)(I - Tt L) = I - (G + Tt)L + G Tt L^2
Q1 = P1 + G; Q2 = -G @ P1
Pi_new = -(np.eye(2) - Q1 - Q2)                     # Delta z = Pi z_{t-1} + Gam1 Dz_{t-1} + ...
Gam1_new = -Q2
Th1_new = G + Tt; Th2_new = -G @ Tt
print("Pi_new = (I-G)A alpha' ; rank", np.linalg.matrix_rank(Pi_new), "; alpha_new prop. to alpha:",
      np.allclose(Pi_new @ np.array([beta, 1.0]), 0))
print("Gamma1_new @ alpha_perp =", np.round(Gam1_new @ np.array([beta, 1.0]), 4), " (5) needs 0")
print("Theta1_new bottom row =", np.round(Th1_new[1], 4), " (6) needs 0")
print("MA invertible: eig of MA companion", np.round(np.abs(np.linalg.eigvals(
    np.block([[Th1_new, Th2_new], [np.eye(2), np.zeros((2, 2))]]))), 3))
# simulate both from the same innovations and the same start: identical paths
T = 500
a = rng.normal(size=T); eta = rng.normal(size=T) * 0.7 + 0.3 * a
At = np.column_stack([a + beta * eta, eta])
z = np.zeros((T, 2)); z2_ = np.zeros((T, 2))
for t in range(2, T):
    z[t] = P1 @ z[t - 1] + At[t] - Tt @ At[t - 1]
for t in range(2, T):
    if t < 4:
        z2_[t] = z[t]; continue
    z2_[t] = Q1 @ z2_[t - 1] + Q2 @ z2_[t - 2] + At[t] - Th1_new @ At[t - 1] - Th2_new @ At[t - 2]
print("max |path_triangular - path_second_rep| =", np.max(np.abs(z[4:] - z2_[4:])))

print("\n=== (d) article's illustrative full VAR(1) on (w, dz2) -> VEC")
p11, p12, p21, p22, b = sp.symbols('phi11 phi12 phi21 phi22 beta')
# dz1 = dw + b dz2 ; w_t = p11 w_{t-1} + p12 dz2_{t-1} + a1 ; dz2 = p21 w_{t-1} + p22 dz2_{t-1} + a2
Acoef = sp.Matrix([p11 - 1 + b * p21, p21])
Gam1 = sp.Matrix([[0, p12 + b * p22], [0, p22]])
print("A =", list(Acoef), " Gamma1 =", Gam1.tolist())
print("Gamma1 * alpha_perp =", list(sp.simplify(Gam1 * sp.Matrix([b, 1]))),
      " -> non-zero unless p12 + b p22 = p22 = 0: outside the Theorem-1 image")
print("Theorem-1 formula with p*=1 gives Gamma = 0: the 'formulas remain unchanged' remark is false for Gamma")

print("\n=== (e) MA classes in VEC coordinates (drvec convention: Ybar=[dY2; W], W = Y1 + B2'Y2)")
B2, t11, t12, t22, th = sp.symbols('B2 T11 T12 T22 theta')
Cb = sp.Matrix([[0, 1], [1, B2]])                   # Cbar dY = Ybar - Hbar Ybar_{t-1}
Cbi = Cb.inv()
def star(Th): return sp.simplify(Cb * Th * Cbi)
def vec(Ths): return sp.simplify(Cbi * Ths * Cb)
print("-marow  Theta=[[T11,T12],[0,0]]      -> Theta* =", star(sp.Matrix([[t11, t12], [0, 0]])).tolist())
print("-warma  Theta=[[T11,T11*B2],[0,0]]   -> Theta* =", star(sp.Matrix([[t11, t11 * B2], [0, 0]])).tolist())
print("-matri  Theta=[[T11,T12],[0,T22]]    -> Theta* =", star(sp.Matrix([[t11, t12], [0, t22]])).tolist())
leg = vec(sp.Matrix([[th, 0], [0, 0]]))
print("legacy  Theta*=diag(theta on dY2, 0 on W) -> Theta (VEC) =", leg.tolist())
print("   Y2 row of Theta non-zero -> OUTSIDE -marow/-mawarma/-warma; inside -matri (T11=0, T12=-B2*theta... check):",
      star(sp.Matrix([[0, leg[0, 1]], [0, th]])).tolist())
