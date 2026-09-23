#!/usr/bin/env python3
"""Parse drvec .out files and compare them with johansen_<case>.json.
Writes results.md.  Re-run after run_drvec.sh and johansen_ref.py."""
import json, os, re, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); os.chdir(HERE)
RANK = dict(e1=1, e3=1, rao1=2, rao2=3, rao3=3, rao4=2, rao5=2, rao6=5, rao7=2, rao7_sc=2, rao3_3v=1, rao6_sc=5)

def banners(txt):
    out = []
    for m in re.finditer(r"OPTIMIZER (CONVERGED|STOPPED|FAILED)[^\n]*after (\d+) iterations.*?Convergence criterion: ([^\n]*)", txt, re.S):
        st, it, crit = m.group(1), int(m.group(2)), m.group(3).strip()
        tag = "grad" if "gradient" in crit else ("step" if "step tolerance" in crit or "steptol" in crit
               else ("tc3" if "last global step" in crit else crit[:30]))
        out.append("%s/%d" % (tag, it))
    return out

def matrix_after(txt, head, nrows):
    i = txt.find(head)
    if i < 0: return None
    lines = txt[i:].split("\n")[1:1 + nrows]
    try: return np.array([[float(v) for v in l.split()] for l in lines])
    except ValueError: return None

def parse_fit(stem, M):
    p = stem + ".out"
    if not os.path.exists(p): return None
    t = open(p, errors="ignore").read()
    g = lambda pat: (re.search(pat, t).group(1) if re.search(pat, t) else None)
    d = dict(stem=stem, banner=banners(t), conv=g(r"Convergence\s*:\s*([^\n]*)"),
             logL=g(r"logelf\s*:\s*([-\d.]+)"), npar=g(r"npar\s*:\s*(\d+)"),
             exit=None)
    d["alpha"] = matrix_after(t, "alpha matrix (M x r)", M)
    d["beta"] = matrix_after(t, "beta matrix (M x r)", M)
    d["Pi"] = matrix_after(t, "Pi = alpha beta' matrix", M)
    ew = re.search(r"E\[W\] vector:\n((?:\s+[-\d.]+\n)+)", t)
    d["EW"] = [float(x) for x in ew.group(1).split()] if ew else None
    ma = re.search(r"MA \(Theta\)([^\n]*)", t); d["mamin"] = None
    if ma:
        v = [float(x.rstrip("*")) for x in ma.group(1).split() if x.rstrip("*") not in ("inf",)]
        d["mamin"] = min(v) if v else None
    gs = re.search(r"sigma_min[^\n]*?=\s*([-\d.e+]+)", t); d["G"] = gs.group(1) if gs else None
    return d

def parse_lr(stem):
    p = stem + ".out"
    if not os.path.exists(p): return None
    t = open(p, errors="ignore").read()
    a = t.find("npar        logL"); b = t.find("M-r        LR")
    rows = re.findall(r"^\s+(\d+)\s+(\d+)\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s*$", t[a:b], re.M)
    lr = re.findall(r"^\s+(\d+)\s+(\d+)\s+([-\d.]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+(.*)$", t[b:], re.M)
    return dict(banner=banners(t), ll={int(r[0]): float(r[2]) for r in rows},
                npar={int(r[0]): int(r[1]) for r in rows},
                lr=[(int(a_), float(c), g.strip()) for a_, b_, c, d_, e, f, g in lr])

def seq_rank(stats, cv5):
    for i, (s, c) in enumerate(zip(stats, cv5)):
        if s <= c: return i
    return len(stats)

def fmt(v, k=4):
    return "—" if v is None else ("[" + ", ".join(("%.*f" % (k, x)) for x in v) + "]")

