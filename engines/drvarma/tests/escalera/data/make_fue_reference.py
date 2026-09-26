"""fue's fixed-parameter recursive forecasts: the reference for the ladder.

For each univariate model of the bench, as cases/recursive_eval.py does: the
.fuf.inp holds the model estimated on 2002-01..2019-12; to move the origin,
the real observations up to it are appended to its data block and fue
forecasts with the SAME parameters (load_fuf + forecast_fuf). Every origin
from 12/2019 to the end of the data, every horizon 1..24.

The CSVs in cases/*/work/*_recursive_eval*.csv came from an older fue and do
NOT agree with today's (at origin 12/2019, h=1, IPC_ES: 97.1389 there against
97.0785 in fue today and in the ladder). This is the current reference.

    python3 tests/escalera/data/make_fue_reference.py   (from engines/drvarma)
"""
import os, re, tempfile
import numpy as np
import pandas as pd
import fue

XLSX, N_TRAIN, H = "data/IPC.xlsx", 216, 24
MODELS = [("IPC_ES", "cases/IPC_ES/work/IPC_ES_m10.fuf.inp"),
          ("IPC_FR", "cases/IPC_FR/work/IPC_FR_msar.fuf.inp"),
          ("IPC_DE", "cases/IPC_DE/work/IPC_DE_mar3sar.fuf.inp")]

data = pd.read_excel(XLSX, sheet_name="Sheet1")
tmp = tempfile.mkdtemp()
rows = []
for name, path in MODELS:
    lines = open(path).read().split("\n")
    i_hdr = next(i for i, l in enumerate(lines) if l.startswith("** Time series"))
    i_nobs = next(i for i, l in enumerate(lines) if re.match(r"\s*216\s+1\s+2002\s", l))
    vals = data[name].to_numpy(float)
    for n in range(N_TRAIN, len(vals) + 1):
        h = list(lines[:i_hdr + 1])
        h[i_nobs] = re.sub(r"216", str(n), h[i_nobs], count=1)
        p = os.path.join(tmp, "o.inp")
        open(p, "w").write("\n".join(h) + "\n" + "".join(f"{v:.10f} \n" for v in vals[:n]))
        _, m = fue.load_fuf(p)
        lvl = np.asarray(m.forecast_fuf().level, float)
        mon = (n - 1) % 12 + 1; yr = 2002 + (n - 1) // 12
        for k in range(H):
            rows.append((f"{mon}/{yr}", name, k + 1, lvl[k]))
out = "tests/escalera/data/fue_recursive_reference.csv"
with open(out, "w") as f:
    f.write("# fue %s, load_fuf + forecast_fuf, fixed parameters\n" % getattr(fue, "__version__", "?"))
    f.write("origin,series,horizon,level\n")
    for r in rows:
        f.write("%s,%s,%d,%.6f\n" % r)
print(out, len(rows))
