"""Los numeros de cada fichero de los motores, para compararlos.

Lee lo que escriben fue (.out), drtran (.out) y art (_art.cand), y el
.inp que deriva la madre, y devuelve diccionarios planos de numeros con
nombre. Lo usan motores.py (las corridas de los motores sobre las
especificaciones de los videos) y escenas.py (los ficheros que dejan los
GUIs en el proyecto de cada escena).
"""
import math
import re

NUM = r"-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?"


# ---- fue ----------------------------------------------------------------

def fue_out(ruta):
    """Parametros (valor, e.t., indice), sigma, logelf, Q, JB, la media de
    los residuos y, de cada variable determinista con varios omegas, su
    ganancia omega(1) con su error tipico por la covarianza del .out."""
    with open(ruta, encoding="latin-1") as f:
        txt = f.read()
    r = {"convergio": bool(re.search(r"\*\*\*\* CONVERGENCE OBTAINED", txt))}
    m = re.search(r"Parameters\s*:\s*(\d+)", txt)
    r["parametros"] = int(m.group(1)) if m else None
    # los parametros, por bloques
    pars, bloque, k = [], None, 0
    ini = txt.find("Omegas for") if "Omegas for" in txt else txt.find("Coefficients for")
    fin = txt.find("Box-Cox lambda     :")
    for ln in txt[ini:fin].splitlines() if ini >= 0 and fin > 0 else []:
        h = re.match(r"(Omegas for deterministic variable (\d+)|Coefficients for (regular|annual) (AR|MA) factor (\d+)|Mean parameter \(mu\)):", ln)
        if h:
            if h.group(2):
                bloque = "omega%s" % h.group(2)
            elif h.group(3):
                bloque = "%s%s%s" % ("" if h.group(3) == "regular" else "s", h.group(4).lower(), h.group(5))
            else:
                bloque = "mu"
            k = 0
            continue
        v = re.match(r"\s+(%s)\s+\(\s*(%s)\)\s+\[\s*(\d+)\]" % (NUM, NUM), ln)
        if v and bloque:
            k += 1
            pars.append({"nombre": "%s_%d" % (bloque, k) if bloque != "mu" else "mu",
                         "valor": float(v.group(1)), "et": float(v.group(2)), "i": int(v.group(3))})
    r["par"] = pars
    for clave, pat in (("sigma", r"^sigma :\s+(%s)" % NUM), ("logelf", r"^logelf:\s+(%s)" % NUM),
                       ("jb", r"Jarque-Bera:\s+(%s)" % NUM),
                       ("media_res", r"\n\s+Mean:\s+(%s)" % NUM),
                       ("et_media_res", r"Standard error of mean:\s+(%s)" % NUM)):
        m = re.search(pat, txt, re.M)
        r[clave] = float(m.group(1)) if m else None
    # Q de Ljung-Box: las filas de la ACF de los residuos que acaban en «Q DF»
    q = []
    a = txt.find("Autocorrelation function")
    b = txt.find("Partial autocorrelation", a)
    for ln in txt[a:b].splitlines():
        m = re.match(r"\s*(\d+)\s+%s\s+[|+].*[|+]\s+(%s)\s+(\d+)\s*$" % (NUM, NUM), ln)
        if m:
            q.append({"k": int(m.group(1)), "q": float(m.group(2)), "gl": int(m.group(3))})
    r["q"] = q
    # la covarianza
    cov = {}
    a = txt.find("Estimated covariance matrix:")
    b = txt.find("Estimated correlation matrix:", a)
    if a >= 0:
        for ln in txt[a:b].splitlines():
            m = re.match(r"x\[\s*(\d+)\]\s*->\s*(.*)$", ln)
            if m:
                i = int(m.group(1))
                for j, x in enumerate(m.group(2).split(), 1):
                    cov[(i, j)] = cov[(j, i)] = float(x)
    # las ganancias: omega(B) = w0 - w1 B - ..., y fue escribe los retardos
    # con su signo de la ecuacion (el convenio de fue: el primero suma, los
    # retardos restan), asi que omega(1) = w0 - (los demas tal cual).
    gan = {}
    for p in pars:
        if p["nombre"].startswith("omega"):
            gan.setdefault(p["nombre"].split("_")[0], []).append(p)
    r["ganancias"] = {}
    for v, ps in gan.items():
        if len(ps) < 2:
            continue
        c = [1.0] + [-1.0] * (len(ps) - 1)
        g = sum(ci * p["valor"] for ci, p in zip(c, ps))
        var = sum(ci * cj * cov.get((p["i"], q_["i"]), 0.0)
                  for ci, p in zip(c, ps) for cj, q_ in zip(c, ps))
        et = math.sqrt(var) if var > 0 else None
        r["ganancias"][v] = {"valor": g, "et": et, "w": (g / et) ** 2 if et else None,
                             "omegas": [p["valor"] for p in ps]}
    return r


