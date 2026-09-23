import numpy as np, sys
from scipy.optimize import minimize
sys.path.insert(0,'.')
import cml_opt as C
for t0 in (0,1,2):
    r = minimize(C.nll, C.x0, args=(t0,), method='Nelder-Mead', options=dict(maxiter=40000, maxfev=40000, xatol=1e-9, fatol=1e-11))
    r = minimize(C.nll, r.x, args=(t0,), method='Nelder-Mead', options=dict(maxiter=40000, maxfev=40000, xatol=1e-9, fatol=1e-11))
    print("t0=%d local CML from published point: L=%.4f  start L=%.4f  max|dx|=%.3f  x=%s" % (t0, -r.fun, -C.nll(C.x0,t0), np.abs(r.x-C.x0).max(), r.x.round(3)))
