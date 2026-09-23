"""Numeric verification: paper (independent numpy) vs drvec's vec_shootx + elf.

For each configuration: draw VEC parameters (Lam, B2, F_i, Theta_j, Sigma with
Sigma[0,0]=1, mean m), simulate levels, write the .inp-ordered data [Y2 | Y1],
pack x[] in drvec's documented layout, run the harness (drvec.c unchanged,
main renamed), and compare
  * Phi*_k, Theta*_k, Sigma*, mu and the data Ybar  (paper eqs 11-18, 21)
  * the exact log-likelihood of Ybar computed here from the block-Toeplitz
    covariance (no recursion, no engine) against elf's value.
Also: algebraic identities of the paper (Phi(z) = PhiBar(z) G(z) etc.).
"""
import subprocess, os, sys
import numpy as np
from paper import (transform, Phi_poly, G_poly, polymul, exact_loglik,
                   simulate_vec, det_poly_roots, blocks)

HERE = os.path.dirname(os.path.abspath(__file__))
HARN = os.path.join(HERE, 'drvec_copy', 'harness')


def companion_maxmod(PhS, M):
    p = len(PhS) - 1
    C = np.zeros((M * p, M * p))
    for i in range(p):
        C[:M, i*M:(i+1)*M] = PhS[i + 1]
    if p > 1:
        C[M:, :-M] = np.eye(M * (p - 1))
    return max(abs(np.linalg.eigvals(C)))


def ma_maxmod(ThS, M):
    q = len(ThS) - 1
    if q == 0:
        return 0.0
    C = np.zeros((M * q, M * q))
    for i in range(q):
        C[:M, i*M:(i+1)*M] = ThS[i + 1]
    if q > 1:
        C[M:, :-M] = np.eye(M * (q - 1))
    return max(abs(np.linalg.eigvals(C)))


def draw(M, r, p, q, rng, marow=False):
    s = M - r
    while True:
        B2 = rng.uniform(-1, 1, (s, r))
        Lam = rng.uniform(-0.6, 0.6, (M, r))
        F = [rng.uniform(-0.3, 0.3, (M, M)) for _ in range(p - 1)]
        Th = [rng.uniform(-0.3, 0.3, (M, M)) for _ in range(q)]
        if marow:
            for T in Th:
                T[r:, :] = 0.0          # lower s rows (the Y2 equations) zero
        L = rng.uniform(-0.5, 0.5, (M, M)); Sig = L @ L.T + np.eye(M) * 0.5
        Sig = Sig / Sig[0, 0]
        tr = transform(B2, Lam, F, Th, Sig, p)
        if companion_maxmod(tr['PhS'], M) < 0.95 and ma_maxmod(tr['ThS'], M) < 0.9:
            return B2, Lam, F, Th, Sig, tr


def pack_x(case, m, Lam, F, Th, Sig, B2, r, marow=False):
    M = Lam.shape[0]; s = M - r
    x = []
    if case == 2:
        x += list(m[s:])
    elif case == 3:
        x += list(m)
    x += list(Lam.reshape(-1))                       # i outer, j inner
    for Fi in F:
        x += list(Fi.reshape(-1))
    for T in Th:
        x += list((T[:r, :] if marow else T).reshape(-1))
    x += [Sig[i, i] for i in range(1, M)]
    x += [Sig[i, j] for i in range(1, M) for j in range(i)]
    x += list(B2.T.reshape(-1))                      # j outer, i inner
    return np.array(x)


