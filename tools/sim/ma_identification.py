#!/usr/bin/env python3
"""Is the moving average identified?  Move the MA operator and ask the program.

The question this instrument answers, and why it needed one.  `drvec` estimates
a free `Theta` badly on the bank and well on some simulated data, and the record
had no measurement that said WHEN.  The candidates were the sample size, the
number of MA parameters, the optimiser, and the proximity of the MA root to the
unit circle.  See HOMOLOGATION.md 4q for what this measured and for one reading
it measured and then REFUTED.

SIGN CONVENTION -- Box and Jenkins, which is the house convention and the
program's own (`README.md`: the model carries `(I - Theta_1 L - ...) A_t`).  So
`theta = 0.8` IS the operator `(1 - 0.8B)`, and a NEGATIVE theta here means
`(1 + |theta|B)`.  The distinction is not cosmetic: it is the single largest
effect this script measures, so every table prints the polynomial next to the
coefficient.

The DGP is `sim_vec.py`'s: B2 = -0.5, Lambda = (0.30, 0.10), F1 = 0.2 I,
Theta1 = theta I, Sigma = I.  Its AR roots do not depend on theta.  They are
known in closed form rather than estimated: with Phi(L) = (I - F1 L)(1-L) +
Lambda beta' L and c = Lambda_1 + Lambda_2 B2 = 0.25,

    det Phi(L) = a(L) * (a(L) + c L),      a(L) = (1 - 0.2L)(1 - L)

whose roots are 1 (the unit root), 5.0, 1.5746 and 3.1754 -- all REAL and
POSITIVE.  That is what the `dist` column is measured against, and knowing they
are real is what makes a signed comparison legitimate.

Modes:

    sign         the main table: matched |theta|, both signs.  This is the
                 comparison that carries the result, and the one that refuted
                 the root-distance reading -- see `separation` below
    separation   recovery against the distance from the true MA root to the
                 nearest AR root.  KEPT because it is the measurement that was
                 wrong: read on its own it suggests the distance is what
                 matters, and `sign` shows it is not -- (1-0.1B) sits 5.00 away
                 from every AR root and is estimated as badly as one sitting on
                 top of them
    multistart   the `sign` cells with -multistart 20, to separate "the
                 optimiser does not find the optimum" from "the optimum is not
                 the truth"
    gap          n = 4000, free Theta: per-replication AR-MA gap against the
                 error in Theta and against the admissibility statistic G, which
                 is what shows that G does not see this failure mode
    algebra      no simulation: checks Corollary 6.3 numerically for several
                 (r, s, q).  With the bottom s rows of every Theta_k zero,
                 det Theta(1) = det(I_r - sum T11_k) and the non-zero
                 eigenvalues of the M*q companion are exactly those of the r*q
                 companion of the r x r block -- which is why the engine's own
                 invertibility gate enforces admissibility in this class
    structural   the question `inherited` could not answer: does the class of
                 Theorem 6 recover its OWN truth when the DGP is identified?
                 Varies theta against the FIXED phi = 0.6 of the w block, so
                 |phi - theta| says whether that ARMA(1,1) is estimable at all,
                 and fits each sample both free and with -mawarma
    inherited    the WARMA DGP of `sim_warma.py` fitted three ways -- free,
                 -mawarma and -marow -- which asks whether the restricted class
                 recovers its OWN truth.  Note that DGP is near-cancelling by
                 construction (phi = 0.6 against theta = 0.5), so what this
                 measures is the restricted class under weak identification

Usage:  python3 tools/sim/ma_identification.py [mode] [nrep]
"""
import os
import re
import statistics as st
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DRVEC = os.path.join(ROOT, "bin", "drvec")
SIM = os.path.join(ROOT, "tools", "sim", "sim_vec.py")
SIM_W = os.path.join(ROOT, "tools", "sim", "sim_warma.py")
TMP = os.path.join("/tmp", "drvec_ma_ident")

AR_TRUE = (1.5746, 3.1754, 5.0)      # exactas, en forma cerrada; ver la cabecera
B2_TRUE = -0.5


