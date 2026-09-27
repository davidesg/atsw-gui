import numpy as np, sys, subprocess, os
sys.path.insert(0, '.')
from exactlik import *
raw = np.loadtxt('run/mink_muskrat.inp', skiprows=10)
Ylev = raw[:, ::-1]                      # paper order [mink ; muskrat]
T4 = dict(EW=8.1345, Lam=np.array([[0.8392], [0.5881]]), B2=np.array([[-0.2042]]),
          F=np.array([[0.5848, -0.6458], [-0.6621, 0.0]]),
          Th=np.array([[0.0, -1.1148], [-0.8953, -0.0174]]),
          Sig=np.array([[0.0385, 0.0181], [0.0181, 0.0549]]))
def model(par, thscale=1.0):
    phis, thetas, Cb = mauricio_varma(par['Lam'], par['B2'], [par['F']], [thscale * par['Th']], 2, 1)
    Yb = ybar_from_levels(Ylev, par['B2'], 1)
    return phis, thetas, Cb, Yb
def drvec_eval(par, thscale, extra=()):
    Q = par['Sig'] / par['Sig'][0, 0]
    x = [par['EW'], *par['Lam'].ravel(), *par['F'].ravel(), *(thscale * par['Th']).ravel(), Q[1, 1], Q[1, 0], par['B2'][0, 0]]
    open('run/x.txt', 'w').write(' '.join('%.12g' % v for v in x))
    out = subprocess.run(['../drvec_copy/bin/drvec', 'mink_muskrat', '2', '1', '1', '-case', '2', '-mean', '-mafree', '-eval', *extra],
                         cwd='run', env=dict(os.environ, DRVEC_X='x.txt', DRVEC_DUMP='dump.txt'), capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if 'eval' in l or 'inject' in l]
if __name__ == '__main__':
    for sc in (0.9, 0.99):
        phis, thetas, Cb, Yb = model(T4, sc)
        mu = np.array([0, T4['EW']])
        Q = T4['Sig'] / T4['Sig'][0, 0]
        L, s2 = loglik_conc(Yb, mu, phis, thetas, Cb @ Q @ Cb.T)
        print("Theta x %.2f  independent logL(conc) = %.10f sigma2 = %.8f" % (sc, L, s2))
        print("               drvec:", drvec_eval(T4, sc))
        print("               eig Theta:", np.linalg.eigvals(sc * T4['Th']))
    # the published point itself (non-invertible Theta), exact Gaussian density needs no invertibility
    phis, thetas, Cb, Yb = model(T4, 1.0)
    mu = np.array([0, T4['EW']])
    Lfix = loglik(Yb, mu, phis, thetas, Cb @ T4['Sig'] @ Cb.T)
    Q = T4['Sig'] / T4['Sig'][0, 0]
    Lc, s2 = loglik_conc(Yb, mu, phis, thetas, Cb @ Q @ Cb.T)
    print("Published Table 4 point, published Sigma : logL = %.4f   (paper prints 15.1257)" % Lfix)
    print("Published Table 4 point, scale concentrated: logL = %.4f, implied Sigma = %s" % (Lc, (s2 * Q).round(5).tolist()))
    print("eig Theta1 (Table 4):", np.linalg.eigvals(T4['Th']))
    # Kalman cross-check at the published point
    print("Kalman at published point:", loglik_kalman(Yb, mu, phis, thetas, Cb @ T4['Sig'] @ Cb.T))
