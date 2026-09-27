#!/usr/bin/env python3
"""External validation of drvec against published Johansen cointegration studies.

    python3 bench.py [case ...]        (default: all cases)

For every case it
  1. builds the drvec .inp in cases/<case>/ following the published spec as far
     as drvec can express it (column order [Y2 ; Y1], last r columns = Y1 block);
  2. runs urca::ca.jo (johansen.R) under the PUBLISHED spec (seasonals / dumvar /
     ecdet as in the paper) and under the drvec-expressible spec on the very
     same data drvec sees;
  3. runs statsmodels coint_johansen + VECM on the drvec data;
  4. runs drvec q=0 -lrtest, q=0 at the published rank (cold, -seedjoh,
     -multistart 20) and q=1 (default MA class) at the published rank;
  5. writes cases/<case>/result.json, consumed by report.py.

Conventions (drvec docs): p = K (urca's K = levels VAR order; statsmodels
k_ar_diff = K-1; drvec carries p-1 lags of nabla Y).  drvec case 2 = restricted
constant (ca.jo ecdet="const", statsmodels "ci"); case 3 = unrestricted constant
(ca.jo ecdet="none", statsmodels "co").  drvec beta = [B2 ; I_r] with the identity
on the LAST r columns; alpha has Johansen's sign; E[W] = -(Johansen's restricted
constant) after the same normalisation.
"""
import json
import os
import re
import subprocess
import sys

import numpy as np
import pandas as pd
from statsmodels.tsa.vector_ar.vecm import VECM, coint_johansen

HERE = os.path.dirname(os.path.abspath(__file__))
DRVEC = "/home/david/Dropbox/SRC/drvec/bin/drvec"
DS = "/home/david/Dropbox/SRC/drvec/datasets"


# --------------------------------------------------------------- data prep --
def seasonal_dummies(n, s=4):
    """Centered seasonal dummies (s-1 columns), phase = index, like ca.jo."""
    D = np.zeros((n, s - 1))
    for t in range(n):
        q = t % s
        if q < s - 1:
            D[t, q] = 1.0
    return D - 1.0 / s


def preadjust(Y, season=0, X=None):
    """Remove deterministic seasonals and/or exogenous I(0) regressors from the
    LEVELS by regressing nabla Y on [1, D_t, X_t] and cumulating the fitted
    exogenous part (constant excluded).  An APPROXIMATION of partialling them
    out inside the VAR: drvec cannot hold them in the model."""
    n = len(Y)
    cols = []
    if season:
        cols.append(seasonal_dummies(n, season))
    if X is not None:
        cols.append(np.asarray(X, float))
    if not cols:
        return Y.copy()
    Z = np.hstack(cols)
    dY = np.diff(Y, axis=0)
    Zd = np.hstack([np.ones((n - 1, 1)), Z[1:]])
    g, *_ = np.linalg.lstsq(Zd, dY, rcond=None)
    e = Z[1:] @ g[1:]                         # exogenous contribution to dY_t
    S = np.vstack([np.zeros((1, Y.shape[1])), np.cumsum(e, axis=0)])
    S -= S.mean(axis=0)
    return Y - S


def write_inp(path, Y, names, freq, start_sub, start_year, comment):
    with open(path, "w") as f:
        for c in comment:
            f.write("* %s\n" % c)
        f.write("%d\n%d %d %d %d\n%s\n1.0 0 0\n"
                % (freq, Y.shape[1], len(Y), start_sub, start_year, " ".join(names)))
        for row in Y:
            f.write(" ".join("%.10f" % v for v in row) + "\n")


# ---------------------------------------------------------------- Johansen --
def cajo(tag, cdir, Y, names, K, ecdet, season=0, dumvar=None):
    csv = os.path.join(cdir, "R_%s.csv" % tag)
    pd.DataFrame(Y, columns=names).to_csv(csv, index=False)
    out = os.path.join(cdir, "R_%s.json" % tag)
    cmd = ["Rscript", os.path.join(HERE, "johansen.R"), csv, str(K), ecdet,
           str(season), out]
    if dumvar is not None:
        dcsv = os.path.join(cdir, "R_%s_dum.csv" % tag)
        pd.DataFrame(dumvar).to_csv(dcsv, index=False)
        cmd.append(dcsv)
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        return {"error": p.stderr[-800:]}
    r = json.load(open(out))
    return r


