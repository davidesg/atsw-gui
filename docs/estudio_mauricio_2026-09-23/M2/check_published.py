import numpy as np, sys
sys.path.insert(0, '.')
from exactlik import *
from published import EML, CML
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
Y = raw[:, ::-1]                       # [mink ; muskrat], 1850..1911
T = len(Y)
LC = np.log(2*np.pi) + 1
print("== 1. information criteria: AIC=(-2L+2K)/N, BIC=(-2L+K ln N)/N with N=61")
for tag, tab in (('EML', EML), ('CML', CML)):
    for k, d in tab.items():
        N = 61
        print(" %s %s  AIC %.4f (pub %.4f)  BIC %.4f (pub %.4f)" % (tag, k, (-2*d['L']+2*d['K'])/N, d['IC'][0], (-2*d['L']+d['K']*np.log(N))/N, d['IC'][1]))
print("== 2. LR statistics from the tables")
print(" EML 2(L2-L1) = %.4f (pub 0.9718); B=[1,0]': 2(L4-L5) = %.4f (pub 5.4512)" % (2*(EML['T2']['L']-EML['T4']['L']), 2*(EML['T4']['L']-EML['T5']['L'])))
print(" CML 2(L2-L1) = %.4f (pub 3.7228); B=[1,0]': 2(L4-L5) = %.4f (pub 14.5320)" % (2*(CML['T2']['L']-CML['T4']['L']), 2*(CML['T4']['L']-CML['T5']['L'])))
print("== 3. eigenvalues")
for tag, tab in (('EML', EML), ('CML', CML)):
    for k, d in tab.items():
        ev = np.linalg.eigvals(d['Th'])
        s = "  Theta1 eig %s max|.|=%.4f" % (np.round(ev, 4), abs(ev).max())
        if k == 'T2':
            Pi = np.eye(2) - d['Phi1'] - d['Phi2']
            s += "  Pi eig %s" % np.round(np.sort(np.linalg.eigvals(Pi).real), 4)
            Tc = np.block([[d['Phi1'], d['Phi2']], [np.eye(2), np.zeros((2, 2))]])
            s += "  AR companion max|.|=%.4f" % abs(np.linalg.eigvals(Tc)).max()
        else:
            Pi = d['Lam'] @ np.array([[1, d['B2'][0, 0]]])
            s += "  Pi eig %s" % np.round(np.sort(np.linalg.eigvals(Pi).real), 4)
        print(" %s %s %s |Sigma|=%.6f" % (tag, k, s, np.linalg.det(d['Sig'])))
print("== 4. CML/concentrated identity  L = -(NM/2)(1+log2pi) - (N/2)log|Sigma_pub|, N=61")
for tag, tab in (('EML', EML), ('CML', CML)):
    for k, d in tab.items():
        Lid = -61*LC - 30.5*np.log(np.linalg.det(d['Sig']))
        print(" %s %s identity %.4f  published %.4f  diff %.4f" % (tag, k, Lid, d['L'], d['L']-Lid))

# ---------------- conditional residuals, several start-up conventions
def vec_resid(d, t0, zero_pre=True):
    """VEC (25) recursion on levels: A_t = dY_t - F dY_{t-1} + Lam(B'Y_{t-1}-EW) + Th A_{t-1}."""
    B = np.array([1.0, d['B2'][0, 0]])
    dY = np.vstack([np.zeros(2), np.diff(Y, axis=0)])   # dY[0] := 0 (presample)
    A = np.zeros((T, 2)); out = []
    for t in range(t0, T):
        a = dY[t] - d['F'] @ dY[t-1] + d['Lam'][:, 0] * (B @ Y[t-1] - d['EW']) + d['Th'] @ A[t-1]
        A[t] = a; out.append(a)
    return np.array(out)
def var_resid(d, t0):
    """model (22) on levels: A_t = (Y_t-c) - Phi1 (Y_{t-1}-c) - Phi2 (Y_{t-2}-c) + Th A_{t-1}; presample Y := c."""
    Z = Y - d['c']
    Zp = np.vstack([np.zeros((2, 2)), Z])       # two presample zeros
    A = np.zeros((T+2, 2)); out = []
    for t in range(t0+2, T+2):
        a = Zp[t] - d['Phi1'] @ Zp[t-1] - d['Phi2'] @ Zp[t-2] + d['Th'] @ A[t-1]
        A[t] = a; out.append(a)
    return np.array(out)
def cml(Ares):
    N = len(Ares); S = Ares.T @ Ares / N
    return -N*LC - 0.5*N*np.log(np.linalg.det(S)), S, N
print("== 5. conditional log-likelihood at the published CML points (and at the EML points)")
for tag, tab in (('CML', CML), ('EML', EML)):
    for k, d in tab.items():
        res = []
        if k == 'T2':
            for t0, lab in ((0, 'Y1849,Y1848:=c, t=1850..1911 (62)'), (1, 'Y1849:=c, t=1851.. (61)'), (2, 't=1852.. (60)')):
                L, S, N = cml(var_resid(d, t0)); res.append((lab, L, S, N))
        else:
            for t0, lab in ((1, 'dY1850:=0, t=1851..1911 (61)'), (2, 't=1852..1911 (60)')):
                L, S, N = cml(vec_resid(d, t0)); res.append((lab, L, S, N))
        for lab, L, S, N in res:
            print(" %s %s [%s] L=%.4f (pub %.4f) Sigma_hat=%s |S|=%.6f" % (tag, k, lab, L, d['L'], np.round(S, 4).tolist(), np.linalg.det(S)))
