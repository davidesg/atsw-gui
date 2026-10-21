# atsw-gui-mcp

Los MCP a medida de ATSW: el asistente sobre los motores en C de la madre.
El diseño está en `docs/ESTUDIO-terminal-mcp.md`.

**Fase 0.** Tres servidores, `atsw-us`, `atsw-mt` y `atsw-sis`, que arrancan
y dicen su versión (`version`) y dónde han encontrado el proyecto y los
motores (`estado`). Todavía no analizan ni estiman.

- `ATSW_PROYECTO`: el proyecto. Si no está, se busca `proyecto.yaml` desde la
  carpeta de arranque hacia arriba.
- `ATSW_BIN`: la carpeta de los motores del paquete. Si no está, la de
  `ATSW_GUI`; y sólo al final, el PATH.

Instalar, con las versiones de `uv.lock`:

    uv sync --frozen --no-dev --no-editable                 # el núcleo
    uv sync --frozen --no-dev --no-editable --extra python  # con lo que queda en Python

Prueba de humo, sin cliente de LLM: `python tests/humo.py .venv/bin`.
La prueba a mano con los clientes, en `docs/ENSAYO-clientes.md`.
