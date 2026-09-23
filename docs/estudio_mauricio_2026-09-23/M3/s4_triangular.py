"""s4_triangular.py -- Theorem 6 / 6b, Corollaries 6.1, 6.2.

(a) Direct direction (T6 + T6b): simulate the WARMA  Phi_w(B) w = Theta_w(B) a,
    dz2 = sum_{j=0..k} Psi_j w_{t-1-j} + eta, and verify EXACTLY (to rounding) that
      dz_t = A alpha' z_{t-1} + sum_i Gamma_i dz_{t-i} + A_t - sum_j Th~_j A_{t-j}
    with M_l, A, Gamma_i of the doc, Th~_j = [[Th_wj, -Th_wj beta'],[0,0]],
    A_t = (a_t + beta' eta_t ; eta_t).
(b) Converse of the BVECM appendix ("ANY VEC of rank r is WARMA") is false:
    VEC(2) with Gamma_1 of rank 2 (M=2, r=1).  The WARMA class forces
    rank(Gamma_i) <= r; OLS on a long simulated path recovers rank 2.
(c) C6.1 table, symbolic (sympy): Theta* = Cbar Theta Cbar^-1 per pattern.
(d) C6.2: Theta(1) under (6); the proof's Theta(L) cross block is misprinted.
"""
import numpy as np, sympy as sp
rng = np.random.default_rng(3)

# ---------- (a)
def check_direct(m, r, p, k, q, n=400):
    s = m - r
    beta = rng.normal(size=(s, r))          # alpha = [I_r ; -beta]
    alpha = np.vstack([np.eye(r), -beta])
    Phi = [0.3 * rng.normal(size=(r, r)) for _ in range(p)]
    Psi = [rng.normal(size=(s, r)) for _ in range(k + 1)]   # Psi_0 = gamma
    Thw = [0.4 * rng.normal(size=(r, r)) for _ in range(q)]
    a = rng.normal(size=(n, r)); eta = rng.normal(size=(n, s))
    # a and eta correlated contemporaneously is allowed; add some
    eta = eta + 0.3 * a[:, :1] if s >= 1 else eta
    w = np.zeros((n, r)); z = np.zeros((n, m))
    for t in range(n):
        acc = a[t].copy()
        for j in range(1, p + 1):
            if t - j >= 0: acc += Phi[j - 1] @ w[t - j]
        for j in range(1, q + 1):
            if t - j >= 0: acc -= Thw[j - 1] @ a[t - j]
        w[t] = acc
        dz2 = eta[t].copy()
        for j in range(k + 1):
            if t - 1 - j >= 0: dz2 += Psi[j] @ w[t - 1 - j]
        z2 = (z[t - 1, r:] if t > 0 else 0) + dz2
        z1 = w[t] + beta.T @ z2
        z[t] = np.concatenate([z1, z2])
    ps = max(p, k + 1)
    Ml = []
    for l in range(1, ps + 1):
        Phl = Phi[l - 1] if l <= p else np.zeros((r, r))
        if l == 1:
            top = beta.T @ Psi[0] + Phl - np.eye(r); bot = Psi[0]
        else:
            Pl = Psi[l - 1] if l - 1 <= k else np.zeros((s, r))
            top = beta.T @ Pl + Phl; bot = Pl
        Ml.append(np.vstack([top, bot]))
    A = sum(Ml)
    Gam = [-sum(Ml[l - 1] for l in range(i + 1, ps + 1)) @ alpha.T for i in range(1, ps)]
    Th = [np.block([[T, -T @ beta.T], [np.zeros((s, r)), np.zeros((s, s))]]) for T in Thw]
    At = np.hstack([a + eta @ beta, eta])
    err = 0
    dz = np.vstack([np.zeros((1, m)), np.diff(z, axis=0)])
    t0 = ps + q + 2
    for t in range(t0, n):
        rhs = A @ alpha.T @ z[t - 1] + At[t]
        for i in range(1, ps): rhs += Gam[i - 1] @ dz[t - i]
        for j in range(1, q + 1): rhs -= Th[j - 1] @ At[t - j]
        err = max(err, np.abs(rhs - dz[t]).max() / max(1.0, np.abs(dz[t]).max()))
    ranks = [np.linalg.matrix_rank(G, tol=1e-10) for G in Gam]
    return err, ranks

