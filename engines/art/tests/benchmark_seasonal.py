#!/usr/bin/env python3
"""benchmark_seasonal.py — the C's SEASONAL path, through files (-i).

benchmark_c.py simulates inside the C, and that mode skips the seasonal
detection and the deseasonalization. Here monthly series (n = 216) are written
to a file and identified with `-S 12 -d 1 --log`, as a user does:

    log y_t = 4.6 + sum(0.002 + 0.01 w) + 0.01 * A * P_t

with w the regular x seasonal ARMA below and P_t a deterministic monthly
pattern (sd of its difference = sd of w times A; A = 0 means no pattern).
The "true" orders are those of w: the deterministic pattern is not an ARMA
order, art removes it before identifying.

Per case: how often seasonality is detected, and the exact rate of the
regular (p, q), the seasonal (P, Q) and the whole (p, q)(P, Q).

Use:  python3 tests/benchmark_seasonal.py [REPS] [--default] [--cli PATH] [--json OUT]
      --default: the classic mode instead of --mlp-direct (about 1.6 s/series)
"""
import json
import os
import re
import subprocess
import sys
import tempfile

import numpy as np
from scipy.signal import lfilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

#  name, phi, theta, Phi, Theta (s = 12, Box-Jenkins signs), pattern A, true (p,q,P,Q)
CASES = [
    ("WN + pattern",            [],    [],    [],    [],    1.0, (0, 0, 0, 0)),
    ("AR(1) .6 + pattern",      [0.6], [],    [],    [],    1.0, (1, 0, 0, 0)),
    ("MA(1) .5 + pattern",      [],    [0.5], [],    [],    1.0, (0, 1, 0, 0)),
    ("AR(1) .6",                [0.6], [],    [],    [],    0.0, (1, 0, 0, 0)),
    ("MA(1) .5",                [],    [0.5], [],    [],    0.0, (0, 1, 0, 0)),
    ("WN + SAR(1) .6",          [],    [],    [0.6], [],    0.0, (0, 0, 1, 0)),
    ("AR(1) .5 x SAR(1) .5",    [0.5], [],    [0.5], [],    0.0, (1, 0, 1, 0)),
    ("MA(1) .5 x SMA(1) .5",    [],    [0.5], [],    [0.5], 0.0, (0, 1, 0, 1)),
]
CSV = re.compile(r"^1,")


def poly(c, step=1):
    a = np.zeros(len(c) * step + 1)
    a[0] = 1.0
    for i, v in enumerate(c, 1):
        a[i * step] = -v
    return a


def sim(case, n, rng):
    _, phi, th, Phi, Th, A, _ = case
    ar = np.convolve(poly(phi), poly(Phi, 12))
    ma = np.convolve(poly(th), poly(Th, 12))
    w = lfilter(ma, ar, rng.standard_normal(n + 400))[400:]
    t = np.arange(n)
    P = np.cos(2 * np.pi * t / 12 + 0.7) + 0.5 * np.sin(4 * np.pi * t / 12 + 0.3)
    dP = np.diff(P)
    P = P / dP.std() * w.std()
    return np.exp(4.6 + np.cumsum(0.002 + 0.01 * w) + 0.01 * A * P)


def run(cli, y, d, default):
    path = os.path.join(d, "s.txt")
    out = os.path.join(d, "o.csv")
    np.savetxt(path, y)
    args = [cli, "-i", path, "-S", "12", "-d", "1", "--log", "--output", out]
    if not default:
        args.append("--mlp-direct")
    res = subprocess.run(args, capture_output=True, text=True).stdout
    det = "DETECTADA (" in res and "NO DETECTADA" not in res
    row = open(out).read().splitlines()[1].split(",")
    return det, tuple(int(v) for v in row[5:9])


def main():
    argv = sys.argv[1:]
    default = "--default" in argv
    cli = os.path.join(ROOT, "bin", "art_cli")
    out = None
    if "--cli" in argv:
        cli = argv[argv.index("--cli") + 1]
    if "--json" in argv:
        out = argv[argv.index("--json") + 1]
    nums = [a for a in argv if a.isdigit()]
    reps = int(nums[0]) if nums else (30 if default else 100)
    rng = np.random.default_rng(2026)
    print(f"{cli}  reps={reps}  mode={'default' if default else 'mlp-direct'}")
    print(f"{'case':<24}{'detect':>8}{'(p,q)':>8}{'(P,Q)':>8}{'all':>7}")
    rows = {}
    with tempfile.TemporaryDirectory() as d:
        for case in CASES:
            det = reg = sea = full = 0
            tp, tq, tP, tQ = case[-1]
            for _ in range(reps):
                dd, (p, q, P, Q) = run(cli, sim(case, 216, rng), d, default)
                det += dd
                reg += (p, q) == (tp, tq)
                sea += (P, Q) == (tP, tQ)
                full += (p, q, P, Q) == (tp, tq, tP, tQ)
            r = [100.0 * v / reps for v in (det, reg, sea, full)]
            rows[case[0]] = r
            print(f"{case[0]:<24}{r[0]:7.0f}%{r[1]:7.0f}%{r[2]:7.0f}%{r[3]:6.0f}%", flush=True)
    m = np.mean(list(rows.values()), axis=0)
    print(f"{'MEAN':<24}{m[0]:7.0f}%{m[1]:7.0f}%{m[2]:7.0f}%{m[3]:6.0f}%")
    if out:
        json.dump({"reps": reps, "default": default, "rows": rows,
                   "mean": list(map(float, m))}, open(out, "w"), indent=1)


if __name__ == "__main__":
    main()
