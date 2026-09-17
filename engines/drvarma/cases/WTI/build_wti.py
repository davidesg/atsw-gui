"""
WTI base univariate model (pass-through input): log, d=1, NO seasonality.
Compare AR(1) vs MA(1) (both + drift mu).  Pick per AR-preference / fit.
Built directly in fue (no deterministic seasonal terms at all).
"""
import sys
import numpy as np
import fue
from art.describe import model_equation, describe_diagnosis

ts, _ = fue.load("cases/WTI/WTI.inp")


def build(p, q, with_mu):
    return fue.Model(
        ts, d=1, D=0, boxlam=0.0,
        ar=[[0.0] * p] if p else [[0.0]],
        ar_free=[[True] * p] if p else [[False]],
        ma=[[-0.3] * q] if q else [],
        ma_free=[[True] * q] if q else None,
        ar_s=[], ma_s=[], interventions=[], ifadf=[0] * 7,
        mu=0.0, estimate_mu=with_mu, refactor=100.0,
    )


for (p, q, with_mu, name) in [(1, 0, True, "AR(1)+mu"),
                              (0, 1, True, "MA(1)+mu"),
                              (1, 0, False, "AR(1) no-mu (BASE)")]:
    m = build(p, q, with_mu)
    m.fit()
    print(f"================ WTI  {name} ================")
    print(model_equation(ts, m))
    d = describe_diagnosis(m)
    print(d.summary)
    print(f"loglik={m.loglik:.3f}  aic={m.aic:.3f}  bic={m.bic:.3f}\n")
    if name.startswith("AR(1) no-mu"):
        m.write_pre("cases/WTI/work/WTI_ar1.pre")
        m.write_fuf(horizon=24, path="cases/WTI/work/WTI_ar1.fuf.inp")
        print("saved BASE: cases/WTI/work/WTI_ar1.pre + .fuf.inp\n")