for dims in [(2,1,1,0,1), (3,1,2,1,2), (4,2,3,2,1), (5,3,2,3,3)]:
    e, rk = check_direct(*dims)
    print(f"(a) m,r,p,k,q={dims}: max rel |VEC residual| = {e:.2e}; rank(Gamma_i) = {rk} (<= r)")

# ---------- (b)
n = 200000
lam = np.array([[0.4], [-0.1]]); B = np.array([[1.0], [-1.0]])     # dz = -lam B'z + G1 dz(-1) + e
G1 = np.array([[0.3, 0.2], [-0.1, 0.25]])                          # rank 2
z = np.zeros((n, 2)); dz = np.zeros((n, 2)); e = rng.normal(size=(n, 2))
for t in range(2, n):
    dz[t] = -lam @ (B.T @ z[t - 1]) + G1 @ dz[t - 1] + e[t]
    z[t] = z[t - 1] + dz[t]
X = np.hstack([z[1:-1] @ B, dz[1:-1]]); Yv = dz[2:]
coef, *_ = np.linalg.lstsq(X, Yv, rcond=None)
G1hat = coef[1:].T
print("\n(b) VEC(2), true Gamma_1 =", G1.tolist())
print("    OLS Gamma_1 =", np.round(G1hat, 3).tolist(), " singular values:", np.round(np.linalg.svd(G1hat, compute_uv=False), 3))
print("    every WARMA-derived VEC has rank(Gamma_i) <= r = 1 (see (a)); this one has 2 => no WARMA form.")

# ---------- (c)
r, s = 1, 2
T11 = sp.Matrix(r, r, lambda i, j: sp.Symbol(f't11_{i}{j}'))
T12 = sp.Matrix(r, s, lambda i, j: sp.Symbol(f't12_{i}{j}'))
T21 = sp.Matrix(s, r, lambda i, j: sp.Symbol(f't21_{i}{j}'))
T22 = sp.Matrix(s, s, lambda i, j: sp.Symbol(f't22_{i}{j}'))
B2 = sp.Matrix(s, r, lambda i, j: sp.Symbol(f'b_{i}{j}'))
Cb = sp.BlockMatrix([[sp.zeros(s, r), sp.eye(s)], [sp.eye(r), B2.T]]).as_explicit()
Ci = Cb.inv()
def star(T): return sp.simplify(Cb * T * Ci)
pats = {
  'free': sp.BlockMatrix([[T11, T12], [T21, T22]]).as_explicit(),
  'matri [T11 T12;0 T22]': sp.BlockMatrix([[T11, T12], [sp.zeros(s, r), T22]]).as_explicit(),
  'marow [T11 T12;0 0]': sp.BlockMatrix([[T11, T12], [sp.zeros(s, r), sp.zeros(s, s)]]).as_explicit(),
  'mawarma [T11 T11B2\';0 0]': sp.BlockMatrix([[T11, T11 * B2.T], [sp.zeros(s, r), sp.zeros(s, s)]]).as_explicit(),
}
print("\n(c) C6.1: Theta* = Cbar Theta Cbar^-1 in Ybar=[dY2;W] order (r=1,s=2)")
for k, T in pats.items():
    S = star(T)
    zero_pattern = [[('0' if sp.simplify(S[i, j]) == 0 else 'x') for j in range(r + s)] for i in range(r + s)]
    print(f"   {k:28s} pattern {zero_pattern};  W-block = {sp.simplify(S[s:, s:])}")
S = star(pats['matri [T11 T12;0 T22]'])
print("   matri: dY2 block of Theta* equals T22:", sp.simplify(S[:s, :s] - T22) == sp.zeros(s, s))

# ---------- (d)
L = sp.Symbol('L')
Tw = sp.Symbol('tw'); b = sp.Symbol('b')
ThL = sp.eye(2) - sp.Matrix([[Tw, Tw * b], [0, 0]]) * L       # M=2, r=1, q=1, (6)
print("\n(d) Theta(L) under (6):", ThL.tolist(), "; doc's proof writes the cross block as Theta_w(L)B2' =", sp.expand((1 - Tw * L) * b))
print("    Theta(1) =", ThL.subs(L, 1).tolist(), " det =", sp.factor(ThL.subs(L, 1).det()))
