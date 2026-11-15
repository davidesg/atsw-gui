"""atsw-sis sobre los binarios del arbol: una sesion MCP a pelo.

Sobre una copia del ejemplo de Jenkins y Alavi (MUSKRAT y MINK, con sus m01
estimados y su caso C1, que es de SIM): la puerta con las CCF, el VARMA(1,0)
con la tabla de drvarma C, el enlace restringido, y lo que no se puede (estimar
sin la puerta, un caso de MT, un enlace mal escrito). open_case de un caso que
existe abriria una ventana: aqui solo el que no existe.

    python sis.py RAIZ_DEL_ARBOL
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
    for b in (raiz / "gui/atsw-cli/atsw-cli", raiz / "gui/sim/sim_gui",
              raiz / "engines/drvarma/bin/drvarma"):
        shutil.copy2(str(b) + exe, binarios / (b.name + exe))
    proy = w / "proy"
    shutil.copytree(raiz / "docs/ejemplos/jenkins_alavi", proy)
    env = dict(os.environ, ATSW_BIN=str(binarios), ATSW_PROYECTO=str(proy))
    params = StdioServerParameters(command=sys.executable, args=["-m", "atsw_mcp.sis"],
                                   env=env, cwd=str(w))
    async with stdio_client(params) as (r, wr):
        async with ClientSession(r, wr) as s:
            ini = await s.initialize()
            check("atsw-sis" in (ini.instructions or "") and len(ini.instructions) < 20_000,
                  "las instrucciones: de atsw-sis, en menos de 20 KB", str(len(ini.instructions or "")))
            ts = {t.name for t in (await s.list_tools()).tools}
            check({"guion_map", "load_pre", "run_gate", "estimate", "open_case"} <= ts,
                  "las herramientas, con los nombres de sima", str(sorted(ts)))

            async def t(nombre, **a):
                return texto(await s.call_tool(nombre, a))

            res = await t("estimate", case="C1", reason="sin puerta", p=1, q=0)
            check(res.startswith("No se pudo") and "caso-puerta" in res,
                  "estimate sin la puerta: antes, la puerta", res)
            res = await t("run_gate", case="C1")
            check("GATE: PASSED" in res and "MUSKRAT<-MINK" in res,
                  "run_gate: la puerta de drvarma y las CCF del par", res)
            res = await t("estimate", case="C1", reason="el VARMA(1,0) completo", p=1, q=0)
            check("C1/c00" in res and "-560.335924" in res and "AR1[MINK<-MUSKRAT]" in res,
                  "estimate: c00, la logL de drvarma C y la tabla con los cruzados", res)
            res = await t("estimate", case="C1", reason="solo MINK<-MUSKRAT", p=1, q=0,
                          links="MINK<-MUSKRAT", from_run="c00")
            check("C1/c01" in res and "-564.754457" in res,
                  "estimate links: c01, el enlace restringido", res)
            res = await t("estimate", case="C1", reason="mal", p=1, q=0, links="MINK-MUSKRAT")
            check(res.startswith("No se pudo") and "A<-B" in res,
                  "estimate con un enlace mal escrito: lo dice", res)
            res = await t("load_pre", inputs=["MINK/m01"], reason="uno solo")
            check(res.startswith("No se pudo") and "dos series" in res, "load_pre con una serie: no", res)
            res = await t("load_pre", inputs=["MINK/m01", "MUSKRAT/m01"], reason="otro orden de Cholesky")
            check("C2" in res, "load_pre: nace C2, de SIM", res)
            res = await t("open_case", case="C9")
            check(res.startswith("No se pudo"), "open_case de uno que no está: lo dice", res)

    y = (proy / "proyecto.yaml").read_text(encoding="utf-8")
    check("C1/c00" in y and "otro orden de Cholesky" in y,
          "el manifiesto tiene las corridas y el caso nuevo", y[-400:])


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: python sis.py RAIZ_DEL_ARBOL")
        return 2
    with tempfile.TemporaryDirectory() as d:
        asyncio.run(corre(Path(sys.argv[1]).resolve(), Path(d)))
    print(f"\n{len(fallos)} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