def inp_media(ruta):
    """(valor, libre) de la media en un .inp de fue: libre = 1 si se estima."""
    with open(ruta, encoding="latin-1") as f:
        ls = f.read().splitlines()
    for i, ln in enumerate(ls):
        if ln.startswith("** Mean parameter"):
            x = ls[i + 1].split()
            return float(x[0]), (int(x[1]) if len(x) > 1 else 0)
    return None, None


# ---- art ----------------------------------------------------------------

def art_cand(ruta):
    """Los candidatos de art: {"p0q1P0Q1": {sim, weight, aicc, propuesto,
    coeficientes}}, y cual es el propuesto (lib/artcand lee lo mismo)."""
    cands, cur = {}, None
    with open(ruta, encoding="latin-1") as f:
        for ln in f:
            x = ln.split()
            if not x:
                continue
            if x[0] == "candidate":
                d = dict(zip(x[2::2], x[3::2]))
                clave = "p%sq%sP%sQ%s" % (d["p"], d["q"], d["P"], d["Q"])
                cur = cands[clave] = {"sim": float(d["sim"]), "peso": float(d["weight"]),
                                      "aicc": float(d["aicc"]), "propuesto": d.get("proposed") == "1",
                                      "coef": {}}
            elif cur is not None and x[0] in ("phi", "theta", "Phi", "Theta"):
                cur["coef"][x[0]] = [float(v) for v in x[1:]]
    return cands


# ---- drtran -------------------------------------------------------------

def drtran_out(ruta):
    with open(ruta, encoding="latin-1") as f:
        txt = f.read()
    r = {"convergio": bool(re.search(r"\*\*\*\* (CONVERGENCE OBTAINED|STOPPED AT A POINT)", txt))}
    m = re.search(r"Log-likelihood = (%s)" % NUM, txt)
    r["loglik"] = float(m.group(1)) if m else None
    par = {}
    for m in re.finditer(r"^(\S+)\s+(%s)\s+(%s)\s+(%s)\s+%s" % (NUM, NUM, NUM, NUM), txt, re.M):
        par[m.group(1)] = {"valor": float(m.group(2)), "et": float(m.group(3)), "t": float(m.group(4))}
    r["par"] = par
    m = re.search(r"gain\s+(%s)\s+(%s)\s+(%s)" % (NUM, NUM, NUM), txt)
    if m:
        r["ganancia"] = {"valor": float(m.group(1)), "et": float(m.group(2)), "t": float(m.group(3))}
    m = re.search(r"mean lag\s+(%s)\s+(%s)" % (NUM, NUM), txt)
    if m:
        r["retardo_medio"] = {"valor": float(m.group(1)), "et": float(m.group(2))}
    m = re.search(r"noise\s+\(ACF of the residuals\)\s+p = (%s)\s+\[Q\((\d+)\) = (%s)\]" % (NUM, NUM), txt)
    if m:
        r["q_ruido"] = {"p": float(m.group(1)), "gl": int(m.group(2)), "q": float(m.group(3))}
    # la prevision: el primer mes de cada informe, por variable
    prev = {}
    for m in re.finditer(r"VARIABLE NAME\s*:\s*(\S+)\s+FORECAST ORIGIN:\s*(\S+)\s+LEAD TIME:\s*(\d+)(.*?)\+-{10,}\+\s*\n\s*\n", txt, re.S):
        filas = [ln.split() for ln in m.group(4).splitlines()]
        filas = [f for f in filas if f and re.match(r"\d+/\d{4}$", f[0]) and len(f) >= 3 and f[2] != "-"]
        if filas:
            prev[m.group(1)] = {"fecha": filas[0][0], "valor": float(filas[0][1]),
                                "et": float(filas[0][2]), "horizonte": len(filas)}
    r["prevision"] = prev
    return r


# ---- la regla de la madre para la media (lib/analisis/an_identifica.c) --

def t_media(x, lam, d, D, freq):
    """La t de la media de la serie diferenciada, con el error tipico
    corregido por su r1: la misma cuenta que media_libre() en
    lib/analisis/an_identifica.c. La madre deja libre la media del hijo con
    d + D = 1 si |t| >= 2."""
    w = [math.log(v) if lam == 0 else v for v in x]
    for k in range(d + D):
        s = 1 if k < d else freq
        w = [w[j] - w[j - s] for j in range(s, len(w))]
    n = len(w)
    m = sum(w) / n
    v = sum((a - m) ** 2 for a in w)
    c = sum((w[i] - m) * (w[i - 1] - m) for i in range(1, n)) / v
    c = max(-0.9, min(0.9, c))
    return m / math.sqrt(v / n / n * (1 + c) / (1 - c))
