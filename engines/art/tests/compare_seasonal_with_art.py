#!/usr/bin/env python3
"""compare_seasonal_with_art.py — the C's top model against art-python's on
the seasonal battery of benchmark_seasonal.py (the same series to both).

art: suggest_orders(ts, d=1, D=0, lam=0) — harmonics removed from w, as the C
now does. Reports, per case, how often each one is exact and how often the C's
top-1 (p,q)(P,Q) equals art's.

Use:  python3 tests/compare_seasonal_with_art.py [REPS]
"""
import os
import sys
import tempfile
import warnings

import numpy as np

warnings.filterwarnings("ignore")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.expanduser(os.environ.get(
    "ART_PYTHON_SRC", "~/Dropbox/SRC/ART/art-python/src")))
import benchmark_seasonal as B  # noqa: E402
import fue  # noqa: E402
import art  # noqa: E402

reps = int(sys.argv[1]) if len(sys.argv) > 1 else 20
cli = os.path.join(B.ROOT, "bin", "art_cli")
rng = np.random.default_rng(7)
print(f"reps={reps}")
print(f"{'case':<24}{'C exact':>9}{'art exact':>10}{'same top-1':>11}")
tot = []
with tempfile.TemporaryDirectory() as d:
    for case in B.CASES:
        truth = case[-1]
        ce = ae = same = 0
        for _ in range(reps):
            y = B.sim(case, 216, rng)
            _, c = B.run(cli, y, d, False)
            ts = fue.TimeSeries(list(y), freq=12, start=(2000, 1), name="S")
            sp = art.suggest_orders(ts, d=1, D=0, lam=0.0, top_n=5)
            a = (sp[0].p, sp[0].q, sp[0].P, sp[0].Q) if sp else None
            ce += c == truth
            ae += a == truth
            same += c == a
        r = [100.0 * v / reps for v in (ce, ae, same)]
        tot.append(r)
        print(f"{case[0]:<24}{r[0]:8.0f}%{r[1]:9.0f}%{r[2]:10.0f}%", flush=True)
m = np.mean(tot, axis=0)
print(f"{'MEAN':<24}{m[0]:8.0f}%{m[1]:9.0f}%{m[2]:10.0f}%")
