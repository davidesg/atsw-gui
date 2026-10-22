#!/usr/bin/env python3
"""hacer.py -- los iconos de los paquetes, del mismo dibujo que la web.

El dibujo es el de packaging/linux/atsw.svg y el <link rel="icon"> de
web/index.html: el cuadrado azul #1f5fa8 de esquinas redondeadas con la
quebrada blanca. Dentro de los programas lo dibuja lib/barra/iconos.c
(at_logo) con las mismas medidas; esto hace los ficheros que no se pueden
dibujar en marcha:

    lib/windows/atsw.ico      el icono de los .exe (utf8.rc) y del instalador
    packaging/macos/atsw.icns el de ATSW.app

Se corre a mano cuando cambia el dibujo, y lo que sale se versiona: los
paquetes no tienen que llevar pycairo ni Pillow para construirse.

    python3 packaging/icono/hacer.py
"""

import io
import math
import os

import cairo
from PIL import Image

RAIZ = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

# las medidas de atsw.svg, en su viewBox de 64
LADO, RADIO, TRAZO = 64.0, 14.0, 6.0
QUEBRADA = [(8, 44), (20, 28), (30, 36), (42, 16), (56, 24)]


def dibuja(px):
    s = cairo.ImageSurface(cairo.FORMAT_ARGB32, px, px)
    c = cairo.Context(s)
    c.scale(px / LADO, px / LADO)
    r = RADIO
    c.new_sub_path()
    c.arc(LADO - r, r, r, -math.pi / 2, 0)
    c.arc(LADO - r, LADO - r, r, 0, math.pi / 2)
    c.arc(r, LADO - r, r, math.pi / 2, math.pi)
    c.arc(r, r, r, math.pi, 3 * math.pi / 2)
    c.close_path()
    c.set_source_rgb(0x1f / 255, 0x5f / 255, 0xa8 / 255)
    c.fill()
    c.move_to(*QUEBRADA[0])
    for p in QUEBRADA[1:]:
        c.line_to(*p)
    c.set_source_rgb(1, 1, 1)
    c.set_line_width(TRAZO)
    c.set_line_cap(cairo.LINE_CAP_ROUND)
    c.set_line_join(cairo.LINE_JOIN_ROUND)
    c.stroke()
    b = io.BytesIO()
    s.write_to_png(b)
    b.seek(0)
    return Image.open(b).convert("RGBA")


def main():
    grande = dibuja(1024)
    ico = os.path.join(RAIZ, "lib", "windows", "atsw.ico")
    # cada tamaño dibujado a su resolucion: reducido de uno grande, el de
    # 16 sale borroso
    tam = (16, 24, 32, 48, 64, 128, 256)
    dibuja(256).save(ico, sizes=[(n, n) for n in tam],
                     append_images=[dibuja(n) for n in tam[:-1]])
    icns = os.path.join(RAIZ, "packaging", "macos", "atsw.icns")
    grande.save(icns)
    print(ico)
    print(icns)


if __name__ == "__main__":
    main()
