#!/usr/bin/env python3
"""regression_identify.py — the refactoring (18.2.1) must not move a result.

A fixed set of series goes through `bin/art_cli` with files (-i), the path a
user takes, and every result that matters is extracted: the seasonal HAC F
and its verdict, ADF and KPSS, the effective orders, the whole shortlist
(orders, similarity, Akaike weight) and the identified model. The default
(classic) mode, slow, runs on a subset.

    python3 tests/regression_identify.py --save   # write tests/regression_golden.json
    python3 tests/regression_identify.py          # compare against it

Exit 1 on the first difference, printing it.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CLI = os.path.join(ROOT, "bin", "art_cli")
GOLDEN = os.path.join(HERE, "regression_golden.json")
sys.path.insert(0, HERE)
import benchmark_seasonal as BS  # noqa: E402
from scipy.signal import lfilter  # noqa: E402

NONSEAS = [([0.6], []), ([0.5, 0.3], []), ([1.0, -0.5], []), ([0.8, -0.64], []),
           ([0.5, -0.3, 0.2], []), ([], [0.6]), ([], [-0.5]), ([], [0.5, 0.3]),
           ([0.5], [0.4]), ([0.5, -0.3], [0.4]), ([0.95], []), ([], [])]

KEYS = [
    ("hac", re.compile(r"^HAC F=\S+ \(p=\S+\)")),
    ("verdict", re.compile(r"^Resultado detección estacional: .*")),
    ("adf", re.compile(r"^ADF: .*")),
    ("kpss", re.compile(r"^KPSS: .*")),
    ("eff", re.compile(r"^Órdenes efectivos determinados: .*")),
    ("shortlist", re.compile(r"^MLP shortlist.*")),
    ("ident", re.compile(r"^MLP-direct identification: .*")),
]


def series():
    out = []
    for name, s, d, log in (("GY", 4, 1, 1), ("PS", 12, 1, 1), ("wti", 12, 1, 1)):
        out.append((f"real:{name}", np.loadtxt(os.path.join(ROOT, "data", name + ".txt")),
                    s, d, log, True))
    rng = np.random.default_rng(1802)
    for i, (phi, th) in enumerate(NONSEAS):
        for r in range(5):
            x = lfilter(np.r_[1, -np.array(th, float)], np.r_[1, -np.array(phi, float)],
                        rng.standard_normal(500))[300:]
            out.append((f"ns{i}.{r}", x, 1, 0, 0, r == 0 and i % 3 == 0))
    for i, case in enumerate(BS.CASES):
        for r in range(5):
            out.append((f"se{i}.{r}", BS.sim(case, 216, rng), 12, 1, 1, r == 0 and i % 2 == 0))
    return out


def run(x, s, d, log, mlp, tmp):
    path = os.path.join(tmp, "x.txt")
    csv = os.path.join(tmp, "o.csv")
    np.savetxt(path, x, fmt="%.10g")
    args = [CLI, "-i", path, "-S", str(s), "-d", str(d), "--output", csv]
    if s == 1:
        args += ["--pmax", "5", "--qmax", "5"]
    if log:
        args.append("--log")
    if mlp:
        args.append("--mlp-direct")
    so = subprocess.run(args, capture_output=True, text=True).stdout
    rec = {}
    for line in so.splitlines():
        line = line.strip()
        for k, rx in KEYS:
            m = rx.match(line)
            if m:
                rec.setdefault(k, m.group(0))
    row = open(csv).read().splitlines()[1].split(",")
    rec["det"] = ",".join(row[5:10])
    return rec


def collect():
    res = {}
    with tempfile.TemporaryDirectory() as tmp:
        for name, x, s, d, log, classic in series():
            res[name + "|mlp"] = run(x, s, d, log, True, tmp)
            if classic:
                res[name + "|classic"] = run(x, s, d, log, False, tmp)
    return res


def main():
    res = collect()
    if "--save" in sys.argv:
        json.dump(res, open(GOLDEN, "w"), indent=0, ensure_ascii=False)
        print(f"saved {len(res)} runs to {GOLDEN}")
        return
    gold = json.load(open(GOLDEN))
    bad = 0
    for k in gold:
        for f in gold[k]:
            a, b = gold[k][f], res.get(k, {}).get(f)
            if a != b:
                bad += 1
                if bad <= 10:
                    print(f"DIFF {k} {f}\n  golden: {a}\n  now:    {b}")
    for k in res:
        if k not in gold:
            print(f"NEW {k}")
    print(f"{len(gold)} runs, {bad} differences")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
