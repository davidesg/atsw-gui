"""AddOn A2 (Census housing): internal consistency of Tables A1-A3 and the common-trend algebra."""
import numpy as np
LC = np.log(2*np.pi) + 1; N = 112
tabs = {'A1 EML': (-663.4662, 9, [[29.2020, 5.5274], [5.5274, 10.3041]], (12.0083, 12.2268)),
        'A1 CML': (-669.9050, 9, [[38.1903, 6.7457], [6.7457, 15.2644]], (12.1233, 12.3418)),
        'A3 EML': (-664.2429, 8, [[29.4526, 5.7112], [5.7112, 9.9830]], (12.0043, 12.1985)),
        'A3 CML': (-670.8983, 8, [[37.4868, 7.0347], [7.0347, 15.9136]], (12.1232, 12.3174))}
for k, (L, K, S, ic) in tabs.items():
    S = np.array(S); ident = -N*LC - N/2*np.log(np.linalg.det(S))
    print("%s  AIC %.4f (pub %.4f) BIC %.4f (pub %.4f) |Sigma| %.2f  identity %.4f  pub L %.4f  diff %.2f" %
          (k, (-2*L+2*K)/N, ic[0], (-2*L+K*np.log(N))/N, ic[1], np.linalg.det(S), ident, L, L-ident))
print("LR EML 2(L2-L1) = %.4f (pub 1.5534); CML = %.4f (pub 1.9866)" % (2*(-663.4662+664.2429), 2*(-669.9050+670.8983)))
print("implied L_E(0) = %.4f, L_C(0) = %.4f" % (-664.2429-59.2195/2, -670.8983-60.0643/2))
for k, P in (('A1 EML', [[0.4753, 0.9306], [0.0974, 0.7648]]), ('A1 CML', [[0.5033, 0.8408], [0.1251, 0.7109]])):
    print(k, "eig(I-Phi1) =", np.sort(np.linalg.eigvals(np.eye(2)-np.array(P)).real).round(4))
for k, lam, b in (('A3 EML', [0.5191, -0.1085], -1.8625), ('A3 CML', [0.4922, -0.1343], -1.8223)):
    lam = np.array(lam); print(k, "nonzero eig(Lam B') = B'Lam = %.4f" % (lam[0] + b*lam[1]))
for k, th in (('A1 EML', (0.9641, 1.0844)), ('A1 CML', (0.7309, 0.7012)), ('A3 EML', (0.9600, 1.1318)), ('A3 CML', (0.7531, 0.6918))):
    print(k, "seasonal MA root moduli |x| = theta^(-1/12):", np.round(np.array(th)**(-1/12), 5))
Q = np.array([[1.0, -1.8625], [0.2090, 1.0]])
print("Q^-1 =", np.linalg.inv(Q).round(4).tolist(), " P'Lam = %.5f" % (0.2090*0.5191 - 0.1085))
print("Lam_perp normalised: P = [%.4f, 1]" % (0.1085/0.5191))
print("2/sqrt(112) = %.4f" % (2/np.sqrt(112)))
