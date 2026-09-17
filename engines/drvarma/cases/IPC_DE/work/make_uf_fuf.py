"""
Build the iterative uf model (mar3sar + ifadf[1,2] + MA_f[1,2]), write its fuf,
and FIX the fixed-frequency section format (fue writer/reader are inconsistent:
writer emits 'count / ** / freq phi2 flag', reader wants 'count freq1 freq2 / ** / coef flag').
Verify the round-trip matches the in-memory forecast.
"""
import re
import numpy as np
import fue

# --- build & fit (iterative from mar3sar) ---
ts, mb = fue.load("cases/IPC_DE/work/IPC_DE_mar3sar.pre")
itvs = [i for i in mb.interventions
        if not (i.type in ("cos", "sin") and getattr(i, "harmonic", 0.0) in (1.0, 2.0))]
m = fue.Model(
    ts, d=1, D=0, boxlam=0.0,
    ar=[[0.0, 0.0, 0.0]], ar_free=[[True, True, True]],
    ar_s=[[0.0]], ar_s_free=[[True]],
    ma=[], ma_s=[],
    ma_f=[fue.FixedFreqFactor(1.0, -0.5, True), fue.FixedFreqFactor(2.0, -0.5, True)],
    interventions=itvs, ifadf=[0, 1, 1, 0, 0, 0, 0],
    mu=0.0, estimate_mu=True, refactor=100.0,
)
m.fit()
lvl_mem = np.asarray(m.forecast(24).level, float)

raw = "cases/IPC_DE/work/_raw_uf.fuf.inp"
m.write_fuf(horizon=24, path=raw)


def fix_ffixed_section(lines, header_substr):
    """Rewrite a fixed-frequency section from writer-format to reader-format."""
    out = list(lines)
    hi = next(i for i, l in enumerate(out) if header_substr in l)
    count = int(out[hi + 1].split()[0])
    if count == 0:
        return out
    # writer blocks after count line: '**' then 'freq phi2 flag', repeated count times
    freqs, coefs = [], []
    j = hi + 2
    while len(freqs) < count and j < len(out):
        if out[j].strip() == "**":
            j += 1
            continue
        toks = out[j].split()
        if len(toks) >= 3:
            freqs.append(toks[0]); coefs.append((toks[1], toks[2]))
        j += 1
    # rebuild: count + freqs ; then per factor: ** / coef flag
    new = [out[hi], f"{count} " + " ".join(freqs)]
    for c, fl in coefs:
        new += ["**", f"{c} {fl}"]
    return out[:hi] + new + out[j:]


with open(raw) as fh:
    lines = fh.read().split("\n")
lines = fix_ffixed_section(lines, "regular AR(2) operators with fixed frequency")
lines = fix_ffixed_section(lines, "regular MA(2) operators with fixed frequency")
fixed = "cases/IPC_DE/work/IPC_DE_uf.fuf.inp"
with open(fixed, "w") as fh:
    fh.write("\n".join(lines))

# --- verify round-trip ---
ts2, m2 = fue.load_fuf(fixed)
print("round-trip ma_f:", [(ff.freq, round(ff.coef, 4)) for ff in m2.ma_f], " ifadf:", m2.ifadf)
fr = np.asarray(m2.forecast_fuf().level, float)
print("\n h   in-memory      fuf-roundtrip    diff")
for h in (1, 12, 24):
    print(f"{h:2d}  {lvl_mem[h-1]:12.5f}  {fr[h-1]:12.5f}  {lvl_mem[h-1]-fr[h-1]:+.2e}")
print("max abs diff:", float(np.max(np.abs(lvl_mem - fr))))
print("fuf written:", fixed)
