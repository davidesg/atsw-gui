#!/usr/bin/env python3
"""BUG-0002 (fuf): la tabla de previsión escribe los campos pegados.

forecast_table_acii (src/usfo.c) escribe sus campos con anchos fijos y sin
separador («%9.4f%7.4f%8.4f…»). Cuando un número no cabe en su ancho se come
el espacio y se junta con el de al lado:

  - un nivel de 1000 o más, con la fecha:     «  1/20202000.1234»;
  - una desviación típica de 10 o más, con el campo anterior.

Se ve con un modelo limpio: forecast_D.1 del corpus (logaritmos, d = 1),
cuya tabla sale bien, y la misma serie multiplicada por 20 (niveles de unos
2000). En logaritmos multiplicar la serie por una constante no cambia el
modelo de las diferencias: las variaciones son las mismas y el nivel se
multiplica por 20. Lo único que cambia es el ancho del nivel, y la tabla
deja de poderse leer: a ojo, y a máquina por espacios.

Sale con 1 mientras el fallo siga abierto, con 0 cuando esté arreglado.
"""
import pathlib, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
FUF = AQUI.parents[1] / "bin" / "fuf"
INP = AQUI / "forecast_D.1.inp"
FECHA = re.compile(r"^\s*\d{1,2}/\d{4}")


def corre(d, factor):
    """La tabla de previsión de fuf con la serie multiplicada por factor."""
    lineas = INP.read_text().split("\n")
    i = next(k for k, l in enumerate(lineas) if l.startswith("** Time series"))
    datos = [f"{float(l) * factor:.10f} " if l.strip() else l
             for l in lineas[i + 1:]]
    (d / "x.inp").write_text("\n".join(lineas[:i + 1] + datos))
    subprocess.run([str(FUF), "x"], cwd=d, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
    txt = (d / "x.out").read_text(errors="replace")
    tabla = txt[txt.index("FORECAST REPORT"):].split("\n")
    return [l for l in tabla if FECHA.match(l)]


def pegadas(filas):
    """Las filas que no se parten en sus campos por espacios: fecha y siete
    campos en lo observado (los «-» cuentan), fecha y seis en la previsión."""
    malas = []
    for f in filas:
        c = f.split()
        esperados = 8 if "-" in c else 7      # observada o prevista
        if len(c) != esperados or not re.fullmatch(r"\d{1,2}/\d{4}", c[0]):
            malas.append(f)
    return malas


if not FUF.exists():
    sys.exit(f"falta {FUF}: compila fuf (make en engines/fuf)")

with tempfile.TemporaryDirectory() as t:
    d = pathlib.Path(t)
    uno = corre(d, 1)
    veinte = corre(d, 20)

print(f"serie tal cual: {len(pegadas(uno))} de {len(uno)} filas pegadas")
print(f"serie por 20:   {len(pegadas(veinte))} de {len(veinte)} filas pegadas")
for f in pegadas(veinte)[:3]:
    print("   ", f.rstrip())

if pegadas(uno):
    sys.exit("el control falla: la tabla de la serie tal cual ya sale pegada")
abierto = bool(pegadas(veinte))
print("ABIERTO" if abierto else "cerrado")
sys.exit(1 if abierto else 0)
