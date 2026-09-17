"""
Fixed-parameter, multi-origin recursive forecast comparison at horizons 1,12,24.
All models trained ONCE on data to 12/2019; origin rolls 12/2019..12/2021.

  ART        : ART univariate (fue, fixed params)  -- per-origin errors from
               cases/IPC_<s>/work/IPC_<s>_recursive_eval.csv
  diag(uni)  : drvarma VARMA(3,0) diagonal, -estwin 216  (univariate-in-drvarma)
  full       : drvarma VARMA(3,0) complete,  -estwin 216  (multivariate)

Reports MAPE/RMSE and the NUMBER OF ERRORS used at each horizon.
"""
import re
import numpy as np
import pandas as pd

SERIES = ["IPC_ES", "IPC_FR", "IPC_DE"]
HORIZONS = [1, 12, 24]
ORI_MAX = (2021, 12)        # last origin to match the ART evaluation set

df = pd.read_excel("data/IPC.xlsx", "Sheet1")
# date -> value per series  (FECHA like "1/2002")
val = {s: {} for s in SERIES}
for _, r in df.iterrows():
    mm, yy = map(int, str(r["FECHA"]).split("/"))
    for s in SERIES:
        val[s][(yy, mm)] = float(r[s])


def add_months(y, m, h):
    t = (y * 12 + (m - 1)) + h
    return t // 12, t % 12 + 1


def parse_recursive(path):
    """origin(y,m) -> series -> {h: level}."""
    fc = {}
    for line in open(path):
        mm = re.match(r"\s*(\d+)/(\d+)\s+(\S+)\s+(\d+)\s+([-\d.]+)", line)
        if not mm:
            continue
        om, oy, s, h, lv = int(mm.group(1)), int(mm.group(2)), mm.group(3), int(mm.group(4)), float(mm.group(5))
        fc.setdefault((oy, om), {}).setdefault(s, {})[h] = lv
    return fc


def score_drvarma(path, label):
    fc = parse_recursive(path)
    rows = []
    for s in SERIES:
        for h in HORIZONS:
            pe, e = [], []
            for (oy, om), d in fc.items():
                if (oy, om) > ORI_MAX:
                    continue
                ty, tm = add_months(oy, om, h)
                a = val[s].get((ty, tm))
                f = d.get(s, {}).get(h)
                if a is not None and f is not None:
                    e.append(a - f); pe.append(100 * (a - f) / a)
            e, pe = np.array(e), np.array(pe)
            rows.append({"series": s, "model": label, "h": h, "n": len(e),
                         "MAPE": np.abs(pe).mean(), "RMSE": np.sqrt((e**2).mean())})
    return pd.DataFrame(rows)


ART_CSV = {"IPC_ES": "cases/IPC_ES/work/IPC_ES_recursive_eval.csv",
           "IPC_FR": "cases/IPC_FR/work/IPC_FR_recursive_eval.csv",
           "IPC_DE": "cases/IPC_DE/work/IPC_DE_recursive_eval_det.csv"}


def score_art():
    rows = []
    for s in SERIES:
        d = pd.read_csv(ART_CSV[s])
        # origins are 12/2019.. ; all <= 12/2021 already
        for h in HORIZONS:
            e = d[f"e{h}"].dropna().to_numpy()
            pe = d[f"pe{h}"].dropna().to_numpy()
            rows.append({"series": s, "model": "ART", "h": h, "n": len(e),
                         "MAPE": np.abs(pe).mean(), "RMSE": np.sqrt((e**2).mean())})
    return pd.DataFrame(rows)


res = pd.concat([score_art(),
                 score_drvarma("data/models_group1/IPC3_ext_diag.recursive", "diag(uni)"),
                 score_drvarma("data/models_group1/IPC3_ext_full.recursive", "full")],
                ignore_index=True)
res.to_csv("cases/recursive_compare.csv", index=False, float_format="%.4f")

order = {"ART": 0, "diag(uni)": 1, "full": 2}
for s in SERIES:
    print(f"\n==== {s}  (fixed params, origins 12/2019..12/2021) ====")
    sub = res[res.series == s].copy().sort_values(["h", "model"], key=lambda c: c.map(lambda v: order.get(v, v) if isinstance(v, str) else v))
    print(sub[["h", "model", "n", "MAPE", "RMSE"]].to_string(index=False, float_format=lambda x: f"{x:.4f}"))
print("\nsaved: cases/recursive_compare.csv")
