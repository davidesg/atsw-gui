#!/usr/bin/env python3
"""Compara `drvec` con la regresion de rango reducido de Johansen.

    python3 tools/compare_johansen.py <fichero.inp> [kmax]

PARA QUE ES.  Homologacion externa de drvec: Johansen es la implementacion de
referencia de la cointegracion y `statsmodels` una implementacion sin ancestro
comun con este codigo.  El programa hace DOS comparaciones distintas, y la
distincion es el punto:

  A. LA MISMA ESPECIFICACION en los dos.  Con q = 0 y p = k+1, drvec y Johansen
     ajustan EL MISMO MODELO por dos rutas que no comparten nada: un problema de
     autovalores en forma cerrada frente a una optimizacion numerica de la
     verosimilitud exacta.  Aqui deben COINCIDIR, y la discrepancia mide lo que
     aporta el metodo -- que deberia ser casi nada.

     La correspondencia de ordenes es  p = k + 1:  drvec lleva F_1..F_{p-1}
     sobre nabla Y, o sea p-1 retardos, y Johansen lleva k.  Se barre k, no se
     comprueba en un punto: si la correspondencia fuese otra, el acuerdo se
     daria en un k por casualidad y no en todos.

  B. CADA UNO EN SU OPTIMO.  Johansen con su orden por criterio de informacion;
     drvec con la especificacion que quiera, MA incluido.  Se recorre q = 0, 1 y
     2: hasta el 19-08-2026 q >= 2 abortaba por un defecto del asignador del
     motor, asi que el barrido estaba acotado por un fallo y no por una
     decision (HOMOLOGATION.md 3b).  Se marca ademas si la raiz MA queda sobre
     la frontera de invertibilidad, porque entonces el optimo es RESTRINGIDO y
     los errores estandar no estan definidos en esa direccion.  Aqui NO tienen por que
     coincidir, y lo que se mide es cuanto cambia la respuesta al poder
     representar un MA.  Con la diagnosis de residuos de los dos lados, que es lo
     unico comparable entre ellos: AIC y BIC no lo son, porque la verosimilitud
     de Johansen es condicional y la de drvec exacta y no condicional.

CONVENCIONES.  El .inp lleva las columnas [Y2 ; Y1]; drvec normaliza en Y1 (la
ultima columna) y statsmodels en la primera, asi que el beta de Johansen se
RENORMALIZA antes de comparar.  `det_order=0` / `deterministic="ci"` es la
constante restringida a la relacion, que es el caso 2 de drvec.
"""
import subprocess
import sys

import numpy as np
from statsmodels.tsa.vector_ar.vecm import VECM, coint_johansen, select_order

DRVEC = "bin/drvec"


def read_inp(path):
    body = [l for l in open(path, errors="ignore").read().split("\n")
            if l and not l.startswith("*")]
    hdr = body[1].split()
    m, n, year = int(hdr[0]), int(hdr[1]), int(hdr[3])
    names = body[2].split()
    rows = [list(map(float, l.split())) for l in body[4:4 + n]]
    return np.asarray(rows), names, year


def _rank_sequential(jo):
    """El rango de la traza, leido COMO SE LEE: parando en el primer no rechazo.

    Aqui habia `int(sum(jo.lr1 > jo.cvt[:, 1]))`, que cuenta CUANTAS filas
    rechazan.  No es lo mismo.  El procedimiento de Johansen contrasta H0: r = 0,
    y si no la rechaza se para y devuelve 0; solo si la rechaza pasa a r <= 1.
    Contar filas coincide con eso mientras los rechazos sean un prefijo -- que es
    lo habitual y por eso duro --, y falla en cuanto una fila posterior rechaza
    con una anterior que no.

    Encontrado sobre tres IPC de la zona euro (HOMOLOGATION.md 4u): con
    det_order = 1 la fila r <= 2 da traza 4.11 contra 3.84 y las dos primeras no
    rechazan, asi que contar devuelve 1 y la secuencia devuelve 0.  Daba la
    respuesta correcta -- coincidia con lo que dice drvec -- por la razon
    equivocada, que es la unica forma en que estas cosas sobreviven.
    """
    for i in range(len(jo.lr1)):
        if jo.lr1[i] <= jo.cvt[i, 1]:      # no se rechaza H0: r <= i
            return i
    return len(jo.lr1)                     # se rechazaron todas


