#!/usr/bin/env python3
"""tools/mc_trend4.py -- the size of the rank test with -trend (case 4), by
Monte Carlo, through drvec's rank test and its bootstrap.

The DGP is tools/sim/sim_trend4.py's: M = 3, true rank 1, a trend d in the
equilibrium error W = Y1 - Y2a - 0.5 Y2b, the Y2 block drifting (0.2, 0.1),
Lam = (0.30, 0.10, 0), p = 1, Sigma = I.  Each replication writes one .inp
and runs

    drvec <inp> 1 0 0 -case 3 -trend -lrtest -bootstrap B      (--spec trend)
    drvec <inp> 1 0 0 -case 3 -lrtest -bootstrap B             (--spec case3)

The comparison r = 1 -> 2 is the test of the true rank: its rejection rate
at 5 % is the size.  Under case 4 its H0 carries W's slope, which the
bootstrap must simulate (simulate_h0's dtr).  --spec case3 is the same
data fitted without the trend, the misspecification -trend removes.  The
CSV and the summary are mc_crt.py's (one row per replication and
comparison, resumable).

Usage:
    tools/mc_trend4.py run --T 150 --reps 100 --B 99 --out mc.csv \\
                           [--spec trend] [--d 0.05] [--jobs 3] [--seed 1]
    tools/mc_trend4.py summary mc.csv
"""
import argparse
import csv
import os
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor, as_completed

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mc_crt import parse_out                                  # noqa: E402

DEFAULT_DRVEC = os.path.join(HERE, "..", "bin", "drvec")
FIELDS = ["spec", "d", "T", "B", "rep", "seed", "h0", "lr", "boot5", "pboot",
          "reps", "i1r", "check", "seconds"]
SPECS = {"trend": ["-case", "3", "-trend"], "case3": ["-case", "3"]}


def dgp(T, d, rng, burn=200):
    """T + 1 rows of levels, columns (Y2a, Y2b, Y1): sim_trend4.py's model."""
    B2, g, muw = np.array([-1.0, -0.5]), np.array([0.2, 0.1]), 1.0
    lam = np.array([0.30, 0.10, 0.0])
    c = np.concatenate(([d - B2 @ g], g))
    N = T + burn + 1
    tc = 0.5 * (N + 1)
    Y = np.zeros((N, 3))
    Y[0, 0] = muw - d * (1 - tc)
    for t in range(1, N):
        z = Y[t - 1, 0] + B2 @ Y[t - 1, 1:] - muw - d * (t - 1 - tc)
        Y[t] = Y[t - 1] - lam * z + c + rng.standard_normal(3)
    return Y[burn:][:, [1, 2, 0]]


def one(args):
    spec, d, T, B, rep, seed, drvec = args
    X = dgp(T, d, np.random.default_rng(seed))
    with tempfile.TemporaryDirectory(prefix="mctr4") as tmp:
        base = os.path.join(tmp, "s")
        with open(base + ".inp", "w") as f:
            f.write(f"* trend4 d={d} T={T} seed={seed}\n1\n3 {len(X)} 1 1900\n"
                    "y2a y2b y1\n1.0 0 0\n")
            for row in X:
                f.write(" ".join(f"{v:.10f}" for v in row) + "\n")
        t0 = time.time()
        subprocess.run([drvec, base, "1", "0", "0"] + SPECS[spec]
                       + ["-lrtest", "-bootstrap", str(B)], cwd=tmp,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=7200)
        secs = round(time.time() - t0, 1)
        try:
            text = open(base + ".out").read()
        except OSError:
            text = ""
    res = parse_out(text)
    base_row = {"spec": spec, "d": d, "T": T, "B": B, "rep": rep, "seed": seed,
                "seconds": secs}
    rows = [base_row | {"h0": h0, "lr": r.get("lr", ""), "boot5": r.get("boot5", ""),
                        "pboot": r.get("pboot", ""), "reps": r.get("reps", ""),
                        "i1r": r.get("i1r", "ok"), "check": r.get("check", "")}
            for h0, r in sorted(res.items())]
    return rows or [{k: "" for k in FIELDS} | base_row | {"h0": -1}]


def cmd_run(a):
    done = set()
    new = not os.path.exists(a.out) or os.path.getsize(a.out) == 0
    if not new:
        with open(a.out) as f:
            done = {int(r["rep"]) for r in csv.DictReader(f)}
    todo = [(a.spec, a.d, a.T, a.B, rep, a.seed * 100000 + rep, os.path.abspath(a.drvec))
            for rep in range(1, a.reps + 1) if rep not in done]
    print(f"{len(todo)} replications to run ({len(done)} already in {a.out})", flush=True)
    with open(a.out, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        with ProcessPoolExecutor(max_workers=a.jobs) as ex:
            for k, fu in enumerate(as_completed([ex.submit(one, t) for t in todo]), 1):
                for row in fu.result():
                    w.writerow(row)
                f.flush()
                print(f"  {k}/{len(todo)}", flush=True)


def cmd_summary(a):
    with open(a.csv) as f:
        rows = list(csv.DictReader(f))
    reps = {r["rep"] for r in rows}
    r0 = rows[0]
    print(f"spec {r0['spec']}, d {r0['d']}, T {r0['T']}, B {r0['B']}: "
          f"{len(reps)} replications")
    for h0, what in ((0, "power (true rank 1)"), (1, "SIZE (the true rank)")):
        ps = [float(r["pboot"]) for r in rows if r["h0"] == str(h0) and r["pboot"] != ""]
        if not ps:
            continue
        n = len(ps)
        for lev in (0.10, 0.05, 0.01):
            k = sum(p <= lev for p in ps)
            se = (lev * (1 - lev) / n) ** 0.5
            print(f"  r = {h0} -> {h0 + 1}, {what}: rejects at {lev:.0%} in "
                  f"{k}/{n} = {k / n:.3f}  (nominal {lev:.2f}, s.e. {se:.3f})")
    fails = sum(r["i1r"] == "fail" for r in rows)
    if fails:
        print(f"  H0 models that failed the I(1, r) check: {fails}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--spec", choices=sorted(SPECS), default="trend")
    r.add_argument("--d", type=float, default=0.05)
    r.add_argument("--T", type=int, default=150)
    r.add_argument("--reps", type=int, default=100)
    r.add_argument("--B", type=int, default=99)
    r.add_argument("--out", required=True)
    r.add_argument("--jobs", type=int, default=3)
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--drvec", default=DEFAULT_DRVEC)
    s = sub.add_parser("summary")
    s.add_argument("csv")
    a = ap.parse_args()
    (cmd_run if a.cmd == "run" else cmd_summary)(a)


if __name__ == "__main__":
    main()
