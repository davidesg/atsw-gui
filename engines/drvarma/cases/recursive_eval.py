"""
Recursive (pseudo out-of-sample) forecast evaluation — fixed-parameter ARIMA.

Goal: produce per-origin forecast errors at horizons 1, 12, 24 to benchmark
these univariate models against more sophisticated multivariate models.

METHOD (no re-estimation):
  The model is estimated once on the sample truncated to 12/2019. Its forecast
  state lives in a fue `.fuf` .inp file (data + fixed params + innovation var).
  To move the forecast origin forward WITHOUT re-estimating, we manipulate that
  .inp directly: append real observations to the data block and bump the
  observation-count field, then `load_fuf` + `forecast_fuf`. This is exactly the
  fixed-parameter projection fue performs internally (load_fuf path), so every
  origin uses the SAME parameters estimated to 12/2019.

  Origin walks monthly from 12/2019 to 12/2021 (25 origins). For each origin we
  forecast 24 steps and compare to the actuals (available 2020-01 .. 2023-11).
  At horizon h the error is  e = actual(origin+h) - forecast(origin+h).
  If the actual for origin+h is not yet available (h=24 for the last origins),
  that cell is left empty.

Usage:
  python3 cases/recursive_eval.py IPC_ES cases/IPC_ES/work/IPC_ES_m10.fuf.inp
  python3 cases/recursive_eval.py IPC_FR cases/IPC_FR/work/IPC_FR_<model>.fuf.inp
"""
import os, re, sys, tempfile
import numpy as np
import pandas as pd
import fue as _fue

XLSX = "data/IPC.xlsx"
N_TRAIN = 216           # obs up to 12/2019 (2002-01 start)
START_YEAR, START_PER = 2002, 1
HORIZONS = [1, 12, 24]
N_ORIGINS = 25          # 12/2019 .. 12/2021 inclusive


def load_template(fuf_path):
    with open(fuf_path) as fh:
        lines = fh.readlines()
    hdr_end = next(i for i, l in enumerate(lines) if l.startswith("** Time series"))
    header = lines[:hdr_end + 1]
    base_vals = [float(l) for l in lines[hdr_end + 1:] if l.strip()]
    nobs_idx = next(i for i, l in enumerate(header)
                    if re.match(r"\s*\d+\s+\d+\s+\d{4}\s+\S+", l))
    nobs_tail = header[nobs_idx].split(None, 1)[1]
    return header, nobs_idx, nobs_tail, base_vals


def write_fuf_with(header, nobs_idx, nobs_tail, n_total, values, path):
    h = list(header)
    h[nobs_idx] = f" {n_total}  {nobs_tail}"
    with open(path, "w") as fh:
        fh.writelines(h)
        fh.write("".join(f"{v:.10f} \n" for v in values))


def origin_label(k):
    m = START_YEAR * 12 + (START_PER - 1) + (N_TRAIN + k) - 1
    return f"{m % 12 + 1:02d}/{m // 12}"


def evaluate(series, fuf_path):
    header, nobs_idx, nobs_tail, base_vals = load_template(fuf_path)
    assert len(base_vals) == N_TRAIN, f"expected {N_TRAIN} base obs, got {len(base_vals)}"
    actuals = pd.read_excel(XLSX, sheet_name="Sheet1")[series].iloc[N_TRAIN:].to_numpy(float)
    n_act = len(actuals)
    tmp = tempfile.mkdtemp()

    rows = []
    for k in range(N_ORIGINS):
        vals = base_vals + list(actuals[:k])
        p = os.path.join(tmp, f"o{k}.inp")
        write_fuf_with(header, nobs_idx, nobs_tail, len(vals), vals, p)
        _, m = _fue.load_fuf(p)
        lvl = np.asarray(m.forecast_fuf().level, float)
        rec = {"origin": origin_label(k)}
        for h in HORIZONS:
            tgt = k + h - 1
            if tgt < n_act:
                a, f = actuals[tgt], lvl[h - 1]
                rec[f"actual_h{h}"], rec[f"fcst_h{h}"] = a, f
                rec[f"e{h}"], rec[f"pe{h}"] = a - f, 100.0 * (a - f) / a
            else:
                for col in (f"actual_h{h}", f"fcst_h{h}", f"e{h}", f"pe{h}"):
                    rec[col] = np.nan
        rows.append(rec)
    return pd.DataFrame(rows)


def summary(ev):
    out = []
    for h in HORIZONS:
        e = ev[f"e{h}"].dropna().to_numpy()
        pe = ev[f"pe{h}"].dropna().to_numpy()
        out.append({"h": h, "n": len(e), "ME": e.mean(), "MAE": np.abs(e).mean(),
                    "RMSE": np.sqrt((e**2).mean()), "MPE": pe.mean(),
                    "MAPE": np.abs(pe).mean(), "RMSPE": np.sqrt((pe**2).mean())})
    return pd.DataFrame(out)


if __name__ == "__main__":
    series = sys.argv[1] if len(sys.argv) > 1 else "IPC_ES"
    fuf = sys.argv[2] if len(sys.argv) > 2 else f"cases/{series}/work/{series}_m10.fuf.inp"
    tag = ("_" + sys.argv[3]) if len(sys.argv) > 3 else ""
    ev = evaluate(series, fuf)
    out_csv = f"cases/{series}/work/{series}_recursive_eval{tag}.csv"
    ev.to_csv(out_csv, index=False, float_format="%.6f")
    sm = summary(ev)
    sm.to_csv(f"cases/{series}/work/{series}_recursive_summary{tag}.csv", index=False, float_format="%.6f")
    print(f"sanity: origin 12/2019 fcst_h1 = {ev.fcst_h1.iloc[0]:.4f}  "
          f"(must equal the generate_forecast 1-step value)")
    print(f"\nCSV per-origin : {out_csv}")
    print(f"CSV summary    : cases/{series}/work/{series}_recursive_summary.csv\n")
    print(sm.to_string(index=False, float_format=lambda x: f"{x:.4f}"))
