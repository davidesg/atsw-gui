"""A1 check: AddOn closed form (A.5)-(A.6) vs drvec, and the likelihood by two independent routes."""
import numpy as np, sys
sys.path.insert(0, '.')
from exactlik import *
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)      # cols: muskrat, mink (drvec order [Y2;Y1])
Ylev = raw[:, ::-1]                                          # paper order [Y1=mink ; Y2=muskrat]
l1, l2, b2 = 0.3, 0.1, -0.5
EW = 10.0
Q = np.array([[1.0, 0.3], [0.3, 1.5]])
# --- AddOn (A.5): Ybar_t = (H - C Lam) Ybar_{t-1} + C A_t
Cb = np.array([[0, 1], [1, b2]]); Hb = np.array([[0, 0], [0, 1]]); Lb = np.array([[0, l1], [0, l2]])
Phi_addon = Hb - Cb @ Lb
Phi_A6 = np.array([[0, -l2], [0, 1 - l1 - l2 * b2]])
print("A.5 == A.6:", np.abs(Phi_addon - Phi_A6).max())
# general map
phis, thetas, Cbar = mauricio_varma(np.array([[l1], [l2]]), np.array([[b2]]), [], [], 2, 1)
print("general map (eq.16/18) vs A.6:", np.abs(phis[0] - Phi_A6).max())
# drvec dump
d = open('run/dump_a1.txt').read().split('\n')
dphi = np.array([[float(v) for v in d[2].split()], [float(v) for v in d[3].split()]])
dqq = np.array([[float(v) for v in d[4].split()], [float(v) for v in d[5].split()]])
dw = np.array([[float(v) for v in line.split()] for line in d[6:] if line.strip()])
print("drvec Phi* vs A.6:", np.abs(dphi - Phi_A6).max())
print("drvec Sigma* vs C Sigma C':", np.abs(dqq - Cb @ Q @ Cb.T).max())
Yb = ybar_from_levels(Ylev, np.array([[b2]]), 1)
print("drvec Ybar vs eq.(17) Ybar:", np.abs(dw - Yb).max(), Yb.shape)
mu = np.array([0, EW])
L1, s2 = loglik_conc(Yb, mu, [Phi_A6], [], Cb @ Q @ Cb.T)
L2 = loglik_kalman(Yb, mu, [Phi_A6], [], s2 * Cb @ Q @ Cb.T)
print("independent Toeplitz logL (conc.) = %.10f  sigma2=%.10f" % (L1, s2))
print("independent Kalman   logL at s2 Q = %.10f" % L2)
print("drvec -eval                        = -215.2907070434  sigma2=1.6696365819")
# eigenvalues: mu2 of Phi1 = I - Lam B'
Phi1 = np.eye(2) - np.array([[l1], [l2]]) @ np.array([[1, b2]])
print("eig(Phi1) =", np.linalg.eigvals(Phi1), " eig(A.6) =", np.linalg.eigvals(Phi_A6))
