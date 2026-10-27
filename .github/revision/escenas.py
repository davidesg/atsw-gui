#!/usr/bin/env python3
"""LAS ESCENAS DE LOS VIDEOS, con los GUIs instalados, en esta plataforma.

    python3 escenas.py --bin DIR --modulo LIB --salida OUT [--caso es-core|ipc-wti]

Corre los guiones de docs/manual/video/escenas-es-core/ (el IPC subyacente) y
de docs/manual/video/escenas/ (el IPC y el WTI) con el modulo de escenas
(docs/manual/capturas/escenas.c) cargado en cada GUI por GTK_MODULES, en el
escritorio de la maquina: sin broadway, como lo veria un usuario.

A cada guion se le añade, antes de cada «cierra» y del «salir», una
«foto»: la ventana de ese momento, pintada desde dentro del proceso. Ademas
se fotografia el escritorio entero mientras corre (la ultima foto antes de que
acabe se queda).

Por escena deja en OUT/<caso>/<escena>/: el guion, la salida, las fotos, y una
linea en OUT/escenas.json con el estado, el tiempo, los avisos de stderr y, en
Windows, las ventanas de consola que asomaron mientras corria.

Un proceso que pasa de su limite se mata por su PID (y su arbol: en Windows
taskkill /T /PID, en los demas su grupo de procesos, que es el suyo).
"""
import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

R = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VIDEO = os.path.join(R, "docs", "manual", "video")
DATOS = os.path.join(R, "docs", "manual", "datos")
WIN = sys.platform.startswith("win")
MAC = sys.platform == "darwin"

CASOS = {
    "es-core": os.path.join(VIDEO, "escenas-es-core"),
    "ipc-wti": os.path.join(VIDEO, "escenas"),
}

# Lo que GTK escribe en stderr y no es de los programas (el mismo filtro que
# capturas.sh), y lo que si cuenta como aviso.
RUIDO = re.compile(r"pixman|preferred_height_for_width|Negative content|AccelLabel|queue_draw_area"
                   r"|\*\*\* BUG \*\*\*|\*\* Message:|no trigger event for menu popup")
AVISO = re.compile(r"WARNING|CRITICAL|ERROR|[Ee]rror|[Ww]arning|[Ff]allo|No encuentro|no pude")


def ruta(p):
    """Las rutas que van al guion y al GUI, con / tambien en Windows."""
    return p.replace("\\", "/")


def prepara(guion, proyecto, nombre):
    """El guion con sus rutas y con una foto antes de cada cierra y del salir."""
    fuera, k = [], 0
    with open(guion, encoding="utf-8") as f:
        lineas = f.read().splitlines()
    for ln in lineas:
        ln = ln.replace("@PROYECTO@", ruta(proyecto)).replace("@DATOS@", ruta(DATOS))
        orden = ln.strip().split(" ", 1)[0]
        if orden in ("cierra", "salir"):
            k += 1
            fuera.append("foto %s-%02d" % (nombre, k))
        fuera.append(ln)
    return fuera


# ---- el escritorio ------------------------------------------------------

def foto_escritorio(destino):
    try:
        if WIN:
            ps = ("Add-Type -AssemblyName System.Windows.Forms,System.Drawing;"
                  "$b=[System.Windows.Forms.SystemInformation]::VirtualScreen;"
                  "$i=New-Object System.Drawing.Bitmap $b.Width,$b.Height;"
                  "[System.Drawing.Graphics]::FromImage($i).CopyFromScreen($b.Left,$b.Top,0,0,$i.Size);"
                  "$i.Save('%s')" % destino.replace("'", "''"))
            subprocess.run(["powershell", "-NoProfile", "-Command", ps], timeout=20,
                           capture_output=True, creationflags=0x08000000)
        elif MAC:
            subprocess.run(["screencapture", "-x", destino], timeout=20, capture_output=True)
        elif os.environ.get("DISPLAY"):
            subprocess.run(["import", "-window", "root", destino], timeout=20, capture_output=True)
    except Exception:
        pass
    return os.path.exists(destino)


