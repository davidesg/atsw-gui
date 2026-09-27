#!/usr/bin/env python3
"""Compact per-case tables (source | rank | lambda-max | trace | beta | alpha | logL |
converged) from result.json + scan.json -> RESULTS_summary.md.

drvec rows: (a) `-lrtest` exactly as the program reports it; (b) the fit at the
published rank from the BEST route found by scan.py (cold / -seedjoh /
-multistart 30 / profiled at Johansen's beta); (c) q=1 default MA.
"""
import json
import os
import re

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TC = {1: "yes (tc1 gradient)", 2: "yes (tc2 steptol)", 3: "NO (tc3)", 4: "NO (tc4 iter limit)",
      5: "NO (tc5)"}

PUBLISHED = {
    "denmark": "JJ1990 Danish: r=1 chosen; lambda=(.4332,.1776,.1128,.0434); trace (49.14, 19.06, "
               "8.69, 2.35); beta=(m2 1, y -1.03, ib 5.21, id -4.22, const -6.06); "
               "alpha=(-.213, .115, .023, .029) [values from memory of JJ1990 Tables 3-4 / urca "
               "docs; paper not in literature/ -- reproduced EXACTLY by ca.jo row 1]",
    "finland": "JJ1990 Finnish: paper not in repo; benchmark README records r=2 (from a generic "
               "ca.jo K=2 const, not the paper's spec)",
    "UKconinc": "no published Johansen spec for these data (HEGY is a seasonal-cointegration "
                "paper); benchmark README asserts r=1",
    "UKconsumption": "Pokorny (1987) p.408: data source only; the r=2 in the README comes from "
                     "ca.jo (levels)",
    "Canada": "Pfaff (2008)/vars: ecdet='trend', K=3: trace (84.92, 36.42, 18.72, 3.85) -> r=1 "
              "[README]; reproduced EXACTLY by ca.jo row 1",
    "UKpppuip": "JJ1992: r=2; lambda=(.407,.285,.254,.102,.083) [from memory of JJ1992 Table 1; "
                "reproduced by ca.jo row 1 with season=4 + dumvar]",
}


def f(x, d=3):
    return "-" if x is None else "%.*f" % (d, x)


def vec(v, d=3):
    return "(" + ", ".join(f(x, d) for x in v) + ")"


def cols(B, d=3):
    B = np.asarray(B, float)
    return "; ".join(vec(B[:, j], d) for j in range(B.shape[1]))


def seq_rank(stats, cv5, full):
    for i, (s, c) in enumerate(zip(stats, cv5)):
        if s <= c:
            return str(i)
    return str(len(stats)) if full else "%d+" % len(stats)


def jnorm(cj, r):
    M = cj["M"]
    V = np.asarray(cj["V"], float)
    W = np.asarray(cj["W"], float)
    Bk = V[M - r:M, :r]
    Bi = np.linalg.inv(Bk)
    return V[:M, :r] @ Bi, W[:, :r] @ Bk.T, (V[M, :r] @ Bi if V.shape[0] > M else None)


def parse_out(path, M, r):
    txt = open(path, errors="ignore").read()

    def mat(h):
        i = txt.find(h)
        if i < 0:
            return None
        rows = []
        for l in txt[i:].split("\n")[1:]:
            try:
                rows.append([float(x) for x in l.split()])
            except ValueError:
                break
            if len(rows) == M:
                break
        return np.asarray(rows)
    ew = None
    i3 = txt.find("E[nablaY2] and E[W] vector:")
    i2 = txt.find("\nE[W] vector:")
    if i3 >= 0 or i2 >= 0:
        start, nread = (i3, M) if i3 >= 0 else (i2 + 1, r)
        vals = []
        for l in txt[start:].split("\n")[1:1 + nread]:
            vals.append(float(l.split()[0]))
        ew = vals[-r:]
    m = re.search(r"logelf\s*:\s*(-?[\d.]+)", txt)
    tcs = []
    for mm in re.finditer(r"Convergence criterion: ([^\n]+)", txt):
        c = mm.group(1)
        tcs.append(1 if "gradient" in c else 2 if "steptol" in c else 3 if "lower point" in c
                   else 4 if "iteration" in c else 5)
    bs = re.search(r"best is start (\d+)", txt)
    note = re.search(r"^Convergence\s+:\s*([^\n]+)", txt, re.M)
    return {"alpha": mat("alpha matrix (M x r)"), "beta": mat("beta matrix (M x r)"), "EW": ew,
            "logL": float(m.group(1)) if m else None, "tcs": tcs,
            "best_start": int(bs.group(1)) if bs else None,
            "note": note.group(1).strip() if note else None,
            "ma": re.search(r"MA \(Theta\)([^\n]*)", txt).group(1).split()
            if re.search(r"MA \(Theta\)([^\n]*)", txt) else None}


