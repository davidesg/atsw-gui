import numpy as np
np.set_printoptions(precision=6, suppress=True)
# --- parametros VEC verdaderos (M=2, r=1, s=1, p=2, q=1) --------------------
M,r,s,p,q = 2,1,1,2,1
B2  = np.array([[-0.5]])                 # s x r
Lam = np.array([[0.30],[0.10]])          # M x r
F1  = np.diag([0.20,0.20])
Th1 = np.array([[0.50,0.20],[-0.10,0.30]])   # no escalar: discrimina
# --- simular  dY_t = -Lam W_{t-1} + F1 dY_{t-1} + A_t - Th1 A_{t-1} ---------
rng = np.random.default_rng(11); N = 3000
A = rng.standard_normal((N,M)); Y = np.zeros((N,M)); dY = np.zeros((N,M))
for t in range(2,N):
    W = Y[t-1,0] + B2[0,0]*Y[t-1,1]
    dY[t] = -Lam[:,0]*W + F1@dY[t-1] + A[t] - Th1@A[t-1]
    Y[t]  = Y[t-1] + dY[t]
# --- las matrices del cast, EXACTAMENTE como en vec_shootx ------------------
Cbar = np.zeros((M,M)); Cinv = np.zeros((M,M)); Hbar = np.zeros((M,M)); Lb = np.zeros((M,M))
for i in range(s):   Cbar[i, r+i] = 1.0
for j in range(r):   Cbar[s+j, j] = 1.0
for j in range(r):
    for i in range(s): Cbar[s+j, r+i] = B2[i,j]
for i in range(r):
    for j in range(s): Cinv[i, j] = -B2[j,i]
for i in range(r):   Cinv[i, s+i] = 1.0
for i in range(s):   Cinv[r+i, i] = 1.0
for i in range(r):   Hbar[s+i, s+i] = 1.0
for i in range(M):
    for j in range(r): Lb[i, s+j] = Lam[i,j]
assert np.allclose(Cbar@Cinv, np.eye(M)), "Cbar Cinv != I"
PhB = [None]*(p+1)
PhB[0] = Cinv.copy()
PhB[1] = Cinv@Hbar - Lb + F1@Cinv
PhB[2] = -(F1@Cinv@Hbar)
Phs = [Cbar@PhB[k] for k in range(p+1)]      # Phi*_k = Cbar PhiBar_k
Ths = Cbar@Th1@Cinv                          # Theta*_1 = Cbar Theta Cinv
# --- Ybar y A* --------------------------------------------------------------
Yb = np.zeros((N,M))
for t in range(1,N):
    Yb[t,0] = Y[t,1]-Y[t-1,1]                 # nabla Y2
    Yb[t,1] = Y[t,0] + B2[0,0]*Y[t,1]         # W
As = A @ Cbar.T                               # A*_t = Cbar A_t
# --- la identidad:  Ybar_t - sum Phi*_k Ybar_{t-k}  ==  A*_t - Theta*_1 A*_{t-1}
res = []
for t in range(600, N):
    lhs = Yb[t] - Phs[1]@Yb[t-1] - Phs[2]@Yb[t-2]
    rhs = As[t] - Ths@As[t-1]
    res.append(lhs-rhs)
res = np.array(res)
print("cast tal como esta en el codigo   -> error max %.3e" % np.abs(res).max())
# --- variantes, para localizar el termino que falla ------------------------
for nombre, TT in [("Theta* = Cbar Theta        ", Cbar@Th1),
                   ("Theta* = Cbar Theta Cbar^-1", Cbar@Th1@Cinv),
                   ("Theta* = Theta             ", Th1),
                   ("Theta* = Cinv Theta Cbar   ", Cinv@Th1@Cbar)]:
    e = []
    for t in range(600, N):
        lhs = Yb[t] - Phs[1]@Yb[t-1] - Phs[2]@Yb[t-2]
        rhs = As[t] - TT@As[t-1]
        e.append(lhs-rhs)
    print("  %s -> error max %.3e" % (nombre, np.abs(np.array(e)).max()))
