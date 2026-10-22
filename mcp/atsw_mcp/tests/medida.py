"""Lo que pesa instalar atsw-gui-mcp en esta plataforma (fase 0, §6.2).

Para cada distribucion instalada en el entorno, busca en uv.lock la rueda
que eligio uv para este interprete (la de mejor etiqueta) y suma lo que se
descarga. Dice tambien que paquetes no tienen rueda binaria aqui y caen en
la pura Python (py3-none-any) o en el fuente, que es lo que hay que saber de
cada plataforma antes de prometer nada.

    uv run --no-project --python ENTORNO/python --with packaging==24.2 \
        python medida.py UV_LOCK SITE_PACKAGES [TITULO]

Escribe una tabla en Markdown por la salida estandar.
"""

from __future__ import annotations

import sys
import tomllib
from importlib import metadata
from pathlib import Path

from packaging.tags import sys_tags
from packaging.utils import canonicalize_name, parse_wheel_filename


def main() -> int:
    lock = tomllib.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    site = sys.argv[2]
    titulo = sys.argv[3] if len(sys.argv) > 3 else "instalacion"
    orden = {t: i for i, t in enumerate(sys_tags())}

    por_nombre = {}
    for p in lock.get("package", []):
        por_nombre[(canonicalize_name(p["name"]), p["version"])] = p

    filas, total, avisos = [], 0, []
    for d in metadata.distributions(path=[site]):
        n, v = canonicalize_name(d.metadata["Name"]), d.version
        p = por_nombre.get((n, v))
        if p is None or {"editable", "directory", "virtual"} & set(p.get("source", {})):
            continue  # el propio atsw-gui-mcp, instalado desde la carpeta
        mejor, rango = None, None
        for w in p.get("wheels", []):
            _, _, _, tags = parse_wheel_filename(w["url"].rsplit("/", 1)[-1])
            r = min((orden[t] for t in tags if t in orden), default=None)
            if r is not None and (rango is None or r < rango):
                mejor, rango = w, r
        if mejor is None:
            sd = p.get("sdist", {})
            filas.append((n, v, sd.get("size", 0), "fuente (compila)"))
            total += sd.get("size", 0)
            avisos.append(f"{n} {v}: sin rueda para esta plataforma, se compila")
            continue
        nombre = mejor["url"].rsplit("/", 1)[-1]
        pura = nombre.endswith("-py3-none-any.whl")
        binarias = any(not w["url"].endswith("-none-any.whl") for w in p.get("wheels", []))
        if pura and binarias:
            avisos.append(f"{n} {v}: hay ruedas binarias para otras plataformas, "
                          "aqui cae en la pura Python")
        filas.append((n, v, mejor["size"], nombre))
        total += mejor["size"]

    filas.sort(key=lambda f: -f[2])
    print(f"#### {titulo}: {len(filas)} paquetes, {total / 2**20:.1f} MB descargados\n")
    print("| paquete | version | MB | rueda |")
    print("|---|---|---|---|")
    for n, v, s, w in filas[:8]:
        print(f"| {n} | {v} | {s / 2**20:.1f} | `{w}` |")
    if len(filas) > 8:
        resto = sum(f[2] for f in filas[8:])
        print(f"| (otros {len(filas) - 8}) | | {resto / 2**20:.1f} | |")
    for a in avisos:
        print(f"\n- **{a}**")
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
