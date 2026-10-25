#!/usr/bin/env python3
"""tools/mc_crt.py -- the Monte Carlo of Cavaliere, Rahbek and Taylor (2012,
Econometrica 80, Section 4), run through drvec's rank test and its bootstrap.

The DGP is theirs, equation (8): a 4-dimensional VAR(2),

    dX_t = a b' X_{t-1} + G dX_{t-1} + e_t,   e_t ~ iid N(0, I_4),
    X_0 = dX_0 = 0,
    b = (1, 0, 0, 0)',  a = (alpha, 0, 0, 0)',
    G = [[g, d, 0, 0], [d, g, 0, 0], [0, 0, g, 0], [0, 0, 0, g]],

with alpha = -0.4, g = 0.8 and d = delta for their panel (a) (r0 = 1), and
alpha = 0, G = g I_4 for panel (b) (r0 = 0).  They fit a restricted constant,
which is drvec's case 2 (E[W] free, no drift in nabla Y2).

Each replication writes one .inp, runs

    drvec <inp> 2 0 0 -case 2 -lrtest -bootstrap B

and reads off every comparison r -> r+1: the LR, the asymptotic 5 % value,
the bootstrap's 5 % value, its p-value and the usable replications, plus a
line drvec prints when the model under H0 fails the I(1, r) check.  One CSV
row per (replication, comparison), appended as each replication ends, so a
run that is stopped keeps what it did and a rerun skips what is there.

Column order.  drvec normalises B = [I_r ; B2] on the LAST r columns, and
the true relation loads on X1 alone, so X1 goes last: (X2, X3, X4, X1).  At
r = 2 the normalisation is on (X4, X1), which is generic.

Usage:
    tools/mc_crt.py run  --panel a --delta 0 --T 50 --reps 200 --B 199 \
                         --out mc.csv [--jobs 3] [--seed 1] [--drvec bin/drvec]
    tools/mc_crt.py summary mc.csv
"""
import argparse
import csv
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DRVEC = os.path.join(HERE, "..", "bin", "drvec")
FIELDS = ["panel", "delta", "gamma", "T", "B", "rep", "seed", "h0", "lr",
          "asym5", "boot5", "pboot", "reps", "i1r", "check", "seconds"]


def dgp(panel, delta, gamma, T, rng, alpha=-0.4):
    """T + 1 rows of levels: X_0 = 0 and X_1 .. X_T (dX_0 = 0)."""
    G = gamma * np.eye(4)
    if panel == "a":
        G[0, 1] = G[1, 0] = delta
        a = np.array([alpha, 0.0, 0.0, 0.0])
    else:
        a = np.zeros(4)
    b = np.array([1.0, 0.0, 0.0, 0.0])
    X = np.zeros((T + 1, 4))
    dx_prev = np.zeros(4)
    for t in range(1, T + 1):
        dx = a * (b @ X[t - 1]) + G @ dx_prev + rng.standard_normal(4)
        X[t] = X[t - 1] + dx
        dx_prev = dx
    return X[:, [1, 2, 3, 0]]


def write_inp(path, X, note):
    n = X.shape[0]
    with open(path, "w") as f:
        f.write(f"* {note}\n1\n4 {n} 1 1900\nx2 x3 x4 x1\n1.0 0 0\n")
        for row in X:
            f.write(" ".join(f"{v:.10f}" for v in row) + "\n")


# One row of the bootstrap table:   r  M-r  LR  10%  5%  1%  p-value  reps
BOOT = re.compile(r"^\s*(\d)\s+(\d)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+"
                  r"(-?[\d.]+)\s+([\d.]+)\s+(\d+)")


def parse_out(text):
    """{h0: {...}} from the .out: the asymptotic table, then the bootstrap's."""
    res = {}
    lines = text.splitlines()
    try:
        i0 = next(i for i, l in enumerate(lines)
                  if "bootstrap under h0" in l.lower())
    except StopIteration:
        i0 = len(lines)
    # The asymptotic sequence: "r  M-r  LR  10%  5%  1%  verdict  [check: ...]"
    for l in lines[:i0]:
        m = re.match(r"^\s*(\d)\s+(\d)\s+(-?[\d.]+)\s+(-?[\d.]+|-)\s+"
                     r"(-?[\d.]+|-)\s+(-?[\d.]+|-)\s+(reject|H0|-)", l)
        if m:
            h0 = int(m.group(1))
            d = res.setdefault(h0, {})
            d["lr"] = float(m.group(3))
            d["asym5"] = (float(m.group(5)) if m.group(5) != "-" else "")
            d["check"] = "stopped" if "[check:" in l else ""
    for l in lines[i0:]:
        m = BOOT.match(l)
        if m:
            h0 = int(m.group(1))
            d = res.setdefault(h0, {})
            d["lr"] = float(m.group(3))
            d["boot5"] = float(m.group(5))
            d["pboot"] = float(m.group(7))
            d["reps"] = int(m.group(8))
        m = re.match(r"^\s*(\d)\s.*not I\(1, ?\d\)", l)
        if m:
            res.setdefault(int(m.group(1)), {})["i1r"] = "fail"
    return res


