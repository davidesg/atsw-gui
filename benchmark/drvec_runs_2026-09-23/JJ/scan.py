#!/usr/bin/env python3
"""Rank-by-rank optimum audit of drvec q=0 (run AFTER bench.py).

    python3 scan.py [case ...]

For each rank r = 1..M-1 of each case, on the .inp bench.py wrote:
  cold         drvec p 0 r -case c
  seedjoh      ... -seedjoh
  ms30         ... -multistart 30
  johfix       the exact likelihood PROFILED AT JOHANSEN'S beta: the Y1 columns
               are replaced by W_J = Y1 + B2_J'Y2 (Johansen's beta, same
               deterministic case, same data, normalised on Y1) and drvec is run
               with -fixb2 0.  The map is unit-triangular (Jacobian 1), so this
               is logL_exact(B2 = B2_J, everything else maximised).  It is a
               LOWER BOUND for the free optimum: if it beats the free fits, the
               free fits stopped at a local optimum.
and r = 0 comes from the -lrtest run in result.json.  Writes cases/<c>/scan.json.
"""
import json
import os
import re
import subprocess
import sys

import numpy as np
from statsmodels.tsa.vector_ar.vecm import VECM

HERE = os.path.dirname(os.path.abspath(__file__))
DRVEC = "/home/david/Dropbox/SRC/drvec/bin/drvec"


def read_inp(path):
    L = [l for l in open(path).read().split("\n") if l and not l.startswith("*")]
    freq = int(L[0]); hdr = L[1].split(); names = L[2].split()
    M, n = int(hdr[0]), int(hdr[1])
    Y = np.array([[float(x) for x in l.split()] for l in L[4:4 + n]])
    return freq, hdr, names, Y


def write_inp(path, freq, hdr, names, Y, comment):
    with open(path, "w") as f:
        f.write("* %s\n%d\n%s\n%s\n1.0 0 0\n" % (comment, freq, " ".join(hdr), " ".join(names)))
        for row in Y:
            f.write(" ".join("%.12f" % v for v in row) + "\n")


def banners(txt):
    out = []
    for m in re.finditer(r"OPTIMIZER (?:CONVERGED|STOPPED) after \d+ iterations.*?"
                         r"Convergence criterion: ([^\n]+)", txt, re.S):
        c = m.group(1)
        out.append(1 if "gradient" in c else 2 if "steptol" in c else
                   3 if "lower point" in c else 4 if "iteration" in c else 5)
    return out


def run(stem, p, r, case, extra=()):
    cmd = [DRVEC, stem, str(p), "0", str(r), "-case", str(case), *extra]
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=7200)
    txt = open(stem + ".out", errors="ignore").read()
    tag = "r%d_%s" % (r, "_".join(e.strip("-") for e in extra) or "cold")
    open("%s.scan_%s.out" % (stem, tag), "w").write(txt)
    m = re.search(r"logelf\s*:\s*(-?[\d.]+)", txt)
    b = banners(txt)
    d = {"cmd": " ".join(cmd), "rc": pr.returncode,
         "logL": float(m.group(1)) if (m and pr.returncode == 0) else None,
         "termcodes": b}
    m = re.search(r"best is start (\d+)", txt)
    if m:
        d["best_start"] = int(m.group(1))
        m2 = re.search(r"Multi-start: (\d+) of (\d+) starting points converged; logL from "
                       r"(-?[\d.]+) to (-?[\d.]+)", txt)
        if m2:
            d["ms_summary"] = m2.group(0)
    if pr.returncode != 0:
        m = re.search(r"ESTIMATION FAILED[^\n]*\n[^\n]*", txt)
        d["fail"] = m.group(0) if m else pr.stderr[-200:]
    return d


def main():
    args = sys.argv[1:]
    only_joh = "--johfix-only" in args
    args = [a for a in args if a != "--johfix-only"]
    names = args or sorted(os.listdir(os.path.join(HERE, "cases")))
    for name in names:
        cdir = os.path.join(HERE, "cases", name)
        res = json.load(open(os.path.join(cdir, "result.json")))
        stem = os.path.join(cdir, name)
        freq, hdr, cols, Y = read_inp(stem + ".inp")
        M, p, case = res["M"], res["p"], res["drcase"]
        det = {2: "ci", 3: "co"}[case]
        if only_joh:
            out = json.load(open(os.path.join(cdir, "scan.json")))
        else:
          out = {"r0_lrtest": [x for x in res["dr_lr"]["ranks"] if x["r"] == 0][0]["logL"],
               "r0_tc": res["dr_lr"]["banners"][0]["termcode"], "ranks": {}}
        print("==", name, flush=True)
        for r in range(1, M):
            if only_joh:
                row = out["ranks"][str(r)]
            else:
                row = {"cold": run(stem, p, r, case),
                       "seedjoh": run(stem, p, r, case, ("-seedjoh",)),
                       "ms30": run(stem, p, r, case, ("-multistart", "30"))}
            # Johansen's beta on the same data / deterministic case
            v = VECM(Y, k_ar_diff=p - 1, coint_rank=r, deterministic=det).fit()
            b = np.asarray(v.beta)
            bn = b @ np.linalg.inv(b[M - r:M])
            s = M - r
            W = Y[:, s:] + Y[:, :s] @ bn[:s]
            # Y2 DEMEANED: likelihood-invariant in cases 2/3 (E[W] absorbs B2'c,
            # nabla Y2 unchanged), and it neutralises the -fixb2 seeding defect
            # (E[W], Lambda, F, Sigma are seeded from the static-OLS W, not from
            # the pinned B2; see RESULTS.md, defect D1).
            Yj = np.hstack([Y[:, :s] - Y[:, :s].mean(axis=0), W])
            jstem = os.path.join(cdir, "%s_johfix_r%d" % (name, r))
            write_inp(jstem + ".inp", freq, hdr, cols[:s] + ["W%d" % (j + 1) for j in range(r)],
                      Yj, "Y1 replaced by Johansen W = Y1 + B2_J'Y2 (scan.py)")
            row["johfix"] = run(jstem, p, r, case, ("-fixb2", "0"))
            row["johfix_ms"] = run(jstem, p, r, case, ("-fixb2", "0", "-multistart", "20"))
            row["B2_J"] = bn[:s].tolist()
            cands = [(k, row[k]["logL"]) for k in ("cold", "seedjoh", "ms30", "johfix", "johfix_ms")
                     if row[k]["logL"] is not None]
            row["best"] = max(cands, key=lambda x: x[1])
            out["ranks"][str(r)] = row
            print(r, {k: (row[k]["logL"], row[k]["termcodes"][-1:] if row[k]["termcodes"] else None)
                      for k in ("cold", "seedjoh", "ms30", "johfix", "johfix_ms")}, flush=True)
        json.dump(out, open(os.path.join(cdir, "scan.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
