"""Long-run variance of dY2 implied by a legacy(-instrumented) fit, relative to its innovation variance:
omega = [Phi(1)^-1 Theta(1) Sigma Theta(1)' Phi(1)^-T]_{dY2,dY2} / Sigma_{dY2,dY2}.
omega -> 0 means Y2 carries no stochastic trend: the fitted model denies rank r = M-1 (common (1-L) factor)."""
import sys, glob, re, os
import numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0]); from runlib import *
def omega(f):
    t = open(f, encoding='latin-1').read()
    r = parse_legacy(f)
    ph = r['phi_leg']; P = np.eye(2)
    for k, m in ph.items(): P -= np.array(m)
    th = np.array(r['theta_leg']) if r['theta_leg'] else np.zeros((2, 2))
    T = np.eye(2) - th
    s = re.search(r'Matrix a = sigma2 Q \(symmetric\):\n\s*(\S+)\n\s*(\S+)\s+(\S+)', t)
    Sg = np.array([[float(s.group(1)), float(s.group(2))], [float(s.group(2)), float(s.group(3))]])
    Pi = np.linalg.inv(P); L = Pi @ T @ Sg @ T.T @ Pi.T
    return L[1, 1] / Sg[1, 1], r
if __name__ == '__main__':
    for pat in sys.argv[1:]:
        best = None
        for f in glob.glob(os.path.join(WORK, pat, 'm.out')):
            try: w, r = omega(f)
            except Exception as e: continue
            if r['logL'] is not None and (best is None or r['logL'] > best[1]['logL']): best = (w, r, f)
        if best: print(f"{pat:40} logL {best[1]['logL']:12.4f}  a/theta22={best[1]['theta_leg'][1][1] if best[1]['theta_leg'] else 0:+.3f}  omega={best[0]:.4f}")
