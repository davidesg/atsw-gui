"""
IPC_DE — iterative construction FROM the last model (mar3sar).
Take AR(3)+SAR(1)+full harmonics+mu and ADD stochastic seasonality at the
freq the MEG flagged (1,2): ifadf[1]=ifadf[2]=1 + MA_f at freq 1,2, removing
ONLY the freq 1,2 deterministic harmonics. Keep everything else.
"""
import fue
from art.describe import model_equation, describe_diagnosis

ts, mb = fue.load("cases/IPC_DE/work/IPC_DE_mar3sar.pre")

# keep all interventions except the freq 1,2 harmonics (now stochastic)
itvs = [itv for itv in mb.interventions
        if not (itv.type in ("cos", "sin") and getattr(itv, "harmonic", 0.0) in (1.0, 2.0))]

m = fue.Model(
    ts, d=mb.d, D=mb.D, boxlam=mb.boxlam,
    ar=[[0.0, 0.0, 0.0]], ar_free=[[True, True, True]],     # AR(3)  (from mar3sar)
    ar_s=[[0.0]], ar_s_free=[[True]],                        # SAR(1) (KEPT, from mar3sar)
    ma=[], ma_free=None, ma_s=[], ma_s_free=None,
    ma_f=[fue.FixedFreqFactor(freq=1.0, coef=-0.5, free=True),   # ADD MA_f at freq 1
          fue.FixedFreqFactor(freq=2.0, coef=-0.5, free=True)],  # ADD MA_f at freq 2
    interventions=itvs,
    ifadf=[0, 1, 1, 0, 0, 0, 0],                             # ADD seasonal unit roots freq 1,2
    mu=0.0, estimate_mu=True, refactor=mb.refactor,
)
m.fit()

print("================ EQUATION ================")
print(model_equation(ts, m))
print("\n================ DIAGNOSIS ================")
print(describe_diagnosis(m).summary)
print("\nloglik=%.4f  aic=%.4f  bic=%.4f" % (m.loglik, m.aic, m.bic))

m.write_pre("cases/IPC_DE/work/IPC_DE_mar3sar_uf.pre")
try:
    m.write_out("cases/IPC_DE/work/IPC_DE_mar3sar_uf.out")
except Exception as e:
    print("write_out warn:", e)
print("saved: cases/IPC_DE/work/IPC_DE_mar3sar_uf.pre")
