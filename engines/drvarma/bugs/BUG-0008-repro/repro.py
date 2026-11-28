#!/usr/bin/env python3
"""BUG-0008 (drvarma): -deseason resta el patron estacional con la fase
corrida cuando la serie no empieza en el subperiodo 1.

deseasonalize_raw (src/deseason.c) estima los armonicos con t = i + d + 1,
en fase RELATIVA al primer dato, y resta las dummies en fase ABSOLUTA,
(i + start_sub - 1) % s. Solo coinciden con start_sub = 1; si no, el patron
se resta corrido start_sub - 1 meses y el ajuste ANADE varianza estacional.
Es silencioso. El porte (drvarma-python) ya estaba arreglado: era su
BUG-0001.

El caso: IPC_ES (log, d = 1, AR(1) con media), 2002-2019, con -deseason
force -forecast 12. Una vez desde enero de 2002 (216 datos) y otra quitando
el primer dato, desde febrero (215). Las dos previsiones cubren 1/2020 a
12/2020, en niveles reestacionalizados (Level). Su tasa mensual,
100 log(L_t / L_t-1), es casi toda estacional: con la fase bien las dos
coinciden; con la fase corrida el perfil sale desplazado.

Sale con 1 mientras la diferencia de esas tasas pase del 25 % de su amplitud,
con 0 cuando no, y con 2 si el caso no vale. No depende del estado de git.
"""
import math, pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
BIN = AQUI.parents[1] / "bin" / ("drvarma.exe" if sys.platform == "win32" else "drvarma")
if not BIN.exists():
    sys.exit(f"falta {BIN}: make -C engines/drvarma")

ORIG = (AQUI / "IPC_ES.inp").read_text().splitlines()


def febrero(lineas):
    """El mismo .inp sin el primer dato: 215 observaciones desde 2/2002."""
    out, i = list(lineas), lineas.index("** Data:")
    cab = lineas.index("** Series, observations, start (subperiod year):") + 1
    k, n, sub, anio = lineas[cab].split()
    assert (n, sub) == ("216", "1"), lineas[cab]
    out[cab] = f" {k} 215 2 {anio}"
    del out[i + 1]
    return out


def mensual(texto):
    """{fecha: 100 log(L_t / L_t-1)} del nivel previsto de la serie 1."""
    filas = re.findall(r"^\s+(\d+/\d{4})\s+(-?[\d.]+)", texto, re.M)
    return {f: 100 * math.log(v / u)
            for (_, u), (f, v) in zip([(a, float(b)) for a, b in filas],
                                      [(a, float(b)) for a, b in filas][1:])}


res = {}
with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    for nombre, lineas in (("ENE", ORIG), ("FEB", febrero(ORIG))):
        (d / f"{nombre}.inp").write_text("\n".join(lineas) + "\n")
        subprocess.run([str(BIN), nombre, "1", "0", "-mean", "-deseason", "force",
                        "-forecast", "12"], cwd=d, capture_output=True, text=True,
                       errors="replace", timeout=600)
        f = d / f"{nombre}.forecast"
        res[nombre] = mensual(f.read_text(errors="replace")) if f.exists() else {}

ene, feb = res["ENE"], res["FEB"]
comunes = sorted(set(ene) & set(feb), key=lambda s: (s.split("/")[1], int(s.split("/")[0])))
if len(comunes) < 11:
    print("el caso no vale: previsiones", len(ene), len(feb))
    sys.exit(2)
amp = max(ene[c] for c in comunes) - min(ene[c] for c in comunes)
dif = max(abs(ene[c] - feb[c]) for c in comunes)
for c in comunes:
    print(f"  {c:>8}  desde enero {ene[c]:8.4f}   desde febrero {feb[c]:8.4f}")
print(f"amplitud de la tasa {amp:.4f}; mayor diferencia {dif:.4f} ({100 * dif / amp:.0f} %)")
if amp <= 0:
    print("el caso no vale: sin patron estacional")
    sys.exit(2)
if dif > 0.25 * amp:
    print("ABIERTO: el patron estacional sale con la fase corrida desde febrero")
    sys.exit(1)
print("cerrado: el mismo patron desde enero y desde febrero")
sys.exit(0)