def main():
    md = ["# drvec external validation — Lütkepohl e1/e3 and Rao tables 1–7\n",
          "Spec everywhere: K=2 (ca.jo) = 1 lagged difference = drvec `p=2`; ecdet='const' (restricted constant) = drvec `-case 2`; q=0 for the rank.\n",
          "Johansen = independent numpy RRR (identical to ca.jo and statsmodels VECM(ci) to 1e-9, see johansen_ref.py). drvec LR is the λ-max form 2[L(r+1)−L(r)] and stops at r=M−1.\n"]
    for c, R in RANK.items():
        jf = "johansen_%s.json" % c
        if not os.path.exists(jf): continue
        J = json.load(open(jf)); M = J["M"]
        md.append("\n## %s  (M=%d, n=%d, reference rank r=%d; Y1 = %s)\n" % (c, M, J["n"], R, ", ".join(J["names"][M - R:])))
        L = parse_lr(c + "_lr")
        md.append("### Rank\n\n| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=%d) | drvec optimizer |\n|---|---|---|---|---|---|" % J["T"])
        for r in range(M):
            jl = J["lmax"][r]
            dl = next((x[1] for x in (L or {}).get("lr", []) if x[0] == r), None)
            ll = (L or {}).get("ll", {}).get(r)
            bn = (L or {}).get("banner", [])
            md.append("| %d | %.3f | %s | %s | %.3f | %s |" % (r, jl, "—" if dl is None else "%.3f" % dl,
                      "—" if ll is None else "%.3f" % ll, J["llf"][r], bn[r] if r < len(bn) else "—"))
        md.append("| %d | %.3f | (not expressible) | — | %.3f | — |" % (M, 0, J["llf"][M]) if False else "")
        md.append("\nJohansen trace (= ca.jo): %s\n" % fmt(J["trace"], 3))
        if L and L["lr"]:
            md.append("drvec verdicts: " + "; ".join("r=%d: %s" % (a, g) for a, _, g in L["lr"]) + "\n")
        md.append("### Fit at r=%d (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)\n" % R)
        md.append("| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |\n|---|---|---|---|---|---|---|---|")
        Jb, Ja, JP = np.array(J["beta"]), np.array(J["alpha"]).reshape(M, R), np.array(J["Pi"])
        md.append("| Johansen RRR | %.3f (cond.) | — | closed form | 0 | 0 | 0 | %s |" % (J["llf"][R], fmt([-x for x in J["beta_const"]])))
        for suf, lab in (("", "drvec q=0"), ("_ms", "drvec q=0 multistart 20"), ("_sj", "drvec q=0 -seedjoh"), ("q1", "drvec q=1 (default MA, marow)")):
            stem = "%s_q1r%d" % (c, R) if suf == "q1" else "%s_q0r%d%s" % (c, R, suf)
            d = parse_fit(stem, M)
            if not d: continue
            db = None if d["beta"] is None else np.abs(d["beta"] - Jb).max()
            da = None if d["alpha"] is None else np.abs(d["alpha"] - Ja).max()
            dP = None if d["Pi"] is None else np.abs(d["Pi"] - JP).max()
            f = lambda x: "—" if x is None else "%.4g" % x
            md.append("| %s | %s | %s | %s | %s | %s | %s | %s |" % (lab, d["logL"], d["npar"], ",".join(d["banner"]) + " — " + (d["conv"] or "?"),
                      f(db), f(da), f(dP), fmt(d["EW"])))
        md.append("\nJohansen β' (rows = relations):\n```\n%s\n```\nJohansen α (M×r):\n```\n%s\n```" % (np.array2string(Jb.T, precision=4, suppress_small=True), np.array2string(Ja, precision=4, suppress_small=True)))
        d = parse_fit("%s_q0r%d" % (c, R), M)
        if d and d["beta"] is not None:
            md.append("drvec q=0 β':\n```\n%s\n```\ndrvec q=0 α:\n```\n%s\n```" % (np.array2string(d["beta"].T, precision=4, suppress_small=True), np.array2string(d["alpha"], precision=4, suppress_small=True)))
    open("results_auto.md", "w").write("\n".join(md) + "\n")
    print("\n".join(md))

main()
