import numpy as np
np.set_printoptions(precision=6, suppress=True)
phi, th, gamma, beta = 0.6, 0.5, 0.2, 0.5      # B2 = -beta = -0.5
rng = np.random.default_rng(3); N = 20000
a, eta = rng.standard_normal(N), rng.standard_normal(N)
w = np.zeros(N); z2 = np.zeros(N)
for t in range(1,N):
    w[t]  = phi*w[t-1] + a[t] - th*a[t-1]
    z2[t] = z2[t-1] + gamma*w[t-1] + eta[t]
z1 = w + beta*z2
# error de la representacion VEC, segun el corolario 2
eps = np.column_stack([beta*eta + a - th*np.r_[0,a[:-1]], eta])
A   = np.column_stack([a + beta*eta, eta])      # innovacion reagrupada
for nombre, Tt in [("Theta = [[th, -th*beta],[0,0]]  (= +th*B2')", np.array([[th, -th*beta],[0,0]])),
                   ("Theta = [[th, +th*beta],[0,0]]  (= -th*B2')", np.array([[th,  th*beta],[0,0]]))]:
    r = eps[1:] - A[1:] + A[:-1] @ Tt.T
    print("%-44s error max %.3e" % (nombre, np.abs(r).max()))
# y ahora la ECUACION VEC entera, para fijar Lambda y F de este DGP
dz1 = np.diff(z1); dz2 = np.diff(z2); dz = np.column_stack([dz1,dz2])
W   = (z1 - beta*z2)
# dz_t = A_mat * W_{t-1} + eps_t  con A_mat = [beta*gamma + (phi-1) ; gamma]
Amat = np.array([beta*gamma + (phi-1.0), gamma])
res = dz[1:] - np.outer(W[1:-1], Amat) - eps[2:]
print("ecuacion VEC  dz_t = A W_{t-1} + eps_t          error max %.3e" % np.abs(res).max())
print("   A (= -Lambda de drvec) =", Amat, "  luego Lambda =", -Amat)
