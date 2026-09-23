#!/usr/bin/env python3
"""Build drvec .inp files for the Lutkepohl (e1, e3) and Rao (Raotbl1..7) cases.

Specification mirrors the reference ca.jo runs (K=2, ecdet='const'):
  * Lutkepohl e1: log(invest, income, cons), 1960Q1-1982Q4, 92 obs.
  * Lutkepohl e3: log M1, log GNP, rd, rb (rates in levels), 1954Q1-1987Q4, 136 obs.
  * Rao: every column of urca's CSV exactly as ca.jo used it (NA rows dropped
    listwise, as ca.jo does), no transformation.
Column order is [Y2 ; Y1]: the LAST column is the Y1 (normalisation) variable
for r=1.  Output: <case>.inp in this directory.
"""
import csv, math, os
HERE = os.path.dirname(os.path.abspath(__file__))
DS = "/home/david/Dropbox/SRC/drvec/datasets"

def read_dat(path):
    lines = open(path).read().split("\n")
    i = [k for k, l in enumerate(lines) if l.startswith("<")][0]
    names = lines[i + 1].split()
    rows = [list(map(float, l.split())) for l in lines[i + 2:] if l.strip()]
    return names, rows

def write_inp(stem, names, rows, freq, sub, year, note):
    with open(os.path.join(HERE, stem + ".inp"), "w") as f:
        f.write("* %s\n" % note)
        f.write("%d\n%d %d %d %d\n" % (freq, len(names), len(rows), sub, year))
        f.write(" ".join(names) + "\n1.0 0 0\n")
        for r in rows:
            f.write(" ".join("%.10g" % v for v in r) + "\n")

def reorder(names, rows, order):
    idx = [names.index(o) for o in order]
    return order, [[r[i] for i in idx] for r in rows]

# --- Lutkepohl e1: logs, Y1 = cons (last)
n, r = read_dat(DS + "/lutkepohl/e1.dat")
r = [[math.log(v) for v in row] for row in r]
n = ["linvest", "lincome", "lcons"]
write_inp("e1", n, r, 4, 1, 1960, "Lutkepohl e1 logs invest income cons 1960Q1-1982Q4; Y1=lcons")
# same data, Y1 = invest (ca.jo's normalisation variable) for a normalisation check
n2, r2 = reorder(n, r, ["lincome", "lcons", "linvest"])
write_inp("e1_yinv", n2, r2, 4, 1, 1960, "e1, Y1=linvest")

# --- Lutkepohl e3: log M1, log gnp, rd, rb ; Y1 = lM1 (last)
n, r = read_dat(DS + "/lutkepohl/e3.dat")
r = [[math.log(a), math.log(b), c, d] for a, b, c, d in r]
n, r = reorder(["lM1", "lgnp", "rd", "rb"], r, ["lgnp", "rd", "rb", "lM1"])
write_inp("e3", n, r, 4, 1, 1954, "Lutkepohl e3 log M1, log gnp, rd, rb 1954Q1-1987Q4; Y1=lM1")

# --- Rao tables, exactly ca.jo's data (listwise NA drop); Y1 = FIRST csv column
starts = {1: (4, 1953, 1), 2: (4, 1953, 1), 3: (4, 1966, 4), 4: (1, 1870, 1),
          5: (1, 1870, 1), 6: (4, 1959, 1), 7: (4, 1956, 1)}
for i in range(1, 8):
    with open("%s/urca_Raotbl%d.csv" % (DS, i)) as f:
        rd = list(csv.reader(f))
    names, body = rd[0], rd[1:]
    keep = [(k, row) for k, row in enumerate(body) if "NA" not in row]
    first = keep[0][0]
    rows = [[float(v) for v in row] for _, row in keep]
    freq, y, s = starts[i]
    # advance start by the dropped leading rows
    t = (y * freq + s - 1) + first
    y, s = divmod(t, freq); s += 1
    order = names[1:] + names[:1]          # first csv column -> Y1
    names2, rows2 = reorder(names, rows, order)
    write_inp("rao%d" % i, names2, rows2, freq, s, y,
              "urca Raotbl%d as used by ca.jo (NA rows dropped: %d); Y1=%s" % (i, first, names[0]))
print("ok")

# --- rescaled copies (each column divided by the sd of its first difference).
# Johansen's statistics and exact-ML LRs are invariant to this; it isolates
# numerical scaling from the model.  Factors are written in the comment line.
import statistics
for c in ("rao6", "rao7", "rao2", "e3"):
    L = [l for l in open(os.path.join(HERE, c + ".inp"))]
    body = [l for l in L if not l.startswith("*")]
    hdr, rows = body[:4], [list(map(float, l.split())) for l in body[4:] if l.strip()]
    M = len(rows[0])
    sd = [statistics.pstdev([rows[t][j] - rows[t - 1][j] for t in range(1, len(rows))]) for j in range(M)]
    with open(os.path.join(HERE, c + "_sc.inp"), "w") as f:
        f.write("* %s rescaled: column j divided by %s\n" % (c, " ".join("%.6g" % s for s in sd)))
        f.writelines(hdr)
        for r in rows:
            f.write(" ".join("%.10g" % (v / s) for v, s in zip(r, sd)) + "\n")

# --- rao3 without the three dummies (lc, li, lw), Y1 = lc: the closest drvec can
# get to Holden & Perman's system, since drvec's .inp route has no exogenous
# dummies (they would have to come in as fue interventions via -interv/.pre).
L = [l for l in open(os.path.join(HERE, "rao3.inp"))]
body = [l for l in L if not l.startswith("*")]
names = body[2].split(); idx = [names.index(v) for v in ("li", "lw", "lc")]
rows = [l.split() for l in body[4:] if l.strip()]
with open(os.path.join(HERE, "rao3_3v.inp"), "w") as f:
    f.write("* Raotbl3 lc li lw only (dummies dropped); Y1 = lc\n")
    f.write(body[0]); f.write("3 %s" % body[1].split(None, 1)[1]); f.write("li lw lc\n"); f.write(body[3])
    for r in rows: f.write(" ".join(r[i] for i in idx) + "\n")