def one(args):
    panel, delta, gamma, T, B, rep, seed, drvec = args
    import time
    rng = np.random.default_rng(seed)
    X = dgp(panel, delta, gamma, T, rng)
    with tempfile.TemporaryDirectory(prefix="mccrt") as d:
        base = os.path.join(d, "s")
        write_inp(base + ".inp", X, f"CRT panel {panel} d={delta} T={T} seed={seed}")
        t0 = time.time()
        subprocess.run([drvec, base, "2", "0", "0", "-case", "2", "-lrtest",
                        "-bootstrap", str(B)], cwd=d, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=7200)
        secs = time.time() - t0
        try:
            text = open(base + ".out").read()
        except OSError:
            text = ""
    res = parse_out(text)
    rows = []
    for h0 in sorted(res):
        r = res[h0]
        rows.append({"panel": panel, "delta": delta, "gamma": gamma, "T": T,
                     "B": B, "rep": rep, "seed": seed, "h0": h0,
                     "lr": r.get("lr", ""), "asym5": r.get("asym5", ""),
                     "boot5": r.get("boot5", ""), "pboot": r.get("pboot", ""),
                     "reps": r.get("reps", ""), "i1r": r.get("i1r", "ok"),
                     "check": r.get("check", ""),
                     "seconds": round(secs, 1)})
    if not rows:
        rows.append({k: "" for k in FIELDS} | {"panel": panel, "delta": delta,
                    "gamma": gamma, "T": T, "B": B, "rep": rep, "seed": seed,
                    "h0": -1, "seconds": round(secs, 1)})
    return rows


def cmd_run(a):
    done = set()
    if os.path.exists(a.out):
        with open(a.out) as f:
            for r in csv.DictReader(f):
                done.add(int(r["rep"]))
    new = not os.path.exists(a.out)
    todo = [(a.panel, a.delta, a.gamma, a.T, a.B, rep,
             a.seed * 100000 + rep, os.path.abspath(a.drvec))
            for rep in range(1, a.reps + 1) if rep not in done]
    print(f"{len(todo)} replications to run ({len(done)} already in {a.out})",
          flush=True)
    with open(a.out, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        with ProcessPoolExecutor(max_workers=a.jobs) as ex:
            futs = [ex.submit(one, t) for t in todo]
            for k, fu in enumerate(as_completed(futs), 1):
                for row in fu.result():
                    w.writerow(row)
                f.flush()
                print(f"  {k}/{len(todo)}", flush=True)


def selected(rows, key):
    """The sequential rank: the first h0 not rejected at 5 %."""
    for r in sorted(rows, key=lambda r: int(r["h0"])):
        lr, cv = r["lr"], r[key]
        if lr == "" or cv == "":
            return None
        if float(lr) <= float(cv):
            return int(r["h0"])
    return 3          # every comparison rejected: r = M - 1 is the most drvec says


def cmd_summary(a):
    by = {}
    with open(a.csv) as f:
        for r in csv.DictReader(f):
            by.setdefault(int(r["rep"]), []).append(r)
    reps = sorted(by)
    n = len(reps)
    first = by[reps[0]][0]
    print(f"panel {first['panel']}, delta {first['delta']}, gamma {first['gamma']}, "
          f"T {first['T']}, B {first['B']}: {n} replications")
    for h0 in (0, 1, 2):
        rows = [r for rep in reps for r in by[rep] if int(r["h0"]) == h0
                and r["lr"] != ""]
        ra = [float(r["lr"]) > float(r["asym5"]) for r in rows if r["asym5"] != ""]
        rb = [float(r["lr"]) > float(r["boot5"]) for r in rows if r["boot5"] != ""]
        fails = sum(r["i1r"] == "fail" for r in rows)
        def pct(v):
            if not v:
                return "   -  "
            p = np.mean(v)
            return f"{100*p:5.1f} ±{100*np.sqrt(p*(1-p)/len(v)):4.1f}"
        print(f"  H0 r = {h0}: rejects at 5 %  asymptotic {pct(ra)}  "
              f"bootstrap {pct(rb)}   (n {len(rb)}, I(1,r) check failed {fails})")
    for key, name in (("asym5", "asymptotic"), ("boot5", "bootstrap")):
        sel = [selected(by[rep], key) for rep in reps]
        sel = [s for s in sel if s is not None]
        if sel:
            c = [100 * np.mean([s == k for s in sel]) for k in (0, 1, 2, 3)]
            print(f"  sequential, {name:10s}: r = 0 {c[0]:5.1f}   1 {c[1]:5.1f}   "
                  f"2 {c[2]:5.1f}   3 {c[3]:5.1f}   (n {len(sel)})")
    secs = [float(by[rep][0]["seconds"]) for rep in reps if by[rep][0]["seconds"]]
    if secs:
        print(f"  {np.mean(secs):.0f} s a replication")


def main():
    p = argparse.ArgumentParser()
    sp = p.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("run")
    r.add_argument("--panel", choices=("a", "b"), default="a")
    r.add_argument("--delta", type=float, default=0.0)
    r.add_argument("--gamma", type=float, default=0.8)
    r.add_argument("--T", type=int, required=True)
    r.add_argument("--reps", type=int, required=True)
    r.add_argument("--B", type=int, default=199)
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--jobs", type=int, default=3)
    r.add_argument("--out", required=True)
    r.add_argument("--drvec", default=DEFAULT_DRVEC)
    s = sp.add_parser("summary")
    s.add_argument("csv")
    a = p.parse_args()
    {"run": cmd_run, "summary": cmd_summary}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
