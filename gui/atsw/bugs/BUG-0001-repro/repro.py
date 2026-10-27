#!/usr/bin/env python3
"""BUG-0001 (gui/atsw, lib/engine, lib/sitio): en Windows, cada motor que lanza
un GUI abre una ventana de consola, y el vistazo congela la madre.

Visto por el usuario en Windows 10/11 con el paquete v0.2.1: el vistazo
(«media - desv. típica», «datos + acf/pacf», el histograma) tarda mucho en
rehacerse al tocar λ, d o D, y en cada cambio asoma una consola; «Derivar»
un modelo a fue_gui también la asoma. En Linux y macOS no pasa.

LO QUE MIRA ESTE GUION ES EL CÓDIGO, NO LA PANTALLA. La consola solo se puede
ver en Windows, y ahí la mira la CI: lib/sitio/prueba_consola.sh, el paso
«Sin consola al lanzar los motores» de build.yml, que lanza un programa de
consola por cada camino desde un proceso sin consola y le pregunta si tiene
ventana (GetConsoleWindow). En Windows, si hay bash de MSYS2, este guion
corre también esa prueba.

Lo que sí se comprueba en cualquier plataforma:

  1. vistazo.c no corre fug con g_spawn_sync (en el hilo de la interfaz,
     una vez por clic) sino con engine_start, y espera a que dejen de llegar
     clics antes de lanzarlo;
  2. lib/engine (todas las corridas de la madre, fue_gui y drtran_gui) lanza
     por sitio_lanza_con_tuberias, no por g_spawn_async_with_pipes;
  3. fug_gui corre fug por sitio_lanza_y_espera, no por g_spawn_sync;
  4. el camino de Windows de lib/sitio usa CreateProcessW con
     CREATE_NO_WINDOW cuando el que lanza no tiene consola;
  5. fue_gui no abre los PDF con system("start ...") (cmd.exe, otra consola);
  6. fue_gui se enlaza con -mwindows también en MSYS2 (OS=Windows_NT): si no,
     es un programa de consola y abre una al arrancar.

Sale con 1 mientras alguna falle, con 0 cuando todas pasan. Lo que prueba es
que el código va por el camino arreglado; que de verdad no asome la consola
lo prueba la CI de Windows.
"""
import os, pathlib, re, shutil, subprocess, sys

R = pathlib.Path(__file__).resolve().parents[4]          # la raiz del arbol


def lee(rel):
    try:
        return (R / rel).read_text(encoding="utf-8")
    except OSError:
        return ""


def sin_comentarios(c):
    return re.sub(r"/\*.*?\*/", "", c, flags=re.S)


def main():
    fallos = []
    vz = sin_comentarios(lee("gui/atsw/src/vistazo.c"))
    en = sin_comentarios(lee("lib/engine/engine.c"))
    fr = sin_comentarios(lee("gui/fug/src/fug_run.c"))
    si = sin_comentarios(lee("lib/sitio/sitio.c"))
    fue = sin_comentarios(lee("gui/fue/src/file_io.c") + lee("gui/fue/src/forecast_tab.c"))

    if "g_spawn_sync" in vz:
        fallos.append("vistazo.c corre fug con g_spawn_sync, en el hilo de la interfaz")
    if "engine_start" not in vz or "g_timeout_add" not in vz:
        fallos.append("vistazo.c no lanza fug aparte (engine_start) tras una espera")
    if "g_spawn_async_with_pipes" in en or "sitio_lanza_con_tuberias" not in en:
        fallos.append("lib/engine lanza con g_spawn_async_with_pipes")
    if "g_spawn_sync" in fr or "sitio_lanza_y_espera" not in fr:
        fallos.append("fug_gui corre fug con g_spawn_sync")
    if "CreateProcessW" not in si or "CREATE_NO_WINDOW" not in si:
        fallos.append("lib/sitio no lanza con CreateProcessW y CREATE_NO_WINDOW")
    if re.search(r"\bsystem\s*\(", fue):
        fallos.append("fue_gui abre los PDF con system() (cmd.exe)")

    if "Windows_NT" not in lee("gui/fue/Makefile"):
        fallos.append("gui/fue/Makefile solo pone -mwindows con OS=windows (MSYS2 da Windows_NT)")

    for f in fallos:
        print("abierto:", f)

    # En Windows, la prueba de verdad.
    if sys.platform == "win32" and shutil.which("bash"):
        rc = subprocess.call(["bash", str(R / "lib/sitio/prueba_consola.sh")])
        if rc != 0:
            fallos.append("prueba_consola.sh: asoma una consola")

    if fallos:
        return 1
    print("BUG-0001: el código va por el camino sin consola y el vistazo no bloquea")
    return 0


if __name__ == "__main__":
    sys.exit(main())