# ---- las consolas de Windows (BUG-0001 de la madre) ---------------------

class Consolas:
    """Mira cada 100 ms las ventanas visibles de clase ConsoleWindowClass (y
    la de Windows Terminal) que no estaban al empezar; apunta su PID y su
    programa. La misma pregunta que lib/sitio/prueba_consola.sh, desde
    fuera: ¿asoma una consola mientras el GUI trabaja?"""

    CLASES = ("ConsoleWindowClass", "CASCADIA_HOSTING_WINDOW_CLASS", "PseudoConsoleWindow")

    def __init__(self):
        self.vistas = {}
        self.antes = set()
        self._para = threading.Event()
        self._hilo = None
        if not WIN:
            return
        import ctypes
        from ctypes import wintypes
        self.ct, self.wt = ctypes, wintypes
        self.u32 = ctypes.windll.user32
        self.k32 = ctypes.windll.kernel32
        self.antes = set(h for h, _ in self._enumera())

    def _enumera(self):
        ct, wt = self.ct, self.wt
        res = []
        PROTO = ct.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

        def cada(h, _):
            b = ct.create_unicode_buffer(256)
            self.u32.GetClassNameW(h, b, 256)
            if b.value in self.CLASES:
                res.append((h, b.value))
            return True
        self.u32.EnumWindows(PROTO(cada), 0)
        return res

    def _programa(self, h):
        ct, wt = self.ct, self.wt
        pid = wt.DWORD()
        self.u32.GetWindowThreadProcessId(h, ct.byref(pid))
        nombre = "?"
        hp = self.k32.OpenProcess(0x1000, False, pid.value)
        if hp:
            b = ct.create_unicode_buffer(1024)
            n = wt.DWORD(1024)
            if self.k32.QueryFullProcessImageNameW(hp, 0, b, ct.byref(n)):
                nombre = os.path.basename(b.value)
            self.k32.CloseHandle(hp)
        return pid.value, nombre

    def _mira(self):
        while not self._para.is_set():
            for h, clase in self._enumera():
                if h in self.antes or h in self.vistas:
                    continue
                if not self.u32.IsWindowVisible(h):
                    continue
                pid, prog = self._programa(h)
                self.vistas[h] = {"clase": clase, "pid": pid, "programa": prog,
                                  "t": round(time.time() - self.t0, 1)}
            self._para.wait(0.1)

    def empieza(self):
        self.vistas = {}
        self.t0 = time.time()
        if WIN:
            self.antes = set(h for h, _ in self._enumera())
            self._para.clear()
            self._hilo = threading.Thread(target=self._mira, daemon=True)
            self._hilo.start()

    def control(self):
        """El control positivo: una consola de verdad (cmd con
        CREATE_NEW_CONSOLE) mientras se mira. Si no se ve, el detector no
        vale en este Windows (p. ej. si la consola nueva abre como pestaña de
        una ventana de Windows Terminal que ya estaba)."""
        if not WIN:
            return None
        self.empieza()
        p = subprocess.Popen(["cmd.exe", "/c", "ping -n 4 127.0.0.1 >nul"], creationflags=0x00000010)
        p.wait(30)
        time.sleep(0.5)
        vistas = self.acaba()
        return {"vistas": vistas, "ok": bool(vistas)}

    def acaba(self):
        if self._hilo:
            self._para.set()
            self._hilo.join(5)
        return list(self.vistas.values())


# ---- matar por PID ------------------------------------------------------

def mata(p):
    if p.poll() is not None:
        return
    if WIN:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)], capture_output=True)
    else:
        try:
            os.killpg(p.pid, signal.SIGKILL)   # su grupo: el GUI y lo que lanzo
        except Exception:
            p.kill()


# ---- una escena ---------------------------------------------------------

def programa(bindir, nombre):
    x = os.path.join(bindir, nombre + (".exe" if WIN else ""))
    return x