def joh(y, k, rank=1):
    """(rango elegido, B2 renormalizado, alpha, p portmanteau, p normalidad)."""
    jo = coint_johansen(y, det_order=0, k_ar_diff=k)
    sel = _rank_sequential(jo)
    v = VECM(y, k_ar_diff=k, coint_rank=rank, deterministic="ci").fit()
    b = np.asarray(v.beta)[:, 0]
    b2 = b[0] / b[1] if abs(b[1]) > 1e-12 else float("nan")
    a = np.asarray(v.alpha)[:, 0]
    n = len(y)
    try:
        pw = v.test_whiteness(nlags=max(k + 1, int(round(np.sqrt(n))))).pvalue
    except Exception:                                          # noqa: BLE001
        pw = float("nan")
    try:
        pn = v.test_normality().pvalue
    except Exception:                                          # noqa: BLE001
        pn = float("nan")
    return sel, b2, a, pw, pn


def drvec(stem, p, q, extra=()):
    """(B2, sd, logL, npar, p portmanteau) del ajuste de drvec."""
    cmd = [DRVEC, stem, str(p), str(q), "1", "-case", "2",
           "-multistart", "40", *extra]
    subprocess.run(cmd, capture_output=True, timeout=1800)
    txt = open(stem + ".out", errors="ignore").read()
    out = {}
    for line in txt.split("\n"):
        if line.startswith("B2 (s x r)"):
            out["_next"] = True
        elif out.pop("_next", False):
            t = line.split()
            out["b2"] = float(t[0])
            out["sd"] = float(t[2].rstrip(")")) if len(t) > 2 else float("nan")
        elif line.startswith("logelf"):
            out["ll"] = float(line.split()[2])
        elif line.startswith("npar"):
            out["npar"] = int(line.split()[2])
        elif "Q(" in line and "p-value" in line:
            out.setdefault("pw", float(line.split("p-value =")[1].strip()))
        elif "MA (Theta)" in line:
            mods = []
            for tok in line.split("MA (Theta)")[1].split():
                tok = tok.rstrip("*")
                try:
                    mods.append(float(tok))
                except ValueError:          # "inf": una raiz infinita, no es defecto
                    pass
            if mods:
                out["mamin"] = min(mods)
    return out


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    kmax = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    stem = path[:-4] if path.endswith(".inp") else path
    y, names, year = read_inp(path)

    print("%s   n=%d desde %d   columnas %s" % (path, len(y), year, names))
    print()
    print("A. LA MISMA ESPECIFICACION (q=0, p=k+1): deben coincidir")
    print("   %-3s %-4s | %-11s %-11s | %-9s | %s"
          % ("k", "p", "Johansen B2", "drvec B2", "|dif|", "rango que elige Johansen"))
    print("   " + "-" * 68)
    difs = []
    for k in range(1, kmax + 1):
        try:
            sel, b2j, _, _, _ = joh(y, k)
            d = drvec(stem, k + 1, 0)
            if "b2" not in d:
                print("   %-3d %-4d | drvec no dio B2" % (k, k + 1)); continue
            dif = abs(b2j - d["b2"])
            difs.append(dif)
            print("   %-3d %-4d | %-11.6f %-11.6f | %-9.6f | r=%d"
                  % (k, k + 1, b2j, d["b2"], dif, sel))
        except Exception as exc:                               # noqa: BLE001
            print("   %-3d %-4d | fallo: %s" % (k, k + 1, exc))
    if difs:
        print("   -> |dif| media %.6f, maxima %.6f sobre %d ordenes"
              % (np.mean(difs), max(difs), len(difs)))

    print()
    print("B. CADA UNO EN SU OPTIMO")
    try:
        so = select_order(y, maxlags=min(6, len(y) // 12), deterministic="ci")
        for crit in ("aic", "bic"):
            kv = max(1, getattr(so, crit))
            sel, b2j, a, pw, pn = joh(y, kv)
            print("   Johansen [%s] k=%d: r=%d  B2=%+.6f  alpha=(%+.4f,%+.4f)"
                  "  portm.p=%.4f  norm.p=%.4f"
                  % (crit, kv, sel, b2j, a[0], a[1], pw, pn))
    except Exception as exc:                                   # noqa: BLE001
        print("   seleccion de orden fallida: %s" % exc)
    for (p, q) in ((2, 0), (2, 1), (2, 2)):
        d = drvec(stem, p, q)
        if "b2" in d:
            mm = d.get("mamin")
            flag = ""
            if mm is not None and mm < 1.0001:
                flag = "  <-- raiz MA en la frontera: optimo RESTRINGIDO"
            print("   drvec  (p=%d,q=%d)   : B2=%+.6f (sd %.6f)  logL=%.4f  "
                  "npar=%d  portm.p=%s%s"
                  % (p, q, d["b2"], d.get("sd", float("nan")), d.get("ll", 0),
                     d.get("npar", 0), d.get("pw", "?"), flag))
    print()
    print("   AIC/BIC no se comparan entre los dos: la verosimilitud de Johansen")
    print("   es condicional y la de drvec exacta y no condicional.  Lo comparable")
    print("   es la diagnosis de residuos.")


if __name__ == "__main__":
    main()
