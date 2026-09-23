import numpy as np, sys, itertools
sys.path.insert(0, '.')
from published import EML, CML
from check_published import Y, T, LC, cml
def var_resid(d, t0, P1, P2, Th):
    Z = Y - d['c']; Zp = np.vstack([np.zeros((2, 2)), Z]); A = np.zeros((T+2, 2)); out = []
    for t in range(t0+2, T+2):
        a = Zp[t] - P1 @ Zp[t-1] - P2 @ Zp[t-2] + Th @ A[t-1]; A[t] = a; out.append(a)
    return np.array(out)
d = CML['T2']
rows = []
for tr1, tr2, trt, sg in itertools.product((0, 1), (0, 1), (0, 1), (1, -1)):
    P1 = d['Phi1'].T if tr1 else d['Phi1']; P2 = d['Phi2'].T if tr2 else d['Phi2']; Th = sg * (d['Th'].T if trt else d['Th'])
    for t0 in (0, 1, 2):
        L, S, N = cml(var_resid(d, t0, P1, P2, Th))
        rows.append((L, tr1, tr2, trt, sg, t0, N, np.linalg.det(S)))
rows.sort(reverse=True)
for r in rows[:8]: print("L=%.4f  Phi1^T=%d Phi2^T=%d Th^T=%d sign=%+d t0=%d N=%d |S|=%.6f" % r)
