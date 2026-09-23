"""Q3: check analisis_BEC() algebra on the SL legacy fit (p=2, beta estimated, conv on).
(1) levels VAR implied by the Ybar-VAR directly  vs  levels VAR implied by the printed BEC;
(2) Pi; (3) the innovation covariance of the BEC equations and the structural form."""
import sys, re, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0]); from runlib import *
f = os.path.join(WORK, 'bec', 'SL.out'); t = open(f, encoding='latin-1').read(); r = parse_legacy(f)
p = max(r['phi_leg']); phi = {k: np.array(v) for k, v in r['phi_leg'].items()}
beta = float(re.search(r'ECT = z_1 - (\S+)\*z_2', t).group(1))
s = re.search(r'Matrix a = sigma2 Q \(symmetric\):\n\s*(\S+)\n\s*(\S+)\s+(\S+)', t)
Se = np.array([[float(s.group(1)), float(s.group(2))], [float(s.group(2)), float(s.group(3))]])
K0 = np.array([[1, -beta], [0, 1.]]); K1 = np.array([[0, 0], [0, -1.]]); K0i = np.linalg.inv(K0)
# (1a) levels VAR(p+1) directly from Ybar_t = K0 z_t + K1 z_{t-1}
A = {1: K0i @ (-K1 + phi[1] @ K0)}
for j in range(2, p + 1): A[j] = K0i @ (phi[j] @ K0 + phi[j - 1] @ K1)
A[p + 1] = K0i @ (phi[p] @ K1)
# (1b) from the BEC formulas of analisis_BEC (main.c 559-566)
al = {k: np.array([(phi[k][0, 0] - (k == 1)) + beta * phi[k][1, 0], phi[k][1, 0]]) for k in phi}
G = {k: np.array([[0, beta * phi[k][1, 1] + phi[k][0, 1]], [0, phi[k][1, 1]]]) for k in phi}
bp = np.array([1, -beta]); B = {j: np.zeros((2, 2)) for j in range(1, p + 2)}; B[1] += np.eye(2)
for k in phi:
    B[k] += np.outer(al[k], bp); B[k] += G[k]; B[k + 1] -= G[k]
print('max |A_j(direct) - A_j(from BEC)| =', max(np.abs(A[j] - B[j]).max() for j in A))
Pi_direct = -(np.eye(2) - sum(A.values())); Pi_code = sum(np.outer(al[k], bp) for k in phi)
print('Pi direct:\n', Pi_direct.round(6), '\nPi (analisis_BEC):\n', Pi_code.round(6))
print('Johansen alpha = sum_k alpha(k) =', sum(al.values()).round(4), ' (analisis_BEC prints per-lag alpha(k) only)')
# (3) BEC innovations eps = K0^-1 e ; code uses Sigma_e
Seps = K0i @ Se @ K0i.T
def PD(S): L = np.linalg.cholesky(S); return L[1, 0] / L[0, 0], L[0, 0]**2, L[1, 1]**2
print('Sigma_e (Ybar innovations, used by the code):', Se.round(3).tolist(), ' -> P21, d1, d2 =', np.round(PD(Se), 4))
print('Sigma_eps (BEC innovations, K0^-1 Se K0^-T):  ', Seps.round(3).tolist(), ' -> P21, d1, d2 =', np.round(PD(Seps), 4))
P21 = PD(Seps)[0]
print(f'correct structural eq. 2: dz2 = {P21:+.4f}*dz1 + ... ; code prints Pinv[2][1] = {-PD(Se)[0]:+.4f} (sign flipped AND from the wrong Sigma)')
