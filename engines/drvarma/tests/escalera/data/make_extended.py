"""Build the extended .pre fixtures for the ladder forecast tests.

The univariate models of the bench (cases/IPC_*/work/*.pre) were estimated on
2002-01..2019-12 (216 obs). fue's fixed-parameter recursive forecasts of those
models (cases/IPC_*/work/*_recursive_eval*.csv) use the real data up to
2023-11 from data/IPC.xlsx. To reproduce them with `drvarma -estwin 216`, the
.pre needs the same data: this copies each .pre and replaces only the number
of observations and the data block. The model is untouched.

    python3 tests/escalera/data/make_extended.py      (from engines/drvarma)
"""
import re
import pandas as pd

XLSX = "data/IPC.xlsx"
CASES = [("IPC_ES", "cases/IPC_ES/work/IPC_ES_m10.pre"),
         ("IPC_FR", "cases/IPC_FR/work/IPC_FR_msar.pre"),
         ("IPC_DE", "cases/IPC_DE/work/IPC_DE_mar3sar.pre")]

data = pd.read_excel(XLSX, sheet_name="Sheet1")
for name, src in CASES:
    lines = open(src).read().split("\n")
    vals = data[name].to_numpy(float)
    i_hdr = next(i for i, l in enumerate(lines) if l.startswith("** Time series"))
    old = [float(l) for l in lines[i_hdr + 1:] if l.strip()]
    assert all(abs(a - b) < 1e-6 for a, b in zip(old, vals)), name + ": base data differ"
    i_nobs = next(i for i, l in enumerate(lines) if re.match(r"\s*216\s+1\s+2002\s", l))
    lines[i_nobs] = re.sub(r"216", str(len(vals)), lines[i_nobs], count=1)
    out = lines[:i_hdr + 1] + [f"{v:.10f} " for v in vals] + [""]
    dst = f"tests/escalera/data/{name}_ext.pre"
    open(dst, "w").write("\n".join(out))
    print(dst, len(vals))
