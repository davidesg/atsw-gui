"""
Single-origin (12/2019), FIXED-parameter forecast comparison across horizons.
Each model estimated ONCE on the 216-obs training sample; forecast 24 steps;
errors vs actuals 2020-01..2021-12.  No re-estimation, no rolling origins.

Models per series:
  ART   : ART univariate (fue fuf, fixed params)  -- IPC_ES AR(1), IPC_FR SAR(1), IPC_DE AR(3)+SAR(1)
  diag  : drvarma VARMA(3,0) diagonal  (== univariate-in-drvarma)
  full  : drvarma VARMA(3,0) complete  (cross-dependencies)
"""
import re
import numpy as np
import pandas as pd
import fue

SERIES = ["IPC_ES", "IPC_FR", "IPC_DE"]
ART_FUF = {"IPC_ES": "cases/IPC_ES/work/IPC_ES_m10.fuf.inp",
           "IPC_FR": "cases/IPC_FR/work/IPC_FR_msar.fuf.inp",
           "IPC_DE": "cases/IPC_DE/work/IPC_DE_mar3sar.fuf.inp"}
HMARK = [1, 12, 24]

df = pd.read_excel("data/IPC.xlsx", "Sheet1")
act = {s: df[s].iloc[216:216 + 24].to_numpy(float) for s in SERIES}  # 2020-01..2021-12


def parse_fc(path):
    out, cur = {}, None
    for line in open(path):
        m = re.match(r"\s*Series\s+\d+\s+\(([^)]+)\)", line)
        if m:
            cur = m.group(1); out[cur] = []; continue
        if cur is not None:
            mm = re.match(r"\s*\d+/\d{4}\s+([-\d.]+)", line)
            if mm:
                out[cur].append(float(mm.group(1)))
    return {k: np.array(v) for k, v in out.items()}


full = parse_fc("data/models_group1/IPC3_full24.forecast")
diag = parse_fc("data/models_group1/IPC3_diag24.forecast")
art = {s: np.asarray(fue.load_fuf(ART_FUF[s])[1].forecast_fuf(24).level, float) for s in SERIES}

rows = []
for s in SERIES:
    a = act[s]
    fc = {"ART": art[s], "diag(uni)": diag[s], "full": full[s]}
    for label, f in fc.items():
        pe = 100 * (a - f) / a
        rows.append({"series": s, "model": label,
                     "MAPE_1_24": np.abs(pe).mean(),
                     **{f"pe_h{h}": pe[h - 1] for h in HMARK}})

res = pd.DataFrame(rows)
res.to_csv("cases/forecast_compare_single_origin.csv", index=False, float_format="%.4f")

pd.set_option("display.width", 200)
for s in SERIES:
    print(f"\n==== {s}  (origen 12/2019, params fijos) ====")
    sub = res[res.series == s].set_index("model")[["pe_h1", "pe_h12", "pe_h24", "MAPE_1_24"]]
    print(sub.to_string(float_format=lambda x: f"{x:+.3f}"))
print("\n(pe_hH = % error a horizonte H ; MAPE_1_24 = media |%error| sobre h=1..24)")
print("saved: cases/forecast_compare_single_origin.csv")
