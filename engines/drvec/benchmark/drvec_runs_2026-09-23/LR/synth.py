#!/usr/bin/env python3
"""Rank synthesis: per case and rank, logL from every drvec route, the independent
exact-ML maximum (exact_ml.py), and the Johansen conditional llf; LRs from the best
drvec fit vs Johansen lambda-max.  Writes results_ranks.md."""
import json, os, sys
sys.argv = [sys.argv[0]]
HERE = os.path.dirname(os.path.abspath(__file__)); os.chdir(HERE)
exec(open("compare.py").read().split("def main():")[0])     # reuse parsers
CASES = dict(e1=1, e3=1, rao1=2, rao2=3, rao3=3, rao4=2, rao5=2, rao7=2, rao7_sc=2, rao3_3v=1, rao6_sc=5)
CV5 = {1: 9.24, 2: 15.67, 3: 22.00, 4: 28.14, 5: 34.40, 6: 40.30, 7: 46.45, 8: 52.00}  # lambda-max, case 2 (Osterwald-Lenum)
md = ["# Rank synthesis (q=0, case 2, p=2)\n",
      "logL columns: `def` = plain `-lrtest`; `sj` = `-lrtest -seedjoh`; `fit` = best standalone fit at the reference rank (default / multistart 20 / seedjoh); `exact` = independent Kalman exact ML (exact_ml.py, BFGS from Johansen's point, and from drvec's where available). Tags: grad = gradient convergence, tc3 = termcode 3 stop, step = steptol stop.\n"]
for c, R in CASES.items():
    if not os.path.exists("johansen_%s.json" % c): continue
    J = json.load(open("johansen_%s.json" % c)); M = J["M"]
    D, S = parse_lr(c + "_lr"), parse_lr(c + "_lrsj")
    md.append("\n## %s (M=%d, reference r=%d)\n\n| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |\n|---|---|---|---|---|---|---|" % (c, M, R))
    best = {}; bestk = {}
    for r in range(M):
        cells = []
        vals = []
        for X in (D, S):
            if X and r in X["ll"]:
                b = X["banner"][r] if r < len(X["banner"]) else "?"
                cells.append("%.3f %s" % (X["ll"][r], b.split("/")[0])); vals.append(X["ll"][r])
            else: cells.append("—")
        fitv = None
        if r == R:
            for suf in ("", "_ms", "_sj", "_sj_ms"):
                d = parse_fit("%s_q0r%d%s" % (c, R, suf), M)
                if d and d["logL"]:
                    v = float(d["logL"]); fitv = v if fitv is None else max(fitv, v)
            if fitv is not None: vals.append(fitv)
        ex = None
        if os.path.exists("exact_%s_r%d.json" % (c, r)):
            E = json.load(open("exact_%s_r%d.json" % (c, r))); ex = E.get("best")
        bd = max(vals) if vals else None; best[r] = bd
        bk = max([v for v in (bd, ex) if v is not None], default=None); bestk[r] = bk
        md.append("| %d | %.3f | %s | %s | %s | %s | %s |" % (r, J["llf"][r], cells[0], cells[1],
                  "—" if fitv is None else "%.3f" % fitv, "—" if ex is None else "%.3f" % ex,
                  "—" if (ex is None or bd is None) else "%+.3f" % (bd - ex)))
    md.append("\n| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |\n|---|---|---|---|---|---|---|---|")
    kr = None
    jr = dr = None
    for r in range(M - 1):
        lm = J["lmax"][r]; cv = CV5.get(M - r)
        lr = 2 * (best[r + 1] - best[r]) if best.get(r) is not None and best.get(r + 1) is not None else None
        jv = "reject" if lm > cv else "accept"; dv = "—" if lr is None else ("reject" if lr > cv else "accept")
        if jr is None and jv == "accept": jr = r
        if dr is None and dv == "accept": dr = r
        lk = 2 * (bestk[r + 1] - bestk[r]) if bestk.get(r) is not None and bestk.get(r + 1) is not None else None
        kv = "—" if lk is None else ("reject" if lk > cv else "accept")
        if kr is None and kv == "accept": kr = r
        md.append("| %d | %.3f | %s | %s | %.2f | %s | %s | %s |" % (r, lm, "—" if lr is None else "%.3f" % lr, "—" if lk is None else "%.3f" % lk, cv, jv, dv, kv))
    md.append("\nSequential λmax rank at 5%%: Johansen r=%s, drvec (best drvec fits) r=%s, best known exact max r=%s.\n" % (jr if jr is not None else "≥%d" % (M - 1), dr if dr is not None else "≥%d" % (M - 1), kr if kr is not None else "≥%d" % (M - 1)))
open("results_ranks.md", "w").write("\n".join(md) + "\n"); print("\n".join(md))
