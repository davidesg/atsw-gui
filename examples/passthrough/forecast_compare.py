#!/usr/bin/env python3
"""Compara dos modelos por su previsión OUT-OF-SAMPLE: ratio B/A + Diebold-Mariano.

Lee dos CSV de `drtran -C` (columnas: origin,horizon,actual,forecast,error) -- el
backtest recursivo de PARÁMETROS FIJOS (`-estwin`), con orígenes balanceados
e = train_end .. n-H (cada origen tiene H reales de test). Computa, por horizonte h:
RMSE_A, RMSE_B, ratio B/A, y el test de Diebold-Mariano (pérdida cuadrática, HAC
rectangular hasta h-1, corrección de muestra pequeña Harvey-Leybourne-Newbold 1997).

Convención: DM>0  <=>  ratio<1  <=>  B predice mejor que A.

Uso:  forecast_compare.py A.csv B.csv [--horizons 1,2,6,12,24] [--labels uni,transfer]
      (réplica del sps/forecast_compare.py de SF_MEG, sobre los CSV del motor C.)
"""
import argparse
import numpy as np
import pandas as pd
from scipy.stats import t


def dm(lossA, lossB, h):
    """Diebold-Mariano sobre la diferencia de pérdidas cuadráticas d=lossA-lossB.
    HAC rectangular hasta el retardo h-1 y corrección HLN. dbar>0 => B mejor."""
    d = np.asarray(lossA, float) - np.asarray(lossB, float)
    N = d.size
    dbar = d.mean()
    dc = d - dbar
    lrv = float(dc @ dc) / N
    for k in range(1, h):
        lrv += 2.0 * float(dc[k:] @ dc[:-k]) / N
    if lrv <= 0:                      # varianza degenerada: cae a la iid
        lrv = float(dc @ dc) / N
        if lrv <= 0:
            return float("nan"), float("nan")
    stat = dbar / np.sqrt(lrv / N)
    stat *= np.sqrt(max((N + 1 - 2 * h + h * (h - 1) / N) / N, 1e-9))   # HLN
    return float(stat), float(2.0 * t.sf(abs(stat), df=N - 1))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csvA")
    ap.add_argument("csvB")
    ap.add_argument("--horizons", default="1,2,6,12,24")
    ap.add_argument("--labels", default="A,B")
    ap.add_argument("--out", default="")
    args = ap.parse_args()

    hs = [int(x) for x in args.horizons.split(",")]
    la, lb = args.labels.split(",")
    A = pd.read_csv(args.csvA)
    B = pd.read_csv(args.csvB)
    M = A.merge(B, on=["origin", "horizon"], suffixes=("_A", "_B"))

    print(f"\nOut-of-sample: {la} (A) vs {lb} (B)   "
          f"ratio = RMSE_B/RMSE_A;  DM>0 => B mejor;  p = Diebold-Mariano (HLN)")
    print(f"  N      = nº de ERR (orígenes balanceados) que promedia cada RMSE(h)")
    print(f"  N_ef   = N/h  ~  bloques NO solapados; a horizonte largo el DM se apoya")
    print(f"           en muy pocos datos independientes (las previsiones solapan h-1).")
    print(f"{'h':>3} {'N':>4} {'N_ef':>6} {'RMSE_'+la:>11} {'RMSE_'+lb:>11} "
          f"{'ratio':>7} {'DM':>7} {'p':>8}")
    rows = []
    for h in hs:
        mh = M[M["horizon"] == h]
        eA = mh["error_A"].values
        eB = mh["error_B"].values
        rA = float(np.sqrt((eA ** 2).mean()))
        rB = float(np.sqrt((eB ** 2).mean()))
        stat, p = dm(eA ** 2, eB ** 2, h)
        N = len(mh)
        neff = N / h                    # bloques no solapados de h pasos
        flag = "**" if p < 0.05 else ("* " if p < 0.10 else "  ")
        print(f"{h:>3} {N:>4} {neff:>6.1f} {rA:11.4f} {rB:11.4f} {rB/rA:7.3f} "
              f"{stat:7.2f} {p:7.4f}{flag}")
        rows.append(dict(h=h, N=N, n_eff=neff, rmse_A=rA, rmse_B=rB,
                         ratio=rB / rA, dm=stat, p=p))
    if args.out:
        pd.DataFrame(rows).to_csv(args.out, index=False)
        print(f"\n-> {args.out}")


if __name__ == "__main__":
    main()
