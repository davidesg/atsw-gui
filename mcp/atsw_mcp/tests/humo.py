"""Prueba de humo de atsw-gui-mcp, sin cliente de LLM (§4 y §8 del estudio).

Una sesion MCP a pelo con cada servidor, por stdio:
  initialize, tools/list, version y estado.

El servidor se lanza desde una SUBCARPETA de un proyecto de mentira, como lo
haria un cliente abierto en ella, y tiene que encontrar proyecto.yaml
subiendo (§7.1). Y ATSW_BIN apunta a una carpeta con un fue de mentira, que
tiene que salir como «ATSW_BIN» y no como «PATH».

No abre ninguna conexion de red: el entorno ya esta instalado y los
servidores se lanzan desde el.

    python humo.py DIR_DE_LOS_EJECUTABLES [--sin-extra | --con-extra]

Sale con 0 si todo va bien.
"""

from __future__ import annotations

import asyncio
import os
import sys
import tempfile
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

SERVIDORES = {"atsw-us": "US", "atsw-mt": "MT", "atsw-sis": "VARMA"}
HERRAMIENTAS = {"version", "estado"}
EXTRA_DE = {"atsw-us": "art", "atsw-mt": "drtran", "atsw-sis": "sima"}

fallos: list[str] = []


def check(ok: bool, que: str, vio: str = "") -> None:
    print(("ok    " if ok else "FALLO ") + que + ("" if ok else f"  -- vio: {vio}"))
    if not ok:
        fallos.append(que)


def texto(res) -> str:
    return "".join(getattr(c, "text", "") for c in res.content)


async def una(exe: Path, nombre: str, proyecto: Path, binarios: Path, extra: str | None) -> None:
    env = dict(os.environ)
    env.pop("ATSW_PROYECTO", None)
    env["ATSW_BIN"] = str(binarios)
    params = StdioServerParameters(
        command=str(exe), args=[], env=env, cwd=str(proyecto / "series" / "ipc")
    )
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            ini = await s.initialize()
            check(ini.serverInfo.name == nombre, f"{nombre}: initialize", ini.serverInfo.name)
            check(bool(ini.instructions) and nombre in ini.instructions,
                  f"{nombre}: lleva instrucciones", repr(ini.instructions)[:80])
            ts = {t.name for t in (await s.list_tools()).tools}
            check(ts == HERRAMIENTAS, f"{nombre}: tools/list", str(sorted(ts)))
            v = texto(await s.call_tool("version", {}))
            check(v.startswith(nombre + " ") and f"clase {SERVIDORES[nombre]}" in v,
                  f"{nombre}: version", v.splitlines()[0] if v else "")
            print("      " + v.replace("\n", "\n      "))
            e = texto(await s.call_tool("estado", {}))
            check(f"proyecto: {proyecto.resolve()}" in e, f"{nombre}: encuentra el proyecto subiendo", e)
            if nombre != "atsw-sis":
                check("fue: " in e and "(ATSW_BIN)" in e.split("fue: ", 1)[1].splitlines()[0],
                      f"{nombre}: fue sale de ATSW_BIN", e)
            if extra:
                tras = e.split("extra [python]:", 1)[-1]
                lin = [l for l in tras.splitlines() if l.strip().startswith(EXTRA_DE[nombre] + ":")]
                hay = bool(lin) and not lin[0].split(":", 1)[1].strip().startswith("no")
                check(hay == (extra == "con"), f"{nombre}: extra [python] {extra}", lin[0] if lin else e)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    bindir = Path(sys.argv[1])
    extra = None
    if "--sin-extra" in sys.argv:
        extra = "sin"
    if "--con-extra" in sys.argv:
        extra = "con"
    suf = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory() as t:
        proyecto = Path(t) / "proyecto"
        (proyecto / "series" / "ipc").mkdir(parents=True)
        (proyecto / "proyecto.yaml").write_text("formato: 1\n", encoding="utf-8")
        binarios = Path(t) / "bin"
        binarios.mkdir()
        fue = binarios / ("fue" + suf)
        fue.write_bytes(b"")
        fue.chmod(0o755)
        for nombre in SERVIDORES:
            exe = bindir / (nombre + suf)
            if not exe.is_file():
                check(False, f"{nombre}: ejecutable instalado", str(exe))
                continue
            asyncio.run(una(exe, nombre, proyecto, binarios, extra))
    print(f"\n{len(fallos)} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
