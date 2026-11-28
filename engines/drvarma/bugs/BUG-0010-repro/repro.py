#!/usr/bin/env python3
"""BUG-0010 (drvarma): -deseason estimates and removes the seasonal pattern in
LEVELS, before the Box-Cox log, so a multiplicative pattern on a trending
series is adjusted at the wrong scale.

The order was d=1 -> deseasonalization -> log: the dummies were estimated on
the differences of the raw levels and added back to the level forecasts as
fixed amounts. A multiplicative pattern is additive only in logs; in levels
its amplitude grows with the series, and one dummy per month cannot follow
it. The port (drvarma-python BUG-0003) and art do log -> d=1 ->
deseasonalization.

The case: MULT.inp, synthetic, 216 months, a known multiplicative pattern
(+-3 %) on a trend that grows 1 % a month (from 97 to 852), with a little
noise. `MULT 1 0 -mean -deseason force -forecast 12`: the monthly rates of
the re-seasonalized level forecast, 100 log(L_t / L_t-1), less their mean,
must be the pattern's monthly changes. In logs they are (error ~0.03 points);
in levels the dummies are the average month's, applied at a level eight times
the first one, and the forecast pattern comes out shrunk.

Exits 1 while the largest error passes 15 % of the pattern's amplitude, 0
when not, 2 if the case does not run.
"""
import math, pathlib, re, subprocess, sys, tempfile, shutil

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"missing {BIN}: make -C engines/drvarma")

# the pattern MULT.inp was built with (log scale, centred), in %
PAT = [-3.0, -2.0, 0.5, 1.5, 2.0, 1.0, -0.5, 0.0, 1.0, 0.5, -0.5, -0.5]
m = sum(PAT) / 12
PAT = [p - m for p in PAT]

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    shutil.copy(AQUI / "MULT.inp", d)
    subprocess.run([str(BIN), "MULT", "1", "0", "-mean", "-deseason", "force",
                    "-forecast", "12"], cwd=d, capture_output=True, text=True,
                   errors="replace", timeout=600)
    f = d / "MULT.forecast"
    txt = f.read_text(errors="replace") if f.exists() else ""

rows = re.findall(r"^\s*(\d+)/(\d{4})\s+(-?[\d.]+)", txt, re.M)
if len(rows) < 12:
    print("the case does not run: forecasts", len(rows))
    sys.exit(2)
lev = [(int(mo), float(v)) for mo, _, v in rows[:12]]
rates = [(lev[k][0], 100 * math.log(lev[k][1] / lev[k - 1][1])) for k in range(1, 12)]
mean = sum(r for _, r in rates) / len(rates)
want = [(mo, PAT[mo - 1] - PAT[mo - 2]) for mo, _ in rates]
wmean = sum(w for _, w in want) / len(want)
amp = max(w for _, w in want) - min(w for _, w in want)
err = 0.0
for (mo, r), (_, w) in zip(rates, want):
    e = (r - mean) - (w - wmean)
    err = max(err, abs(e))
    print(f"  month {mo:2d}  forecast {r - mean:+8.4f}   pattern {w - wmean:+8.4f}   error {e:+8.4f}")
print(f"pattern amplitude {amp:.4f}; largest error {err:.4f} ({100 * err / amp:.0f} %)")
if err > 0.15 * amp:
    print("OPEN: the seasonal pattern is removed and put back in levels, not in logs")
    sys.exit(1)
print("closed: the multiplicative pattern comes back on the log scale")
sys.exit(0)
