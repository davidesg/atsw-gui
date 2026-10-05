#!/usr/bin/env python3
"""benchmark_three_engines.py — the same simulated series to three identifiers.

ART_18's C (`bin/art_cli -i --mlp-direct`), art-python (`art.suggest_orders`,
the reference identifier) and pmdarima (`auto_arima`, stepwise, AIC). For each
model of the battery: the exact order (top-1), the true model among the first
3 and in the shortlist, and the time per series. Box-Jenkins convention, burn-in
300; the SAME series goes to the three.

Caveats when reading it: the C and pmdarima search p, q <= 5, art-python
p <= 3, q <= 2 on annual data with a list of 5 (the C's shortlist holds up to
14); the C's time is its own clock (identification only), the others' wall
time; pmdarima estimates every model it tries, the other two only identify.

Use:  python3 tests/benchmark_three_engines.py [REPS] [N] [SEED]
      ART_PYTHON_SRC=<art-python/src> if art-python is not installed.
Results of 2026-10-01: tests/results_three_engines.txt."""
import os, re, sys, time, subprocess, tempfile, warnings
import numpy as np
from scipy.signal import lfilter
warnings.filterwarnings("ignore")

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 50
N = int(sys.argv[2]) if len(sys.argv) > 2 else 200
SEED = int(sys.argv[3]) if len(sys.argv) > 3 else 2026
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "bin", "art_cli")
if not os.path.exists(CLI):
    sys.exit(f"No {CLI}: build it with  make cli")
sys.path.insert(0, os.path.expanduser(os.environ.get(
    "ART_PYTHON_SRC", "~/Dropbox/SRC/ART/art-python/src")))
import fue, art, pmdarima as pm   # noqa: E402

MODELS = [
    ("AR(1) .6",             [0.6],        [],         1, 0),
    ("AR(2) real .5,.3",     [0.5, 0.3],   [],         2, 0),
    ("AR(2) cplx 1,-.5 (8)", [1.0, -0.5],  [],         2, 0),
    ("AR(2) cplx .8,-.64(6)",[0.8, -0.64], [],         2, 0),
    ("AR(2) cplx 1.56,-.81(12)", [1.56, -0.81], [],    2, 0),
    ("AR(2) .5,-.3",         [0.5, -0.3],  [],         2, 0),
    ("AR(3) .5,-.3,.2",      [0.5, -0.3, 0.2], [],     3, 0),
    ("MA(1) .6",             [],           [0.6],      0, 1),
    ("MA(1) -.5",            [],           [-0.5],     0, 1),
    ("MA(2) .5,.3",          [],           [0.5, 0.3], 0, 2),
    ("ARMA(1,1) .5/.4",      [0.5],        [0.4],      1, 1),
    ("ARMA(2,1)",            [0.5, -0.3],  [0.4],      2, 1),
]

ARG = re.compile(r"MLP-direct identification:\s*\((\d+),(\d+)\)\((\d+),(\d+)\)")
SHORT = re.compile(r"\((\d+),(\d+)\)\((\d+),(\d+)\)")
TIME = re.compile(r"Tiempo medio por ejecución:\s*([\d.]+)\s*ms")


def sim(phi, theta, n, rng):
    a = rng.normal(size=n + 300)
    return lfilter(np.r_[1, -np.array(theta, float)], np.r_[1, -np.array(phi, float)], a)[300:]


def run_c(x, d):
    path = os.path.join(d, "s.txt")
    np.savetxt(path, x)
    t0 = time.time()
    out = subprocess.run([CLI, "-i", path, "-S", "1", "--pmax", "5", "--qmax", "5",
                          "--mlp-direct"], capture_output=True, text=True).stdout
    wall = (time.time() - t0) * 1000
    am = ARG.search(out)
    top = (int(am.group(1)), int(am.group(2))) if am else None
    sl = []
    for line in out.splitlines():
        if line.startswith("MLP shortlist"):
            sl = [(int(m.group(1)), int(m.group(2))) for m in SHORT.finditer(line)]
            break
    tm = TIME.search(out)
    return top, sl, float(tm.group(1)) if tm else wall


def run_py(x):
    ts = fue.TimeSeries(list(x + 100.0), freq=1, start=(2000, 1), name="S")
    t0 = time.time()
    specs = art.suggest_orders(ts, d=0, D=0, lam=1.0, top_n=5)
    ms = (time.time() - t0) * 1000
    return [(s.p, s.q) for s in specs], ms


def run_pmd(x):
    t0 = time.time()
    m = pm.auto_arima(x, start_p=0, max_p=5, start_q=0, max_q=5, d=0, D=0,
                      seasonal=False, stepwise=True, error_action="ignore",
                      suppress_warnings=True, n_jobs=1)
    return (m.order[0], m.order[2]), (time.time() - t0) * 1000


def main():
    rng = np.random.default_rng(SEED)
    print(f"reps={REPS}  n={N}  seed={SEED}   exact % (top-1) · in top-3 · in shortlist")
    hdr = (f"{'model':<26}{'C top1':>7}{'C top3':>7}{'C SL':>6} |{'Py top1':>8}{'Py top3':>8}"
           f"{'Py top5':>8} |{'pmd':>6} | {'C ms':>6}{'Py ms':>7}{'pmd ms':>8}")
    print(hdr); print("-" * len(hdr))
    tot = {k: [] for k in ("c1", "c3", "csl", "p1", "p3", "p5", "pm", "cms", "pms", "mms")}
    with tempfile.TemporaryDirectory() as d:
        for name, phi, theta, tp, tq in MODELS:
            c1 = c3 = csl = p1 = p3 = p5 = pmok = 0
            cms, pms, mms = [], [], []
            for _ in range(REPS):
                x = sim(phi, theta, N, rng)
                top, sl, ms = run_c(x, d); cms.append(ms)
                c1 += top == (tp, tq); c3 += (tp, tq) in sl[:3]; csl += (tp, tq) in sl
                lst, ms = run_py(x); pms.append(ms)
                p1 += bool(lst) and lst[0] == (tp, tq); p3 += (tp, tq) in lst[:3]; p5 += (tp, tq) in lst[:5]
                try:
                    o, ms = run_pmd(x); mms.append(ms); pmok += o == (tp, tq)
                except Exception:
                    pass
            r = lambda v: 100.0 * v / REPS  # noqa: E731
            row = [r(c1), r(c3), r(csl), r(p1), r(p3), r(p5), r(pmok),
                   np.mean(cms), np.mean(pms), np.mean(mms) if mms else np.nan]
            for k, v in zip(tot, row):
                tot[k].append(v)
            print(f"{name:<26}{row[0]:6.0f}%{row[1]:6.0f}%{row[2]:5.0f}% |{row[3]:7.0f}%{row[4]:7.0f}%"
                  f"{row[5]:7.0f}% |{row[6]:5.0f}% | {row[7]:6.1f}{row[8]:7.0f}{row[9]:8.0f}", flush=True)
    print("-" * len(hdr))
    m = {k: np.nanmean(v) for k, v in tot.items()}
    print(f"{'MEAN':<26}{m['c1']:6.0f}%{m['c3']:6.0f}%{m['csl']:5.0f}% |{m['p1']:7.0f}%{m['p3']:7.0f}%"
          f"{m['p5']:7.0f}% |{m['pm']:5.0f}% | {m['cms']:6.1f}{m['pms']:7.0f}{m['mms']:8.0f}")
    print("\nC ms: the CLI's own clock (identification only); Py and pmd: wall time per series.")


if __name__ == "__main__":
    main()
