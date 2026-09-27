#!/usr/bin/env python3
"""VEC against univariate, out of sample.  The criterion, not a likelihood.

Why this and not something else.  A multivariate model exists to say what a
univariate one cannot, and the empirical record of forecasting says that is hard
to achieve.  So the question `drvec` has to answer is not whether its likelihood
is higher -- it is, it nests the alternative -- but whether it FORECASTS better
than an ARIMA on each series.  Nothing in this project measured that until now.

The counterfactual is `drvec`'s own diagonal rung: with `r = 0` and
`-diagar -diagma -diagcov` the exact likelihood FACTORISES (Theorem 9 of
docs/DEMOSTRACIONES.md), so that fit IS an ARIMA(p-1,1,q) on each series
separately.  Using it rather than an outside program means both sides share the
sample, the estimator, the forecast recursion and the scoring, so what is left
between them is the cointegrated structure and nothing else.

Both sides are scored by `-estwin E -f H`: estimated once on 1..E, parameters
held fixed, origin rolled forward, each forecast compared with what happened.

Usage:  python3 tools/forecast_vs_univariate.py [p] [q] [H] [frac]
        frac is the share of the sample used for estimation (default 0.75).
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRVEC = os.path.join(ROOT, "bin", "drvec")
TMP = "/tmp/drvec_fc_cmp"

CASES = [("datasets/mauricio/mink_muskrat.inp", 1),
         ("data/pairs/milan.inp", 1), ("data/pairs/vienna.inp", 1),
         ("data/pairs/penn.inp", 1), ("data/pairs/utrecht.inp", 1),
         ("data/pairs/aix.inp", 1), ("data/pairs/arevalo.inp", 1),
         ("data/pairs/angers.inp", 1), ("data/pairs/strasbourg.inp", 1)]


def run(inp, args):
    os.makedirs(TMP, exist_ok=True)
    base = os.path.join(TMP, "case")
    open(base + ".inp", "wb").write(open(os.path.join(ROOT, inp), "rb").read())
    subprocess.run([DRVEC, base, *args], capture_output=True, timeout=3600)
    t = open(base + ".out", encoding="latin-1", errors="replace").read()
    n = re.search(r"\((\d+) of \d+ observations used\)", t)
    rows = re.findall(r"^ *(\d+) +(\S+) +([\-0-9.]+) +([\-0-9.]+) +([\-0-9.]+)$",
                      t.split("Rolling-origin")[-1], re.M) if "Rolling-origin" in t else []
    rmse = {}
    for h, ser, mae, rm, mape in rows:
        rmse.setdefault(int(h), []).append(float(rm))
    return rmse, (int(n.group(1)) if n else 0)


def main(p=2, q=1, H=4, frac=0.75, extra=()):
    print("VEC against its own diagonal rung (= an ARIMA per series, Theorem 9),")
    print(f"out of sample.  p = {p}, q = {q}, H = {H}, estimation window = "
          f"{frac:.0%} of the sample." + (f"  VEC class: {' '.join(extra)}" if extra else ""))
    print("RMSE averaged over the series.  ratio < 1 means the VEC forecasts better.\n")
    print(f"{'case':<16} {'n':>5} {'E':>5}" + "".join(f"{'h=%d' % h:>9}" for h in range(1, H+1)))
    wins = {h: 0 for h in range(1, H+1)}
    tot = {h: 0 for h in range(1, H+1)}
    for inp, r in CASES:
        if not os.path.exists(os.path.join(ROOT, inp)):
            continue
        _, n = run(inp, [str(p), str(q), "0", "-case", "2", "-mean"])
        if n < 40:
            continue
        E = int(frac * n)
        vec, _ = run(inp, [str(p), str(q), str(r), "-case", "2", "-mean",
                           *extra, "-estwin", str(E), "-f", str(H)])
        uni, _ = run(inp, [str(p), str(q), "0", "-case", "2", "-mean",
                           "-diagar", "-diagma", "-diagcov",
                           "-estwin", str(E), "-f", str(H)])
        if not vec or not uni:
            print(f"{os.path.basename(inp)[:-4]:<16} {n:5d} {E:5d}   (no evaluation)")
            continue
        cells = []
        for h in range(1, H + 1):
            if h in vec and h in uni and uni[h]:
                a = sum(vec[h]) / len(vec[h]); b = sum(uni[h]) / len(uni[h])
                cells.append(f"{a/b:9.3f}")
                tot[h] += 1
                wins[h] += 1 if a < b else 0
            else:
                cells.append(f"{'--':>9}")
        print(f"{os.path.basename(inp)[:-4]:<16} {n:5d} {E:5d}" + "".join(cells))
    print("\n" + f"{'VEC wins':<16} {'':>5} {'':>5}"
          + "".join(f"{('%d/%d' % (wins[h], tot[h])):>9}" for h in range(1, H+1)))


if __name__ == "__main__":
    if not os.access(DRVEC, os.X_OK):
        sys.exit(f"{DRVEC} not built; run make first")
    a = sys.argv[1:]
    main(int(a[0]) if a else 2, int(a[1]) if len(a) > 1 else 1,
         int(a[2]) if len(a) > 2 else 4, float(a[3]) if len(a) > 3 else 0.75,
         tuple(a[4:]))
