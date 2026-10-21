"""Lo que comparten los tres servidores (docs/ESTUDIO-terminal-mcp.md §2.1).

Fase 0: encontrar el proyecto y los motores en C, y decirlo. Nada de
analisis todavia.

EL PROYECTO se busca como dice §7.1: ATSW_PROYECTO si esta, y si no desde la
carpeta en la que arranca el servidor, subiendo hasta dar con proyecto.yaml.
CodeWhale y Codex tienen la configuracion global y no saben de proyectos;
por eso se sube, y no se mira solo la carpeta de arranque.

LOS MOTORES, los del paquete y no otros (§7.1): ATSW_BIN si esta; si no, la
carpeta de ATSW_GUI; y solo al final el PATH, dicho asi en el estado para
que se vea que no son los de la madre.
"""

from __future__ import annotations

import importlib.util
import os
import platform
import shutil
import sys
from importlib import metadata
from pathlib import Path

DISTRIBUCION = "atsw-gui-mcp"
MANIFIESTO = "proyecto.yaml"

# Los motores de cada clase (§2.1). atsw-cli y atsw_gui los usan todos.
MOTORES = {
    "us": ["fue", "fuf", "fug", "art"],
    "mt": ["drtran", "fue"],
    "sis": ["drvarma", "drvec"],
}
COMUNES = ["atsw-cli", "atsw_gui"]

# Lo que el extra [python] trae, por clase (decision 15).
EXTRA = {
    "us": ["art"],
    "mt": ["drtran"],
    "sis": ["sima"],
}


def version() -> str:
    try:
        return metadata.version(DISTRIBUCION)
    except metadata.PackageNotFoundError:
        return "sin instalar"


def busca_proyecto(desde: str | os.PathLike | None = None) -> tuple[Path | None, str]:
    """El proyecto y como se encontro."""
    env = os.environ.get("ATSW_PROYECTO")
    if env:
        p = Path(env).expanduser()
        if (p / MANIFIESTO).is_file():
            return p.resolve(), "ATSW_PROYECTO"
        return None, f"ATSW_PROYECTO={env} no tiene {MANIFIESTO}"
    d = Path(desde or os.getcwd()).resolve()
    for c in (d, *d.parents):
        if (c / MANIFIESTO).is_file():
            return c, "subiendo desde " + str(d)
    return None, f"ningun {MANIFIESTO} desde {d} hacia arriba"


def _carpetas_del_paquete() -> list[tuple[Path, str]]:
    out = []
    b = os.environ.get("ATSW_BIN")
    if b:
        out.append((Path(b).expanduser(), "ATSW_BIN"))
    g = os.environ.get("ATSW_GUI")
    if g:
        out.append((Path(g).expanduser().parent, "ATSW_GUI"))
    return out


def busca_motor(nombre: str) -> tuple[str | None, str]:
    """La ruta del binario y de donde salio: ATSW_BIN, ATSW_GUI o PATH."""
    exe = nombre + (".exe" if os.name == "nt" else "")
    for carpeta, origen in _carpetas_del_paquete():
        p = carpeta / exe
        if p.is_file() and os.access(p, os.X_OK):
            return str(p), origen
    p = shutil.which(nombre)
    if p:
        return p, "PATH (no es el del paquete)"
    return None, "no encontrado"


def extra_disponible(modulo: str) -> str:
    """La version del paquete del extra, o «no».

    Sin importarlo: un import de art trae matplotlib y compania, tarda, y lo
    que escriba por la salida estandar romperia el canal MCP, que es esa
    misma salida. Basta con saber que esta instalado.
    """
    if importlib.util.find_spec(modulo) is None:
        return "no"
    if os.environ.get("ATSW_MCP_IMPORTA") == "1":
        # Solo para el diagnostico de la CI: importarlo de verdad, dentro del
        # servidor, que es lo que hara la fase 1b.
        try:
            __import__(modulo)
        except Exception as e:
            return f"no ({type(e).__name__})"
    dist = {"art": "art-tseries", "drtran": "drtran", "sima": "sima-tseries"}[modulo]
    try:
        return metadata.version(dist)
    except metadata.PackageNotFoundError:
        return "si"


def ficha_version(servidor: str, clase: str) -> str:
    return "\n".join([
        f"{servidor} {version()} ({DISTRIBUCION}), clase {clase}",
        f"mcp {metadata.version('mcp')}",
        f"Python {platform.python_version()} en {sys.platform} ({platform.machine()})",
    ])


def estado(servidor: str, clave: str) -> str:
    """Donde esta el proyecto, que motores hay y si esta el extra."""
    lineas = [f"{servidor} {version()}"]
    p, como = busca_proyecto()
    lineas.append(f"proyecto: {p if p else 'no'} ({como})")
    lineas.append("motores:")
    for m in MOTORES[clave] + COMUNES:
        ruta, origen = busca_motor(m)
        lineas.append(f"  {m}: {ruta or '-'} ({origen})")
    lineas.append("extra [python]:")
    for mod in EXTRA[clave]:
        lineas.append(f"  {mod}: {extra_disponible(mod)}")
    return "\n".join(lineas)


def principal(crea, instrucciones: str) -> None:
    """La entrada de los tres: --version e --instrucciones, o el servidor."""
    args = sys.argv[1:]
    if "--version" in args:
        print(version())
        return
    if "--instrucciones" in args:
        print(instrucciones)
        return
    crea().run()


def crea_servidor(servidor: str, clave: str, clase: str, instrucciones: str):
    """Un servidor con las dos herramientas de la fase 0."""
    from mcp.server.fastmcp import FastMCP

    app = FastMCP(servidor, instructions=instrucciones)

    @app.tool(name="version")
    def _version() -> str:
        """La version del servidor, del SDK de MCP y de Python."""
        return ficha_version(servidor, clase)

    @app.tool(name="estado")
    def _estado() -> str:
        """Donde ha encontrado el proyecto y los motores en C, y si esta el extra [python]."""
        return estado(servidor, clave)

    return app
