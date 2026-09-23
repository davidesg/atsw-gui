"""s6_cvals.py -- which deterministic case do drvec's 'case 1: no constant'
critical values belong to?  (drvec.c:148-158, taken from urca ca.jo type='eigen',
ecdet='none'; urca's ecdet='none' ADDS AN UNRESTRICTED CONSTANT to the
short-run regressors: Z1 <- cbind(1, Z1)).

Monte Carlo of the M-r = 1 statistic (random walk, no drift):
  LR = T log(s0^2 / s1^2) for H0: Pi = 0 vs Pi free, in
   (i)  dy = Pi y_{-1} + e              (no deterministic term = drvec case 1)
   (ii) dy = mu + Pi y_{-1} + e          (unrestricted constant = urca 'none')
   (iii) dy = Pi (y_{-1} - c) + e        (restricted constant = drvec case 2)
"""
import numpy as np
rng = np.random.default_rng(0)
R, T = 20000, 500
lr1, lr2, lr3 = [], [], []
for _ in range(R):
    e = rng.normal(size=T + 1); y = np.cumsum(e)
    dy = np.diff(y); yl = y[:-1]
    s0 = dy @ dy / T
    b = (yl @ dy) / (yl @ yl); s1 = ((dy - b * yl) ** 2).mean()
    lr1.append(T * np.log(s0 / s1))
    X = np.column_stack([np.ones(T), yl])
    r2 = dy - X @ np.linalg.lstsq(X, dy, rcond=None)[0]
    d0 = dy - dy.mean()
    lr2.append(T * np.log((d0 @ d0) / (r2 @ r2)))
    r3 = dy - X @ np.linalg.lstsq(X, dy, rcond=None)[0]   # restricted const: mu = -Pi c -> same span, but H0 has no const
    lr3.append(T * np.log(s0 / ((r3 @ r3) / T)))
q = lambda v: np.round(np.quantile(v, [0.90, 0.95, 0.99]), 2)
print("(i)   no deterministic term     :", q(lr1), "  (Osterwald-Lenum table 0: 2.86 3.84 6.51)")
print("(ii)  unrestricted constant     :", q(lr2), "  (drvec 'case 1' uses 6.50 8.18 11.65)")
print("(iii) restricted constant       :", q(lr3), "  (drvec 'case 2' uses 7.52 9.24 12.97)")