def normalise(V, W, M, r):
    """Normalise Johansen (V, W) on the last r of the first M rows -> drvec's
    beta = [B2 ; I_r].  Returns beta (M x r), alpha (M x r), const (r) or None."""
    V = np.asarray(V, float)
    W = np.asarray(W, float)
    Vr = V[:, :r]
    Bk = Vr[M - r:M, :]
    Binv = np.linalg.inv(Bk)
    beta = Vr[:M] @ Binv
    alpha = W[:, :r] @ Bk.T
    const = (Vr[M] @ Binv) if V.shape[0] > M else None
    return beta, alpha, const


def sm_johansen(Y, K, drcase, r):
    det = {2: "ci", 3: "co", 1: "n"}[drcase]
    out = {}
    for d in (-1, 0, 1):
        jo = coint_johansen(Y, det_order=d, k_ar_diff=K - 1)
        out["coint_johansen_det%d" % d] = {
            "trace": jo.lr1.tolist(), "eigen": jo.lr2.tolist(),
            "cvt5": jo.cvt[:, 1].tolist(), "cvm5": jo.cvm[:, 1].tolist(),
            "eig": jo.eig.tolist()}
    v = VECM(Y, k_ar_diff=K - 1, coint_rank=r, deterministic=det).fit()
    M = Y.shape[1]
    beta = np.asarray(v.beta)
    Bk = beta[M - r:M, :]
    Binv = np.linalg.inv(Bk)
    out["vecm"] = {"det": det, "beta": (beta @ Binv).tolist(),
                   "alpha": (np.asarray(v.alpha) @ Bk.T).tolist(),
                   "llf": float(v.llf),
                   "const_coint": (np.asarray(v.const_coint).ravel() @ Binv).tolist()
                   if det == "ci" else None}
    return out


# ------------------------------------------------------------------- drvec --
CRIT = {1: "gradient", 2: "steptol", 3: "termcode3(no lower point)",
        4: "iteration limit", 5: "5 max steps"}


def parse_banners(txt):
    res = []
    for m in re.finditer(r"OPTIMIZER (CONVERGED|STOPPED) after (\d+) iterations.*?"
                         r"Convergence criterion: ([^\n]+)", txt, re.S):
        c = m.group(3)
        code = (1 if "gradient" in c else 2 if "steptol" in c else
                3 if "lower point" in c else 4 if "iteration" in c else 5)
        res.append({"termcode": code, "iters": int(m.group(2))})
    return res


def parse_matrix(txt, header, nrows):
    i = txt.find(header)
    if i < 0:
        return None
    lines = txt[i:].split("\n")[1:]
    rows = []
    for l in lines:
        t = l.split()
        try:
            rows.append([float(x) for x in t])
        except ValueError:
            break
        if len(rows) == nrows:
            break
    return rows


def run_drvec(stem, p, q, r, case, extra=(), lrtest=False):
    cmd = [DRVEC, stem, str(p), str(q), str(max(r, 1) if lrtest else r),
           "-case", str(case), *extra]
    if lrtest:
        cmd.append("-lrtest")
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=7200)
    txt = open(stem + ".out", errors="ignore").read()
    tag = "_".join(["p%d" % p, "q%d" % q, ("lr" if lrtest else "r%d" % r)]
                   + [e.strip("-") for e in extra])
    with open(stem + "." + tag + ".out", "w") as f:
        f.write(txt)
    res = {"cmd": " ".join(cmd), "rc": pr.returncode, "banners": parse_banners(txt),
           "stderr_tail": pr.stderr[-600:]}
    if lrtest:
        tab = re.findall(r"^\s*(\d+)\s+(\d+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s*$",
                         txt, re.M)
        res["ranks"] = [{"r": int(a), "npar": int(b), "logL": float(c)}
                        for a, b, c, d, e in tab]
        return res
    m = re.search(r"logelf\s*:\s*(-?[\d.]+)", txt)
    res["logL"] = float(m.group(1)) if m else None
    m = re.search(r"npar\s*:\s*(\d+)", txt)
    res["npar"] = int(m.group(1)) if m else None
    M = None
    m = re.search(r"M = (\d+) series", txt)
    if m:
        M = int(m.group(1))
    if M and r:
        res["alpha"] = parse_matrix(txt, "alpha matrix (M x r)", M)
        res["beta"] = parse_matrix(txt, "beta matrix (M x r)", M)
        ew = parse_matrix(txt, "E[W] vector:", 1)
        res["EW"] = ew[0] if ew else None
    m = re.search(r"MA \(Theta\)([^\n]*)", txt)
    if m:
        mods = [float(t.rstrip("*")) for t in m.group(1).split()
                if re.match(r"^[\d.]+\*?$", t)]
        res["ma_min_root"] = min(mods) if mods else None
    m = re.search(r"relation 1: the Y1 block carries\s+([\d.]+)%", txt)
    res["norm_share"] = [float(x) for x in
                         re.findall(r"relation \d+: the Y1 block carries\s+([\d.]+)%", txt)]
    m = re.search(r"Convergence\s+:\s*([^\n]+)", txt)
    res["note"] = m.group(1).strip() if m else None
    return res