def fit(n, theta, seed, extra=()):
    """Simulate one sample and fit it; return the parsed .out, or None."""
    os.makedirs(TMP, exist_ok=True)
    base = os.path.join(TMP, "case")
    subprocess.run([sys.executable, SIM, str(n), str(theta), str(seed), base + ".inp"],
                   capture_output=True, check=True)
    subprocess.run([DRVEC, base, "2", "1", "1", "-case", "1", *extra],
                   capture_output=True, timeout=1800)
    try:
        t = open(base + ".out", encoding="latin-1", errors="replace").read()
    except OSError:
        return None
    d = {}
    m = re.search(r"Theta\[1\] \(M x M\)[^\n]*=\n\s+([\-0-9.]+)\s+([\-0-9.]+)"
                  r"\n\s+([\-0-9.]+)\s+([\-0-9.]+)", t)
    if not m:
        return None
    d["th"] = [float(x) for x in m.groups()]
    m = re.search(r"B2 \(s x r\) =\n\s+([\-0-9.]+)", t)
    d["b2"] = float(m.group(1)) if m else None
    m = re.search(r"sigma_min\(Lambda_perp[^=]*= ([0-9.eE+\-]+)", t)
    d["g"] = float(m.group(1)) if m else None
    for key, tag in (("ar", r"AR \(Phi\)\s+(.*)"), ("ma", r"MA \(Theta\)\s+(.*)")):
        m = re.search(tag, t)
        d[key] = ([float(x) for x in m.group(1).replace("*", "").split()
                   if x not in ("inf", "nan")] if m else [])
    d["mamin"] = min(d["ma"]) if d["ma"] else None
    d["gap"] = (min(abs(a - b) for a in d["ar"] for b in d["ma"])
                if d["ar"] and d["ma"] else None)
    return d


def cell(n, theta, nrep, extra=()):
    rows = [fit(n, theta, 900 + k, extra) for k in range(nrep)]
    return [r for r in rows if r]


def iqr(vals):
    v = sorted(vals)
    q = len(v)
    return v[int(0.75 * (q - 1))] - v[int(0.25 * (q - 1))]


def boundary(rows):
    return sum(1 for r in rows if r["mamin"] and r["mamin"] <= 1.0005) / len(rows)


THETAS = (-0.3, -0.5, -0.8, 0.4167, 0.5, 0.6, 0.8)
PAIRED = (0.1, 0.2, 0.3, 0.5, 0.8)      # magnitudes, corridas con los dos signos


def poly(theta):
    """The operator, written out, so no table can be read with the wrong sign."""
    return f"(1-{theta}B)" if theta > 0 else f"(1+{abs(theta)}B)"


def mode_sign(nrep):
    """Matched magnitudes, both signs.  The comparison that carries the result."""
    print("Box-Jenkins: Theta(B) = I - Theta_1 B, so theta>0 is (1 - theta B).")
    print(f"True AR roots (closed form): {AR_TRUE} -- all real and positive.")
    print(f"{nrep} replications per cell, free Theta, p=2 q=1 r=1 -case 1\n")
    print(f"{'|theta|':>8} {'operator':>13} {'MA root':>9} {'dist AR':>8} {'n':>5} "
          f"{'Th11 med':>9} {'IQR':>7} {'bnd%':>5}")
    for mag in PAIRED:
        for sgn in (+1, -1):
            theta = sgn * mag
            root = 1.0 / theta
            dist = min(abs(root - a) for a in AR_TRUE)
            for n in (120, 250):
                rows = cell(n, theta, nrep)
                if not rows:
                    continue
                t11 = [r["th"][0] for r in rows]
                print(f"{mag:8.3f} {poly(theta):>13} {root:9.2f} {dist:8.2f} {n:5d} "
                      f"{st.median(t11):9.3f} {iqr(t11):7.3f} "
                      f"{100 * boundary(rows):5.0f}", flush=True)


