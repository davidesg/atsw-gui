"""atsw-mt sobre los binarios del arbol: una sesion MCP a pelo.

Sobre una copia del ejemplo de Jenkins y Alavi (MUSKRAT y MINK, con sus m01
estimados): identificar el enlace, crear el caso y lo que no se puede (una
clave que no es SERIE/MODELO, un caso que no esta). open_case de un caso que
existe abriria una ventana: aqui solo el que no existe, que drtran_gui
rechaza antes de abrir nada.

    python mt.py RAIZ_DEL_ARBOL
"""

from __future__ import annotations

import asyncio
import os
import shutil
import sys
import tempfile
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

fallos: list[str] = []


def check(ok: bool, que: str, vio: str = "") -> None:
    print(("ok    " if ok else "FALLO ") + que + ("" if ok else f"  -- vio: {vio[:300]}"))
    if not ok:
        fallos.append(que)


def texto(res) -> str:
    return "".join(getattr(c, "text", "") for c in res.content)


async def corre(raiz: Path, w: Path) -> None:
    exe = ".exe" if os.name == "nt" else ""
    binarios = w / "bin"
    binarios.mkdir()
    for b in (raiz / "gui/atsw-cli/atsw-cli", raiz / "gui/drtran/drtran_gui",
              raiz / "engines/drtran/bin/drtran"):
        shutil.copy2(str(b) + exe, binarios / (b.name + exe))
    proy = w / "proy"
    shutil.copytree(raiz / "docs/ejemplos/jenkins_alavi", proy)
    env = dict(os.environ, ATSW_BIN=str(binarios), ATSW_PROYECTO=str(proy))
    params = StdioServerParameters(command=sys.executable, args=["-m", "atsw_mcp.mt"],
                                   env=env, cwd=str(w))
    async with stdio_client(params) as (r, wr):
        async with ClientSession(r, wr) as s:
            ini = await s.initialize()
            check("atsw-mt" in (ini.instructions or "") and len(ini.instructions) < 20_000,
                  "las instrucciones: de atsw-mt, en menos de 20 KB", str(len(ini.instructions or "")))
            ts = {t.name for t in (await s.list_tools()).tools}
            check({"guion_map", "identify_link", "load_pre", "estimate",
                   "open_case"} <= ts,
                  "las herramientas", str(sorted(ts)))

            async def t(nombre, **a):
                return texto(await s.call_tool(nombre, a))

            res = await t("identify_link", output="MINK/m01", inputs=["MUSKRAT/m01"])
            check("Identificación de MINK/m01" in res and "MUSKRAT/m01" in res and "Hosking" in res,
                  "identify_link: la CCF preblanqueada y la exogeneidad", res)
            res = await t("identify_link", output="MINK", inputs=["MUSKRAT/m01"])
            check(res.startswith("No se pudo") and "SERIE/MODELO" in res,
                  "una salida sin modelo: es SERIE/MODELO", res)
            res = await t("load_pre", output="MINK/m01", inputs=["MUSKRAT/m01"], reason="")
            check(res.startswith("No se pudo") and "razón" in res, "load_pre sin razón: no", res)
            res = await t("load_pre", output="MINK/m01", inputs=["MUSKRAT/m01"],
                          reason="el visón responde a la rata", title="MINK <- MUSKRAT")
            check("C2" in res, "load_pre: nace C2 (C1 ya estaba)", res)
            res = await t("estimate", case="C2", reason="que drtran identifique")
            check("C2/c00" in res and "terminado" in res, "estimate: c00, drtran identifica la red", res)
            res = await t("estimate", case="C2", reason="un omega más", from_run="c00",
                          links=["MUSKRAT->MINK:0,0,1"])
            check("C2/c01" in res and "enlaces dados" in res, "estimate: c01, con el enlace dado", res)
            res = await t("estimate", case="C2", reason="con previsión", from_run="c01", horizon=5)
            check("C2/c02" in res and "FORECAST REPORT" in res, "estimate prever: la tabla de la salida", res)
            res = await t("estimate", case="C2", reason="la covarianza libre", from_run="c01",
                          constraints=["q[2,1] = free"])
            check("C2/c03" in res and "q[2,1]" in res and "Log-likelihood" in res,
                  "estimate constraints: c03, con la covarianza libre en la tabla", res)
            res = await t("estimate", case="C2", reason="mal", from_run="c01",
                          constraints=["omega9[1] = 0"])
            check(res.startswith("No se pudo") and "unknown parameter" in res,
                  "estimate con un slot que no existe: lo dice drtran", res)
            res = await t("estimate", case="C2", reason="mal", links=["MUSKRAT-MINK:0,0,1"])
            check(res.startswith("No se pudo") and "ENTRADA->SALIDA" in res,
                  "estimate con un enlace mal escrito: lo dice", res)
            res = await t("estimate", case="C1", reason="es de sim")
            check(res.startswith("No se pudo") and "caso de SIM" in res and "atsw-sis" in res,
                  "estimate de un caso de SIM: no", res)
            res = await t("open_case", case="C9")
            check(res.startswith("No se pudo") and "no es un caso" in res,
                  "open_case de uno que no está: lo dice drtran_gui antes de abrir", res)

    y = (proy / "proyecto.yaml").read_text(encoding="utf-8")
    check("el visón responde a la rata" in y, "el manifiesto tiene el caso con su razón", y[-400:])


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: python mt.py RAIZ_DEL_ARBOL")
        return 2
    with tempfile.TemporaryDirectory() as d:
        asyncio.run(corre(Path(sys.argv[1]).resolve(), Path(d)))
    print(f"\n{len(fallos)} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