def corre(caso, guion, base, args, consolas, proyecto=None, sin_path=False):
    nombre = os.path.splitext(os.path.basename(guion))[0]
    proyecto = proyecto or os.path.join(base, caso)
    dest = os.path.join(args.salida, caso, nombre)
    fotos = os.path.join(dest, "fotos")
    os.makedirs(fotos, exist_ok=True)
    lineas = prepara(guion, proyecto, nombre)
    g = os.path.join(dest, "guion.txt")
    with open(g, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lineas) + "\n")
    cmd = lineas[0][3:].split()
    prog = cmd[0]
    argv = [programa(args.bin, prog)] + cmd[1:]
    if prog == "atsw_gui" and args.madre:
        argv = [args.madre] + cmd[1:]
    env = dict(os.environ)
    if args.path and not sin_path:
        env["PATH"] = args.bin + os.pathsep + env.get("PATH", "")
    env.update({"GTK_MODULES": ruta(os.path.abspath(args.modulo)), "ESCENA": ruta(g),
                "FOTOS": ruta(fotos), "PASO_MS": str(args.paso_ms), "TIEMPO": str(args.tiempo),
                "NO_AT_BRIDGE": "1"})
    reg = {"caso": caso, "escena": nombre, "programa": prog, "argv": argv[1:]}
    print("== %s/%s: %s" % (caso, nombre, " ".join(argv)), flush=True)
    sal = open(os.path.join(dest, "salida.txt"), "w", encoding="utf-8", errors="replace")
    err_f = os.path.join(dest, "stderr.txt")
    err = open(err_f, "w", encoding="utf-8", errors="replace")
    kw = {}
    if not WIN:
        kw["start_new_session"] = True
    bw = None
    if args.broadway:
        # Solo para probar en Linux sin escritorio: un broadwayd por escena,
        # como capturas.sh (si un cliente sale con ventanas, a veces se cae).
        args.broadway += 1
        bw = subprocess.Popen(["broadwayd", ":%d" % args.broadway],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        env.update({"GDK_BACKEND": "broadway", "BROADWAY_DISPLAY": ":%d" % args.broadway})
        time.sleep(1)
    consolas.empieza()
    t0 = time.time()
    p = subprocess.Popen(argv, cwd=base, env=env, stdout=sal, stderr=err,
                         stdin=subprocess.DEVNULL, **kw)
    reg["pid"] = p.pid
    ultima = os.path.join(dest, "escritorio.png")
    tmp = os.path.join(dest, "escritorio-tmp.png")
    proxima = t0 + 4
    colgado = False
    while p.poll() is None:
        if time.time() - t0 > args.limite:
            colgado = True
            foto_escritorio(ultima)
            mata(p)
            break
        if time.time() >= proxima:
            if foto_escritorio(tmp):
                shutil.move(tmp, ultima)
            proxima = time.time() + 4
        time.sleep(0.2)
    p.wait()
    t = time.time() - t0
    if bw:
        bw.kill()
        bw.wait()
    reg["consolas"] = consolas.acaba()
    sal.close()
    err.close()
    reg["segundos"] = round(t, 1)
    reg["rc"] = p.returncode
    reg["colgado"] = colgado
    with open(err_f, encoding="utf-8", errors="replace") as f:
        e = [ln.rstrip() for ln in f if ln.strip() and not RUIDO.search(ln)]
    with open(os.path.join(dest, "salida.txt"), encoding="utf-8", errors="replace") as f:
        o = f.read().splitlines()
    todo = e + [ln for ln in o if not ln.startswith("escenas: ")]
    reg["fallo_guion"] = next((ln for ln in e + o if re.match(r"escenas: linea \d+", ln)), "")
    reg["avisos"] = [ln for ln in todo if AVISO.search(ln) and not ln.startswith("escenas: linea")][:40]
    hechas = [ln for ln in o if re.match(r"escenas: \d+ ", ln)]
    reg["ordenes"] = len(hechas)
    reg["ultima_orden"] = hechas[-1][9:] if hechas else ""
    reg["fotos"] = sorted(x for x in os.listdir(fotos) if x.endswith(".png"))
    reg["ok"] = (p.returncode == 0 and not colgado and not reg["fallo_guion"]
                 and not reg["consolas"])
    estado = "OK" if reg["ok"] else "FALLA"
    print("   %s en %.1f s, rc=%s, %d ordenes, %d fotos, %d avisos%s%s" % (
        estado, t, p.returncode, len(hechas), len(reg["fotos"]), len(reg["avisos"]),
        (", " + reg["fallo_guion"]) if reg["fallo_guion"] else "",
        (", CONSOLAS: %s" % reg["consolas"]) if reg["consolas"] else ""), flush=True)
    return reg


VISTAZO = """#! atsw_gui --proyecto @PROYECTO@/proyecto.yaml
ventana el taller
pestana 2002-2019
fila subyacente
derecho subyacente
espera 900
menu Serie y ACF / PACF…
ventana vistazo
espera 1500
salir
"""


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--bin", required=True, help="el directorio de los programas instalados")
    a.add_argument("--modulo", required=True, help="el modulo de escenas compilado")
    a.add_argument("--salida", required=True)
    a.add_argument("--base", help="donde se crean los proyectos (por defecto, SALIDA/proyectos)")
    a.add_argument("--madre", help="como se lanza la madre, si no es BIN/atsw_gui (el lanzador de la .app)")
    a.add_argument("--broadway", type=int, default=0,
                   help="solo Linux sin escritorio: pantallas de broadway desde N+1")
    a.add_argument("--path", action="store_true",
                   help="pone BIN el primero del PATH (lo que hace el lanzador de macOS)")
    a.add_argument("--extra", action="store_true",
                   help="ademas, el vistazo de la madre sin BIN en el PATH (BUG-0002)")
    a.add_argument("--caso", action="append", choices=sorted(CASOS))
    a.add_argument("--paso-ms", type=int, default=450)
    a.add_argument("--tiempo", type=int, default=30, help="segundos que una orden espera a su ventana")
    a.add_argument("--limite", type=int, default=600, help="segundos por escena antes de matarla")
    args = a.parse_args()
    args.salida = os.path.abspath(args.salida)
    base = os.path.abspath(args.base or os.path.join(args.salida, "proyectos"))
    if os.path.isdir(base):
        shutil.rmtree(base)
    os.makedirs(base)
    consolas = Consolas()
    control = consolas.control()
    if control is not None:
        print("control de consolas: %s" % control, flush=True)
        with open(os.path.join(args.salida, "control-consolas.json"), "w", encoding="utf-8") as f:
            json.dump(control, f, ensure_ascii=False, indent=1)
    todos = []
    for caso in args.caso or ["es-core", "ipc-wti"]:
        os.makedirs(os.path.join(base, caso), exist_ok=True)
        for g in sorted(os.listdir(CASOS[caso])):
            if not g.endswith(".txt"):
                continue
            r = corre(caso, os.path.join(CASOS[caso], g), base, args, consolas)
            todos.append(r)
            # una escena rota deja el proyecto a medias: las siguientes del
            # mismo caso se corren igual, y su fallo se lee como consecuencia
    if args.extra and "es-core" in (args.caso or ["es-core"]):
        # EL VISTAZO SIN EL PATH: la madre tiene que encontrar a fug a su lado,
        # como encuentra a fue y a art (gui/atsw/BUGS.md, BUG-0002). Sin
        # --path, aunque las escenas de los casos lo lleven.
        g = os.path.join(args.salida, "vistazo-sin-path.txt")
        with open(g, "w", encoding="utf-8", newline="\n") as f:
            f.write(VISTAZO)
        todos.append(corre("extra", g, base, args, consolas,
                           proyecto=os.path.join(base, "es-core"), sin_path=True))
    with open(os.path.join(args.salida, "escenas.json"), "w", encoding="utf-8") as f:
        json.dump(todos, f, ensure_ascii=False, indent=1)
    malas = [r for r in todos if not r["ok"]]
    print("escenas: %d de %d bien" % (len(todos) - len(malas), len(todos)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
