#!/usr/bin/env python3
"""BUG-0009 (drvarma): when the diagonal system cannot be evaluated at the
univariate optima, the gate printed "joint diagonal 0.000000" and a
"difference" the size of the SUM, and blamed the cast. It did not say that
the joint likelihood failed, nor the likely cause in sight: an own MA factor
of a univariate optimum on the invertibility wall.

The case (drvarma-python BUG-0020): the echelon study's system (2,2,1),
rep 45, on route V's specifications. y2's univariate MA(2) ends with an
inverse root at 0.99995, inside chekma's tolerance: alone elf accepts it; the
diagonal system of three fails elf (ifault 3, the Cholesky of mtmp0).

Exits 1 while the gate does not say that the joint diagonal was not
evaluated and name y2's MA on the wall, 0 when it does, 2 if the case does
not fail the gate (then it says nothing). It does not depend on git.
"""
import pathlib, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for f in ("y1.inp", "y2.inp", "y3.inp"):
        shutil.copy(AQUI / f, d / f)
    r = subprocess.run([str(BIN), "y1.inp", "y2.inp", "y3.inp", "0", "0", "-o", "esc"],
                       cwd=d, capture_output=True, text=True, errors="replace", timeout=900)
    out = r.stdout + r.stderr
    sal = (d / "esc.out").read_text(errors="replace") if (d / "esc.out").exists() else ""

if "GATE: FAILED" not in out:
    print("the case does not fail the gate (exit %d)" % r.returncode)
    sys.exit(2)
dice = ("not evaluated" in out and "invertibility wall" in out and "y2 (" in out
        and "not evaluated" in sal and "invertibility wall" in sal)
print("\n".join(l for l in out.splitlines() if "joint diagonal" in l or "ERROR" in l
                or "wall" in l))
if not dice:
    print("ABIERTO: the gate does not say the joint diagonal failed, nor y2's MA on the wall")
    sys.exit(1)
print("cerrado: the gate says why")
sys.exit(0)
