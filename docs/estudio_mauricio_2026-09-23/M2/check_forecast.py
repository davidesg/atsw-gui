"""Independent check of drvec's level forecasts and bands (P5 / Proposition 2).
Route: invert Mauricio's map to recover the VEC parameters (Lam, B2, F1, Theta, Sigma) from the dumped
transformed system, then forecast IN LEVELS directly from VEC (5) -- no Ybar, no Psi weights, no G_m --
(a) point forecast by the VEC recursion with future shocks zero, (b) bands by Monte Carlo of VEC (5)."""
import numpy as np, re, sys
L = [l.split() for l in open('run/fdump.txt').read().strip().split('\n')]
M, n, p, q, r = map(int, L[0][:5]); s2 = float(L[0][5]); s = M - r
mu = np.array(L[1], float); B2 = np.array(L[2:2+s], float).reshape(s, r); i = 2 + s
phis = [np.array(L[i+k*M:i+(k+1)*M], float) for k in range(p)]; i += p*M
ths = [np.array(L[i+k*M:i+(k+1)*M], float) for k in range(q)]; i += q*M
Qs = np.array(L[i:i+M], float); i += M
WA = np.array(L[i:i+n], float); Wb, Ast = WA[:, :M], WA[:, M:]
Cb = np.zeros((M, M)); Cb[:s, r:] = np.eye(s); Cb[s:, :r] = np.eye(r); Cb[s:, r:] = B2.T
Ci = np.linalg.inv(Cb); Hb = np.zeros((M, M)); Hb[s:, s:] = np.eye(r)
PhB = [Ci @ P for P in phis]                        # PhiBar_k = Cinv Phi*_k
assert p == 2
FCi = np.zeros((M, M)); FCi[:, :s] = (PhB[0] - Ci @ Hb)[:, :s]; FCi[:, s:] = -PhB[1][:, s:]
F1 = FCi @ Cb
Lam = (Ci @ Hb + F1 @ Ci - PhB[0])[:, s:]
print("check PhiBar_2 = -F1 Cinv Hbar:", np.abs(PhB[1] + F1 @ Ci @ Hb).max())
Th = [Ci @ T @ Cb for T in ths]; Sig = s2 * Ci @ Qs @ Ci.T
print("recovered VEC (paper order [Y1;Y2]): Lam", Lam.ravel().round(4), "B2", B2.ravel().round(4), "\nF1", F1.round(4).tolist(), "\nTheta", Th[0].round(4).tolist(), "\nSigma", Sig.round(5).tolist(), " E[W]", mu[s:])
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
Y = raw[:, ::-1]                                    # [Y1 = mink ; Y2 = muskrat]
A = Ast @ Ci.T                                      # A_t = Cinv A*_t   (Y order)
B = np.vstack([np.eye(r), B2])                      # B = [I_r ; B2]
EW = mu[s:]
H = 8
def vec_path(Yhist, Ahist, shocks):
    Yh = list(Yhist); Ah = list(Ahist)
    for h in range(len(shocks)):
        d1 = Yh[-1] - Yh[-2]
        dy = F1 @ d1 - Lam @ (B.T @ Yh[-1] - EW) + shocks[h] - Th[0] @ Ah[-1]
        Yh.append(Yh[-1] + dy); Ah.append(shocks[h])
    return np.array(Yh[len(Yhist):])
# (a) point forecast, VEC recursion in levels
pt = vec_path(Y[-2:], A[-1:], np.zeros((H, M)))
# drvec's table
txt = open('run/mink_muskrat.out').read()
blk = txt[txt.index('steps ahead, in levels'):].split('\n')[4:4+H]
dv = np.array([[float(v) for v in l.split()[1:]] for l in blk])   # muskrat, se, mink, se
print("\n(a) point forecast, VEC-in-levels vs drvec (max abs diff): muskrat %.2e  mink %.2e" %
      (np.abs(pt[:, 1] - dv[:, 0]).max(), np.abs(pt[:, 0] - dv[:, 2]).max()))
# (b) Monte Carlo bands
rng = np.random.default_rng(7); R = 200000
Lc = np.linalg.cholesky(Sig)
E = rng.standard_normal((R, H, M)) @ Lc.T
Yh = np.zeros((R, H, M)); Yp, Yc, Ap = np.tile(Y[-2], (R, 1)), np.tile(Y[-1], (R, 1)), np.tile(A[-1], (R, 1))
for h in range(H):
    dy = (Yc - Yp) @ F1.T - (Yc @ B - EW) @ Lam.T + E[:, h] - Ap @ Th[0].T
    Yn = Yc + dy; Yh[:, h] = Yn; Yp, Yc, Ap = Yc, Yn, E[:, h]
sd = Yh.std(0)
print("(b) Monte Carlo s.e. (R=%d) vs drvec s.e.:" % R)
for h in range(H):
    print("   h=%d  muskrat MC %.4f drvec %.4f (%.2f%%)   mink MC %.4f drvec %.4f (%.2f%%)" %
          (h+1, sd[h, 1], dv[h, 1], 100*(sd[h, 1]/dv[h, 1]-1), sd[h, 0], dv[h, 3], 100*(sd[h, 0]/dv[h, 3]-1)))
print("   MC mean vs drvec point, max abs: %.4f" % np.abs(Yh.mean(0)[:, ::-1] - dv[:, [0, 2]]).max())