# ------------------------------------------------------------------- cases --
def load(name):
    return pd.read_csv(os.path.join(DS, name + ".csv"))


def case_defs():
    C = {}
    dk = load("urca_denmark")
    C["denmark"] = dict(
        desc="JJ1990 Danish money demand, published spec: m2,y,ib,id; K=2; "
             "restricted constant; centered seasonals",
        df=dk, order=["LRY", "IBO", "IDE", "LRM"], K=2, ecdet="const", drcase=2,
        season=4, r=1, freq=4, start=(1, 1974))
    C["denmark5"] = dict(
        desc="benchmark-file variant: 5 vars incl. LPY; K=2; restricted constant;"
             " NO seasonals (the spec of benchmark/urca_denmark.results.txt)",
        df=dk, order=["LPY", "IBO", "IDE", "LRM", "LRY"], K=2, ecdet="const",
        drcase=2, season=0, r=2, freq=4, start=(1, 1974))
    fi = load("urca_finland")
    C["finland"] = dict(
        desc="JJ1990 Finnish money demand: lrm1,lny,lnmr,difp; K=2; unrestricted "
             "constant; centered seasonals (urca example spec)",
        df=fi, order=["lny", "lnmr", "difp", "lrm1"], K=2, ecdet="none", drcase=3,
        season=4, r=2, freq=4, start=(2, 1958))
    C["finland_const"] = dict(
        desc="benchmark-file variant: K=2, restricted constant, no seasonals",
        df=fi, order=["lny", "lnmr", "difp", "lrm1"], K=2, ecdet="const", drcase=2,
        season=0, r=2, freq=4, start=(2, 1958))
    uc = load("urca_UKconinc")
    C["UKconinc"] = dict(
        desc="UK log consumption/income (HEGY data), benchmark spec: K=2, "
             "restricted constant, no seasonals",
        df=uc, order=["incl", "conl"], K=2, ecdet="const", drcase=2, season=0, r=1,
        freq=4, start=(1, 1955))
    C["UKconinc_seas"] = dict(
        desc="UK log consumption/income with centered seasonals (ca.jo season=4),"
             " K=2, restricted constant",
        df=uc, order=["incl", "conl"], K=2, ecdet="const", drcase=2, season=4, r=1,
        freq=4, start=(1, 1955))
    ukc = load("urca_UKconsumption")
    ukcl = np.log(ukc[["cons", "inc", "price"]]).rename(
        columns={"cons": "lcons", "inc": "linc", "price": "lprice"})
    C["UKconsumption"] = dict(
        desc="Pokorny UK cons/inc/price, LOGS, K=2, restricted constant: the "
             "homologated drvec fixture (tests/run_tests.sh) re-run",
        df=ukcl, order=["lcons", "linc", "lprice"], K=2, ecdet="const", drcase=2,
        season=0, r=2, freq=4, start=(1, 1955))
    C["UKconsumption_lev"] = dict(
        desc="same, in LEVELS (the spec of benchmark/urca_UKconsumption.results.txt)",
        df=ukc, order=["cons", "inc", "price"], K=2, ecdet="const", drcase=2,
        season=0, r=2, freq=4, start=(1, 1957))
    ca = load("vars_Canada")
    C["Canada"] = dict(
        desc="Pfaff/Luetkepohl Canada: prod,e,U,rw; K=3; PUBLISHED ecdet='trend' "
             "(not expressible) -> drvec closest = case 3 (ecdet='none')",
        df=ca, order=["e", "U", "rw", "prod"], K=3, ecdet="none", drcase=3,
        season=0, r=1, freq=4, start=(1, 1980), published_ecdet="trend")
    uk = load("urca_UKpppuip")
    C["UKpppuip"] = dict(
        desc="JJ1992 PPP/UIP: p1,p2,e12,i1,i2; K=2; unrestricted constant; "
             "centered seasonals; dumvar=(doilp0,doilp1) unrestricted I(0)",
        df=uk, order=["p1", "p2", "i2", "e12", "i1"], K=2, ecdet="none", drcase=3,
        season=4, r=2, freq=4, start=(1, 1971), dumvar=["doilp0", "doilp1"])
    return C


