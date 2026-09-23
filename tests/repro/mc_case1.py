"""BUG: case-1 critical values. Under H0 (M=2 independent random walks, no
constant, true r=0) the rank test's LR(0->1) should exceed the printed 5 %
critical value in ~5 % of samples. Prints the empirical size and quantiles."""
import re, subprocess, sys
import numpy as np
BIN = sys.argv[1] if len(sys.argv) > 1 else "drvec"
R, n = int(sys.argv[2]) if len(sys.argv) > 2 else 200, 500
rng = np.random.default_rng(20260923)
lr, cv5, neg = [], None, 0
for k in range(R):
    x = np.cumsum(rng.standard_normal((n, 2)), axis=0)
    with open("rw.inp", "w") as f:
        f.write("* rw\n1\n2 %d 1 1000\na b\n1.0 0 0\n" % n)
        for r in x: f.write("%.10f %.10f\n" % tuple(r))
    subprocess.run([BIN, "rw", "1", "0", "1", "-case", "1", "-lrtest"], capture_output=True)
    t = open("rw.out").read()
    t = t[t.find("M-r        LR"):]
    m = re.search(r"^\s+0\s+2\s+(-?[\d.]+)(?:\s+([\d.]+)\s+([\d.]+)\s+([\d.]+))?", t, re.M)
    if not m: continue
    v = float(m.group(1))
    if m.group(3): cv5 = float(m.group(3))
    if v < 0: neg += 1
    lr.append(max(v, 0.0))  # a negative LR is censored at 0 (see the LR<0 entry)
lr = np.array(lr)
print(f"replications={len(lr)}  printed 5% cv={cv5}  empirical size={np.mean(lr > cv5):.3f}"
      f"  (nominal 0.05)  LR<0 printed in {neg}")
print("empirical quantiles 90/95/99: %.2f %.2f %.2f" % tuple(np.quantile(lr, [.9, .95, .99])))
