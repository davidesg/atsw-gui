#!/usr/bin/env python3
"""benchmark_c.py — the C identifier alone, fast, to measure each 18.2 change.

The same battery and simulator as benchmark_three_engines.py (Box-Jenkins
convention, burn-in 300), the C only (`bin/art_cli --mlp-direct`), so a run
takes seconds. Per model: exact order (top-1), in the top 3 and in the
shortlist, and the OVER-identification rate (top-1 with p + q above the true
p + q), which is what 18.2 set out to bring down (Phase 4).

Use:  python3 tests/benchmark_c.py [REPS] [N] [SEED] [--json OUT] [--cli PATH]
"""
import json
import os
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "b3", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "benchmark_three_engines.py"))


def _load_b3():
    # benchmark_three_engines imports art-python and pmdarima at the top; only
    # its battery, simulator and C runner are needed here
    import types
    for mod in ("fue", "art", "pmdarima"):
        sys.modules.setdefault(mod, types.ModuleType(mod))
    b3 = importlib.util.module_from_spec(_spec)
    saved = sys.argv
    sys.argv = [saved[0]]
    try:
        _spec.loader.exec_module(b3)
    finally:
        sys.argv = saved
    return b3


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = cli = None
    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        args = [a for a in args if a != out]
    if "--cli" in sys.argv:
        cli = sys.argv[sys.argv.index("--cli") + 1]
        args = [a for a in args if a != cli]
    reps = int(args[0]) if len(args) > 0 else 100
    n = int(args[1]) if len(args) > 1 else 200
    seed = int(args[2]) if len(args) > 2 else 2026
    b3 = _load_b3()
    if cli:
        b3.CLI = cli
    print(b3.CLI)
    rng = np.random.default_rng(seed)
    print(f"reps={reps} n={n} seed={seed}")
    print(f"{'model':<26}{'top1':>6}{'top3':>6}{'SL':>6}{'over':>6}{'ms':>6}")
    rows = {}
    with tempfile.TemporaryDirectory() as d:
        for name, phi, theta, tp, tq in b3.MODELS:
            c1 = c3 = sl_ = over = 0
            ms = []
            for _ in range(reps):
                x = b3.sim(phi, theta, n, rng)
                top, sl, t = b3.run_c(x, d)
                ms.append(t)
                c1 += top == (tp, tq)
                c3 += (tp, tq) in sl[:3]
                sl_ += (tp, tq) in sl
                over += top is not None and sum(top) > tp + tq
            r = [100.0 * v / reps for v in (c1, c3, sl_, over)] + [float(np.mean(ms))]
            rows[name] = r
            print(f"{name:<26}{r[0]:5.0f}%{r[1]:5.0f}%{r[2]:5.0f}%{r[3]:5.0f}%{r[4]:6.1f}",
                  flush=True)
    m = np.mean(list(rows.values()), axis=0)
    print(f"{'MEAN':<26}{m[0]:5.0f}%{m[1]:5.0f}%{m[2]:5.0f}%{m[3]:5.0f}%{m[4]:6.1f}")
    if out:
        json.dump({"reps": reps, "n": n, "seed": seed, "rows": rows,
                   "mean": list(map(float, m))}, open(out, "w"), indent=1)


if __name__ == "__main__":
    main()
