#!/usr/bin/env python3
"""Build RESULTS.md from cases/*/result.json (written by bench.py)."""
import glob
import json
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TC = {1: "gradient (tc1)", 2: "steptol (tc2)", 3: "NO lower point (tc3)",
      4: "iter limit (tc4)", 5: "max steps (tc5)"}


def f(x, d=3):
    return "-" if x is None else ("%.*f" % (d, x))


def vec(v, d=3):
    return "(" + ", ".join(f(x, d) for x in v) + ")"


def mat_cols(B, d=3):
    B = np.asarray(B)
    return "; ".join(vec(B[:, j], d) for j in range(B.shape[1]))


def seq_rank(stats, cv5):
    for i, (s, c) in enumerate(zip(stats, cv5)):
        if s <= c:
            return i
    return len(stats)


def lam_from_lr(ranks):
    ll = [x["logL"] for x in sorted(ranks, key=lambda x: x["r"])]
    return [2 * (ll[i + 1] - ll[i]) for i in range(len(ll) - 1)]


def johansen_norm(cj, M, r):
    V = np.asarray(cj["V"], float)
    W = np.asarray(cj["W"], float)
    Vr = V[:, :r]
    Bk = Vr[M - r:M, :]
    Binv = np.linalg.inv(Bk)
    beta = Vr[:M] @ Binv
    alpha = W[:, :r] @ Bk.T
    const = (Vr[M] @ Binv) if V.shape[0] > M else None
    return beta, alpha, const