def run(tag, M, r, p, q, case, seed, marow=False, n=60, levels=1):
    rng = np.random.default_rng(seed)
    s = M - r
    B2, Lam, F, Th, Sig, tr = draw(M, r, p, q, rng, marow)
    m = np.zeros(M)
    if case == 2:
        m[s:] = rng.uniform(-2, 2, r)
    if case == 3:
        m = rng.uniform(-2, 2, M)
    # simulate with the VEC intercept that corresponds to E[Ybar] = m:
    #   mu_vec = PhiBar(1) m  (proved in STUDY_M1 sec. 5)
    PhB1 = tr['PhB'][0] - sum(tr['PhB'][1:])
    mu_vec = PhB1 @ m
    Y = simulate_vec(B2, Lam, F, Th, Sig * 0.01, n, mu_vec=mu_vec, rng=seed)
    Y = Y + 5.0        # an arbitrary level: matters for case 1 only through W
    # .inp order: [Y2 | Y1]
    raw = np.hstack([Y[:, r:], Y[:, :r]])
    if not levels:
        raw = raw.copy()
        raw[1:, :s] = np.diff(raw[:, :s], axis=0)
        raw = raw[1:]                                  # user supplies n-1 rows
    dfile = os.path.join(HERE, f'data_{tag}.txt'); xfile = os.path.join(HERE, f'x_{tag}.txt')
    with open(dfile, 'w') as f:
        f.write(f'{raw.shape[0]} {M}\n')
        for row in raw:
            f.write(' '.join(f'{v:.17g}' for v in row) + '\n')
    x = pack_x(case, m, Lam, F, Th, Sig, B2, r, marow)
    np.savetxt(xfile, x, fmt='%.17g')
    out = subprocess.run([HARN, dfile, xfile, str(p), str(q), str(r), str(case),
                          str(int(marow)), str(levels)],
                         capture_output=True, text=True).stdout
    phi = {}; th = {}; qq = np.zeros((M, M)); mu = np.zeros(M); w = {}; elfres = []
    for line in out.splitlines():
        t = line.split()
        if t[0] == 'phi': phi[(int(t[1]), int(t[2]) - 1, int(t[3]) - 1)] = float(t[4])
        elif t[0] == 'theta': th[(int(t[1]), int(t[2]) - 1, int(t[3]) - 1)] = float(t[4])
        elif t[0] == 'qq': qq[int(t[1]) - 1, int(t[2]) - 1] = float(t[3])
        elif t[0] == 'mu': mu[int(t[1]) - 1] = float(t[2])
        elif t[0] == 'w': w[(int(t[1]), int(t[2]) - 1)] = float(t[3])
        elif t[0] == 'elf': elfres.append((float(t[2]), int(t[4]), float(t[6]), float(t[8])))
        elif t[0] in ('npar', 'ifault', 'granger_sv'): print('   ', line)
    nob = max(k[0] for k in w)
    W = np.array([[w[(t, i)] for i in range(M)] for t in range(1, nob + 1)])
    # paper's Ybar, from the levels (t = 2..n)
    Yb = np.hstack([np.diff(Y[:, r:], axis=0), Y[1:, :r] + Y[1:, r:] @ B2])
    d_phi = max(abs(phi[(k, i, j)] - tr['PhS'][k][i, j]) for k in range(1, p + 1)
                for i in range(M) for j in range(M))
    d_th = max([abs(th[(k, i, j)] - tr['ThS'][k][i, j]) for k in range(1, q + 1)
                for i in range(M) for j in range(M)] or [0.0])
    d_qq = abs(qq - tr['SgS']).max()
    d_mu = abs(mu - m).max()
    if levels:
        d_w = abs(W - Yb).max()
    else:
        # legacy layout: W shifted by the constant -B2' Y2_(first row level)
        shift = W - Yb
        d_w = abs(shift - shift[0]).max()
        print(f'    legacy layout: W - Wtrue = constant {shift[0]} (max dev {d_w:.2e})')
    full, conc, s2 = exact_loglik(W, mu, tr['PhS'], tr['ThS'], tr['SgS'])
    print(f'[{tag}] M={M} r={r} p={p} q={q} case={case} marow={marow} levels={levels} n_Ybar={nob}')
    print(f'    max|Phi*-paper| {d_phi:.2e}  max|Theta*-paper| {d_th:.2e}  '
          f'max|Sigma*-paper| {d_qq:.2e}  max|mu-m| {d_mu:.2e}  max|Ybar-paper| {d_w:.2e}')
    for tol, ife, le, cc in elfres:
        print(f'    elf xitol={tol:+.0e}: ifault={ife}  logL(Sigma*=qq) {le:.10f} vs direct {full:.10f} '
              f'(diff {le-full:+.2e});  concentrated {cc:.10f} vs direct {conc:.10f} (diff {cc-conc:+.2e})')
    return dict(d_phi=d_phi, d_th=d_th, d_qq=d_qq, d_mu=d_mu, d_w=d_w,
                ll=[(e[2] - full, e[3] - conc) for e in elfres])


