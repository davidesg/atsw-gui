#!/usr/bin/env python3
"""BUG-0004 (gui/atsw): una muestra no se recorta por el principio.

Visto por el analista (16-nov-2026) con el IPC de Portugal: una muestra
«desde 1/2010 hasta 12/2019» sigue empezando en 2002, el principio de los
datos. El final si se recorta.

Por que: la muestra guarda desde y hasta (lib/proyecto, PrMuestra), pero
el .inp de su nodo de datos se genera con atsw_genera_inp
(gui/atsw/src/genera.c), que solo recibe el hasta: atsw_hasta_n cuenta las
observaciones desde el comienzo del .csv y el desde no se usa en ningun
sitio. Pasa igual al poblar la muestra (atsw_puebla_muestra) y al derivar
un modelo en ella (gestos.c). Todo lo que cuelga de la muestra --art, fue,
los casos-- se estima entonces desde el principio de los datos.

Lo que hace este guion: compila recorta.c con genera.c y lib/, sobre los
datos del manual (el IPC de España, que tambien empieza en 2002), declara
la muestra 1/2010 - 12/2019, la puebla como la madre y lee el .inp. Sale
con 1 mientras no empiece en 1/2010 con 120 observaciones; con 0 cuando si.

Con un argumento, otro .csv (fecha y una serie; la primera columna de datos
es la que se usa), mensual y que empiece antes de 2010:

    python3 repro.py OTRO.csv
"""
import os, pathlib, shutil, subprocess, sys, tempfile

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol
AQUI = pathlib.Path(__file__).resolve().parent
L = R / "lib"


def main():
    cc = os.environ.get("CC", "cc")
    gtk = subprocess.run(["pkg-config", "--cflags", "--libs", "gtk+-3.0"],
                         capture_output=True, text=True).stdout.split()
    with tempfile.TemporaryDirectory() as d:
        exe = pathlib.Path(d, "recorta")
        inc = [f"-I{R/'gui/atsw/include'}"] + [f"-I{L/x}" for x in (
            "datos", "xlsx", "proyecto", "outdiag", "outfile", "dates", "dictamen",
            "tabla", "preview", "fugdraw", "engine", "inpcheck")] + \
            [f"-I{R/'engines/fug/src'}"]
        src = [AQUI / "recorta.c", R / "gui/atsw/src/genera.c", L / "datos/datos.c",
               L / "xlsx/xlsx.c", L / "proyecto/proyecto.c", L / "outdiag/outdiag.c",
               L / "dates/dates.c", R / "engines/fug/src/inpfile.c"]
        r = subprocess.run([cc, "-O1", "-w", *inc, *map(str, src), "-o", str(exe),
                            *gtk, "-lz", "-lm"], capture_output=True, text=True)
        if r.returncode != 0:
            print("no compila:\n" + r.stderr[-2000:])
            return 2
        # solo la columna ipc de los datos del manual
        datos = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else R / "docs/manual/datos/ipc_wti.csv"
        filas = datos.read_text().splitlines()
        csv = pathlib.Path(d, "ipc.csv")
        csv.write_text("\n".join(",".join(f.split(",")[:2]) for f in filas) + "\n")
        proy = pathlib.Path(d, "p")
        proy.mkdir()
        r = subprocess.run([str(exe), str(proy), str(csv)], capture_output=True, text=True)
        if r.returncode != 0:
            print("el arnes falla:", r.stderr)
            return 2
        anio, per, nobs = map(int, r.stdout.split())
    print(f"muestra 1/2010 - 12/2019: el .inp empieza en {per}/{anio} con {nobs} observaciones")
    if (anio, per, nobs) == (2010, 1, 120):
        print("ARREGLADO: la muestra se recorta por los dos lados")
        return 0
    print("ABIERTO: debe empezar en 1/2010 con 120 observaciones")
    return 1


if __name__ == "__main__":
    sys.exit(main())
