#!/usr/bin/env python3
"""BUG-0001 (gui/drtran, lib/engine): al cerrar drtran_gui, drtran sigue vivo.

drtran_gui lanza el motor con engine_start (lib/engine/engine.c, lanza) y no
lo para nunca al irse: ni al cerrar la ventana, ni cuando la madre se va
(--madre), ni si al GUI lo matan (el `timeout` de capturas.sh, broadwayd que
se cae). El hijo se queda huerfano, colgado de init, con el CPU que tuviera.
Visto el 2026-10-24: dos drtran huerfanos de capturas.sh, ~5 h al 88 % de CPU
cada uno (el de dentro era drtran BUG-0058, que no acaba nunca).

Lo que hace esto, sin depender de que drtran cuelgue:

  1. pone drtran_gui (el del arbol) en un directorio con un «drtran» FALSO al
     lado (estima.c, mtram_hermano: el motor se busca primero ahi), que
     apunta su PID y duerme 10 minutos;
  2. abre el caso C1 del proyecto de aqui (IPC <- WTI del manual) en un
     broadwayd propio, pulsa Estimar y CIERRA LA VENTANA, como un usuario
     (escenas.c, «cierra»: gtk_window_close);
  3. espera a que drtran_gui salga y mira si el «drtran» sigue vivo.

Sale con 1 mientras el motor sobreviva al GUI, con 0 cuando el GUI lo pare
al irse, con 2 si la prueba no llega a lanzar el motor.

Necesita el arbol construido (make), broadwayd (libgtk-3-bin) y cc con
gtk+-3.0. La pantalla de broadway es ATSW_BROADWAY (89 por defecto).
"""
import os, pathlib, shutil, signal, subprocess, sys, tempfile, time

AQUI = pathlib.Path(__file__).resolve().parent
R = AQUI.parents[3]                                  # la raiz del arbol
GUI = R / "gui/drtran/drtran_gui"
ESCENAS_C = R / "docs/manual/capturas/escenas.c"
PANTALLA = os.environ.get("ATSW_BROADWAY", "89")

GUION = """\
ventana drtran
espera 2500
pestana 5 · Estimación
boton Estimar
espera 2000
cierra
"""

FALSO = """#!/bin/sh
# un drtran que no acaba: apunta su PID y duerme
echo $$ > "$REPRO_PIDFILE"
exec sleep 600
"""


def vivo(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    # un zombi no cuenta
    try:
        st = pathlib.Path(f"/proc/{pid}/stat").read_text().split()
        return st[2] != "Z"
    except OSError:
        return True


def padre(pid):
    try:
        return int(pathlib.Path(f"/proc/{pid}/stat").read_text().split()[3])
    except OSError:
        return -1


def main():
    for falta, que in ((GUI, "make en la raiz"), (ESCENAS_C, "el arbol")):
        if not falta.exists():
            sys.exit(f"no encuentro {falta} ({que})")
    if not shutil.which("broadwayd"):
        sys.exit("hace falta broadwayd (libgtk-3-bin)")

    with tempfile.TemporaryDirectory() as t:
        t = pathlib.Path(t)
        so = t / "libescenas.so"
        flags = subprocess.run(["pkg-config", "--cflags", "--libs", "gtk+-3.0"],
                               capture_output=True, text=True, check=True).stdout.split()
        subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-o", str(so),
                        str(ESCENAS_C), *flags], check=True)

        bin_ = t / "ATSW/bin"
        bin_.mkdir(parents=True)
        shutil.copy2(GUI, bin_ / "drtran_gui")
        (bin_ / "drtran").write_text(FALSO)
        (bin_ / "drtran").chmod(0o755)
        proy = t / "proyecto"
        shutil.copytree(AQUI / "proyecto", proy)
        (t / "guion.txt").write_text(GUION)
        pidfile = t / "drtran.pid"
        for d in ("cache", "config", "data", "fotos"):
            (t / d).mkdir()

        bw = subprocess.Popen(["broadwayd", f":{PANTALLA}"],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        motor = None
        try:
            time.sleep(1)
            env = dict(os.environ,
                       GDK_BACKEND="broadway", BROADWAY_DISPLAY=f":{PANTALLA}",
                       GTK_THEME="Adwaita", NO_AT_BRIDGE="1",
                       GTK_MODULES=str(so), ESCENA=str(t / "guion.txt"),
                       FOTOS=str(t / "fotos"), XDG_CACHE_HOME=str(t / "cache"),
                       XDG_CONFIG_HOME=str(t / "config"),
                       XDG_DATA_HOME=str(t / "data"),
                       LC_ALL="es_ES.UTF-8", REPRO_PIDFILE=str(pidfile))
            gui = subprocess.Popen([str(bin_ / "drtran_gui"), "--proyecto",
                                    str(proy / "proyecto.yaml"), "--caso", "C1"],
                                   cwd=t, env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
            try:
                salida, _ = gui.communicate(timeout=90)
            except subprocess.TimeoutExpired:
                gui.kill()
                salida, _ = gui.communicate()
                print("drtran_gui no se cerro en 90 s")
            print(f"drtran_gui salio con {gui.returncode}")

            if not pidfile.exists():
                print(salida[-2000:])
                print("NO CONCLUYENTE: el GUI no llego a lanzar el motor")
                return 2
            motor = int(pidfile.read_text().split()[0])
            time.sleep(1.5)            # lo que tarde un SIGTERM en llegar
            if vivo(motor):
                print(f"ABIERTO: el motor (PID {motor}) sigue vivo despues de "
                      f"cerrar drtran_gui; su padre es ahora el PID {padre(motor)}")
                return 1
            motor = None
            print("cerrado: el motor no sobrevive al GUI")
            return 0
        finally:
            # solo lo que lanzo esta prueba, por su PID
            if motor is not None and vivo(motor):
                os.kill(motor, signal.SIGKILL)
            bw.terminate()
            try:
                bw.wait(5)
            except subprocess.TimeoutExpired:
                bw.kill()


if __name__ == "__main__":
    sys.exit(main())
