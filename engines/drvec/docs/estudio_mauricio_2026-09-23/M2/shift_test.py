"""Could the published columns correspond to a differently ALIGNED sample? Evaluate the conditional
log-likelihood of model (22) at the published CML Table-2 point under data variants."""
import numpy as np, sys
sys.path.insert(0, '.')
from published import EML, CML
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
mink, musk = raw[:, 1], raw[:, 0]
LC = np.log(2*np.pi) + 1
def cml22(Y, d, t0):
    T = len(Y); Z = Y - d['c']; Zp = np.vstack([np.zeros((2, 2)), Z]); A = np.zeros((T+2, 2)); out = []
    for t in range(t0+2, T+2):
        a = Zp[t] - d['Phi1'] @ Zp[t-1] - d['Phi2'] @ Zp[t-2] + d['Th'] @ A[t-1]; A[t] = a; out.append(a)
    Ar = np.array(out); N = len(Ar); S = Ar.T @ Ar / N
    return -N*LC - 0.5*N*np.log(np.linalg.det(S)), N, np.linalg.det(S)
variants = {
  'as is 1850-1911': np.c_[mink, musk],
  'muskrat shifted +1 (mink_t, musk_t+1)': np.c_[mink[:-1], musk[1:]],
  'muskrat shifted -1 (mink_t+1, musk_t)': np.c_[mink[1:], musk[:-1]],
  'drop 1850 (1851-1911)': np.c_[mink[1:], musk[1:]],
  'drop 1911 (1850-1910)': np.c_[mink[:-1], musk[:-1]],
}
for k, Y in variants.items():
    res = [cml22(Y, CML['T2'], t0) for t0 in (0, 1, 2)]
    print("%-40s " % k + "  ".join("t0=%d L=%.3f (N=%d |S|=%.6f)" % (t0, *res[t0]) for t0 in range(3)))
print("published CML Table 2: L = 12.0470, |Sigma| = %.6f" % np.linalg.det(CML['T2']['Sig']))
