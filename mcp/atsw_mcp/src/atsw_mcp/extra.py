"""El extra Python, en un proceso aparte (docs/ESTUDIO-terminal-mcp.md §2.2).

Lo que no tiene equivalente en C --la λ de Box-Cox, la escalera de
intervenciones-- es art-python, del extra [python]. Se corre AQUI, en un
proceso hijo, y no dentro del servidor, por tres razones:

- el canal MCP es la salida estandar del servidor, y art (con matplotlib y
  compania) escribe por ella: dentro del servidor lo romperia;
- importar art tarda unos segundos, y el servidor arranca sin pagarlos;
- lo que art estima lo estima con su fue en Python: si se cuelga o revienta,
  revienta el hijo, con su plazo, y no el servidor.

Se llama a LA MISMA herramienta de art (art.mcp_server), con la ruta que da
atsw-cli proyecto ruta: el texto es el de art, sin rehacer. Las figuras no
se devuelven (el asistente trabaja en texto; el analista ve las ventanas de
la madre) y se escriben en un temporal que se borra.

    python -m atsw_mcp.extra ORDEN JSON

Escribe por la salida estandar UNA linea JSON: {"texto": ..., "cpu": s}, con
el tiempo de CPU del hijo para costes.csv (via: python). Estado 0, o 1 con
el motivo en "error".
"""

from __future__ import annotations

import io
import json
import os
import re
import sys
import tempfile
import time
from contextlib import redirect_stdout

# Lo que se puede pedir: la herramienta de art y sus argumentos admitidos.
ORDENES = {
    "boxcox": ("boxcox_analysis", {"inp_path"}),
    "escalera": ("intervention_ladder", {"inp_path", "at", "ventana", "threshold", "umbral_vecino"}),
    "forma": ("suggest_intervention_form", {"inp_path", "output_path", "date", "form", "n_omega",
                                            "n_delta", "rehacer", "modo"}),
}

_FIGURA = re.compile(r"^\s*\*Figura:.*$", re.M)


def corre(orden: str, args: dict) -> str:
    nombre, admitidos = ORDENES[orden]
    sobra = set(args) - admitidos
    if sobra:
        raise ValueError(f"argumentos que {nombre} no tiene: {sorted(sobra)}")
    with tempfile.TemporaryDirectory() as figs:
        os.environ["ART_NO_VIEWER"] = "1"
        os.environ["ART_FIG_DIR"] = figs
        ruido = io.StringIO()
        with redirect_stdout(ruido):          # lo que art imprima, fuera del JSON
            from art import mcp_server as art
            res = getattr(art, nombre)(**args)
    texto = "".join(getattr(c, "text", "") for c in res)
    return _FIGURA.sub("", texto).strip()


def main() -> int:
    if len(sys.argv) != 3 or sys.argv[1] not in ORDENES:
        print(json.dumps({"error": f"uso: python -m atsw_mcp.extra {{{','.join(ORDENES)}}} JSON"}))
        return 2
    try:
        texto = corre(sys.argv[1], json.loads(sys.argv[2]))
    except Exception as e:                    # el motivo, para el asistente
        print(json.dumps({"error": f"{type(e).__name__}: {e}", "cpu": time.process_time()}))
        return 1
    print(json.dumps({"texto": texto, "cpu": time.process_time()}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
