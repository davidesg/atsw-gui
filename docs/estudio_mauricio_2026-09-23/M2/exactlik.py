"""Independent exact Gaussian likelihood for a stationary VARMA(p,q).

Written for the M2 study, with NO code shared with drvec / AS 311:
  * autocovariances from the companion state-space form + discrete Lyapunov;
  * the full nm x nm block-Toeplitz covariance, Cholesky, Gaussian density;
  * a second, independent route: a Kalman filter on the same state space.

Convention (same as the paper): y_t - mu = sum_i Phi_i (y_{t-i}-mu) + a_t - sum_j Theta_j a_{t-j},
a_t ~ N(0, Sigma).
"""
import numpy as np
from scipy.linalg import solve_discrete_lyapunov, cho_factor, cho_solve

LOG2PI = np.log(2 * np.pi)


def state_space(phis, thetas, m):
    p = max(len(phis), 1)
    q = max(len(thetas), 1)
    phis = list(phis) + [np.zeros((m, m))] * (p - len(phis))
    thetas = list(thetas) + [np.zeros((m, m))] * (q - len(thetas))
    k = m * (p + q)
    T = np.zeros((k, k))
    R = np.zeros((k, m))
    for i in range(p):
        T[0:m, i * m:(i + 1) * m] = phis[i]
    for j in range(q):
        T[0:m, (p + j) * m:(p + j + 1) * m] = -thetas[j]
    for i in range(1, p):
        T[i * m:(i + 1) * m, (i - 1) * m:i * m] = np.eye(m)
    for j in range(1, q):
        T[(p + j) * m:(p + j + 1) * m, (p + j - 1) * m:(p + j) * m] = np.eye(m)
    R[0:m, :] = np.eye(m)
    R[p * m:(p + 1) * m, :] = np.eye(m)
    Z = np.zeros((m, k)); Z[:, :m] = np.eye(m)
    return T, R, Z


def acov(phis, thetas, Sigma, nlag):
    m = Sigma.shape[0]
    T, R, Z = state_space(phis, thetas, m)
    if np.max(np.abs(np.linalg.eigvals(T))) >= 1:
        raise ValueError("nonstationary AR")
    P = solve_discrete_lyapunov(T, R @ Sigma @ R.T)
    G = []
    Th = np.eye(T.shape[0])
    for h in range(nlag):
        G.append(Z @ Th @ P @ Z.T)
        Th = T @ Th
    return G


def full_cov(phis, thetas, Sigma, n):
    m = Sigma.shape[0]
    G = acov(phis, thetas, Sigma, n)
    V = np.zeros((n * m, n * m))
    for i in range(n):
        for j in range(i + 1):
            V[i * m:(i + 1) * m, j * m:(j + 1) * m] = G[i - j]
            if i != j:
                V[j * m:(j + 1) * m, i * m:(i + 1) * m] = G[i - j].T
    return V


def loglik(Y, mu, phis, thetas, Sigma):
    """Exact Gaussian log-likelihood at the given Sigma (no concentration)."""
    n, m = Y.shape
    y = (Y - mu).reshape(-1)
    V = full_cov(phis, thetas, Sigma, n)
    c = cho_factor(V, lower=True)
    logdet = 2 * np.sum(np.log(np.diag(c[0])))
    S = y @ cho_solve(c, y)
    return -0.5 * (n * m * LOG2PI + logdet + S)


def loglik_conc(Y, mu, phis, thetas, Q):
    """Scale-concentrated: Sigma = c Q with c at its ML value (drvec's convention)."""
    n, m = Y.shape
    y = (Y - mu).reshape(-1)
    V = full_cov(phis, thetas, Q, n)
    c = cho_factor(V, lower=True)
    logdet = 2 * np.sum(np.log(np.diag(c[0])))
    S = y @ cho_solve(c, y)
    s2 = S / (n * m)
    return -0.5 * (n * m * (LOG2PI + 1 + np.log(s2)) + logdet), s2


def loglik_kalman(Y, mu, phis, thetas, Sigma):
    """Same likelihood through a Kalman filter (prediction-error decomposition)."""
    n, m = Y.shape
    T, R, Z = state_space(phis, thetas, m)
    P = solve_discrete_lyapunov(T, R @ Sigma @ R.T)
    x = np.zeros(T.shape[0])
    ll = 0.0
    RQR = R @ Sigma @ R.T
    for t in range(n):
        e = (Y[t] - mu) - Z @ x
        F = Z @ P @ Z.T
        Fi = np.linalg.inv(F)
        ll += -0.5 * (m * LOG2PI + np.linalg.slogdet(F)[1] + e @ Fi @ e)
        K = P @ Z.T @ Fi
        x = x + K @ e
        P = P - K @ Z @ P
        x = T @ x
        P = T @ P @ T.T + RQR
    return ll


# ---------------------------------------------------------------- Mauricio's map
def mauricio_varma(Lam, B2, F, Theta, M, r):
    """Paper eqs (10),(11),(13),(14),(16),(18). Paper ordering Y=[Y1 (r); Y2 (s)].
    Returns Phi*_1..p, Theta*_1..q, and Cbar (so Sigma* = Cbar Sigma Cbar')."""
    s = M - r
    Cbar = np.zeros((M, M)); Cbar[:s, r:] = np.eye(s); Cbar[s:, :r] = np.eye(r); Cbar[s:, r:] = B2.T
    Cinv = np.zeros((M, M)); Cinv[:r, :s] = -B2.T; Cinv[:r, s:] = np.eye(r); Cinv[r:, :s] = np.eye(s)
    assert np.allclose(Cbar @ Cinv, np.eye(M))
    Hbar = np.zeros((M, M)); Hbar[s:, s:] = np.eye(r)
    LamBar = np.zeros((M, M)); LamBar[:, s:] = Lam
    p = len(F) + 1
    PhB = [None] * (p + 1)
    PhB[1] = Cinv @ Hbar - LamBar + (F[0] @ Cinv if p >= 2 else 0)
    for i in range(2, p):
        PhB[i] = F[i - 1] @ Cinv - F[i - 2] @ Cinv @ Hbar
    if p >= 2:
        PhB[p] = -F[p - 2] @ Cinv @ Hbar
    phis = [Cbar @ PhB[i] for i in range(1, p + 1)]
    thetas = [Cbar @ Th @ Cinv for Th in Theta]
    return phis, thetas, Cbar


def ybar_from_levels(Ylev, B2, r):
    """Ylev in paper order [Y1 (r cols), Y2 (s cols)], T rows. Returns T-1 rows of
    Ybar_t = [nabla Y2_t ; Y1_t + B2' Y2_t] for t = 2..T (eq. 17)."""
    Y1 = Ylev[:, :r]; Y2 = Ylev[:, r:]
    d2 = np.diff(Y2, axis=0)
    W = Y1[1:] + Y2[1:] @ B2
    return np.hstack([d2, W])
