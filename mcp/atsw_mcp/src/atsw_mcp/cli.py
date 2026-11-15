"""Llamar a los binarios del paquete: atsw-cli y atsw_gui --ver.

LAS REGLAS (docs/ESTUDIO-terminal-mcp.md §2.5 y §7.5, y lo que encontro la
fase 0):

- el MTM no escribe proyecto.yaml ni los .inp: lo hace atsw-cli, con
  lib/proyecto y lib/inpdet, los mismos de la madre. Un solo escritor;
- siempre SIN SHELL y con argumentos que se construyen aqui; las
  herramientas reciben series y modelos del proyecto, nunca rutas;
- siempre con la entrada cerrada (stdin=DEVNULL): en Windows un proceso que
  hereda la tuberia del cliente MCP se cuelga (fase 0, hallazgo 1);
- lo que escribe lleva --por asistente: el linaje dice quien dio el paso
  (§7.2).
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

from atsw_mcp import comun

# Una serie o un modelo del proyecto: lo que lib/proyecto admite como clave.
_ID = re.compile(r"^[^\s/\\]{1,47}$")


class NoSePuede(Exception):
    """Lo que hay que decirle al asistente, tal cual."""


def comprueba_id(que: str, v: str | None, opcional: bool = False) -> str:
    if v is None or v == "":
        if opcional:
            return ""
        raise NoSePuede(f"falta {que}")
    if not _ID.match(v) or v in (".", ".."):
        raise NoSePuede(f"{que} «{v}» no es un nombre del proyecto")
    return v


def proyecto() -> Path:
    p, como = comun.busca_proyecto()
    if p is None:
        raise NoSePuede(f"no encuentro el proyecto: {como}")
    return p / comun.MANIFIESTO


def _binario(nombre: str) -> str:
    ruta, origen = comun.busca_motor(nombre)
    if ruta is None:
        raise NoSePuede(f"no encuentro {nombre} (ATSW_BIN, ATSW_GUI o el PATH)")
    return ruta


def atsw_cli(*args: str, muestra: str = "", timeout: float = 900) -> str:
    """atsw-cli ORDEN ... sobre el proyecto; su salida, o NoSePuede con su
    mensaje. args lleva la orden y lo que no es el proyecto: el proyecto lo
    pone esto, en su sitio (justo despues de la orden y su suborden)."""
    exe = _binario("atsw-cli")
    orden = list(args)
    corte = 2 if orden and orden[0] in ("proyecto", "raices", "coste") else 1
    cmd = [exe] + orden[:corte] + [str(proyecto())] + orden[corte:]
    if muestra:
        cmd += ["--muestra", comprueba_id("la muestra", muestra)]
    r = subprocess.run(cmd, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=timeout)
    if r.returncode != 0:
        msg = (r.stderr or r.stdout).strip().removeprefix("atsw-cli: ")
        raise NoSePuede(msg or f"atsw-cli salió con {r.returncode}")
    return r.stdout.strip()


def _aparte(cmd: list[str], que: str) -> str:
    """Una ventana, aparte: la herramienta vuelve en seguida y la ventana se
    queda. Lo que no se puede abrir, el programa lo dice antes (estado 2 o 3),
    y se espera un momento para recogerlo."""
    kw: dict = {}
    if os.name == "nt":
        kw["creationflags"] = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        kw["start_new_session"] = True
    p = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, text=True, encoding="utf-8",
                         errors="replace", **kw)
    try:
        rc = p.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        return f"abierta (proceso {p.pid}): {que}"
    err = (p.stderr.read() if p.stderr else "").strip()
    nombre = Path(cmd[0]).stem
    if rc in (2, 3):
        # «atsw_gui: ...» o «/ruta/proyecto.yaml: ...»: el motivo, sin el prefijo
        raise NoSePuede(err.split(": ", 1)[-1] if ": " in err else (err or f"{nombre} salió con {rc}"))
    return f"la ventana se cerró en seguida (estado {rc}){': ' + err if err else ''}"


def ver(que: str, objeto: str, muestra: str = "") -> str:
    """atsw_gui --ver: la ventana de la madre, de solo lectura."""
    cmd = [_binario("atsw_gui"), "--proyecto", str(proyecto()), "--ver", que, objeto]
    if muestra:
        cmd += ["--muestra", comprueba_id("la muestra", muestra)]
    return _aparte(cmd, f"{que} de {objeto}")


def abre_caso(caso: str, corrida: str = "") -> str:
    """drtran_gui --caso: el caso de transferencia, para trabajarlo en su
    ventana. Un caso que no esta lo dice drtran_gui antes de abrir (estado 3)."""
    cmd = [_binario("drtran_gui"), "--proyecto", str(proyecto()),
           "--caso", comprueba_id("el caso", caso)]
    if corrida:
        cmd += ["--modelo", comprueba_id("el modelo del caso", corrida)]
    return _aparte(cmd, f"el caso {caso} en drtran_gui")


def ruta(serie: str, modelo: str, ext: str, muestra: str = "") -> str:
    """El fichero de un modelo, como lo compone lib/proyecto (atsw-cli proyecto
    ruta). Solo para el extra Python, que llama a art con rutas; el asistente
    nunca las ve. modelo «datos» es el nodo de datos de la serie."""
    try:
        return atsw_cli("proyecto", "ruta", serie, modelo, ext, muestra=muestra)
    except NoSePuede as e:
        if "no es un modelo" in str(e) or "no tiene datos" in str(e):
            raise
        raise NoSePuede(f"{serie}/{modelo} no tiene {ext}: ¿está estimado?") from None


def extra(orden: str, args: dict, objeto: str, paso: str, timeout: float | None = None) -> str:
    """Una herramienta de art-python (el extra [python]) en un proceso aparte,
    python -m atsw_mcp.extra. Lo que corre queda en costes.csv con via: python,
    como pide §7.7: el coste de un paso no depende de por donde se dio."""
    if comun.extra_disponible("art") == "no":
        raise NoSePuede("esta herramienta es del extra Python, que no está instalado "
                        "(atsw-gui-mcp[python]: Asistente ▸ «Instalar el asistente…», o "
                        "uv pip install 'atsw-gui-mcp[python]'). Sin él, sigue con lo que hay en C.")
    if timeout is None:
        timeout = float(os.environ.get("ATSW_PLAZO_MOTOR_S", "300") or 300)
    t0 = time.monotonic()
    try:
        r = subprocess.run([sys.executable, "-m", "atsw_mcp.extra", orden, json.dumps(args)],
                           stdin=subprocess.DEVNULL, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        raise NoSePuede(f"art no terminó en {timeout:g} s; se ha parado") from None
    pared = time.monotonic() - t0
    lineas = [l for l in r.stdout.splitlines() if l.startswith("{")]
    try:
        d = json.loads(lineas[-1])
    except (IndexError, ValueError):
        raise NoSePuede(f"el proceso de art salió con {r.returncode} sin respuesta: "
                        f"{(r.stderr or '').strip()[-400:]}") from None
    try:
        atsw_cli("coste", "apunta", "--motor", f"art {comun.extra_disponible('art')}",
                 "--paso", paso, "--cpu", f"{d.get('cpu', 0.0):.3f}", "--pared", f"{pared:.3f}",
                 "--via", "python", "--objeto", objeto, "--quien", "atsw-mcp", "--por", "asistente")
    except NoSePuede:
        pass                    # el coste no para el análisis: se apunta si se puede
    if "error" in d:
        raise NoSePuede(f"art: {d['error']}")
    texto = d.get("texto", "")
    if texto.startswith("❌ Error:") or texto.startswith("⛔"):
        raise NoSePuede("art: " + texto.split(":", 1)[-1].strip()[:1500])
    return texto
