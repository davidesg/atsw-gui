import numpy as np, sys
from scipy.optimize import minimize
sys.path.insert(0, '.')
from published import EML, CML
from check_published import Y, T, LC, cml
d0 = CML['T2']
def unpack(x):
    c = x[0:2]; P1 = x[2:6].reshape(2, 2); P2 = np.array([[x[6], x[7]], [x[8], 0.0]]); Th = np.array([[0, x[9]], [x[10], 0.0]])
    return c, P1, P2, Th
def resid(x, t0):
    c, P1, P2, Th = unpack(x)
    Z = Y - c; Zp = np.vstack([np.zeros((2, 2)), Z]); A = np.zeros((T+2, 2)); out = []
    for t in range(t0+2, T+2):
        a = Zp[t] - P1 @ Zp[t-1] - P2 @ Zp[t-2] + Th @ A[t-1]; A[t] = a; out.append(a)
    return np.array(out)
def nll(x, t0):
    if np.abs(np.linalg.eigvals(unpack(x)[3])).max() > 0.9999: return 1e6
    return -cml(resid(x, t0))[0]
x0 = np.r_[d0['c'], d0['Phi1'].ravel(), d0['Phi2'][0, 0], d0['Phi2'][0, 1], d0['Phi2'][1, 0], d0['Th'][0, 1], d0['Th'][1, 0]]
print("published CML point:", x0.round(4))
if __name__ == "__main__":
  for t0 in (0, 1, 2):
      best = None
      rng = np.random.default_rng(1)
      for k in range(30):
          xs = x0 if k == 0 else x0 + rng.normal(0, 0.3, x0.size) * np.r_[1, 1, np.ones(9)]
          r = minimize(nll, xs, args=(t0,), method='Nelder-Mead', options=dict(maxiter=20000, maxfev=20000, xatol=1e-8, fatol=1e-10))
          r = minimize(nll, r.x, args=(t0,), method='Nelder-Mead', options=dict(maxiter=20000, maxfev=20000, xatol=1e-8, fatol=1e-10))
          if best is None or r.fun < best.fun: best = r
      L, S, N = cml(resid(best.x, t0))
      print("t0=%d N=%d  max CML L=%.4f |S|=%.6f x=%s" % (t0, N, L, np.linalg.det(S), best.x.round(4)))
  