def section(name):
    cdir = os.path.join(HERE, "cases", name)
    res = json.load(open(os.path.join(cdir, "result.json")))
    sc = json.load(open(os.path.join(cdir, "scan.json")))
    M, r = res["M"], res["r_pub"]
    L = ["### %s — %s" % (name, res["desc"]),
         "M=%d, n=%d, K=%d -> drvec p=%d, case %d; columns [Y2;Y1]=%s%s. Published/assumed rank r=%d." % (
             M, res["n"], res["K"], res["p"], res["drcase"], " ".join(res["order"]),
             "; **drvec data pre-adjusted** (seasonals%s removed from levels, not jointly estimated)"
             % ("+" + "+".join(res["dumvar"]) if res.get("dumvar") else "") if res["preadjusted"] else "",
             r), ""]
    base = name.split("_")[0]
    if name in PUBLISHED or base in PUBLISHED:
        L.append("Published: " + PUBLISHED.get(name, PUBLISHED.get(base)) + "\n")
    L.append("| source | rank 5%% (trace / lmax) | lmax r=0..%d | trace r=0..%d | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |" % (M - 1, M - 1))
    L.append("|---|---|---|---|---|---|---|---|---|")

    def jrow(label, cj):
        if "error" in cj:
            L.append("| %s | ca.jo failed | | | | | | | |" % label)
            return
        cvt = [x[1] for x in cj["cv_trace"]]
        cve = [x[1] for x in cj["cv_eigen"]]
        b, a, c = jnorm(cj, r)
        L.append("| %s | %s / %s | %s | %s | %s | %s | %s | %s cond. | closed form |" % (
            label, seq_rank(cj["trace"], cvt, True), seq_rank(cj["eigen"], cve, True),
            vec(cj["eigen"], 2), vec(cj["trace"], 2), cols(b), cols(a),
            vec(-c) if c is not None else "-", f(cj["logL"][r])))
    cp = res["cajo_pub"]
    jrow("ca.jo, published spec (ecdet=%s%s%s)" % (
        cp.get("ecdet", "?"), ", season=4" if res["season"] else "",
        ", dumvar" if res.get("dumvar") else ""), cp)
    jrow("ca.jo, drvec's data & case (ecdet=%s)" % res["ecdet"], res["cajo_same"])
    v = res["sm"]["vecm"]
    L.append("| statsmodels VECM('%s'), drvec's data | - | - | - | %s | %s | %s | %s cond. | closed form |" % (
        v["det"], cols(v["beta"]), cols(v["alpha"]),
        vec([-x for x in v["const_coint"]]) if v.get("const_coint") else "-", f(v["llf"])))
    # drvec -lrtest as reported
    cs = res["cajo_same"]
    cve = [x[1] for x in cs["cv_eigen"]]
    ll = {x["r"]: x["logL"] for x in res["dr_lr"]["ranks"]}
    seq = [2 * (ll[i + 1] - ll[i]) for i in range(M - 1)]
    tcs = [b["termcode"] for b in res["dr_lr"]["banners"]]
    L.append("| drvec q=0 `-lrtest` (as reported) | - / %s | %s | partial %s | | | | %s | %s |" % (
        seq_rank(seq, cve, False), vec(seq, 2), vec([sum(seq[i:]) for i in range(M - 1)], 2),
        vec([ll[i] for i in range(M)], 2),
        ", ".join("r%d %s" % (i, "ok" if t in (1, 2) else "tc%d" % t) for i, t in enumerate(tcs))))
    # best-found sequence
    best = {0: sc["r0_lrtest"]}
    route = {0: "lrtest"}
    for rr in range(1, M):
        row = sc["ranks"][str(rr)]
        best[rr] = row["best"][1]
        route[rr] = row["best"][0]
    bseq = [2 * (best[i + 1] - best[i]) for i in range(M - 1)]
    L.append("| drvec q=0 best-found per rank | - / %s | %s | partial %s | | | | %s | routes: %s |" % (
        seq_rank(bseq, cve, False), vec(bseq, 2), vec([sum(bseq[i:]) for i in range(M - 1)], 2),
        vec([best[i] for i in range(M)], 2), ", ".join("r%d %s" % (i, route[i]) for i in range(M))))
    # fit at published rank, best route
    rt = route[r]
    tagmap = {"cold": "cold", "seedjoh": "seedjoh", "ms30": "multistart_30"}
    if rt in tagmap:
        d = parse_out(os.path.join(cdir, "%s.scan_r%d_%s.out" % (name, r, tagmap[rt])), M, r)
        b, a = d["beta"], d["alpha"]
    else:
        tag = "fixb2_0" if rt == "johfix" else "fixb2_0_multistart_20"
        d = parse_out(os.path.join(cdir, "%s_johfix_r%d.scan_r%d_%s.out" % (name, r, r, tag)), M, r)
        b = np.vstack([np.asarray(sc["ranks"][str(r)]["B2_J"]), np.eye(r)])
        a = d["alpha"]
    tcd = d["tcs"][-1] if d["tcs"] and d["best_start"] is None else None
    conv = TC.get(tcd, "?") if tcd else ("multistart best start %s: %s" % (
        d["best_start"], "NO (tc3)" if "NOT a convergence" in (d["note"] or "") else
        "steptol" if "steptol" in (d["note"] or "") else "yes (gradient)"))
    cold = sc["ranks"][str(r)]["cold"]
    L.append("| drvec q=0 at r=%d, best route = %s | (imposed) | | | %s | %s | %s | %s exact | %s%s |" % (
        r, rt, cols(b), cols(a), vec(d["EW"]) if d["EW"] else "-", f(d["logL"]), conv,
        "" if rt == "cold" else "; cold/-lrtest fit: %s %s" % (
            f(cold["logL"]), "tc%d" % cold["termcodes"][-1] if cold["termcodes"] else "")))
    q1 = res["dr_q1"]
    if q1.get("beta"):
        d1 = parse_out(os.path.join(cdir, "%s.p%d_q1_r%d.out" % (name, res["p"], r)), M, r)
        L.append("| drvec q=1 (default MA class) at r=%d | (imposed) | | | %s | %s | %s | %s exact | %s; min MA root %s |" % (
            r, cols(d1["beta"]), cols(d1["alpha"]), vec(d1["EW"]) if d1.get("EW") else "-",
            f(d1["logL"]), TC.get(d1["tcs"][-1], "?") if d1["tcs"] else "?",
            f(q1.get("ma_min_root"), 4)))
    else:
        L.append("| drvec q=1 | failed rc=%s | | | | | | | |" % q1.get("rc"))
    L.append("")
    return "\n".join(L)


def main():
    order = ["denmark", "denmark5", "finland", "finland_const", "UKconinc", "UKconinc_seas",
             "UKconsumption", "UKconsumption_lev", "Canada", "UKpppuip"]
    out = ["# Compact tables — generated by final.py\n"]
    for n in order:
        if os.path.exists(os.path.join(HERE, "cases", n, "scan.json")):
            out.append(section(n))
    open(os.path.join(HERE, "RESULTS_summary.md"), "w").write("\n".join(out))
    print("\n".join(out))


if __name__ == "__main__":
    main()
