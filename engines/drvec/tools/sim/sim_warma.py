import numpy as np, sys
# DGP WARMA del paper BVECM (ecs. 24-25), M=2, r=1:
#    Phi(B) w_t = Theta(B) a_t        w_t = z1_t - beta*z2_t
#    Delta z2_t = gamma*w_{t-1} + eta_t
n    = int(sys.argv[1]); th = float(sys.argv[2]); seed = int(sys.argv[3]); out = sys.argv[4]
phi, gamma, beta = 0.6, 0.2, 0.5      # B2 = -beta = -0.5
rng  = np.random.default_rng(seed); burn = 500; N = n + burn + 2
a, eta = rng.standard_normal(N), rng.standard_normal(N)
w  = np.zeros(N); z2 = np.zeros(N)
for t in range(1, N):
    w[t]  = phi*w[t-1] + a[t] - th*a[t-1]
    z2[t] = z2[t-1] + gamma*w[t-1] + eta[t]
z1 = w + beta*z2
z1, z2 = z1[burn+2:], z2[burn+2:]
lines = ["* WARMA (BVECM cor. 2): phi=%.2f theta=%.2f gamma=%.2f beta=%.2f -> B2=%.2f"
         % (phi, th, gamma, beta, -beta),
         "1", "2 %d 1 1900" % len(z1), "z2 z1", "1.0 0 0"]
lines += ["%.10f %.10f" % (z2[t], z1[t]) for t in range(len(z1))]   # [Y2 ; Y1]
open(out,"w").write("\n".join(lines)+"\n")
