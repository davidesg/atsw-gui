"""atsw-mt sobre los binarios del arbol: una sesion MCP a pelo.

Sobre una copia del ejemplo de Jenkins y Alavi (MUSKRAT y MINK, con sus m01
estimados): identificar el enlace, crear el caso y lo que no se puede (una
clave que no es SERIE/MODELO, un caso que no esta). abrir_caso de un caso que
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
            check({"proyecto_resumen", "identificar_enlace", "caso_nuevo", "estimar_caso",
                   "abrir_caso"} <= ts,
                  "las herramientas", str(sorted(ts)))

            async def t(nombre, **a):
                return texto(await s.call_tool(nombre, a))

            res = await t("identificar_enlace", salida="MINK/m01", entradas=["MUSKRAT/m01"])
            check("Identificación de MINK/m01" in res and "MUSKRAT/m01" in res and "Hosking" in res,
                  "identificar_enlace: la CCF preblanqueada y la exogeneidad", res)
            res = await t("identificar_enlace", salida="MINK", entradas=["MUSKRAT/m01"])
            check(res.startswith("No se pudo") and "SERIE/MODELO" in res,
                  "una salida sin modelo: es SERIE/MODELO", res)
            res = await t("caso_nuevo", salida="MINK/m01", entradas=["MUSKRAT/m01"], razon="")
            check(res.startswith("No se pudo") and "razón" in res, "caso_nuevo sin razón: no", res)
            res = await t("caso_nuevo", salida="MINK/m01", entradas=["MUSKRAT/m01"],
                          razon="el visón responde a la rata", titulo="MINK <- MUSKRAT")
            check("C2" in res, "caso_nuevo: nace C2 (C1 ya estaba)", res)
            res = await t("estimar_caso", caso="C2", razon="que drtran identifique")
            check("C2/c00" in res and "terminado" in res, "estimar_caso: c00, drtran identifica la red", res)
            res = await t("estimar_caso", caso="C2", razon="un omega más", desde="c00",
                          enlaces=["MUSKRAT->MINK:0,0,1"])
            check("C2/c01" in res and "enlaces dados" in res, "estimar_caso: c01, con el enlace dado", res)
            res = await t("estimar_caso", caso="C2", razon="mal", enlaces=["MUSKRAT-MINK:0,0,1"])
            check(res.startswith("No se pudo") and "ENTRADA->SALIDA" in res,
                  "estimar_caso con un enlace mal escrito: lo dice", res)
            res = await t("estimar_caso", caso="C1", razon="es de sim")
            check(res.startswith("No se pudo") and "sim_gui" in res,
                  "estimar_caso de un caso de SIM: no", res)
            res = await t("abrir_caso", caso="C9")
            check(res.startswith("No se pudo") and "no es un caso" in res,
                  "abrir_caso de uno que no está: lo dice drtran_gui antes de abrir", res)

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
