#!/usr/bin/env python3
"""Identity check: evaluate an INDEPENDENT exact Gaussian likelihood (Kalman, exact_ml.py)
at the point drvec prints, and compare with drvec's logelf.  Gamma(1) and Sigma are
read twice: with the labels drvec prints (.inp order) and as the internal [Y1;Y2] order.
Only the second reproduces logelf -> drvec's Gamma/Q/Sigma are mislabelled (BUG-18 + Gamma)."""
import numpy as np, exact_ml as E, warnings; warnings.filterwarnings("ignore")
PTS = [("e1",1,"e1_q0r1"),("e3",1,"e3_q0r1"),("rao2",1,"rao2_q0r1"),("rao1",1,"rao1_q0r1sj"),("rao1",2,"rao1_q0r2"),
       ("rao5",2,"rao5_q0r2"),("rao5",2,"rao5_q0r2_sj"),("rao7_sc",2,"rao7_sc_q0r2_ms"),("rao2",3,"rao2_q0r3_sj"),
       ("rao2",3,"rao2_q0r3"),("rao4",2,"rao4_q0r2"),("rao3",3,"rao3_q0r3"),("rao3_3v",1,"rao3_3v_q0r1")]
print("| drvec fit | drvec logelf | indep. exact, Γ/Σ as internal [Y1;Y2] | indep. exact, Γ/Σ with printed labels |\n|---|---|---|---|")
for case, r, stem in PTS:
    y = E.read_inp(case + ".inp"); n, M = y.shape
    a, b2, G, mu, S, ll = E.drvec_point(stem + ".out", M, r)
    P = np.eye(M)[list(range(M - r, M)) + list(range(M - r))]
    li = E.loglik(y, r, a, b2, P.T @ G @ P, mu, P.T @ S @ P); lp = E.loglik(y, r, a, b2, G, mu, S)
    print("| %s | %.4f | %.4f | %s |" % (stem, ll, li, "infeasible (non-stationary)" if lp < -1e9 else "%.4f" % lp))
