#!/usr/bin/env python3
"""BUG-0002 (gui/atsw): instalada con el .deb, la madre no encuentra a fug y
el vistazo no se abre.

Visto en la revision de v0.2.2 con los casos de los videos
(.github/workflows/revision-casos.yml): en Linux, con el .deb instalado y
/usr/lib/atsw/bin fuera del PATH --como queda al abrir ATSW desde el menu o
con `atsw`--, «Serie y ACF / PACF…» y «Media – desviación típica» no abren
su ventana. La escena del video del IPC subyacente se para en la linea
«ventana vistazo». Con /usr/lib/atsw/bin en el PATH, si se abre.

Por que: el vistazo busca a fug con sitio_busca(), y su lista de sitios
(gui/atsw/src/vistazo.c, lanza_fug) solo tiene el del arbol de compilacion,
"../../engines/fug/%s". Le falta el del paquete, "./%s" (todos los programas
en el mismo directorio, packaging/programas.sh), que si tienen las demas
listas: gestos.c, an_identifica.c (art), an_formales.c (fue)... Sin el, cae al
PATH:

  - en Linux el PATH no tiene /usr/lib/atsw/bin: fug no se encuentra, o se
    encuentra otro mas viejo (un /usr/local/bin/fug de una instalacion
    anterior: se cuela sin avisar, como ya paso con fue_gui);
  - en Windows no se ve, porque g_find_program_in_path mira primero el
    directorio del propio .exe;
  - en macOS tampoco, porque el lanzador de la .app pone Resources/bin el
    primero del PATH.

Lo que mira este guion es la lista de sitios de lanza_fug(): resuelve cada
sitio como lo haria sitio_busca() en la disposicion del paquete
(DIR/atsw_gui y DIR/fug, nada en el PATH) y comprueba que alguno da con fug.
Sale con 1 mientras ninguno lo encuentre, con 0 cuando el vistazo lo
encuentre a su lado. Lo que pasa en la pantalla lo prueba la fila «Escena
extra/vistazo-sin-path» de revision-casos.yml.
"""
import os, pathlib, re, sys, tempfile

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol


def main():
    src = (R / "gui/atsw/src/vistazo.c").read_text(encoding="utf-8")
    m = re.search(r"static\s+gboolean\s+lanza_fug\s*\(.*?\n\}", src, re.S)
    if not m:
        print("no encuentro lanza_fug() en vistazo.c")
        return 1
    cuerpo = m.group(0)
    lista = re.search(r"sitio\[\]\s*=\s*\{(.*?)\};", cuerpo, re.S)
    if not lista or "sitio_busca" not in cuerpo:
        print("lanza_fug() ya no busca con sitio_busca y una lista: mirar a mano")
        return 1
    sitios = re.findall(r'"([^"]*)"', lista.group(1))
    print("sitios de lanza_fug():", sitios)

    # el paquete: todo en un directorio (packaging/programas.sh)
    with tempfile.TemporaryDirectory() as d:
        for p in ("atsw_gui", "fug"):
            f = pathlib.Path(d, p)
            f.write_text("")
            f.chmod(0o755)
        hallado = None
        for s in sitios:
            c = pathlib.Path(d, s % "fug")
            if c.is_file() and os.access(c, os.X_OK):
                hallado = s
                break
    if hallado is None:
        print("BUG-0002 ABIERTO: en el paquete, ningun sitio da con fug al lado de la madre;"
              " el vistazo depende del PATH (en Linux, /usr/lib/atsw/bin no esta).")
        return 1
    print("arreglado: el vistazo encuentra a fug con el sitio «%s»" % hallado)
    return 0


if __name__ == "__main__":
    sys.exit(main())
