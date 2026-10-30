#!/usr/bin/env python3
"""El macOS minimo de un .dmg, el mismo en la web y en el LEEME.

    comprueba_textos.py <nombre> <minimo>

<nombre> es el del .dmg (arm64, x86_64, x86_64-macos11) y <minimo> el macOS
que promete (14.0, 15.0, 11.0): el de la matriz de paquetes.yml, que es el
que bundle.sh hace cumplir a los binarios. Aqui se mira que los dos textos que
lee el usuario digan ese mismo: el <li> de web/index.html cuyo data-patron es
el de ese .dmg, y la linea «<nombre>» de packaging/LEEME.txt. Paso en la 0.2.3:
el runner de Intel paso a macOS 15, el .dmg pedia 15 y los textos decian 13.
"""
import pathlib
import re
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[2]


def main():
    nombre, minimo = sys.argv[1], sys.argv[2]
    mayor = minimo.split(".")[0]
    dice = re.compile(r"macOS %s\b" % re.escape(mayor))
    mal = []

    web = (RAIZ / "web" / "index.html").read_text(encoding="utf-8")
    patron = 'data-patron="macos-%s\\.dmg$"' % nombre
    m = re.search(re.escape(patron) + r".*?</li>", web, re.S)
    if m is None:
        mal.append("web/index.html: no hay <li> con %s" % patron)
    elif not dice.search(re.sub(r"<[^>]+>", " ", m.group(0))):
        mal.append("web/index.html: el de %s no dice «macOS %s»" % (nombre, mayor))

    leeme = (RAIZ / "packaging" / "LEEME.txt").read_text(encoding="utf-8")
    linea = [l for l in leeme.splitlines() if l.strip().startswith("«%s»" % nombre)]
    if not linea:
        mal.append("packaging/LEEME.txt: no hay linea «%s»" % nombre)
    elif not dice.search(linea[0]):
        mal.append("packaging/LEEME.txt: la de «%s» no dice «macOS %s»" % (nombre, mayor))

    for m in mal:
        print("::error::%s" % m)
    if not mal:
        print("%s: macOS %s, lo mismo en la web y en el LEEME" % (nombre, mayor))
    return 1 if mal else 0


if __name__ == "__main__":
    sys.exit(main())
