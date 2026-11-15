"""atsw-us sobre los motores en C: una sesion MCP a pelo, sin cliente de LLM.

Sobre una copia del ejemplo de Jenkins y Alavi (docs/ejemplos), con ATSW_BIN
apuntando a los binarios del arbol (atsw-cli, fue, atsw_gui), el camino de un
modelo como lo recorreria el asistente:

  proyecto_resumen -> derivar -> especificar -> estimar -> dictamen,
  formales, anomalos, ficha -> razon, elegir

y lo que no se puede: tocar lo estimado, una ruta en vez de un nombre, una
ventana que no existe. Al final, el manifiesto dice «por: asistente».

    python us.py RAIZ_DEL_ARBOL

Sale con 0 si todo va bien.
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
    for b in (raiz / "gui/atsw-cli/atsw-cli", raiz / "engines/fue/bin/fue",
              raiz / "engines/art/bin/art",
              raiz / "gui/atsw/atsw_gui"):
        shutil.copy2(str(b) + exe, binarios / (b.name + exe))
    proy = w / "proy"
    shutil.copytree(raiz / "docs/ejemplos/jenkins_alavi", proy)
    env = dict(os.environ, ATSW_BIN=str(binarios), ATSW_PROYECTO=str(proy))
    params = StdioServerParameters(command=sys.executable, args=["-m", "atsw_mcp.us"],
                                   env=env, cwd=str(w))
    async with stdio_client(params) as (r, wr):
        async with ClientSession(r, wr) as s:
            ini = await s.initialize()
            check(len(ini.instructions or "") < 20_000, "las instrucciones caben en 20 KB",
                  str(len(ini.instructions or "")))
            ts = {t.name for t in (await s.list_tools()).tools}
            quiero = {"proyecto_resumen", "identificar", "derivar", "especificar", "estimar", "dictamen",
                      "formales", "anomalos", "raices", "ficha", "razon", "elegir", "ver"}
            check(quiero <= ts, "las herramientas del camino en C", str(sorted(ts)))

            async def t(nombre, **a):
                return texto(await s.call_tool(nombre, a))

            res = await t("proyecto_resumen")
            check("MINK" in res and "MUSKRAT" in res, "proyecto_resumen: las dos series", res)

            res = await t("identificar", serie="MINK", transformacion="0,1,0")
            check("propuesto" in res and "candidatos" in res, "identificar: art propone", res)

            res = await t("derivar", serie="MINK", razon="")
            check(res.startswith("No se pudo") and "razón" in res, "derivar sin razón: no", res)
            res = await t("derivar", serie="MINK", razon="un AR(1) en logaritmos y diferencias")
            check("m02" in res, "derivar: nace MINK/m02 de los datos", res)

            res = await t("especificar", serie="MINK", modelo="m02", transformacion="0,1,0",
                          arma="1,0,0,0", media="libre")
            check("fue lo acepta" in res, "especificar: log, una diferencia y un AR(1)", res)
            res = await t("especificar", serie="MINK", modelo="m02", intervenciones=["step"])
            check(res.startswith("No se pudo") and "queda como estaba" in res,
                  "especificar algo que no vale: no se cambia nada", res)

            res = await t("estimar", serie="MINK", modelo="m02")
            check("log-verosimilitud" in res, "estimar: fue termina", res)
            res = await t("estimar", serie="MINK", modelo="m02")
            check(res.startswith("No se pudo") and "ya está estimado" in res,
                  "estimar otra vez: no se reestima", res)
            res = await t("especificar", serie="MINK", modelo="m02", arma="2,0,0,0")
            check(res.startswith("No se pudo"), "lo estimado no se reescribe", res)

            res = await t("dictamen", serie="MINK", modelo="m02")
            check("Diagnosis de MINK/m02" in res, "dictamen", res)
            res = await t("formales", serie="MINK", modelo="m02", familia="integracion")
            check("ORDEN DE INTEGRACIÓN" in res and "Shin-Fuller" in res, "formales", res)
            res = await t("anomalos", serie="MINK", modelo="m02")
            check("Anómalos de MINK/m02" in res and "Q(" in res, "anomalos", res)
            res = await t("ficha", serie="MINK", modelo="m02")
            check("m02" in res and "asistente" in res, "ficha: el paso es del asistente", res)

            res = await t("razon", serie="MINK", modelo="m02", texto="el AR(1) basta")
            check(not res.startswith("No se pudo"), "razon", res)
            res = await t("elegir", serie="MINK", modelo="m02", razon="el mejor de los dos")
            check(not res.startswith("No se pudo"), "elegir", res)

            res = await t("dictamen", serie="../MINK", modelo="m02")
            check(res.startswith("No se pudo") and "no es un nombre" in res,
                  "una ruta en vez de una serie: rechazada", res)
            res = await t("ver", que="otra", serie="MINK")
            check(res.startswith("No se pudo") and "serie, media-dt" in res,
                  "ver una ventana que no existe: lo dice la madre", res)
            res = await t("ver", que="serie", serie="OTRA")
            check(res.startswith("No se pudo") and "no es una serie" in res,
                  "ver una serie que no está: lo dice la madre", res)

    y = (proy / "proyecto.yaml").read_text(encoding="utf-8")
    bloque = y[y.index("MINK/m02:"):]
    check("por: asistente" in bloque.split("\n  ", 1)[0] + bloque[:400],
          "el manifiesto: m02 es del asistente", bloque[:300])


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: python us.py RAIZ_DEL_ARBOL")
        return 2
    with tempfile.TemporaryDirectory() as d:
        asyncio.run(corre(Path(sys.argv[1]).resolve(), Path(d)))
    print(f"\n{len(fallos)} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
