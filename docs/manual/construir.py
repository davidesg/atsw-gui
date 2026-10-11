#!/usr/bin/env python3
"""El manual en HTML y en PDF, desde MANUAL.md:

    python3 docs/manual/construir.py

Deja docs/manual/MANUAL.html (con las imagenes incrustadas: un solo fichero
que se puede mandar por correo) y, si hay Chromium o Chrome, MANUAL.pdf.
Necesita el modulo «markdown» de Python (pip install markdown).
"""

import base64
import os
import re
import shutil
import subprocess
import sys

import markdown

AQUI = os.path.dirname(os.path.abspath(__file__))

CSS = """
@page { size: A4; margin: 18mm 16mm 18mm 16mm; }
:root { --tinta: #1d2430; --suave: #5b6676; --raya: #d9dee5; --fondo: #f5f7fa;
        --acento: #1f5fa8; }
* { box-sizing: border-box; }
body { font-family: "Source Sans 3", "Segoe UI", "Helvetica Neue", Arial, sans-serif;
       color: var(--tinta); line-height: 1.55; font-size: 11pt; max-width: 46rem;
       margin: 2rem auto; padding: 0 1rem; background: #fff; }
h1 { font-size: 2em; margin: 0 0 .2em; color: var(--acento); }
h2 { font-size: 1.45em; margin-top: 2.2em; padding-bottom: .2em;
     border-bottom: 2px solid var(--raya); break-after: avoid; }
h3 { font-size: 1.15em; margin-top: 1.6em; break-after: avoid; }
p, li { orphans: 3; widows: 3; }
img { display: block; max-width: 100%; margin: 1em auto; border: 1px solid var(--raya);
      border-radius: 6px; box-shadow: 0 2px 8px rgba(0,0,0,.08); break-inside: avoid; }
table { border-collapse: collapse; width: 100%; margin: 1em 0; font-size: .95em;
        break-inside: avoid; }
th, td { border: 1px solid var(--raya); padding: .35em .6em; text-align: left;
         vertical-align: top; }
th { background: var(--fondo); }
blockquote { margin: 1.2em 0; padding: .6em 1em; background: var(--fondo);
             border-left: 4px solid var(--acento); color: var(--tinta); break-inside: avoid; }
blockquote p { margin: .3em 0; }
code { font-family: "JetBrains Mono", Consolas, monospace; font-size: .92em;
       background: var(--fondo); padding: .05em .3em; border-radius: 3px; }
pre { background: var(--fondo); padding: .8em 1em; border-radius: 6px; overflow-x: auto;
      break-inside: avoid; }
pre code { background: none; padding: 0; }
hr { border: 0; border-top: 1px solid var(--raya); margin: 2em 0; }
a { color: var(--acento); }
@media print { body { margin: 0; max-width: none; } a { color: inherit; text-decoration: none; }
               img { max-height: 115mm; width: auto; } }
"""


def incrusta(html):
    """Las imagenes, dentro del HTML como data: URI."""
    def cambia(m):
        ruta = os.path.join(AQUI, m.group(1))
        with open(ruta, "rb") as f:
            datos = base64.b64encode(f.read()).decode("ascii")
        return 'src="data:image/png;base64,%s"' % datos
    return re.sub(r'src="(img/[^"]+\.png)"', cambia, html)


def chromium():
    for p in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser"):
        if shutil.which(p):
            return p
    return None


def main():
    with open(os.path.join(AQUI, "MANUAL.md"), encoding="utf-8") as f:
        md = f.read()
    from markdown.extensions.toc import slugify_unicode
    cuerpo = markdown.markdown(md, extensions=["tables", "fenced_code", "toc"],
                               extension_configs={"toc": {"slugify": slugify_unicode}})
    # Los enlaces del indice: markdown pone a los titulos ids con su texto; los
    # del indice ya vienen escritos con el mismo convenio (el de GitHub).
    html = ("<!doctype html>\n<html lang=\"es\"><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            "<title>ATSW — manual para el alumno</title><style>%s</style></head>"
            "<body>%s</body></html>\n" % (CSS, incrusta(cuerpo)))
    salida = os.path.join(AQUI, "MANUAL.html")
    with open(salida, "w", encoding="utf-8") as f:
        f.write(html)
    print("escrito", salida)

    c = chromium()
    if c is None:
        print("sin Chromium ni Chrome: no hay PDF")
        return 0
    pdf = os.path.join(AQUI, "MANUAL.pdf")
    orden = [c, "--headless", "--disable-gpu", "--no-pdf-header-footer"]
    # En la CI (GitHub Actions) el sandbox de Chrome no arranca.
    if os.environ.get("CI"):
        orden.append("--no-sandbox")
    subprocess.run(orden + ["--print-to-pdf=" + pdf, "file://" + salida],
                   check=True, stderr=subprocess.DEVNULL)
    print("escrito", pdf)
    return 0


if __name__ == "__main__":
    sys.exit(main())
