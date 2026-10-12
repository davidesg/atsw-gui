#!/usr/bin/env python3
"""LOS .xlsx DE PRUEBA, desde los datos del manual (docs/manual/datos/ipc_wti.csv):

    python3 lib/datos/pruebas/fabrica.py

Cada libro guarda LO MISMO que el CSV escrito de otra manera, que es lo que
llega de verdad a la puerta de los datos. test_xlsx.c los compara con el CSV
valor a valor. Hacen falta openpyxl y LibreOffice (soffice). Los .xlsx se
guardan en el repositorio: asi la bateria no depende de ninguno de los dos y
corre igual en Linux, macOS y Windows.

  openpyxl-fecha.xlsx    fechas de Excel (un numero con estilo), "mmm-yy"
  openpyxl-es.xlsx       el formato propio de un Excel en espanol,
                         [$-C0A]mmm\\-yy;@
  openpyxl-texto.xlsx    la fecha como TEXTO, "2002-01" (texto compartido)
  libreoffice.xlsx       el CSV abierto y guardado por LibreOffice: otro
                         escritor, otros estilos y otro XML

Y docs/manual/datos/ipc_wti.xlsx, que es openpyxl-fecha.xlsx: los datos del
manual para quien trabaja con Excel.
"""
import csv, datetime, os, shutil, subprocess, sys, tempfile

import openpyxl

AQUI = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(AQUI, "..", "..", "..", "docs", "manual", "datos", "ipc_wti.csv")


def filas():
    with open(CSV, newline="") as f:
        r = csv.reader(f)
        cab = next(r)
        return cab, [(datetime.date.fromisoformat(a), float(b), float(c)) for a, b, c in r]


def libro(nombre, fecha, formato=None):
    cab, fs = filas()
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "datos"
    ws.append(["Fecha", cab[1], cab[2]])
    for d, a, b in fs:
        ws.append([fecha(d), a, b])
    if formato:
        for c in ws["A"][1:]:
            c.number_format = formato
    wb.save(os.path.join(AQUI, nombre))


def main():
    libro("openpyxl-fecha.xlsx", lambda d: d, "mmm-yy")
    # El mismo, para el alumno que trabaja con Excel: va con el manual.
    shutil.copy(os.path.join(AQUI, "openpyxl-fecha.xlsx"),
                os.path.join(os.path.dirname(CSV), "ipc_wti.xlsx"))
    libro("openpyxl-es.xlsx", lambda d: d, "[$-C0A]mmm\\-yy;@")
    libro("openpyxl-texto.xlsx", lambda d: "%d-%02d" % (d.year, d.month))
    soffice = shutil.which("soffice") or shutil.which("libreoffice")
    if not soffice:
        sys.exit("sin LibreOffice: libreoffice.xlsx no se rehace")
    with tempfile.TemporaryDirectory() as t:
        shutil.copy(CSV, os.path.join(t, "libreoffice.csv"))
        subprocess.run([soffice, "-env:UserInstallation=file://" + t + "/perfil",
                        "--headless", "--convert-to", "xlsx", "--outdir", t,
                        os.path.join(t, "libreoffice.csv")], check=True,
                       stdout=subprocess.DEVNULL)
        shutil.copy(os.path.join(t, "libreoffice.xlsx"), AQUI)


if __name__ == "__main__":
    main()