def section(res):
    M, r, name = res["M"], res["r_pub"], res["name"]
    L = []
    L.append("## %s\n" % name)
    L.append("*%s*  \nM=%d, n=%d, K=%d (drvec p=%d), drvec case %d, columns [Y2;Y1] = %s%s\n"
             % (res["desc"], M, res["n"], res["K"], res["p"], res["drcase"],
                " ".join(res["order"]),
                "  \n**drvec input PRE-ADJUSTED** (seasonals%s partialled out of the levels "
                "beforehand; not jointly estimated)" % (
                    "/dumvar " + ",".join(res["dumvar"]) if res.get("dumvar") else "")
                if res["preadjusted"] else ""))
    L.append("| source | rank (5%%) | lambda-max r=0..%d | trace r=0..%d | beta (norm. on Y1) | alpha | E[W]=-const | logL | converged |"
             % (M - 1, M - 1))
    L.append("|---|---|---|---|---|---|---|---|---|")

    def jrow(label, cj, rr=r):
        if not cj or "error" in cj:
            L.append("| %s | ca.jo FAILED | | | | | | | |" % label)
            return
        cvt = [row[1] for row in cj["cv_trace"]]
        cve = [row[1] for row in cj["cv_eigen"]]
        rt, re_ = seq_rank(cj["trace"], cvt), seq_rank(cj["eigen"], cve)
        b, a, c = johansen_norm(cj, cj["M"], rr)
        L.append("| %s | trace r=%d, lmax r=%d | %s | %s | %s | %s | %s | %s (cond.) | closed form |"
                 % (label, rt, re_, vec(cj["eigen"], 2), vec(cj["trace"], 2),
                    mat_cols(b), mat_cols(a), vec(-c) if c is not None else "-",
                    f(cj["logL"][rr], 3)))
    cp = res["cajo_pub"]
    jrow("ca.jo PUBLISHED spec (ecdet=%s, season=%s%s)" % (
        cp.get("ecdet", "?") if "error" not in cp else "?", res["season"],
        ", dumvar" if res.get("dumvar") else ""), cp)
    jrow("ca.jo same data as drvec (ecdet=%s)" % res["ecdet"], res["cajo_same"])
    if res.get("cajo_same_pubecdet"):
        jrow("ca.jo same data, ecdet=trend", res["cajo_same_pubecdet"])
    sm = res.get("sm", {})
    if "vecm" in sm:
        v = sm["vecm"]
        cc = v.get("const_coint")
        L.append("| statsmodels VECM(%s) same data | - | - | - | %s | %s | %s | %s (cond.) | closed form |"
                 % (v["det"], mat_cols(v["beta"]), mat_cols(v["alpha"]),
                    vec([-x for x in cc]) if cc else "-", f(v["llf"])))
    # drvec
    lr = res["dr_lr"]
    ranks = sorted(lr.get("ranks", []), key=lambda x: x["r"])
    lam = lam_from_lr(ranks) if len(ranks) == M else []
    cs = res["cajo_same"]
    cve = [row[1] for row in cs["cv_eigen"]] if "cv_eigen" in cs else []
    rk = seq_rank(lam, cve) if lam else None
    rk_s = ("%d%s" % (rk, "+ (r=M not testable)" if rk == M - 1 else "")) if rk is not None else "?"
    part = [sum(lam[i:]) for i in range(len(lam))]
    conv = ", ".join("r=%d:%s" % (i, TC[b["termcode"]]) for i, b in enumerate(lr["banners"]))
    L.append("| drvec q=0 -lrtest | lmax r=%s | %s (+ r=%d->M not expressible) | partial %s | | | | %s | %s |"
             % (rk_s, vec(lam, 2), M - 1, vec(part, 2),
                vec([x["logL"] for x in ranks], 3), conv))
    for key, label in (("dr_q0", "drvec q=0 cold"), ("dr_q0_seedjoh", "drvec q=0 -seedjoh"),
                       ("dr_q0_ms", "drvec q=0 -multistart 20"), ("dr_q1", "drvec q=1 (default MA)")):
        d = res[key]
        if d.get("rc") not in (0,) or d.get("beta") is None:
            L.append("| %s | rc=%s | | | | | | | FAILED %s |" % (label, d.get("rc"), d.get("stderr_tail", "")[-120:].replace("\n", " ")))
            continue
        b = np.asarray(d["beta"])
        a = np.asarray(d["alpha"])
        tcs = [bb["termcode"] for bb in d["banners"]]
        if key == "dr_q0_ms":
            conv = "starts: " + ",".join(str(t) for t in tcs)
        else:
            conv = TC[tcs[-1]] if tcs else "?"
        extra = ""
        if d.get("ma_min_root") is not None:
            extra = "; MA min root %.4f" % d["ma_min_root"]
        if d.get("norm_share"):
            extra += "; Y1 share " + "/".join("%.1f%%" % s for s in d["norm_share"])
        L.append("| %s | (r=%d imposed) | | | %s | %s | %s | %s (exact) | %s%s |"
                 % (label, r, mat_cols(b), mat_cols(a), vec(d["EW"]) if d.get("EW") else "-",
                    f(d["logL"], 4), conv, extra))
    # statsmodels coint_johansen, all det orders (to document the mapping)
    if "coint_johansen_det0" in sm:
        L.append("\nstatsmodels `coint_johansen` on the drvec data (it has no restricted-constant option):")
        for dkey in ("coint_johansen_det-1", "coint_johansen_det0", "coint_johansen_det1"):
            s = sm[dkey]
            L.append("- %s: trace %s, lmax %s" % (dkey, vec(s["trace"], 2), vec(s["eigen"], 2)))
    L.append("")
    return "\n".join(L)


def main():
    out = ["# drvec external validation, JJ block — generated by report.py\n"]
    order = ["denmark", "denmark5", "finland", "finland_const", "UKconinc", "UKconinc_seas",
             "UKconsumption", "UKconsumption_lev", "Canada", "UKpppuip"]
    for n in order:
        p = os.path.join(HERE, "cases", n, "result.json")
        if os.path.exists(p):
            out.append(section(json.load(open(p))))
    open(os.path.join(HERE, "RESULTS_tables.md"), "w").write("\n".join(out))
    print("\n".join(out))


if __name__ == "__main__":
    main()
