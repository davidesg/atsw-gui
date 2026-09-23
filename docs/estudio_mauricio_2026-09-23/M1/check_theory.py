"""Checks of the paper's own claims (not of drvec).

1. Unit roots of |Phi(z)| counted by companion linearisation (fixes the crude
   count in check_code.identities).
2. Theorem S (study sec. 3.3): Phi* stationary  <=>  exactly s unit roots
   <=>  det(Lam_perp' F(1) B_perp) != 0.  Counterexample where rank(Pi)=r but
   the I(1) condition fails: Phi* gets a unit root (the transformation still
   holds algebraically, the transformed model is NOT stationary).
3. Remark 6: E[Ybar] = m  <=>  VEC intercept mu = PhiBar(1) m = F(1) gamma + Lam m_W;
   case 2 (E nabla Y2 = 0)  <=>  mu = Lam m_W (restricted constant).
4. -warma class = paper model with F_i = M_i B' and Theta = [T11 T11 B2'; 0 0]
   (bijection, closed form of the inverse).
"""
import numpy as np
from paper import transform, Phi_poly, blocks, simulate_vec


def unit_root_count(P, tol=1e-6):
    M = P[0].shape[0]; k = len(P) - 1
    # det(I + P1 z + ... + Pk z^k) = 0 ; lambda = 1/z : lambda^k I + ... + Pk
    C = np.zeros((M * k, M * k))
    for i in range(k):
        C[:M, i*M:(i+1)*M] = -P[i + 1]
    if k > 1:
        C[M:, :-M] = np.eye(M * (k - 1))
    lam = np.linalg.eigvals(C)
    return int(np.sum(abs(lam - 1) < tol)), lam


def maxmod_star(PhS, M):
    p = len(PhS) - 1
    C = np.zeros((M * p, M * p))
    for i in range(p):
        C[:M, i*M:(i+1)*M] = PhS[i + 1]
    if p > 1:
        C[M:, :-M] = np.eye(M * (p - 1))
    return max(abs(np.linalg.eigvals(C)))


rng = np.random.default_rng(7)
print('--- 1/2: unit roots and stationarity of Phi* ---')
for (M, r, p) in [(3, 1, 3), (3, 2, 2), (4, 2, 3), (2, 1, 1)]:
    s = M - r
    B2 = rng.uniform(-1, 1, (s, r)); Lam = rng.uniform(-.5, .5, (M, r))
    F = [rng.uniform(-.3, .3, (M, M)) for _ in range(p - 1)]
    P = Phi_poly(B2, Lam, F, p)
    nu, _ = unit_root_count(P)
    tr = transform(B2, Lam, F, [], np.eye(M), p)
    F1 = np.eye(M) - sum(F) if F else np.eye(M)
    Bp = np.vstack([-B2.T, np.eye(s)])
    Lp = np.linalg.svd(Lam)[0][:, r:]
    print(f'M={M} r={r} p={p}: unit roots of |Phi| = {nu} (s={s}); '
          f'sigma_min(Lp\'F(1)Bp) = {np.linalg.svd(Lp.T @ F1 @ Bp)[1].min():.3f}; '
          f'max |eig companion Phi*| = {maxmod_star(tr["PhS"], M):.4f}')

