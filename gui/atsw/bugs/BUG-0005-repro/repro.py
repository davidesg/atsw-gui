#!/usr/bin/env python3
"""BUG-0005 (gui/atsw): un proyecto creado con «Nuevo…» no ve lo que hace el
asistente.

Visto al grabar el video del asistente (17-nov-2026): con un proyecto recien
creado en la madre, el asistente crea y estima un modelo, y la madre no lo
ensena. Hay que volver a abrir el proyecto para verlo.

Por que: la vista en vivo (vigila.c, #181) se pone en atsw_abre, que es
por donde pasan «Abrir…», los ejemplos y el arranque con --proyecto. «Nuevo…»
(on_nuevo, main.c) no pasa por ahi: crea el manifiesto y lo guarda, sin
vigilarlo. Tampoco guardaba el estado de la ventana Asistente del proyecto
que se dejaba, ni ponia el del nuevo.

Lo que hace este guion: lanza la madre en un broadwayd, con el modulo de
escenas del manual (docs/manual/capturas/escenas.c), crea un proyecto con
«Nuevo…», carga los datos del manual con «Datos…» y, con la madre abierta,
da un paso como el asistente: atsw-cli proyecto nuevo ... --por asistente.
Espera hasta 20 s a que la barra de estado diga «, del asistente.». Sale
con 1 si no lo dice; con 0 si si. Con 2 si falta algo para correrlo
(broadwayd, el arbol construido).

ARREGLADO (17-nov-2026): on_nuevo llama a atsw_vigila y a
atsw_asistente_restaura, como atsw_abre, y guarda antes el estado del
proyecto que se deja.

    python3 repro.py [PANTALLA]       (la de broadwayd; 93 por defecto)
"""
import os, pathlib, shutil, subprocess, sys, tempfile, time

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol
GUI = R / "gui/atsw/atsw_gui"
CLI = R / "gui/atsw-cli/atsw-cli"
DATOS = R / "docs/manual/datos/es_core.csv"

GUION = """\
ventana el taller
boton Nuevo…
ventana Proyecto nuevo
fichero {p}/proyecto.yaml
espera 400
boton Crear
ventana el taller
boton Datos…
ventana Cargar datos
fichero {datos}
espera 400
boton Abrir
ventana Qué series cargar
boton Cargar las marcadas
espera 400
boton Cerrar
ventana el taller
espera 1000
lanza cd {p} && {cli} proyecto nuevo proyecto.yaml subyacente --razon "un paso del asistente" --por asistente > {d}/cli.log 2>&1
aguarda , del asistente. | 20
salir
"""


def main():
    pantalla = sys.argv[1] if len(sys.argv) > 1 else "93"
    if not shutil.which("broadwayd"):
        print("hace falta broadwayd (libgtk-3-bin)")
        return 2
    for f in (GUI, CLI):
        if not f.exists():
            print(f"falta {f}: construye el arbol (make)")
            return 2
    with tempfile.TemporaryDirectory() as d:
        dd = pathlib.Path(d)
        p = dd / "proyecto"
        p.mkdir()
        mod = dd / "libescenas.so"
        cflags = subprocess.run(["pkg-config", "--cflags", "--libs", "gtk+-3.0"],
                                capture_output=True, text=True, check=True).stdout.split()
        subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-o", str(mod),
                        str(R / "docs/manual/capturas/escenas.c"), *cflags], check=True)
        (dd / "guion.txt").write_text(GUION.format(p=p, datos=DATOS, cli=CLI, d=dd))
        env = dict(os.environ, GDK_BACKEND="broadway", BROADWAY_DISPLAY=":" + pantalla,
                   GTK_THEME="Adwaita", GTK_MODULES=str(mod), ESCENA=str(dd / "guion.txt"),
                   FOTOS=str(dd), NO_AT_BRIDGE="1", PASO_MS="300",
                   XDG_CONFIG_HOME=str(dd / "config"), XDG_DATA_HOME=str(dd / "data"),
                   XDG_CACHE_HOME=str(dd / "cache"),
                   ATSW_VENTANAS_INI=str(dd / "ventanas.ini"),
                   ATSW_ASISTENTE_DIR=str(dd / "asistente"))
        bw = subprocess.Popen(["broadwayd", ":" + pantalla],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            time.sleep(1)
            r = subprocess.run([str(GUI)], env=env, cwd=d, capture_output=True, text=True,
                               timeout=120)
        finally:
            bw.terminate()
            bw.wait()
        salida = r.stdout + r.stderr
        if "no aparece" in salida:
            print("ABIERTO: la madre no ve el modelo que el asistente creo en un proyecto "
                  "nuevo")
            print((dd / "cli.log").read_text() if (dd / "cli.log").exists() else "")
            return 1
        if r.returncode != 0:
            print(salida[-2000:])
            return 2
        print("ARREGLADO: la barra dice que el modelo es del asistente")
        return 0


if __name__ == "__main__":
    sys.exit(main())
