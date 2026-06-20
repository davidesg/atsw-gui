#!/usr/bin/env python3
"""
benchmark_vs_pmdarima.py — ART (motor C) vs pmdarima.auto_arima.

Objetivo: evaluar si ART puede reemplazar a pmdarima como MOTOR de identificación
de la atsw-suite, en dos ejes:
  1) Velocidad  (ART identifica; pmdarima estima+selecciona muchos modelos)
  2) Acierto    (recuperar el orden verdadero (p,q))

Comparación JUSTA: la MISMA serie simulada se pasa a ambos motores.
  - pmdarima: auto_arima -> order (p,d,q); tiempo = wall time de Python.
  - ART:      bin/art_cli -i serie.txt --mlp-direct; tiempo = reloj interno del CLI
              ("Tiempo medio por ejecución"), que excluye el arranque del proceso.

Para ART se reportan dos métricas:
  - argmax:    el candidato top-1 (un único modelo, comparable a pmdarima)
  - shortlist: ¿está el verdadero en el shortlist? (lo que importa en atsw,
               porque atsw-MCP estima/elige entre los candidatos)

Uso:  python3 tests/benchmark_vs_pmdarima.py [REPS] [N] [SEED]
"""
import os, re, sys, time, subprocess, tempfile
import numpy as np

try:
    import pmdarima as pm
except ImportError:
    sys.exit("pmdarima no está instalado en este intérprete. Usa el python3 que lo tenga.")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "bin", "art_cli")
if not os.path.exists(CLI):
    sys.exit(f"No existe {CLI}. Compila con: make cli")

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 50
N    = int(sys.argv[2]) if len(sys.argv) > 2 else 300
SEED = int(sys.argv[3]) if len(sys.argv) > 3 else 123

# (nombre, phi, theta, true_p, true_q)
MODELS = [
    ("AR(1) .6",        [0.6],          [],        1, 0),
    ("AR(2) .5,-.3",    [0.5, -0.3],    [],        2, 0),
    ("AR(3) .5,-.3,.2", [0.5, -0.3, 0.2],[],       3, 0),
    ("MA(1) .6",        [],             [0.6],     0, 1),
    ("MA(2) .5,.3",     [],             [0.5, 0.3],0, 2),
    ("ARMA(1,1) .5/.4", [0.5],          [0.4],     1, 1),
    ("ARMA(2,1)",       [0.5, -0.3],    [0.4],     2, 1),
]

def simulate_arma(phi, theta, n, rng, burn=200):
    """x_t = sum phi_i x_{t-i} + e_t - sum theta_j e_{t-j}  (convención Box-Jenkins)."""
    p, q = len(phi), len(theta)
    m = n + burn
    e = rng.normal(size=m)
    x = np.zeros(m)
    for t in range(m):
        v = e[t]
        for j in range(q):
            if t - j - 1 >= 0:
                v -= theta[j] * e[t - j - 1]
        for i in range(p):
            if t - i - 1 >= 0:
                v += phi[i] * x[t - i - 1]
        x[t] = v
    return x[burn:]

SHORT_RE = re.compile(r"\((\d+),(\d+)\)\((\d+),(\d+)\)")
ARG_RE   = re.compile(r"MLP-direct identification:\s*\((\d+),(\d+)\)\((\d+),(\d+)\)")
TIME_RE  = re.compile(r"Tiempo medio por ejecución:\s*([\d.]+)\s*ms")

def run_art(series, tmpdir):
    path = os.path.join(tmpdir, "s.txt")
    np.savetxt(path, series)
    out = subprocess.run(
        [CLI, "-i", path, "-S", "1", "--pmax", "5", "--qmax", "5", "--mlp-direct"],
        capture_output=True, text=True,
    ).stdout
    # argmax
    am = ARG_RE.search(out)
    argmax = (int(am.group(1)), int(am.group(2))) if am else None
    # shortlist: parsear la línea "MLP shortlist (...)"
    shortlist = set()
    for line in out.splitlines():
        if line.startswith("MLP shortlist"):
            for m in SHORT_RE.finditer(line):
                shortlist.add((int(m.group(1)), int(m.group(2))))
            break
    tm = TIME_RE.search(out)
    ms = float(tm.group(1)) if tm else float("nan")
    return argmax, shortlist, ms

def main():
    print("=" * 78)
    print(f"  ART (motor C) vs pmdarima auto_arima   reps={REPS}, n={N}, seed={SEED}")
    print("=" * 78)
    rng = np.random.default_rng(SEED)

    agg = {"art_arg": [], "art_short": [], "pmd": [],
           "art_ms": [], "pmd_ms": []}

    hdr = f"{'Modelo':<18} {'ARTarg':>7} {'ARTshort':>9} {'pmdarima':>9} | {'ART ms':>7} {'pmd ms':>8} {'speedup':>8}"
    print(hdr); print("-" * len(hdr))

    with tempfile.TemporaryDirectory() as tmpdir:
        for name, phi, theta, tp, tq in MODELS:
            art_arg_ok = art_short_ok = pmd_ok = 0
            art_ms_list, pmd_ms_list = [], []
            for _ in range(REPS):
                series = simulate_arma(phi, theta, N, rng)

                argmax, shortlist, art_ms = run_art(series, tmpdir)
                if argmax == (tp, tq): art_arg_ok += 1
                if (tp, tq) in shortlist: art_short_ok += 1
                if not np.isnan(art_ms): art_ms_list.append(art_ms)

                t0 = time.time()
                try:
                    model = pm.auto_arima(
                        series, start_p=0, max_p=5, start_q=0, max_q=5,
                        d=0, D=0, seasonal=False, stepwise=True,
                        error_action="ignore", suppress_warnings=True, n_jobs=1,
                    )
                    pmd_ms_list.append((time.time() - t0) * 1000)
                    if model.order[0] == tp and model.order[2] == tq:
                        pmd_ok += 1
                except Exception:
                    pmd_ms_list.append(float("nan"))

            a_arg = art_arg_ok / REPS * 100
            a_short = art_short_ok / REPS * 100
            p_acc = pmd_ok / REPS * 100
            a_ms = np.nanmean(art_ms_list) if art_ms_list else float("nan")
            p_ms = np.nanmean(pmd_ms_list) if pmd_ms_list else float("nan")
            speed = p_ms / a_ms if a_ms and a_ms > 0 else float("nan")

            agg["art_arg"].append(a_arg); agg["art_short"].append(a_short)
            agg["pmd"].append(p_acc); agg["art_ms"].append(a_ms); agg["pmd_ms"].append(p_ms)

            print(f"{name:<18} {a_arg:6.1f}% {a_short:8.1f}% {p_acc:8.1f}% | "
                  f"{a_ms:6.2f} {p_ms:7.1f} {speed:7.0f}x")

    print("-" * len(hdr))
    print(f"{'MEDIA':<18} {np.mean(agg['art_arg']):6.1f}% {np.mean(agg['art_short']):8.1f}% "
          f"{np.mean(agg['pmd']):8.1f}% | {np.mean(agg['art_ms']):6.2f} "
          f"{np.mean(agg['pmd_ms']):7.1f} {np.mean(agg['pmd_ms'])/np.mean(agg['art_ms']):7.0f}x")
    print("\nART ms = reloj interno del CLI (identificación); pmd ms = wall time auto_arima.")
    print("ARTshort = recall del shortlist (métrica relevante para atsw-MCP).")

if __name__ == "__main__":
    main()
