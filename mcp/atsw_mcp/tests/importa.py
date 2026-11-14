"""Cuanto tarda importar el extra, y si escribe por la salida estandar.

En la primera vuelta de la CI, el humo con el extra se colgo en Windows
cuando la herramienta estado importaba art dentro del servidor. Esto lo mira
fuera de MCP, de dos maneras: con la entrada estandar cerrada y con una
tuberia que sigue abierta, que es como la tiene un servidor MCP. Lo que un
import escriba por la salida estandar romperia el canal MCP.

    python importa.py      (con el python del entorno con el extra)

Solo informa: sale siempre con 0.
"""

from __future__ import annotations

import os
import subprocess
import sys
import time

# «hilo»: el import en un hilo que no es el principal, con la entrada en
# tuberia. Es como lo hace FastMCP, que corre las herramientas sincronas en
# un hilo de trabajo.
HILO = ("import threading; t = threading.Thread(target=__import__, args=({m!r},)); "
        "t.start(); t.join()")

for m in ("drtran", "sima", "art"):
    for como in ("cerrada", "tuberia", "hilo"):
        # La tuberia: el proceso lee de un extremo y aqui se guarda el otro
        # abierto, sin escribir nada, como un cliente MCP callado.
        r = w = None
        if como != "cerrada":
            r, w = os.pipe()
        codigo = HILO.format(m=m) if como == "hilo" else f"import {m}"
        t0 = time.monotonic()
        p = subprocess.Popen([sys.executable, "-c", codigo],
                             stdin=subprocess.DEVNULL if r is None else r,
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        try:
            out, _ = p.communicate(timeout=150)
            rc = p.returncode
        except subprocess.TimeoutExpired:
            p.kill()
            out, rc = b"", "colgado (150 s)"
        finally:
            for f in (r, w):
                if f is not None:
                    os.close(f)
        s = time.monotonic() - t0
        print(f"- import {m}, entrada {como}: rc={rc}, {s:.1f} s, {len(out)} bytes por stdout")
        if out:
            print("  " + out[:200].decode(errors="replace").replace("\n", "\n  "))