def mode_separation(nrep, extra=(), label=""):
    print(f"DGP sim_vec.py: B2=-0.5, Lambda=(0.30,0.10), F1=0.2I, Theta1=theta*I")
    print(f"True AR roots: {AR_TRUE} (fixed; theta does not move them){label}")
    print(f"{nrep} replications per cell, free Theta, p=2 q=1 r=1 -case 1\n")
    print(f"{'theta':>7} {'MA root':>8} {'dist':>6} {'n':>6} "
          f"{'Th11 med':>9} {'IQR':>7} {'|B2+0.5|':>9} {'bnd%':>5}")
    print("   (this mode is kept as the measurement that was refuted; see `sign`)")
    for theta in THETAS:
        root = 1.0 / theta
        dist = min(abs(root - a) for a in AR_TRUE)
        for n in (120, 250):
            rows = cell(n, theta, nrep, extra)
            if not rows:
                continue
            t11 = [r["th"][0] for r in rows]
            print(f"{theta:7.4f} {root:8.2f} {dist:6.2f} {n:6d} "
                  f"{st.median(t11):9.3f} {iqr(t11):7.3f} "
                  f"{st.median(abs(r['b2'] - B2_TRUE) for r in rows):9.4f} "
                  f"{100 * boundary(rows):5.0f}", flush=True)


def mode_multistart(nrep):
    mode_separation(nrep, extra=("-multistart", "20"), label="   [-multistart 20]")


def mode_gap(nrep):
    theta, n = 0.5, 4000
    print(f"n={n}, theta={theta} (true Theta = diag(0.5,0.5), MA root 2.00),")
    print(f"free Theta, {nrep} replications.  True AR roots {AR_TRUE}: none at 2.00.\n")
    rows = []
    for k in range(nrep):
        d = fit(n, theta, 500 + k)
        if d and d["gap"] is not None and d["g"] is not None:
            rows.append((d["gap"], max(abs(d["th"][j] - v) for j, v in
                                       enumerate((0.5, 0.0, 0.0, 0.5))),
                         d["g"], abs(d["b2"] - B2_TRUE)))
    rows.sort()
    half = len(rows) // 2
    print(f"{'half':<28} {'mean gap':>9} {'med err Theta':>14} "
          f"{'med G':>7} {'med |B2+0.5|':>13}")
    for lab, g in (("smaller gap (cancelling)", rows[:half]),
                   ("larger gap", rows[half:])):
        print(f"{lab:<28} {st.mean(r[0] for r in g):9.4f} "
              f"{st.median(r[1] for r in g):14.3f} "
              f"{st.median(r[2] for r in g):7.3f} "
              f"{st.median(r[3] for r in g):13.4f}")
    print(f"\n{'gap':>8} {'err Theta':>10} {'G':>8} {'|B2+0.5|':>10}")
    for r in rows:
        print(f"{r[0]:8.4f} {r[1]:10.3f} {r[2]:8.3f} {r[3]:10.4f}")


