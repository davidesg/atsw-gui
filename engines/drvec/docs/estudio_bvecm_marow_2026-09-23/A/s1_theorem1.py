"""s1 -- BVECM Appendix Theorem 1 + Corollary 2 (cor:warma_ma), checked numerically.

Direct direction: simulate the WARMA of Definition 3 (+ MA on w, Cor. 2) with
contemporaneously correlated (a, eta), build A = sum M_l, Gamma_i = -sum_{l>i} M_l alpha',
eps_t = (a + beta' eta + ... ; eta) and the regrouped Theta~_j = [[Th_j, -Th_j beta'],[0,0]]
on A_t = (a_t + beta' eta_t ; eta_t); check the VEC reproduces Delta z exactly.

Step 6 (converse) claims checked: Phi_1 = alpha'A + I_r ; gamma = A_2.
"""
import numpy as np
rng = np.random.default_rng(12345)

def run(m, r, p, k, q, T=400):
    s = m - r
    beta = rng.normal(size=(s, r))
    alpha = np.vstack([np.eye(r), -beta])            # m x r
    # stable AR for w
    while True:
        Phi = [0.5 * rng.normal(size=(r, r)) / np.sqrt(r * p) for _ in range(p)]
        comp = np.zeros((r * p, r * p))
        comp[:r, :] = np.hstack(Phi)
        if p > 1:
            comp[r:, :-r] = np.eye(r * (p - 1))
        if np.max(np.abs(np.linalg.eigvals(comp))) < 0.9:
            break
    Psi = [0.4 * rng.normal(size=(s, r)) for _ in range(k + 1)]   # Psi_0 = gamma
    Th = [0.4 * rng.normal(size=(r, r)) / np.sqrt(r) for _ in range(q)]
    L = rng.normal(size=(m, m)); Sig = L @ L.T         # cov of (a, eta), correlated
    e = rng.multivariate_normal(np.zeros(m), Sig, size=T)
    a, eta = e[:, :r], e[:, r:]
    # simulate w and z2, z1
    w = np.zeros((T, r)); dz2 = np.zeros((T, s))
    ma = np.zeros((T, r))
    for t in range(T):
        ma[t] = a[t] - sum(Th[j] @ a[t - 1 - j] for j in range(q) if t - 1 - j >= 0)
        w[t] = sum(Phi[j] @ w[t - 1 - j] for j in range(p) if t - 1 - j >= 0) + ma[t]
        dz2[t] = sum(Psi[j] @ w[t - 1 - j] for j in range(k + 1) if t - 1 - j >= 0) + eta[t]
    z2 = np.cumsum(dz2, axis=0)
    z1 = w + z2 @ beta
    z = np.hstack([z1, z2]); dz = np.vstack([np.zeros(m), np.diff(z, axis=0)])
    # Theorem 1 matrices
    ps = max(p, k + 1)
    Phif = lambda l: Phi[l - 1] if 1 <= l <= p else np.zeros((r, r))
    Psif = lambda j: Psi[j] if 0 <= j <= k else np.zeros((s, r))
    M = {}
    M[1] = np.vstack([beta.T @ Psif(0) + Phif(1) - np.eye(r), Psif(0)])
    for l in range(2, ps + 1):
        M[l] = np.vstack([beta.T @ Psif(l - 1) + Phif(l), Psif(l - 1)])
    A = sum(M.values())
    Gam = {i: -sum(M[l] for l in range(i + 1, ps + 1)) @ alpha.T for i in range(1, ps)}
    At = np.hstack([a + eta @ beta, eta])             # A_t = (a + beta' eta ; eta)
    Tt = [np.block([[Th[j], -Th[j] @ beta.T], [np.zeros((s, r)), np.zeros((s, s))]]) for j in range(q)]
    err = 0.0; scale = 0.0
    t0 = ps + q + 2
    for t in range(t0, T):
        rhs = A @ (alpha.T @ z[t - 1]) + sum(Gam[i] @ dz[t - i] for i in range(1, ps))
        rhs += At[t] - sum(Tt[j] @ At[t - 1 - j] for j in range(q))
        err = max(err, np.max(np.abs(dz[t] - rhs))); scale = max(scale, np.max(np.abs(dz[t])))
    rankG = [np.linalg.matrix_rank(Gam[i], tol=1e-10) for i in Gam]
    rowsp = [np.max(np.abs(Gam[i] @ np.vstack([beta.T, np.eye(s)]))) for i in Gam]  # Gamma_i alpha_perp
    # Step 6 claims
    phi1_claim = alpha.T @ A + np.eye(r)
    step6_phi1 = np.max(np.abs(phi1_claim - Phi[0]))
    step6_gamma = np.max(np.abs(A[r:, :] - Psi[0]))
    return err / scale, rankG, max(rowsp) if rowsp else 0.0, step6_phi1, step6_gamma

print("(m,r,p,k,q) | rel.err VEC vs sim | rank Gamma_i | max|Gamma_i alpha_perp| | Step6: |alpha'A+I-Phi1| , |A2-gamma|")
for cfg in [(2,1,1,0,1),(2,1,2,1,1),(3,1,2,1,2),(4,2,3,2,1),(5,3,2,3,3),(2,1,1,0,0),(3,2,1,0,1)]:
    res = run(*cfg)
    print(cfg, "| %.1e | %s | %.1e | %.3f , %.3f" % (res[0], res[1], res[2], res[3], res[4]))
print("\nStep 6 formulas hold only when p = 1 (Phi_1) and k = 0 (gamma); in general alpha'A + I = Phi(1)-type sum, A_2 = sum_j Psi_j.")
