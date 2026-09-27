"""Mechanism test: is the published EML Sigma the in-sample cross-product of the exact residuals
a_hat_t = E[a_t | data] (t = 1..n), i.e. a Sigma that leaves out the presample part of the quadratic form?
If so, Sigma_pub is (approximately) a fixed point of  Sigma -> (1/n) sum_t a_hat_t(Sigma) a_hat_t(Sigma)'."""
import numpy as np, sys
sys.path.insert(0, '.')
from exactlik import *
from published import EML
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
Y = raw[:, ::-1]
def psi(phis, thetas, m, L):
    P = [np.eye(m)]
    for l in range(1, L):
        acc = np.zeros((m, m))
        for i, Ph in enumerate(phis, 1):
            if l - i >= 0: acc += Ph @ P[l - i]
        if l <= len(thetas): acc -= thetas[l - 1]
        P.append(acc)
    return P
def resid_split(Yd, mu, phis, thetas, Sg):
    n, m = Yd.shape
    y = (Yd - mu).reshape(-1)
    V = full_cov(phis, thetas, Sg, n)
    z = np.linalg.solve(V, y).reshape(n, m)
    Ps = psi(phis, thetas, m, n)
    ah = np.array([Sg @ sum(Ps[s - t].T @ z[s] for s in range(t, n)) for t in range(n)])
    S = y @ z.reshape(-1)
    Sin = np.einsum('ti,ij,tj->', ah, np.linalg.inv(Sg), ah)
    return ah, S, Sin
for tab in ('T2', 'T4', 'T5'):
    d = EML[tab]
    if tab == 'T2':
        Yd, mu, phis, thetas = Y[1:], d['c'], [d['Phi1'], d['Phi2']], [d['Th']]; Sg = d['Sig']; C = np.eye(2)
    else:
        phis, thetas, C = mauricio_varma(d['Lam'], d['B2'], [d['F']], [d['Th']], 2, 1)
        Yd = ybar_from_levels(Y, d['B2'], 1); mu = np.array([0, d['EW']]); Sg = C @ d['Sig'] @ C.T
    ah, S, Sin = resid_split(Yd, mu, phis, thetas, Sg)
    n = len(Yd); Ci = np.linalg.inv(C)
    Sres = Ci @ (ah.T @ ah / n) @ Ci.T               # back to Y coordinates
    print("%s: quadratic form S/(nm) = %.4f ; in-sample part sum a'S^-1 a/(nm) = %.4f ; presample share %.1f%%" % (tab, S/(2*n), Sin/(2*n), 100*(1-Sin/S)))
    print("     (1/n) sum a_hat a_hat' = %s   published Sigma = %s" % (Sres.round(4).tolist(), d['Sig'].tolist()))
    print("     |.| = %.6f vs %.6f" % (np.linalg.det(Sres), np.linalg.det(d['Sig'])))
