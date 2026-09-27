"""s5_factorisation.py -- Theorem 9, with an exact Gaussian likelihood computed
independently of AS 311 (autocovariances -> Toeplitz -> Cholesky).

Reads run/fac.inp (two independent ARMA(1,1) in differences, simulated) and the
drvec estimates printed in run/fac.out (r = 0, -diagar -diagma -diagcov).

drvec conventions: dY_t = F1 dY_{t-1} + A_t - Th1 A_{t-1}; Sigma = sigma2*Q; the
engine concentrates ONE common scale sigma2 (Q[1,1] = 1).

Checks
 (1) exact joint logL (my code) at drvec's (F, Th, Sigma)  vs  drvec's logelf
 (2) sum of my univariate exact logLs at the same point (= (1) exactly: independence)
 (3) the gate's 'univariate' side: each series CONCENTRATES ITS OWN sigma_i^2
     (gate_contract sets qq=1 and concentrates), whereas the joint fit has ONE
     sigma2 and a free ratio Q22.  The two agree only where Q22 is at its
     optimum -- to first order that is where the joint gradient is zero, and
     the residual gap is second order in the optimiser's tolerance.
     We compute the size of that effect at drvec's point.
"""
import numpy as np, re
from scipy.linalg import toeplitz, cho_factor, cho_solve

def acov_arma11(phi, th, s2, n):
    # x_t = phi x_{t-1} + a_t - th a_{t-1}
    g0 = s2 * (1 + th**2 - 2*phi*th) / (1 - phi**2)
    g1 = s2 * (1 - phi*th) * (phi - th) / (1 - phi**2)
    g = np.empty(n); g[0] = g0
    if n > 1: g[1] = g1
    for k in range(2, n): g[k] = phi * g[k-1]
    return g

def exact_ll(x, phi, th, s2):
    n = len(x); V = toeplitz(acov_arma11(phi, th, s2, n))
    c = cho_factor(V, lower=True)
    logdet = 2*np.log(np.diag(c[0])).sum()
    return -0.5*(n*np.log(2*np.pi) + logdet + x @ cho_solve(c, x))

def conc_ll(x, phi, th):
    # concentrate sigma^2 analytically: ll(s2) max at s2 = x'V1^{-1}x / n
    n = len(x); V1 = toeplitz(acov_arma11(phi, th, 1.0, n))
    c = cho_factor(V1, lower=True)
    q = x @ cho_solve(c, x); logdet = 2*np.log(np.diag(c[0])).sum()
    s2 = q / n
    return -0.5*(n*np.log(2*np.pi) + n*np.log(s2) + logdet + n), s2

lines = [l for l in open('run/fac.inp') if not l.startswith('*')]
Y = np.array([[float(v) for v in l.split()] for l in lines[4:]])
dY = np.diff(Y, axis=0)
out = open('run/fac.out').read()
num = lambda pat: float(re.search(pat, out).group(1))
F = [num(r"D\.S1 <- D\.S1\(-1\)\s+(\S+)"), num(r"D\.S2 <- D\.S2\(-1\)\s+(\S+)")]
T = [num(r"D\.S1 <- A\.S1\(-1\)\s+(\S+)"), num(r"D\.S2 <- A\.S2\(-1\)\s+(\S+)")]
sig2 = num(r"sigma2\s+:\s+(\S+)"); Q22 = num(r"var S2\s+(\S+)")
logelf = num(r"logelf\s+:\s+(\S+)")
S = [sig2, sig2*Q22]
ll_i = [exact_ll(dY[:, i], F[i], T[i], S[i]) for i in range(2)]
print("drvec estimates: F =", F, " Theta =", T, " Sigma_ii =", S)
print("(1)/(2) my exact logL:  series1 = %.10f  series2 = %.10f" % tuple(ll_i))
print("        sum (= joint, by independence) = %.10f" % sum(ll_i))
print("        drvec joint logelf               = %.10f" % logelf)
print("        difference drvec - mine          = %.3e" % (logelf - sum(ll_i)))
cc = [conc_ll(dY[:, i], F[i], T[i]) for i in range(2)]
print("(3) each series concentrating its OWN sigma^2: sum = %.10f" % (cc[0][0] + cc[1][0]))
print("    own-concentrated s2 ratio = %.6f   vs  drvec's Q22 = %.6f" % (cc[1][1]/cc[0][1], Q22))
print("    gap (joint common-scale) - (sum own-concentrated) = %.3e" % (sum(ll_i) - cc[0][0] - cc[1][0]))
print("    drvec's printed 'univariate' logLs: -410.4199878434, -628.2181419676 (sum -1038.6381298111)")
print("    mine own-concentrated              : %.10f, %.10f" % (cc[0][0], cc[1][0]))
