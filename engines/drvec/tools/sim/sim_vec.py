import numpy as np, sys
# VEC(p=2,q=1), M=2, r=1:  dY_t = -Lam (B'Y_{t-1}) + F1 dY_{t-1} + A_t - Th1 A_{t-1}
# Orden interno del modelo: Y = [Y1 (r) ; Y2 (s)],  B = [1 ; B2]
n     = int(sys.argv[1]) if len(sys.argv)>1 else 400
th    = float(sys.argv[2]) if len(sys.argv)>2 else 0.5
seed  = int(sys.argv[3]) if len(sys.argv)>3 else 7
out   = sys.argv[4] if len(sys.argv)>4 else "/tmp/sim.inp"
B2    = -0.5
Lam   = np.array([0.30, 0.10])
F1    = np.diag([0.20, 0.20])
Th1   = np.diag([th, th])
rng   = np.random.default_rng(seed)
burn  = 500
N     = n + burn + 2
A     = rng.standard_normal((N, 2))
Y     = np.zeros((N, 2)); dY = np.zeros((N, 2))
for t in range(2, N):
    W  = Y[t-1,0] + B2*Y[t-1,1]                  # W = Y1 + B2*Y2
    dY[t] = -Lam*W + F1 @ dY[t-1] + A[t] - Th1 @ A[t-1]
    Y[t]  = Y[t-1] + dY[t]
Y = Y[burn+2:]
# .inp de drvec: columnas [Y2 (s=1) ; Y1 (r=1)]
lines = ["* VEC simulado: r=1, B2=-0.5, Lam=(0.30,0.10), F1=0.2I, Th1=%.2f I, Sigma=I" % th,
         "1", "2 %d 1 1900" % len(Y), "Y2 Y1", "1.0 0 0"]
lines += ["%.10f %.10f" % (Y[t,1], Y[t,0]) for t in range(len(Y))]
open(out,"w").write("\n".join(lines)+"\n")
print("escrito", out, "n =", len(Y))
