"""atsw-us sobre los motores en C: una sesion MCP a pelo, sin cliente de LLM.

Sobre una copia del ejemplo de Jenkins y Alavi (docs/ejemplos), con ATSW_BIN
apuntando a los binarios del arbol (atsw-cli, fue, atsw_gui), el camino de un
modelo como lo recorreria el asistente:

  guion_map -> new_version -> write_inp -> estimate_and_diagnose -> diagnose,
  formal_tests, residual_outlier_scan, full_report -> guion_annotate, guion_adopt

y lo que no se puede: tocar lo estimado, una ruta en vez de un nombre, una
ventana que no existe. Al final, el manifiesto dice «por: asistente».

    python us.py RAIZ_DEL_ARBOL

Sale con 0 si todo va bien.
"""

from __future__ import annotations

import asyncio
import importlib.util
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
              raiz / "engines/art/bin/art", raiz / "engines/fuf/bin/fuf",
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
            quiero = {"guion_map", "identification_analysis", "new_version", "write_inp",
                      "estimate_and_diagnose", "diagnose", "formal_tests", "residual_outlier_scan",
                      "ar_factorization", "factorized_model", "model_equation_display", "full_report", "guion_annotate", "guion_adopt", "plot",
                      "generate_forecast", "boxcox_analysis", "intervention_ladder"}
            check(quiero <= ts, "las herramientas del camino en C", str(sorted(ts)))

            async def t(nombre, **a):
                return texto(await s.call_tool(nombre, a))

            res = await t("guion_map")
            check("MINK" in res and "MUSKRAT" in res, "guion_map: las dos series", res)

            res = await t("identification_analysis", series="MINK", transform="0,1,0")
            check("propuesto" in res and "candidatos" in res, "identification_analysis: art propone", res)

            # EL EXTRA PYTHON: con él, el texto de art y su línea de coste con
            # via: python; sin él (la CI), la herramienta lo dice y no hace nada.
            hay_art = importlib.util.find_spec("art") is not None
            res = await t("boxcox_analysis", series="MINK")
            if hay_art:
                check("Box-Cox" in res and "λ=0" in res, "boxcox_analysis: la de art", res)
                res = await t("intervention_ladder", series="MINK", model="m01")
                check("Escalera de Ockham" in res and "Peldaño 1" in res,
                      "intervention_ladder: la escalera de art sobre MINK/m01", res)
                costes = (proy / "costes.csv").read_text(encoding="utf-8")
                check(",python," in costes and "MINK/m01,escalera" in costes,
                      "el extra deja su línea en costes.csv, via python", costes[-400:])
            else:
                check(res.startswith("No se pudo") and "extra Python" in res,
                      "boxcox_analysis sin el extra: lo dice", res)
            res = await t("intervention_ladder", series="MINK", model="m00")
            check(res.startswith("No se pudo"), "intervention_ladder de los datos: no", res)

            res = await t("new_version", series="MINK", reason="")
            check(res.startswith("No se pudo") and "razón" in res, "new_version sin razón: no", res)
            res = await t("new_version", series="MINK", reason="un AR(1) en logaritmos y diferencias")
            check("m02" in res, "new_version: nace MINK/m02 de los datos", res)

            res = await t("write_inp", series="MINK", model="m02", transform="0,1,0",
                          arma="1,0,0,0", mean="free")
            check("fue lo acepta" in res, "write_inp: log, una diferencia y un AR(1)", res)
            res = await t("write_inp", series="MINK", model="m02", n_harmonics=1)
            check(res.startswith("No se pudo") and "anual" in res,
                  "write_inp con armónicos en una serie anual: no", res)
            res = await t("write_inp", series="MINK", model="m02", interventions=["step"])
            check(res.startswith("No se pudo") and "queda como estaba" in res,
                  "write_inp algo que no vale: no se cambia nada", res)

            res = await t("estimate_and_diagnose", series="MINK", model="m02")
            check("log-verosimilitud" in res, "estimate_and_diagnose: fue termina", res)
            res = await t("estimate_and_diagnose", series="MINK", model="m02")
            check(res.startswith("No se pudo") and "ya está estimado" in res,
                  "estimate_and_diagnose otra vez: no se reestima", res)
            res = await t("write_inp", series="MINK", model="m02", arma="2,0,0,0")
            check(res.startswith("No se pudo"), "lo estimado no se reescribe", res)

            res = await t("model_equation_display", series="MINK", model="m02")
            check("MODELO ESTIMADO: MINK/m02" in res and "t =" in res and "log-verosimilitud" in res,
                  "model_equation_display: el bloque del .out con sus t", res)
            res = await t("diagnose", series="MINK", model="m02")
            check("Diagnosis de MINK/m02" in res, "diagnose", res)
            res = await t("formal_tests", series="MINK", model="m02", family="integration")
            check("ORDEN DE INTEGRACIÓN" in res and "Shin-Fuller" in res, "formal_tests", res)
            res = await t("residual_outlier_scan", series="MINK", model="m02")
            check("Anómalos de MINK/m02" in res and "Q(" in res, "residual_outlier_scan", res)
            res = await t("full_report", series="MINK", model="m02")
            check("m02" in res and "asistente" in res, "full_report: el paso es del asistente", res)

            res = await t("factorized_model", series="MINK", model="m02", reason="")
            check(res.startswith("No se pudo") and "razón" in res, "factorized_model sin razón: no", res)
            res = await t("factorized_model", series="MINK", model="m02", reason="un AR(1)")
            check(res.startswith("No se pudo") and "irreducible" in res,
                  "factorized_model de un AR(1): ya está factorizado", res)

            res = await t("generate_forecast", series="MINK", model="m02")
            check("FORECAST REPORT" in res and "1912" in res, "generate_forecast: la tabla de fuf, anual", res)

            res = await t("guion_annotate", series="MINK", model="m02", text="el AR(1) basta")
            check(not res.startswith("No se pudo"), "guion_annotate", res)
            res = await t("guion_adopt", series="MINK", model="m02", why="el mejor de los dos")
            check(not res.startswith("No se pudo"), "guion_adopt", res)

            res = await t("diagnose", series="../MINK", model="m02")
            check(res.startswith("No se pudo") and "no es un nombre" in res,
                  "una ruta en vez de una serie: rechazada", res)
            res = await t("formal_tests", series="MINK", model="m02", family="integracion")
            check(res.startswith("No se pudo") and "integration" in res,
                  "formal_tests con la familia en español: dice las suyas", res)
            res = await t("plot", what="otra", series="MINK")
            check(res.startswith("No se pudo") and "series, mean-sd" in res,
                  "plot de una ventana que no existe: dice las que hay", res)
            res = await t("plot", what="series", series="OTRA")
            check(res.startswith("No se pudo") and "no es una serie" in res,
                  "plot de una serie que no está: lo dice la madre", res)

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