def mode_inherited(nrep):
    """The WARMA truth of BVECM Corollary 2, fitted free and restricted."""
    theta = 0.5
    print("DGP sim_warma.py: phi=0.6 theta=0.5 gamma=0.2 beta=0.5")
    print("In VEC coordinates the truth is Theta1 = [[0.50, -0.25],[0, 0]], B2 = -0.50")
    print(f"{nrep} replications per cell.  phi and theta are 0.6 and 0.5, so the")
    print("w block is an ARMA(1,1) with roots 1.67 and 2.00: near-cancelling.\n")
    print(f"{'n':>6} {'spec':>9} {'Th11 med':>9} {'IQR':>7} {'Th21 med':>9} "
          f"{'|B2+0.5|':>9} {'MAmin med':>10} {'G med':>7}")
    for n in (120, 250, 1000):
        for extra, tag in (((), "free"), (("-mawarma",), "mawarma"), (("-marow",), "marow")):
            rows = []
            for k in range(nrep):
                base = os.path.join(TMP, "warma")
                os.makedirs(TMP, exist_ok=True)
                subprocess.run([sys.executable, SIM_W, str(n), str(theta), str(700 + k),
                                base + ".inp"], capture_output=True, check=True)
                subprocess.run([DRVEC, base, "2", "1", "1", "-case", "1", *extra],
                               capture_output=True, timeout=1800)
                try:
                    t = open(base + ".out", encoding="latin-1", errors="replace").read()
                except OSError:
                    continue
                m = re.search(r"Theta\[1\] \(M x M\)[^\n]*=\n\s+([\-0-9.]+)\s+([\-0-9.]+)"
                              r"\n\s+([\-0-9.]+)\s+([\-0-9.]+)", t)
                b = re.search(r"B2 \(s x r\) =\n\s+([\-0-9.]+)", t)
                g = re.search(r"sigma_min\(Lambda_perp[^=]*= ([0-9.eE+\-]+)", t)
                ma = re.search(r"MA \(Theta\)\s+(.*)", t)
                if not (m and b and g and ma):
                    continue
                roots = [float(x) for x in ma.group(1).replace("*", "").split()
                         if x not in ("inf", "nan")]
                rows.append({"th": [float(x) for x in m.groups()],
                             "b2": float(b.group(1)), "g": float(g.group(1)),
                             "mamin": min(roots) if roots else None})
            if not rows:
                print(f"{n:6d} {tag:>9}   (no fit)")
                continue
            t11 = [r["th"][0] for r in rows]
            mam = [r["mamin"] for r in rows if r["mamin"]]
            print(f"{n:6d} {tag:>9} {st.median(t11):9.3f} {iqr(t11):7.3f} "
                  f"{st.median(r['th'][2] for r in rows):9.3f} "
                  f"{st.median(abs(r['b2'] - B2_TRUE) for r in rows):9.4f} "
                  f"{st.median(mam) if mam else float('nan'):10.3f} "
                  f"{st.median(r['g'] for r in rows):7.3f}", flush=True)


def mode_algebra(_nrep=0):
    """Corollary 6.3, checked numerically rather than only proved."""
    import numpy as np
    rng = np.random.default_rng(0)
    print("Corollary 6.3.  Bottom s rows of every Theta_k zero (the class of")
    print("Theorem 6, and also -marow, whose cross block is free).\n")
    print(f"{'r':>3} {'s':>3} {'q':>3} "
          f"{'|det Theta(1) - det(I_r - sum T11)|':>36} "
          f"{'max |eig - eig(r x r block)|':>30}")
    for (r, s_, q) in ((1, 1, 1), (2, 3, 1), (2, 1, 2), (3, 2, 3), (1, 4, 2)):
        m = r + s_
        Th = [np.zeros((m, m)) for _ in range(q)]
        for k in range(q):
            Th[k][:r, :] = rng.normal(size=(r, m)) * 0.4   # filas inferiores = 0
        S = sum(Th)
        d_full = np.linalg.det(np.eye(m) - S)
        d_blk = np.linalg.det(np.eye(r) - S[:r, :r])
        comp = np.zeros((m * q, m * q))
        for k in range(q):
            comp[:m, k * m:(k + 1) * m] = Th[k]
        for k in range(1, q):
            comp[k * m:(k + 1) * m, (k - 1) * m:k * m] = np.eye(m)
        compw = np.zeros((r * q, r * q))
        for k in range(q):
            compw[:r, k * r:(k + 1) * r] = Th[k][:r, :r]
        for k in range(1, q):
            compw[k * r:(k + 1) * r, (k - 1) * r:k * r] = np.eye(r)
        ev = np.linalg.eigvals(comp)
        nz = np.sort_complex(ev[np.abs(ev) > 1e-10])
        evw = np.sort_complex(np.linalg.eigvals(compw))
        gap = np.max(np.abs(nz - evw)) if len(nz) == len(evw) else float("nan")
        print(f"{r:3d} {s_:3d} {q:3d} {abs(d_full - d_blk):36.2e} {gap:30.2e}")


