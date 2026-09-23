"""Hypothesis: the published EML log-likelihood equals the exact concentrated form evaluated with the
PUBLISHED Sigma, i.e.  L_pub = -(nm/2)(log2pi+1) - (n/2)log|Sigma_pub| - (1/2)log|Omega|,
while the true ML scale at the published point is c*Sigma_pub with c > 1.
Then  L_pub - L_true = n*log(c) (m = 2).   Test on Tables 2, 4, 5 (EML)."""
import numpy as np, sys
sys.path.insert(0, '.')
from exactlik import *
from published import EML
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
Y = raw[:, ::-1]
def point(tab):
    d = EML[tab]
    if tab == 'T2':
        return None, d
    phis, thetas, Cb = mauricio_varma(d['Lam'], d['B2'], [d['F']], [d['Th']], 2, 1)
    Yb = ybar_from_levels(Y, d['B2'], 1)
    return (Yb, np.array([0, d['EW']]), phis, thetas, Cb @ d['Sig'] @ Cb.T), d
for tab in ('T2', 'T4', 'T5'):
    spec, d = point(tab)
    cases = []
    if tab == 'T2':
        for lab, Yd in (('n=62 (1850-1911)', Y), ('n=61 (1851-1911)', Y[1:])):
            cases.append((lab, (Yd, d['c'], [d['Phi1'], d['Phi2']], [d['Th']], d['Sig'])))
    else:
        cases.append(('n=61 (Ybar 1851-1911)', spec))
    for lab, (Yd, mu, phis, thetas, Sg) in cases:
        n = len(Yd)
        Lfix = loglik(Yd, mu, phis, thetas, Sg)
        Lc, s2 = loglik_conc(Yd, mu, phis, thetas, Sg)       # scale concentrated: Sigma = s2 * Sg
        pred = Lc + n * np.log(s2)
        print("%s %-22s L(pub Sigma)=%.4f  L(conc)=%.4f  c=%.4f  L(conc)+n*log(c)=%.4f   published %.4f   diff %.4f" %
              (tab, lab, Lfix, Lc, s2, pred, d['L'], pred - d['L']))
