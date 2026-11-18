"""EL ASISTENTE DEL VIDEO, sin modelo de lenguaje: un cliente MCP con un guion.

El video del asistente (escenas-asistente/) ensena lo que ve la madre cuando
el asistente trabaja en el proyecto. Lo que contesta un cliente de LLM
cambia de una vez a otra y no se puede grabar con un guion; las llamadas que
hace, si. Este programa hace esas llamadas, a los servidores DE VERDAD que
la escena acaba de instalar, desde la carpeta del proyecto, como lo haria
el cliente lanzado en la terminal. Lo que escriben lleva «por: asistente»
igual que si las hubiera pedido un modelo.

La escena lo lanza (orden «lanza» de escenas.c) con el Python del asistente
instalado:

    @ASISTENTE@/.venv/bin/python @MANUAL@/video/asistente.py PROYECTO PASO

    PASO  propone  el modelo de las lineas aereas: un modelo nuevo de la
                   serie, su especificacion y su estimacion (new_version,
                   write_inp, estimate_and_diagnose)
          corrige  un hijo suyo con las dos intervenciones del analisis
                   univariante (escenas-es-core): el IVA de 9/2012 y el
                   escalon de 10/2007, con un AR(3)
          elige    el hijo, con su razon (guion_adopt)

Los numeros que dicen las razones son los que da hoy fue sobre los datos del
manual (docs/manual/datos/es_core.csv, 2002-2019).

Lo que contestan los servidores queda en PROYECTO/../asistente-PASO.log, por
si la escena falla.
"""

from __future__ import annotations

import asyncio
import os
import re
import sys
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

SERIE = "subyacente"
MUESTRA = "2002-2019"


def texto(res) -> str:
    return "".join(getattr(c, "text", "") for c in res.content)


async def main(proyecto: Path, paso: str) -> None:
    exe = Path(sys.executable).with_name("atsw-us")
    nota = proyecto / ".asistente-video"          # el modelo que propuso
    log = open(proyecto.parent / f"asistente-{paso}.log", "w")
    params = StdioServerParameters(command=str(exe), args=[], env=dict(os.environ),
                                   cwd=str(proyecto))

    async def llama(s, herramienta, **args):
        t = texto(await s.call_tool(herramienta, args))
        print(f"== {herramienta} {args}\n{t}\n", file=log, flush=True)
        if t.startswith("No se pudo"):
            raise SystemExit(t)
        return t

    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            if paso == "propone":
                m = re.search(r"\bm\d+\b", await llama(s, "new_version", series=SERIE, sample=MUESTRA,
                                 reason="El modelo de las líneas aéreas como punto de partida: "
                                        "logaritmos, d = 1 y D = 1, un MA regular y otro "
                                        "estacional.")).group(0)
                await llama(s, "write_inp", series=SERIE, model=m, sample=MUESTRA,
                            transform="0,1,1", arma="0,1,0,1")
                await llama(s, "estimate_and_diagnose", series=SERIE, model=m, sample=MUESTRA)
                nota.write_text(m)
            elif paso == "corrige":
                padre = nota.read_text().strip()
                m = re.search(r"\bm\d+(?=\.inp)", await llama(s, "new_version", series=SERIE, sample=MUESTRA,
                                 parent=padre,
                                 reason=f"Los residuos de {padre} son blancos, pero con curtosis 4.1: "
                                        "unos pocos atípicos. Son el IVA de septiembre de 2012 y "
                                        "el escalón de octubre de 2007; con ellos, un AR(3) en "
                                        "vez del MA regular.")).group(0)
                await llama(s, "write_inp", series=SERIE, model=m, sample=MUESTRA,
                            arma="3,0,0,1", interventions=["step 9 2012", "step 10 2007"])
                await llama(s, "estimate_and_diagnose", series=SERIE, model=m, sample=MUESTRA)
                nota.write_text(m)
            elif paso == "elige":
                m = nota.read_text().strip()
                await llama(s, "guion_adopt", series=SERIE, model=m, sample=MUESTRA,
                            why="Residuos blancos, y la curtosis baja de 4.1 a 0.8 con las dos "
                                "intervenciones. La normalidad aún falla por poco (p = 0.012): "
                                "lo dejo dicho para que el analista lo revise.")
            else:
                raise SystemExit(f"paso desconocido: {paso}")


if __name__ == "__main__":
    asyncio.run(main(Path(sys.argv[1]), sys.argv[2]))