# counterexample: M=2, r=1, rank(Pi)=1 but Lam_perp' F(1) B_perp = 0  (I(2))
B2 = np.array([[0.5]]); Lam = np.array([[0.4], [0.0]])   # Lam_perp = e2
Bp = np.array([[-0.5], [1.0]])
# need e2' (I - F1) Bp = 0  ->  choose F1 row 2 so that (I-F1)[1,:] @ Bp = 0
F1 = np.array([[0.2, 0.1], [0.0, 0.0]])
F1[1, 0] = 0.0; F1[1, 1] = 1.0 + 0.0      # (I-F1)[1,:] = [0, 0] -> kills it
F1[1, 0] = 0.3; F1[1, 1] = 1.0 - 0.3 * 0.5  # (I-F1)[1,:]=[-0.3,0.15]; @Bp = .15+.15?
# solve exactly: (I-F1)[1,:] = [a, b] with -0.5 a + b = 0 -> b = 0.5 a
a = -0.3; F1[1, 0] = -a; F1[1, 1] = 1 - 0.5 * a
P = Phi_poly(B2, Lam, [F1], 2)
nu, lam = unit_root_count(P, 1e-5)
tr = transform(B2, Lam, [F1], [], np.eye(2), 2)
print(f'COUNTEREXAMPLE rank(Pi)=1 but Lp\'F(1)Bp = {(np.array([[0,1]]) @ (np.eye(2)-F1) @ Bp).item():.2e}: '
      f'unit roots of |Phi| = {nu} (>s=1); max|eig companion Phi*| = {maxmod_star(tr["PhS"], 2):.6f}')

print('--- 3: Remark 6 means ---')
M, r, p = 3, 1, 2; s = M - r
B2 = np.array([[0.7], [-0.4]]); Lam = np.array([[0.3], [-0.2], [0.1]])
F = [np.array([[0.2, 0.1, 0], [0, 0.3, 0.1], [0.1, 0, 0.2]])]
Th = [np.array([[0.3, 0, 0.1], [0, 0.2, 0], [0.1, 0, 0.1]])]
Sig = np.eye(M) * 0.1
tr = transform(B2, Lam, F, Th, Sig, p)
PhB1 = tr['PhB'][0] - sum(tr['PhB'][1:])
F1 = np.eye(M) - F[0]; Bp = np.vstack([-B2.T, np.eye(s)])
for case, m in [(3, np.array([0.05, -0.02, 1.5])), (2, np.array([0, 0, 1.5]))]:
    mu = PhB1 @ m
    gamma = Bp @ m[:s]
    print(f'case {case}: mu = PhiBar(1) m = {mu.round(5)};  F(1)gamma + Lam m_W = '
          f'{(F1 @ gamma + Lam[:, 0] * m[s]).round(5)};  Lam m_W = {(Lam[:, 0]*m[s]).round(5)}')
    Y = simulate_vec(B2, Lam, F, Th, Sig, 200000, mu_vec=mu, rng=3)
    Yb = np.hstack([np.diff(Y[:, r:], axis=0), Y[1:, :r] + Y[1:, r:] @ B2])
    print(f'   sample mean of Ybar (n=2e5) = {Yb.mean(0).round(4)}  vs m = {m};  '
          f'mean nabla Y1 = {np.diff(Y[:, :r], axis=0).mean():.4f} vs -B2\'E nabla Y2 = {(-B2.T @ m[:s]).item():.4f}')

print('--- 4: -warma class <-> paper model with F_i = M_i B\', inverse in closed form ---')
for (M, r, p) in [(3, 1, 3), (3, 2, 2), (2, 1, 1)]:
    s = M - r
    B2 = rng.uniform(-1, 1, (s, r)); B = np.vstack([np.eye(r), B2])
    c = [rng.uniform(-.5, .5, (M, r)) for _ in range(p)]   # W-columns of Phi*_k
    Cbar, Cinv, Hbar, _ = blocks(B2, np.zeros((M, r)))
    D = [Cinv @ ck for ck in c]
    E1 = np.vstack([np.eye(r), np.zeros((s, r))])
    Lam = E1 - sum(D)
    Mk = []
    for k in range(1, p):
        Mk.append(Lam - E1 + D[0] if k == 1 else Mk[-1] + D[k - 1])
    F = [Mi @ B.T for Mi in Mk]
    tr = transform(B2, Lam, F, [], np.eye(M), p)
    err = 0
    for k in range(1, p + 1):
        target = np.zeros((M, M)); target[:, s:] = c[k - 1]
        err = max(err, abs(tr['PhS'][k] - target).max())
    print(f'M={M} r={r} p={p}: max |Phi*_k(paper, reconstructed VEC) - warma Phi*_k| = {err:.1e}')