def identities(M, r, p, q, seed):
    rng = np.random.default_rng(seed)
    B2, Lam, F, Th, Sig, tr = draw(M, r, p, q, rng)
    s = M - r
    Cbar, Cinv, Hbar, LamBar = blocks(B2, Lam)
    e1 = abs(Cbar @ Cinv - np.eye(M)).max()
    # PhiBar(z) := PhB0 - sum PhB_i z^i ;  check PhiBar(z) G(z) == Phi(z)
    PhBz = [tr['PhB'][0]] + [-P for P in tr['PhB'][1:]]
    lhs = polymul(PhBz, G_poly(B2))
    rhs = Phi_poly(B2, Lam, F, p)
    while len(rhs) < len(lhs): rhs.append(np.zeros((M, M)))
    e2 = max(abs(a - b).max() for a, b in zip(lhs, rhs))
    # PhiBar(1) = [F(1) Bperp , Lam]
    F1 = np.eye(M) - sum(F) if F else np.eye(M)
    Bperp = np.vstack([-B2.T, np.eye(s)])
    e3 = abs((tr['PhB'][0] - sum(tr['PhB'][1:])) - np.hstack([F1 @ Bperp, Lam])).max()
    # det Phi(z) = (-1)^{rs} (1-z)^s det PhiBar(z) at random z
    e4 = 0
    for z in rng.standard_normal(5) + 1j * rng.standard_normal(5):
        dP = np.linalg.det(sum(c * z ** k for k, c in enumerate(rhs)))
        dB = np.linalg.det(sum(c * z ** k for k, c in enumerate(PhBz)))
        e4 = max(e4, abs(dP - (-1) ** (r * s) * (1 - z) ** s * dB) / max(1, abs(dP)))
    # det Theta*(z) = det Theta(z)
    e5 = 0
    for z in rng.standard_normal(3):
        a = np.linalg.det(np.eye(M) - sum(T * z ** (k + 1) for k, T in enumerate(Th)))
        b = np.linalg.det(np.eye(M) - sum(tr['ThS'][k] * z ** k for k in range(1, q + 1)))
        e5 = max(e5, abs(a - b))
    # unit roots of det Phi(z): exactly s at z = 1
    roots = det_poly_roots(rhs)
    n_unit = int(np.sum(abs(roots - 1) < 1e-4))
    # Jacobian of Y_2..Y_n -> Ybar_2..Ybar_n: det Cbar = +-1
    e6 = abs(abs(np.linalg.det(Cbar)) - 1)
    print(f'[identities M={M} r={r} p={p} q={q}] |Cbar Cinv - I| {e1:.1e}; '
          f'|PhiBar(z)G(z)-Phi(z)| {e2:.1e}; |PhiBar(1)-[F(1)Bperp,Lam]| {e3:.1e}; '
          f'det relation {e4:.1e}; |detTheta*-detTheta| {e5:.1e}; '
          f'unit roots of |Phi| = {n_unit} (s={s}); ||detCbar|-1| {e6:.1e}')


if __name__ == '__main__':
    for cfg in [(3, 1, 3, 2, 11), (3, 2, 2, 1, 12), (2, 1, 1, 1, 13), (4, 2, 3, 1, 14), (2, 1, 2, 0, 15)]:
        identities(*cfg)
    res = []
    res.append(run('A', 3, 1, 3, 2, 3, 101))
    res.append(run('B', 3, 2, 2, 1, 2, 102))
    res.append(run('C', 2, 1, 1, 1, 1, 103))
    res.append(run('D', 3, 1, 2, 1, 3, 104, marow=True))
    res.append(run('E', 4, 2, 3, 1, 3, 105, n=50))
    res.append(run('F', 2, 1, 2, 0, 2, 106))
    res.append(run('G', 3, 1, 2, 1, 2, 107, levels=0))