def _fit_warma(n, theta, seed, extra):
    """One WARMA sample, fitted; returns the parsed .out or None."""
    os.makedirs(TMP, exist_ok=True)
    base = os.path.join(TMP, "warma")
    subprocess.run([sys.executable, SIM_W, str(n), str(theta), str(seed),
                    base + ".inp"], capture_output=True, check=True)
    subprocess.run([DRVEC, base, "2", "1", "1", "-case", "1", *extra],
                   capture_output=True, timeout=1800)
    try:
        t = open(base + ".out", encoding="latin-1", errors="replace").read()
    except OSError:
        return None
    m = re.search(r"Theta\[1\] \(M x M\)[^\n]*=\n\s+([\-0-9.]+)\s+([\-0-9.]+)"
                  r"\n\s+([\-0-9.]+)\s+([\-0-9.]+)", t)
    b = re.search(r"B2 \(s x r\) =\n\s+([\-0-9.]+)", t)
    g = re.search(r"sigma_min\(Lambda_perp[^=]*= ([0-9.eE+\-]+)", t)
    ma = re.search(r"MA \(Theta\)\s+(.*)", t)
    if not (m and b and g and ma):
        return None
    roots = [float(x) for x in ma.group(1).replace("*", "").split()
             if x not in ("inf", "nan")]
    return {"th": [float(x) for x in m.groups()], "b2": float(b.group(1)),
            "g": float(g.group(1)), "mamin": min(roots) if roots else None}


PHI_W = 0.6      # fijado dentro de sim_warma.py


def mode_structural(nrep):
    """Does the class of Theorem 6 recover its own truth when the DGP allows it?

    The w block is (1 - PHI_W B) w = (1 - theta B) a, so |PHI_W - theta| is the
    distance from an exact common factor.  Small means the ARMA(1,1) is not
    estimable BY ANY METHOD, and a failure there says nothing about the class.
    """
    print(f"DGP sim_warma.py, phi = {PHI_W} fixed.  w block: "
          f"(1-{PHI_W}B) w = (1-theta B) a")
    print("Truth in VEC coordinates: Theta1 = [[theta, -0.5*theta],[0,0]], B2 = -0.5")
    print("|phi - theta| small  =>  near-cancelling ARMA(1,1)  =>  not estimable "
          "by any method\n")
    print(f"{'theta':>7} {'|phi-th|':>9} {'spec':>9} {'n':>5} {'Th11 med':>9} "
          f"{'IQR':>7} {'bias':>7} {'|B2+0.5|':>9} {'MAmin':>7} {'G':>6}")
    for theta in (-0.5, 0.1, 0.9, 0.5):
        for extra, tag in ((("-mawarma",), "mawarma"), ((), "free")):
            for n in (120, 250):
                rows = [r for r in (_fit_warma(n, theta, 700 + k, extra)
                                    for k in range(nrep)) if r]
                if not rows:
                    print(f"{theta:7.2f} {abs(PHI_W - theta):9.2f} {tag:>9} "
                          f"{n:5d}   (no fit)")
                    continue
                v = [r["th"][0] for r in rows]
                mam = [r["mamin"] for r in rows if r["mamin"]]
                print(f"{theta:7.2f} {abs(PHI_W - theta):9.2f} {tag:>9} {n:5d} "
                      f"{st.median(v):9.3f} {iqr(v):7.3f} {st.median(v) - theta:7.3f} "
                      f"{st.median(abs(r['b2'] - B2_TRUE) for r in rows):9.4f} "
                      f"{st.median(mam) if mam else float('nan'):7.3f} "
                      f"{st.median(r['g'] for r in rows):6.3f}", flush=True)


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "sign"
    reps = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    if not os.access(DRVEC, os.X_OK):
        sys.exit(f"{DRVEC} not built; run make first")
    {"sign": mode_sign,
     "algebra": mode_algebra,
     "separation": mode_separation,
     "multistart": mode_multistart,
     "gap": mode_gap,
     "structural": mode_structural,
     "inherited": mode_inherited}[mode](reps)
