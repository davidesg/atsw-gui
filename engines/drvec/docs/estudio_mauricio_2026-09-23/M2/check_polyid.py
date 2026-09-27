"""General stationarity proof support: the operator identity  Phi(x) = PhiBar(x) D(x) Cbar,
Phi(x) = F(x)(1-x) + Lam B' x  (the VEC's AR operator on Y, paper ordering),
PhiBar(x) = PhiBar_0 - sum PhiBar_i x^i (eq. 16),  D(x) = diag((1-x) I_s, I_r).
Hence |Phi(x)| = +-(1-x)^s |PhiBar(x)|, so |PhiBar(x)| (= +-|Phi*(x)|) has no unit root iff x=1 has
multiplicity exactly s in |Phi(x)|, and all its other roots are those of |Phi(x)|."""
import numpy as np, sys
sys.path.insert(0, '.')
rng = np.random.default_rng(3)
for (M, r, p) in [(2, 1, 1), (2, 1, 2), (3, 1, 3), (3, 2, 4), (4, 2, 2)]:
    s = M - r
    Lam = rng.normal(size=(M, r)); B2 = rng.normal(size=(s, r)); B = np.vstack([np.eye(r), B2])
    F = [rng.normal(size=(M, M)) for _ in range(p - 1)]
    Cb = np.zeros((M, M)); Cb[:s, r:] = np.eye(s); Cb[s:, :r] = np.eye(r); Cb[s:, r:] = B2.T
    Ci = np.linalg.inv(Cb); Hb = np.zeros((M, M)); Hb[s:, s:] = np.eye(r); Lb = np.zeros((M, M)); Lb[:, s:] = Lam
    PhB = [Ci]
    PhB.append(Ci @ Hb - Lb + (F[0] @ Ci if p >= 2 else 0))
    for i in range(2, p): PhB.append(F[i-1] @ Ci - F[i-2] @ Ci @ Hb)
    if p >= 2: PhB.append(-F[p-2] @ Ci @ Hb)
    err = 0
    for x in rng.normal(size=5) + 1j * rng.normal(size=5):
        Fx = np.eye(M) - sum(F[i] * x**(i+1) for i in range(p-1))
        Phi = Fx * (1 - x) + Lam @ B.T * x
        PhBx = PhB[0] - sum(PhB[i] * x**i for i in range(1, p+1))
        Dx = np.diag([1 - x] * s + [1] * r)
        err = max(err, np.abs(Phi - PhBx @ Dx @ Cb).max())
    print("M=%d r=%d p=%d  max|Phi(x) - PhiBar(x) D(x) Cbar| = %.1e" % (M, r, p, err))