def run_case(name, c):
    cdir = os.path.join(HERE, "cases", name)
    os.makedirs(cdir, exist_ok=True)
    df = c["df"]
    Yraw = df[c["order"]].to_numpy(float)
    X = df[c["dumvar"]].to_numpy(float) if c.get("dumvar") else None
    M, K, r = Yraw.shape[1], c["K"], c["r"]
    res = {"name": name, "desc": c["desc"], "order": c["order"], "M": M, "n": len(Yraw),
           "K": K, "p": K, "r_pub": r, "drcase": c["drcase"], "ecdet": c["ecdet"],
           "season": c["season"], "dumvar": c.get("dumvar")}

    # (1) Johansen under the published spec, on the raw data
    pub_ecdet = c.get("published_ecdet", c["ecdet"])
    res["cajo_pub"] = cajo("pub", cdir, Yraw, c["order"], K, pub_ecdet, c["season"], X)

    # (2) the data drvec sees: seasonals / exogenous regressors pre-adjusted out
    adjusted = bool(c["season"] or X is not None)
    Y = preadjust(Yraw, c["season"], X) if adjusted else Yraw
    res["preadjusted"] = adjusted
    stem = os.path.join(cdir, name)
    write_inp(stem + ".inp", Y, c["order"], c["freq"], c["start"][0], c["start"][1],
              [c["desc"],
               "column order [Y2 ; Y1]: last r columns are the Y1 block",
               ("seasonals/exogenous regressors PRE-ADJUSTED out of the levels "
                "(bench.py preadjust)" if adjusted else "raw data")])
    res["cajo_same"] = cajo("same", cdir, Y, c["order"], K, c["ecdet"])
    if c.get("published_ecdet"):
        res["cajo_same_pubecdet"] = cajo("same_pubecdet", cdir, Y, c["order"], K,
                                         c["published_ecdet"])
    try:
        res["sm"] = sm_johansen(Y, K, c["drcase"], r)
    except Exception as exc:                                   # noqa: BLE001
        res["sm"] = {"error": str(exc)}

    # (3) drvec
    p = K
    res["dr_lr"] = run_drvec(stem, p, 0, 0, c["drcase"], lrtest=True)
    res["dr_q0"] = run_drvec(stem, p, 0, r, c["drcase"])
    res["dr_q0_seedjoh"] = run_drvec(stem, p, 0, r, c["drcase"], ("-seedjoh",))
    res["dr_q0_ms"] = run_drvec(stem, p, 0, r, c["drcase"], ("-multistart", "20"))
    res["dr_q1"] = run_drvec(stem, p, 1, r, c["drcase"])
    json.dump(res, open(os.path.join(cdir, "result.json"), "w"), indent=1,
              default=lambda o: o.tolist() if hasattr(o, "tolist") else str(o))
    return res


def main():
    C = case_defs()
    names = sys.argv[1:] or list(C)
    for n in names:
        print("==", n, flush=True)
        run_case(n, C[n])


if __name__ == "__main__":
    main()
